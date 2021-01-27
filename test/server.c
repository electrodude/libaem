#define _DEFAULT_SOURCE
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <aem/compiler.h>
#include <aem/log.h>
#include <aem/net.h>
#include <aem/pmcrcu.h>

int should_exit;

struct server_connection
{
	struct aem_net_conn conn;

	struct aem_stringbuf name;

	struct aem_stream_sink sink;
	struct aem_stream_source source;

	struct rcu_head rcu_head;

	int (*process_data)(struct server_connection *conn, struct aem_stringbuf *out, struct aem_stringslice *in, int flags);

	int line_state;
};

static void conn_free_rcu(struct rcu_head *rcu_head)
{
	aem_assert(rcu_head);

	struct server_connection *conn = aem_container_of(rcu_head, struct server_connection, rcu_head);

	aem_logf_ctx(AEM_LOG_DEBUG, "%p\n", conn);

	aem_stream_sink_dtor(&conn->sink);
	aem_stream_source_dtor(&conn->source);
	aem_net_conn_dtor(&conn->conn);
	aem_stringbuf_dtor(&conn->name);

	free(conn);
}
static void conn_free(struct aem_net_conn *sock)
{
	aem_assert(sock);

	struct server_connection *conn = aem_container_of(sock, struct server_connection, conn);

	aem_logf_ctx(AEM_LOG_DEBUG, "%s: fd %d\n", aem_stringbuf_get(&conn->name), conn->conn.sock.evt.fd);

	aem_stream_sink_detach(&conn->sink);
	aem_stream_source_detach(&conn->source);

	aem_net_sock_close(&conn->conn.sock);

	call_rcu(&conn->rcu_head, conn_free_rcu);
}

static int process_data(struct server_connection *conn, struct aem_stringbuf *out, struct aem_stringslice *in, int flags)
{
	aem_assert(conn);
	aem_assert(out);
	aem_assert(in);

	if (!aem_stringslice_ok(*in))
		return 0;

	int rc = 0;

	struct aem_stringslice curr = *in;
	struct aem_stringslice line = aem_stringslice_match_line_multi(&curr, &conn->line_state, flags & AEM_STREAM_FIN);

	aem_logf_ctx(AEM_LOG_DEBUG2, "%s: line %zd bytes, flags %d\n", aem_stringbuf_get(&conn->name), aem_stringslice_len(line), flags);

	if (line.start) {
		struct aem_stringbuf msg = {0};

		aem_stringbuf_puts(&msg, "Line: ");
		aem_stringbuf_putss_quote(&msg, line);
		aem_stringbuf_puts(&msg, "\n");

		//aem_logf_ctx(AEM_LOG_INFO, "%s", aem_stringbuf_get(&msg));
		aem_stringbuf_append(out, &msg);

		aem_stringbuf_dtor(&msg);
	} else if (aem_stringslice_ok(*in)) {
		rc |= AEM_STREAM_NEED_MORE;
	}

	aem_assert(conn->source.stream);
	// Fails if writing side is closed.
	if (!conn->source.stream->sink)
		aem_logf_ctx(AEM_LOG_WARN, "TX closed\n");

	if (flags & AEM_STREAM_FIN) {
		aem_stringbuf_puts(out, "EOF\n");
	}

	if (aem_stringslice_eq(line, "shutdown"))
		should_exit = 1;

	if (aem_stringslice_eq(line, "crash"))
		aem_abort();

	*in = curr;

	return rc;
}

