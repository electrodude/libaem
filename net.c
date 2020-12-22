#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <aem/compiler.h>
#include <aem/log.h>
#include <aem/unix.h>

#ifdef POLLRDHUP
#define HAVE_POLLRDHUP
#endif

#include "net.h"

/// Socket
struct aem_net_sock *aem_net_sock_init(struct aem_net_sock *sock)
{
	aem_assert(sock);

	aem_poll_event_init(&sock->evt);
	sock->poller = NULL;

	sock->rd_open = 0;
	sock->wr_open = 0;

	return sock;
}

void aem_net_sock_dtor(struct aem_net_sock *sock)
{
	aem_net_sock_stop(sock);

	sock->poller = NULL;
}

void aem_net_sock_stop(struct aem_net_sock *sock)
{
	aem_assert(sock);

	if (sock->evt.i >= 0) {
		aem_assert(sock->poller);
		aem_poll_del(sock->poller, &sock->evt);
	}

	struct aem_poll_event *evt = &sock->evt;

	if (evt->fd == -1)
		return;

	aem_assert(evt->fd >= 0);

	aem_logf_ctx(AEM_LOG_DEBUG, "%p: close(%d)\n", sock, evt->fd);

	if (sock->rd_open || sock->wr_open) {
		if (sock->rd_open && sock->wr_open) {
			if (shutdown(evt->fd, SHUT_RDWR) < 0)
				aem_logf_ctx(AEM_LOG_BUG, "shutdown(%d, SHUT_RDWR): %s\n", evt->fd, strerror(errno));
		} else if (sock->rd_open) {
			if (shutdown(evt->fd, SHUT_RD) < 0)
				aem_logf_ctx(AEM_LOG_BUG, "shutdown(%d, SHUT_RD): %s\n", evt->fd, strerror(errno));
		} else if (sock->wr_open) {
			if (shutdown(evt->fd, SHUT_WR) < 0)
				aem_logf_ctx(AEM_LOG_BUG, "shutdown(%d, SHUT_WR): %s\n", evt->fd, strerror(errno));
		}
	}
	sock->rd_open = 0;
	sock->wr_open = 0;

	if (close(evt->fd) < 0)
		aem_logf_ctx(AEM_LOG_BUG, "close(%d): %s\n", evt->fd, strerror(errno));

	evt->fd = -1;
}


int aem_net_socket(struct aem_net_sock *sock, struct addrinfo *ai)
{
	aem_assert(sock);
	aem_assert(ai);

	//aem_net_sock_stop(sock);

	struct aem_poll_event *evt = &sock->evt;

	int fd = socket(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC, ai->ai_protocol);

	if (fd < 0) {
		aem_logf_ctx(AEM_LOG_ERROR, "socket(): %s\n", strerror(errno));
		goto fail_noclose;
	}

	//aem_fd_add_flags(fd, O_NONBLOCK | O_CLOEXEC);

	if (ai->ai_family == AF_INET6) {
		// I vaguely remember there being some reason we always want this.
		const int off = 0;
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off)) < 0) {
			aem_logf_ctx(AEM_LOG_ERROR, "setsockopt(%d, IPV6_V6ONLY, off): %s\n", fd, strerror(errno));
			goto fail;
		}
	}

	const int on = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		aem_logf_ctx(AEM_LOG_ERROR, "setsockopt(%d, SO_REUSEADDR, on): %s\n", fd, strerror(errno));
		goto fail;
	}

	evt->fd = fd;

	sock->rd_open = 1;
	sock->wr_open = 1;

	return 0;

fail:
	if (close(fd) < 0)
		aem_logf_ctx(AEM_LOG_BUG, "close(%d): %s\n", evt->fd, strerror(errno));
fail_noclose:
	return -1;
}

