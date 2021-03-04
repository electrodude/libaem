#define _DEFAULT_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

#include <aem/compiler.h>
#include <aem/log.h>
#include <aem/net.h>
#include <aem/pmcrcu.h>
#include <aem/streams.h>
#include <aem/translate.h>

struct server_connection
{
	struct aem_net_conn conn;

	struct aem_stringbuf name;

	struct aem_stream_transducer tr;

	struct rcu_head rcu_head;

	int line_state;
};

int should_exit = 0;

static void conn_free_rcu(struct rcu_head *rcu_head)
{
	aem_assert(rcu_head);

	struct server_connection *conn = aem_container_of(rcu_head, struct server_connection, rcu_head);

	aem_logf_ctx(AEM_LOG_DEBUG, "%p", conn);

	aem_stream_transducer_dtor_rcu(&conn->tr);
	aem_net_conn_dtor(&conn->conn);
	aem_stringbuf_dtor(&conn->name);

	free(conn);
}
static void conn_free(struct aem_net_conn *sock)
{
	aem_assert(sock);

	struct server_connection *conn = aem_container_of(sock, struct server_connection, conn);

	aem_logf_ctx(AEM_LOG_DEBUG, "%s: fd %d", aem_stringbuf_get(&conn->name), conn->conn.sock.evt.fd);

	aem_stream_sink_detach(&conn->tr.sink);
	aem_stream_source_detach(&conn->tr.source);

	aem_net_sock_close(&conn->conn.sock);

	call_rcu(&conn->rcu_head, conn_free_rcu);
}

static void process_data(struct aem_stream_transducer *tr, struct aem_stringbuf *out, struct aem_stringslice *in, int flags)
{
	aem_assert(tr);
	aem_assert(out);
	aem_assert(in);

	if (!aem_stringslice_ok(*in))
		return;

	struct server_connection *conn = aem_container_of(tr, struct server_connection, tr);

	struct aem_stringslice curr = *in;
	struct aem_stringslice line = aem_stringslice_match_line_multi(&curr, &conn->line_state, flags & AEM_STREAM_FIN);

	//aem_logf_ctx(AEM_LOG_DEBUG2, "%s: line %zd bytes, flags %d", aem_stringbuf_get(&conn->name), aem_stringslice_len(line), flags);

	if (line.start) {
		struct aem_stringbuf msg = {0};

		aem_stringbuf_puts(&msg, "Line: ");
		aem_string_escape(&msg, line);
		aem_stringbuf_puts(&msg, "\n");

		//aem_logf_ctx(AEM_LOG_INFO, "%s", aem_stringbuf_get(&msg));
		aem_stringbuf_append(out, &msg);

		aem_stringbuf_dtor(&msg);
	}

	if (flags & AEM_STREAM_FIN) {
		aem_stringbuf_puts(out, "EOF\n");
	}

	if (aem_stringslice_eq(line, "shutdown"))
		should_exit = 1;

	if (aem_stringslice_eq(line, "crash"))
		aem_assert(!"Crashing due to user command");

	*in = curr;

	return;
}

static void conn_on_sock_close(struct aem_net_sock *sock)
{
	aem_assert(sock);
	struct aem_net_conn *conn2 = aem_container_of(sock, struct aem_net_conn, sock);
	struct server_connection *conn = aem_container_of(conn2, struct server_connection, conn);
	aem_logf_ctx(AEM_LOG_NOTICE, "Socket closed");
	conn_free(&conn->conn);
}
static void conn_on_tr_close(struct aem_stream_transducer *tr)
{
	aem_assert(tr);
	struct server_connection *conn = aem_container_of(tr, struct server_connection, tr);
	aem_logf_ctx(AEM_LOG_NOTICE, "Connection closed");
	conn_free(&conn->conn);
}

static struct aem_net_conn *conn_new(struct aem_net_server *server, struct sockaddr *addr, socklen_t len)
{
	struct server_connection *conn = malloc(sizeof(*conn));
	if (!conn) {
		aem_logf_ctx(AEM_LOG_ERROR, "malloc failed: %s", strerror(errno));
		return NULL;
	}

	aem_stringbuf_init(&conn->name);

	aem_net_conn_init(&conn->conn);
	conn->line_state = 0;
	conn->conn.sock.on_close = conn_on_sock_close;

	aem_stream_transducer_init(&conn->tr);
	conn->tr.go = process_data;
	conn->tr.on_close = conn_on_tr_close;

	return &conn->conn;
}

