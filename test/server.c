#define _DEFAULT_SOURCE
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include <aem/compiler.h>
#include <aem/log.h>
#include <aem/net.h>
#include <aem/pmcrcu.h>
#include <aem/streams.h>

struct server_connection
{
	struct aem_net_conn conn;

	struct aem_stream_sink_lines rx_lines;
	struct aem_stream_source source;

	struct rcu_head rcu_head;
};

static int consume_line(struct aem_stream_sink_lines *sink, struct aem_stringslice line, int flags)
{
	aem_assert(sink);

	struct server_connection *conn = aem_container_of(sink, struct server_connection, rx_lines);

	struct aem_stringbuf out = {0};
	aem_stringbuf_puts(&out, "Line: ");
	aem_stringbuf_putss_quote(&out, line);
	aem_stringbuf_puts(&out, "\n");
	aem_logf_ctx(AEM_LOG_INFO, "%s", aem_stringbuf_get(&out));

	aem_logf_ctx(AEM_LOG_INFO, "EOF\n");

	struct aem_stringbuf *buf = aem_stream_source_getbuf(&conn->source);

	if (!buf) {
		aem_logf_ctx(AEM_LOG_BUG, "TX unconnected");
		goto done;
	}
	aem_assert(conn->source.stream);
	if (!conn->source.stream->sink)
		aem_logf_ctx(AEM_LOG_WARN, "TX closed");

	aem_stringbuf_append(buf, &out);
	if (flags & AEM_STREAM_FIN) {
		conn->source.stream->flags |= AEM_STREAM_FIN;
		aem_stringbuf_puts(buf, "EOF\n");
	}

	aem_stream_consume(conn->source.stream);

done:
	aem_stringbuf_dtor(&out);
	return 0;
}

static void conn_free_rcu(struct rcu_head *rcu_head)
{
	aem_assert(rcu_head);

	struct server_connection *conn = aem_container_of(rcu_head, struct server_connection, rcu_head);

	aem_logf_ctx(AEM_LOG_DEBUG, "%p\n", conn);

	aem_stream_source_dtor(&conn->source, AEM_STREAM_FIN);
	aem_stream_sink_lines_dtor(&conn->rx_lines);
	aem_net_conn_dtor(&conn->conn);

	free(conn);
}
static void conn_free(struct aem_net_conn *sock)
{
	aem_assert(sock);

	struct server_connection *conn = aem_container_of(sock, struct server_connection, conn);

	aem_logf_ctx(AEM_LOG_DEBUG, "%p\n", conn);

	aem_stream_source_detach(&conn->conn.rx, AEM_STREAM_FIN);
	aem_stream_sink_detach(&conn->conn.tx, AEM_STREAM_FIN);

	aem_net_sock_stop(&conn->conn.sock);

	call_rcu(&conn->rcu_head, conn_free_rcu);
}
static int conn_provide(struct aem_stream_source *source, int flags)
{
	struct server_connection *conn = aem_container_of(source, struct server_connection, source);

	if (source->stream)
		aem_stream_consume(source->stream);

	if (flags & AEM_STREAM_FIN) {
		if (source->stream)
			source->stream->flags |= AEM_STREAM_FIN;
		//conn_free(&conn->conn);
	}

	return 0;
}
static struct aem_net_conn *conn_new(struct aem_net_server *server, struct sockaddr *addr, socklen_t len)
{
	struct server_connection *conn = malloc(sizeof(*conn));
	if (!conn) {
		aem_logf_ctx(AEM_LOG_ERROR, "malloc failed: %s\n", strerror(errno));
		return NULL;
	}

	conn->conn.free = conn_free;
	aem_net_conn_init(&conn->conn);
	conn->rx_lines.consume_line = consume_line;
	aem_stream_sink_lines_init(&conn->rx_lines);
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
					break;
				case AF_INET6:
					aem_logf_ctx(AEM_LOG_INFO, "srv %p, fd %d: [%s]:%s\n", server, conn->conn.sock.evt.fd, host, serv);
					break;
			}
			break;
		}
		default:
			aem_logf_ctx(AEM_LOG_INFO, "srv %p, conn %d: unknown AF\n", server, conn->conn.sock.evt.fd);
			break;
	}

	aem_stream_connect(&conn->conn.rx, &conn->rx_lines.sink);
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
	const char *service = NULL;
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

	struct aem_net_server srv;

	aem_net_server_init(&srv);
	srv.sock.poller = &poller;
	aem_net_sock_inet(&srv.sock, bind_path, service, 1);
	srv.conn_new = conn_new;
	srv.setup = conn_setup;
	aem_net_listen(&srv, 4096);

	while (poller.n) {
		aem_poll_poll(&poller);
		synchronize_rcu(); // Call deferred destructors.
		usleep(10000);
	}

	aem_net_server_dtor(&srv);

	aem_poll_dtor(&poller);

	return 0;
}