int aem_net_bind(struct aem_net_sock *sock, struct addrinfo *ai)
{
	aem_assert(sock);
	aem_assert(ai);

	struct aem_poll_event *evt = &sock->evt;

	if (evt->fd < 0) {
		aem_logf_ctx(AEM_LOG_BUG, "Invalid fd\n");
		return -1;
	}

	aem_assert(ai->ai_addr);
	if (bind(evt->fd, ai->ai_addr, ai->ai_addrlen) < 0) {
		aem_logf_ctx(AEM_LOG_ERROR, "bind(%d): %s\n", evt->fd, strerror(errno));
		return -1;
	}

	return 0;
}

int aem_net_sock_inet(struct aem_net_sock *sock, const char *node, const char *service, int do_bind)
{
	aem_assert(sock);

	//struct aem_poll_event *evt = &sock->evt;

	struct addrinfo hints = {0};
	hints.ai_family = AF_UNSPEC;
	//hints.ai_family = AF_INET;
	//hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = !node ? AI_PASSIVE : 0;

	if (!service)
		service = "0";

	struct addrinfo *res;
	int gai_rc = getaddrinfo(node, service, &hints, &res);

	if (gai_rc) {
		aem_logf_ctx(AEM_LOG_ERROR, "getaddrinfo(): %s\n", gai_strerror(gai_rc));
		return -1;
	}

	if (aem_net_socket(sock, res) < 0) {
		goto fail_noclose;
	}

	if (do_bind) {
		if (aem_net_bind(sock, res) < 0) {
			goto fail;
		}
	}

	freeaddrinfo(res);

	return 0;

fail:
#if 0
	if (close(evt->fd) < 0)
		aem_logf_ctx(AEM_LOG_BUG, "close(%d): %s\n", evt->fd, strerror(errno));
	evt->fd = -1;
#endif
fail_noclose:
	freeaddrinfo(res);
	return -1;
}

int aem_net_sock_path(struct aem_net_sock *sock, const char *path, int msg)
{
	aem_assert(sock);
	aem_assert(path);

	struct addrinfo ai = {0};
	ai.ai_family = AF_UNIX;
	ai.ai_socktype = msg ? SOCK_SEQPACKET : SOCK_STREAM;
	ai.ai_protocol = 0;

	if (aem_net_socket(sock, &ai) < 0) {
		return -1;
	}

	if (path) {
		struct sockaddr_un name = {0};
		name.sun_family = AF_UNIX;
		strncpy(name.sun_path, path, sizeof(name.sun_path) - 1);

		struct stat st;
		if (stat(name.sun_path, &st) >= 0) {
			if (st.st_mode & S_IFSOCK) {
				aem_logf_ctx(AEM_LOG_INFO, "Removing old socket %s\n", name.sun_path);
				unlink(name.sun_path);
			} else {
				aem_logf_ctx(AEM_LOG_ERROR, "Pre-bind: %s already exists and is not a socket\n", name.sun_path);
				return -1;
			}
		}

		ai.ai_addr = (struct sockaddr*)&name;
		ai.ai_addrlen = sizeof(name);

		if (aem_net_bind(sock, &ai) < 0) {
			return -1;
		}
	}

	return 0;
}