static int conn_consume(struct aem_stream_sink *sink)
{
	aem_assert(sink);
	aem_assert(sink->stream);

	int flags = sink->flags;

	struct server_connection *conn = aem_container_of(sink, struct server_connection, sink);

	int flags_source = 0;

	struct aem_stringbuf *out = aem_stream_provide_begin(&conn->source, 0);

	if (!out) {
		aem_logf_ctx(AEM_LOG_BUG, "TX disconnected\n");
		flags_source = AEM_STREAM_FIN;
		goto disconnected;
	}

	struct aem_stringslice in = aem_stream_consume_begin(sink);
	if (!in.start)
		return flags_source;

	aem_logf_ctx(AEM_LOG_DEBUG2, "%zd bytes, flags %d\n", aem_stringslice_len(in), flags);

	while (aem_stringslice_ok(in)) {
		struct aem_stringslice in_prev = in;

		aem_assert(conn->process_data);
		int rc = conn->process_data(conn, out, &in, flags);

		if (!rc) {
			aem_assert(in.start != in_prev.start); // Callback didn't make progress without excuse.
		}

		aem_logf_ctx(AEM_LOG_DEBUG3, "%zd bytes remain; rc %d\n", aem_stringslice_len(in), rc);

		if (rc) {
			flags_source = rc;
			break;
		}
	}

	if (flags & AEM_STREAM_FIN) {
		aem_assert(conn->process_data);
		int rc2 = conn->process_data(conn, out, &in, flags);
	}

	aem_logf_ctx(AEM_LOG_DEBUG, "done: %zd bytes remain\n", aem_stringslice_len(in));

	aem_stream_consume_end(sink, in);

	aem_stream_provide_end(&conn->source);

disconnected:
	if (flags & AEM_STREAM_FIN) {
		aem_logf_ctx(AEM_LOG_INFO, "EOF\n");
		conn_free(&conn->conn);
	}

	return flags_source;
}
static int conn_provide(struct aem_stream_source *source)
{
	aem_assert(source);
	aem_assert(source->stream);

	struct server_connection *conn = aem_container_of(source, struct server_connection, source);

	aem_stream_flow(conn->sink.stream, source->flags);

	return 0;
}
static struct aem_net_conn *conn_new(struct aem_net_server *server, struct sockaddr *addr, socklen_t len)
{
	struct server_connection *conn = malloc(sizeof(*conn));
	if (!conn) {
		aem_logf_ctx(AEM_LOG_ERROR, "malloc failed: %s\n", strerror(errno));
		return NULL;
	}

	aem_stringbuf_init(&conn->name);

	conn->process_data = process_data;
	conn->line_state = 0;
	aem_net_conn_init(&conn->conn);
	aem_stream_sink_init(&conn->sink, conn_consume);
	aem_stream_source_init(&conn->source, conn_provide);

	return &conn->conn;
}

static void conn_setup(struct aem_net_conn *sock, struct aem_net_server *server, struct sockaddr *addr, socklen_t len)
{
	aem_assert(server);
	aem_assert(sock);

	struct server_connection *conn = aem_container_of(sock, struct server_connection, conn);

	switch (addr->sa_family) {
		case AF_INET:
		case AF_INET6:
		{
			char host[64];
			char serv[64];
			int gni_rc = getnameinfo(addr, len, host, sizeof(host), serv, sizeof(serv), NI_NUMERICSERV);
			if (gni_rc < 0) {
				aem_logf_ctx(AEM_LOG_ERROR, "getnameinfo(): %s\n", gai_strerror(gni_rc));
				break;
			}
			switch (addr->sa_family) {
				case AF_INET:
					aem_logf_ctx(AEM_LOG_INFO, "srv %p, fd %d: %s:%s\n", server, conn->conn.sock.evt.fd, host, serv);
					aem_stringbuf_printf(&conn->name, "%s:%s", host, serv);
					break;
				case AF_INET6:
					aem_logf_ctx(AEM_LOG_INFO, "srv %p, fd %d: [%s]:%s\n", server, conn->conn.sock.evt.fd, host, serv);
					aem_stringbuf_printf(&conn->name, "[%s]:%s", host, serv);
					break;
			}
			break;
		}
		default:
			aem_logf_ctx(AEM_LOG_INFO, "srv %p, conn %d: unknown AF\n", server, conn->conn.sock.evt.fd);
			break;
	}

	aem_stream_connect(&conn->conn.rx, &conn->sink);
	aem_stream_connect(&conn->source, &conn->conn.tx);
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
		aem_logf_ctx(AEM_LOG_FATAL, "You can't specify both -U and -p.\n");
		exit(1);
	}

	struct aem_poll poller;
	aem_poll_init(&poller);

	should_exit = 0;

	struct aem_net_server srv;

	aem_net_server_init(&srv);
	srv.sock.poller = &poller;
	aem_net_sock_inet(&srv.sock, bind_path, service, 1);
	srv.conn_new = conn_new;
	srv.setup = conn_setup;
	aem_net_listen(&srv, 4096);

	while (poller.n) {
		aem_poll_poll(&poller);
		if (should_exit)
			aem_poll_hup_all(&poller);
		synchronize_rcu(); // Call deferred destructors.
	}

	aem_net_server_dtor(&srv);

	aem_poll_dtor(&poller);

	return 0;
}