static void conn_setup(struct aem_net_conn *sock, struct aem_net_server *server, struct sockaddr *addr, socklen_t len)
{
	aem_assert(server);
	aem_assert(sock);

	struct server_connection *conn = aem_container_of(sock, struct server_connection, conn);

	const int sz = 8192;
	if (setsockopt(sock->sock.evt.fd, SOL_SOCKET, SO_SNDBUF, &sz, sizeof(sz)) < 0) {
		aem_logf_ctx(AEM_LOG_ERROR, "setsockopt(%d, SO_SNDBUF, %zd): %s", sock->sock.evt.fd, sz, strerror(errno));
	}
	if (setsockopt(sock->sock.evt.fd, SOL_SOCKET, SO_RCVBUF, &sz, sizeof(sz)) < 0) {
		aem_logf_ctx(AEM_LOG_ERROR, "setsockopt(%d, SO_RCVBUF, %zd): %s", sock->sock.evt.fd, sz, strerror(errno));
	}

	switch (addr->sa_family) {
		case AF_INET:
		case AF_INET6:
		{
			char host[64];
			char serv[64];
			int gni_rc = getnameinfo(addr, len, host, sizeof(host), serv, sizeof(serv), NI_NUMERICSERV);
			if (gni_rc < 0) {
				aem_logf_ctx(AEM_LOG_ERROR, "getnameinfo(): %s", gai_strerror(gni_rc));
				break;
			}
			switch (addr->sa_family) {
				case AF_INET:
					aem_logf_ctx(AEM_LOG_NOTICE, "srv %p, fd %d: %s:%s", server, conn->conn.sock.evt.fd, host, serv);
					aem_stringbuf_printf(&conn->name, "%s:%s", host, serv);
					break;
				case AF_INET6:
					aem_logf_ctx(AEM_LOG_NOTICE, "srv %p, fd %d: [%s]:%s", server, conn->conn.sock.evt.fd, host, serv);
					aem_stringbuf_printf(&conn->name, "[%s]:%s", host, serv);
					break;
			}
			break;
		}
		default:
			aem_logf_ctx(AEM_LOG_INFO, "srv %p, conn %d: unknown AF", server, conn->conn.sock.evt.fd);
			break;
	}

	aem_stream_connect(&conn->conn.rx, &conn->tr.sink);
	aem_stream_connect(&conn->tr.source, &conn->conn.tx);
}

static void on_sigint(int sig, siginfo_t *si, void *ucontext)
{
	(void)sig;
	(void)si;
	(void)ucontext;
	should_exit = 1;
}

void usage(const char *cmd)
{
	fprintf(stderr, "Usage: %s [<options>] <address>\n", cmd);
	fprintf(stderr, "   %-20s%s\n", "[-p<port>]", "specify port");
	fprintf(stderr, "   %-20s%s\n", "[-b<port>]", "specify local bind address");
	fprintf(stderr, "   %-20s%s\n", "[-U]", "Unix domain socket");
	fprintf(stderr, "   %-20s%s\n", "[-l<logfile>]", "set log file");
	fprintf(stderr, "   %-20s%s\n", "[-v<loglevel>]", "set log level (default: debug)");
	fprintf(stderr, "   %-20s%s\n", "[-h]", "show this help");
}

int main(int argc, char **argv)
{
	aem_log_stderr();

	const char *bind_path = "localhost";
	const char *service = "12345";
	int unix = 0;

	int opt;
	while ((opt = getopt(argc, argv, "b:p:Ul:v::h")) != -1) {
		switch (opt) {
			case 'b': bind_path = optarg; break;
			case 'p': service = optarg; break;
			case 'U': unix = 1; break;
			case 'l': aem_log_fopen(optarg); break;
			case 'v': aem_log_level_parse_set(optarg); break;
			case 'h':
			default:
				usage(argv[0]);
				exit(1);
		}
	}

	argv += optind;
	argc -= optind;

	if (unix && service) {
		aem_logf_ctx(AEM_LOG_FATAL, "You can't specify both -U and -p.");
		exit(1);
	}

	struct aem_poll poller;
	aem_poll_init(&poller);

	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = on_sigint;
	if (sigaction(SIGINT, &sa, NULL) == -1)
		aem_logf_ctx(AEM_LOG_ERROR, "Failed to set SIGINT handler: %s\n", strerror(errno));
	if (sigaction(SIGTERM, &sa, NULL) == -1)
		aem_logf_ctx(AEM_LOG_ERROR, "Failed to set SIGTERM handler: %s\n", strerror(errno));

	struct aem_net_server srv;

	aem_net_server_init(&srv);
	srv.sock.poller = &poller;
	aem_net_sock_inet(&srv.sock, bind_path, service, 1);
	srv.conn_new = conn_new;
	srv.setup = conn_setup;
	aem_net_listen(&srv, 4096);

	int i = 0;

	while (poller.n) {
		aem_logf_ctx(AEM_LOG_NOTICE, "iteration %d", i);
		aem_poll_poll(&poller);
		if (should_exit) {
			aem_logf_ctx(AEM_LOG_NOTICE, "Closing all connections: %zd active fds\n", poller.n);
			aem_poll_hup_all(&poller);
		}
		synchronize_rcu(); // Call deferred destructors.
		i++;
	}

	aem_net_server_dtor(&srv);

	aem_poll_dtor(&poller);

	return 0;
}