/// Stream connection
static int aem_net_on_rx(struct aem_stream_source *source, int flags)
{
	aem_assert(source);

	struct aem_net_conn *conn = aem_container_of(source, struct aem_net_conn, rx);
	struct aem_net_sock *sock = &conn->sock;
	struct aem_poll_event *evt = &sock->evt;

	struct aem_stream *stream = source->stream;
	aem_assert(stream);

	if (!sock->rd_open) {
		aem_assert(flags & AEM_STREAM_FIN);
		evt->events &= ~POLLIN;
		aem_poll_mod(sock->poller, &sock->evt);
		aem_stream_source_detach(source, AEM_STREAM_FIN);
		return 1;
	}

again:;
	struct aem_stringbuf *out = aem_stream_provide_begin(source);
	if (!out) {
		// This probably can't really ever happen.
		flags |= AEM_STREAM_FIN;
		goto done;
	}

	if (out->n > 65536) {
		aem_logf_ctx(AEM_LOG_WARN, "Waiting: buffer has %zd bytes\n", out->n);
		aem_stream_provide_end(source);
		goto done;
	}

	// Make sure we have room in the buffer
	aem_stringbuf_reserve(out, 4096);
	ssize_t nread = recv(evt->fd, aem_stringbuf_end(out), aem_stringbuf_available(out), MSG_DONTWAIT);
	if (nread > 0)
		out->n += nread;
	aem_stream_provide_end(source);

	if (nread > 0) {
		// TODO: if (nread == reserved), reserve more next time.
		aem_logf_ctx(AEM_LOG_DEBUG, "recv(%d): got %zd bytes\n", evt->fd, nread);
	} else if (nread == 0) {
		aem_logf_ctx(AEM_LOG_DEBUG, "recv(%d): EOF\n", evt->fd);
		flags |= AEM_STREAM_FIN;
		goto done;
	} else {
		switch (errno) {
			case EAGAIN:
				goto done;
			case EINTR:
				goto again;
			default:
				aem_logf_ctx(AEM_LOG_ERROR, "recv(%d): unexpected error: %s\n", evt->fd, strerror(errno));
				break;
		}
		evt->events &= ~POLLIN;
		aem_poll_mod(sock->poller, &sock->evt);
	}

done:
	if (flags & AEM_STREAM_FIN) {
		if (sock->rd_open) {
			if (shutdown(evt->fd, SHUT_RD) < 0) {
				aem_logf_ctx(AEM_LOG_BUG, "shutdown(%d, SHUT_RD): %s\n", evt->fd, strerror(errno));
			}
			sock->rd_open = 0;
		}
		evt->events &= ~POLLIN;
		aem_poll_mod(sock->poller, &sock->evt);
		aem_stream_source_detach(source, AEM_STREAM_FIN);
	}

	aem_stream_consume(stream, flags);

	return 0;
}
static int aem_net_on_tx(struct aem_stream_sink *sink, int flags)
{
	aem_assert(sink);

	struct aem_net_conn *conn = aem_container_of(sink, struct aem_net_conn, tx);
	struct aem_net_sock *sock = &conn->sock;
	struct aem_poll_event *evt = &sock->evt;

	if (!sock->wr_open) {
		aem_assert(flags & AEM_STREAM_FIN);
		evt->events &= ~POLLOUT;
		aem_poll_mod(sock->poller, &sock->evt);
		aem_stream_sink_detach(sink, AEM_STREAM_FIN);
		return 1;
	}

	struct aem_stringslice in = aem_stream_consume_begin(sink);

	if (!aem_stringslice_ok(in)) {
		evt->events &= ~POLLOUT;
		aem_poll_mod(sock->poller, &sock->evt);
	}

	if (!in.start) {
		// We don't need to call aem_stream_consume_end, since _begin failed.
		return 0;
	}

	if (aem_stringslice_ok(in)) {
again:;
		ssize_t nread = send(evt->fd, in.start, aem_stringslice_len(in), MSG_DONTWAIT | MSG_NOSIGNAL);
		if (nread > 0)
			in.start += nread;
		aem_stream_consume_end(sink, in);

		if (nread > 0) {
			aem_logf_ctx(AEM_LOG_DEBUG, "send(%d) sent %zd bytes; %zd remain\n", evt->fd, nread, aem_stringslice_len(in));
			// Only request POLLOUT if we still have more to send.
			if (aem_stringslice_ok(in)) {
				evt->events |= POLLOUT;
			}
			// Don't clear POLLOUT yet in case we sent the whole
			// buffer but more stuff would have been put in the
			// buffer if it hadn't already been too full.
			aem_poll_mod(sock->poller, &sock->evt);
		} else {
			switch (errno) {
				case EAGAIN:
					// TODO: Don't shutdown() until all data is sent
					evt->events |= POLLOUT;
					aem_poll_mod(sock->poller, &sock->evt);
					goto done;
				case EINTR:
					goto again;
				case EPIPE:
				case ECONNRESET:
					aem_logf_ctx(AEM_LOG_DEBUG, "fd %d: remote closed write end of connection: %s\n", evt->fd, strerror(errno));
					sock->wr_open = 0;
					evt->events &= ~POLLOUT;
					aem_poll_mod(sock->poller, &sock->evt);
					aem_stream_sink_detach(sink, AEM_STREAM_FIN);
					goto done;
				default:
					aem_logf_ctx(AEM_LOG_ERROR, "send(%d): unexpected error: %s\n", evt->fd, strerror(errno));
					// TODO: If we get lots of errors, cancel POLLOUT;
					break;
			}
		}
	} else {
		aem_stream_consume_end(sink, in);
		evt->events &= ~POLLOUT;
		aem_poll_mod(sock->poller, &sock->evt);
		goto done;
	}

done:

	if (flags & AEM_STREAM_FIN) {
		if (aem_stringslice_ok(in)) {
			aem_logf_ctx(AEM_LOG_WARN, "Destroying socket with %zd bytes still unsent!\n", aem_stringslice_len(in));
		}
		if (sock->wr_open) {
			if (shutdown(evt->fd, SHUT_WR) < 0) {
				aem_logf_ctx(AEM_LOG_BUG, "shutdown(%d, SHUT_WR): %s\n", evt->fd, strerror(errno));
			}
			sock->wr_open = 0;
		}
		evt->events &= ~POLLOUT;
		aem_poll_mod(sock->poller, &sock->evt);
		aem_stream_sink_detach(sink, AEM_STREAM_FIN);
	}

	return 0;
}
static void aem_net_on_conn(struct aem_poll *p, struct aem_poll_event *evt)
{
	aem_assert(p);
	aem_assert(evt);

	struct aem_net_sock *sock = aem_container_of(evt, struct aem_net_sock, evt);
	struct aem_net_conn *conn = aem_container_of(sock, struct aem_net_conn, sock);

	int do_rx = 0;
	int do_tx = 0;
	if (aem_poll_event_check(evt, POLLIN)) {
		do_rx = 1;
	}
	if (aem_poll_event_check(evt, POLLOUT)) {
		do_tx = 1;
	}
#ifdef HAVE_POLLRDHUP
	if (aem_poll_event_check(evt, POLLRDHUP)) {
		aem_logf_ctx(AEM_LOG_DEBUG, "fd %d closed for reading\n", evt->fd);
		evt->events &= ~POLLIN;
		evt->events &= ~POLLRDHUP;
		aem_poll_mod(sock->poller, &sock->evt);
		if (aem_stream_add_flags(conn->rx.stream, AEM_STREAM_FIN))
			aem_logf_ctx(AEM_LOG_WARN, "fd %d has no rx stream\n", evt->fd);
		sock->rd_open = 0;
		do_rx = 1;
	}
#endif
	if (aem_poll_event_check(evt, POLLHUP)) {
		aem_logf_ctx(AEM_LOG_DEBUG, "fd %d closed\n", evt->fd);
		aem_poll_event_check(evt, POLLERR); // Eat any POLLERR
		if (aem_stream_add_flags(conn->rx.stream, AEM_STREAM_FIN))
			aem_logf_ctx(AEM_LOG_WARN, "fd %d has no rx stream; rd_open = %d\n", evt->fd, sock->rd_open);
		if (aem_stream_add_flags(conn->tx.stream, AEM_STREAM_FIN))
			aem_logf_ctx(AEM_LOG_WARN, "fd %d has no tx stream; wr_open = %d\n", evt->fd, sock->wr_open);
		sock->rd_open = 0;
		sock->wr_open = 0;
		do_rx = 1;
		do_tx = 1;
	}
	if (do_rx) {
		if (conn->rx.stream) {
			aem_stream_provide(conn->rx.stream, 0);
		} else {
			aem_logf_ctx(AEM_LOG_WARN, "Removing POLLIN from fd %d due to disconnected stream\n", evt->fd);
			evt->events &= ~POLLIN;
			aem_poll_mod(sock->poller, &sock->evt);
		}
	}
	if (do_tx) {
		if (conn->tx.stream) {
			aem_stream_provide(conn->tx.stream, 0);
		} else {
			aem_logf_ctx(AEM_LOG_WARN, "Removing POLLOUT from fd %d due to disconnected stream\n", evt->fd);
			evt->events &= ~POLLOUT;
			aem_poll_mod(sock->poller, &sock->evt);
		}
	}
}

struct aem_net_conn *aem_net_conn_init(struct aem_net_conn *conn)
{
	aem_assert(conn);

	aem_net_sock_init(&conn->sock);

	aem_stream_source_init(&conn->rx, aem_net_on_rx);
	aem_stream_sink_init(&conn->tx, aem_net_on_tx);

	struct aem_poll_event *evt = &conn->sock.evt;

	//evt->events = POLLIN | POLLOUT;
	evt->events = POLLIN;
#ifdef HAVE_POLLRDHUP
	evt->events |= POLLRDHUP;
#endif
	evt->on_event = aem_net_on_conn;

	conn->sock.rd_open = 1;
	conn->sock.wr_open = 1;

	return conn;
}
void aem_net_conn_dtor(struct aem_net_conn *conn)
{
	aem_assert(conn);

	aem_stream_source_dtor(&conn->rx, AEM_STREAM_FIN);
	aem_stream_sink_dtor(&conn->tx, AEM_STREAM_FIN);

	aem_net_sock_dtor(&conn->sock);
}

int aem_net_connect(struct aem_net_conn *conn, struct addrinfo *ai)
{
	aem_assert(conn);
	aem_assert(ai);

	struct aem_net_sock *sock = &conn->sock;
	struct aem_poll_event *evt = &sock->evt;

	if (evt->fd < 0) {
		aem_logf_ctx(AEM_LOG_BUG, "Invalid fd\n");
		return -1;
	}

	if (connect(evt->fd, ai->ai_addr, ai->ai_addrlen) < 0) {
		aem_logf_ctx(AEM_LOG_ERROR, "connect(%d): %s\n", evt->fd, strerror(errno));
		return -1;
	}

	aem_poll_add(sock->poller, &sock->evt);

	return 0;
}

int aem_net_connect_inet(struct aem_net_conn *conn, const char *node, const char *service)
{
	aem_assert(conn);

	struct addrinfo hints = {0};
	hints.ai_family = AF_UNSPEC;
	//hints.ai_family = AF_INET;
	//hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = node ? AI_PASSIVE : 0;

	struct addrinfo *res;
	int gai_rc = getaddrinfo(node, service, &hints, &res);

	if (gai_rc) {
		aem_logf_ctx(AEM_LOG_ERROR, "getaddrinfo(): %s\n", gai_strerror(gai_rc));
		return -1;
	}

	int rc = aem_net_connect(conn, res);

	freeaddrinfo(res);

	return rc;
}

int aem_net_connect_path(struct aem_net_conn *conn, const char *path, int msg)
{
	aem_assert(conn);

	struct aem_net_sock *sock = &conn->sock;
	struct aem_poll_event *evt = &sock->evt;

	if (evt->fd < 0) {
		aem_logf_ctx(AEM_LOG_BUG, "Invalid fd\n");
		return -1;
	}

	struct addrinfo ai = {0};
	ai.ai_family = AF_UNIX;
	ai.ai_socktype = msg ? SOCK_SEQPACKET : SOCK_STREAM;
	ai.ai_protocol = 0;

	struct sockaddr_un name = {0};
	name.sun_family = AF_UNIX;
	strncpy(name.sun_path, path, sizeof(name.sun_path) - 1);

	ai.ai_addr = (struct sockaddr*)&name;
	ai.ai_addrlen = sizeof(name);

	return aem_net_connect(conn, &ai);
}


/// Server listener socket
struct aem_net_server *aem_net_server_init(struct aem_net_server *server)
{
	aem_assert(server);

	aem_net_sock_init(&server->sock);

	return server;
}
void aem_net_server_dtor(struct aem_net_server *server)
{
	aem_assert(server);

	aem_net_sock_dtor(&server->sock);
}

static void aem_net_on_accept(struct aem_poll *p, struct aem_poll_event *evt)
{
	aem_assert(p);
	aem_assert(evt);

	struct aem_net_sock *sock = aem_container_of(evt, struct aem_net_sock, evt);
	struct aem_net_server *server = aem_container_of(sock, struct aem_net_server, sock);

	aem_logf_ctx(AEM_LOG_DEBUG, "fd %d revents %hx\n", evt->fd, evt->revents);

	if (aem_poll_event_check(evt, POLLIN)) {
		do {
		again:;
			struct sockaddr_storage addr;
			socklen_t len = sizeof(addr);
			int fd = accept(evt->fd, (struct sockaddr*)&addr, &len);

			if (len > sizeof(addr)) {
				aem_logf_ctx(AEM_LOG_BUG, "sockaddr len %zd was smaller than required %zd!\n", sizeof(addr), len);
			}

			if (fd < 0) {
				switch (errno) {
					case EAGAIN:
						goto done;
					case EINTR:
						goto again;
				}
				aem_logf_ctx(AEM_LOG_ERROR, "accept(%d): %s\n", evt->fd, strerror(errno));
				return;
			}

			aem_assert(server->conn_new);
			struct aem_net_conn *conn = server->conn_new(server, (struct sockaddr*)&addr, len);
			if (!conn) {
				if (shutdown(fd, SHUT_RDWR) < 0)
					aem_logf_ctx(AEM_LOG_BUG, "shutdown(%d (rejected by server callback), SHUT_RDWR): %s\n", fd, strerror(errno));

				if (close(fd) < 0)
					aem_logf_ctx(AEM_LOG_BUG, "close(%d (rejected by server callback)): %s\n", fd, strerror(errno));

				continue;
			}

			conn->sock.poller = sock->poller;
			conn->sock.evt.fd = fd;
			aem_poll_add(conn->sock.poller, &conn->sock.evt);

			aem_assert(server->setup);
			// Get conn->rx and conn->tx connected somewhere
			server->setup(conn, server, (struct sockaddr*)&addr, len);

			if (!conn->rx.stream)
				aem_logf_ctx(AEM_LOG_WARN, "Probable bug: connection with no rx callback\n");

			aem_stream_provide(conn->tx.stream, 0);
		} while (1);
		done:;
	}
	if (aem_poll_event_check(evt, POLLHUP)) {
		aem_net_sock_stop(sock);
	}
	if (aem_poll_event_check(evt, POLLERR)) {
		aem_logf_ctx(AEM_LOG_ERROR, "fd %d: POLLERR\n", evt->fd);
	}
}

int aem_net_listen(struct aem_net_server *server, int backlog)
{
	aem_assert(server);

	struct aem_net_sock *sock = &server->sock;
	struct aem_poll_event *evt = &sock->evt;

	if (listen(evt->fd, backlog) < 0) {
		aem_logf_ctx(AEM_LOG_ERROR, "listen(%d): %s\n", evt->fd, strerror(errno));
		return -1;
	}

	evt->events = POLLIN;
	evt->on_event = aem_net_on_accept;

	aem_poll_add(sock->poller, &sock->evt);

	return 0;
}
