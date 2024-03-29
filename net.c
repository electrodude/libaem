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

#define AEM_INTERNAL
#include <aem/log.h>
#include <aem/memory.h>
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

	sock->on_close = NULL;

	sock->rd_open = 0;
	sock->wr_open = 0;

	return sock;
}

void aem_net_sock_dtor(struct aem_net_sock *sock)
{
	aem_net_sock_close(sock);

	sock->poller = NULL;
}

void aem_net_sock_close(struct aem_net_sock *sock)
{
	aem_assert(sock);

	if (sock->evt.i >= 0) {
		aem_assert(sock->poller);
		aem_poll_del(sock->poller, &sock->evt);
	}

	struct aem_poll_event *evt = &sock->evt;

	if (evt->fd == -1) {
		aem_assert(!sock->rd_open);
		aem_assert(!sock->wr_open);
		return;
	}

	aem_assert(evt->fd >= 0);

	aem_logf_ctx(AEM_LOG_DEBUG, "%p: close(%d)", sock, evt->fd);

	if (sock->rd_open || sock->wr_open) {
		int how = SHUT_RDWR;
		if (sock->rd_open && sock->wr_open)
			how = SHUT_RDWR;
		else if (sock->rd_open)
			how = SHUT_RD;
		else if (sock->wr_open)
			how = SHUT_WR;

		if (shutdown(evt->fd, how) < 0 && errno != ENOTCONN) {
			const char *how_str = "(invalid)"; // Is always set, but GCC isn't convinced.
			switch (how) {
				case SHUT_RDWR: how_str = "SHUT_RDWR"; break;
				case SHUT_RD  : how_str = "SHUT_RD"  ; break;
				case SHUT_WR  : how_str = "SHUT_WR"  ; break;
			}

			aem_logf_ctx(AEM_LOG_BUG, "shutdown(%d, %s): %s", evt->fd, how_str, strerror(errno));
		}
	}
	sock->rd_open = 0;
	sock->wr_open = 0;

	if (close(evt->fd) < 0)
		aem_logf_ctx(AEM_LOG_BUG, "close(%d): %s", evt->fd, strerror(errno));

	evt->fd = -1;

	if (!sock->on_close)
		return;

	sock->on_close(sock);

	// Ensure on_close is never called more than once
	sock->on_close = NULL;
}


int aem_net_socket(struct aem_net_sock *sock, struct addrinfo *ai)
{
	aem_assert(sock);
	aem_assert(ai);

	struct aem_poll_event *evt = &sock->evt;

	int fd = socket(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC, ai->ai_protocol);

	if (fd < 0) {
		aem_logf_ctx(AEM_LOG_ERROR, "socket(): %s", strerror(errno));
		goto fail_noclose;
	}

	//aem_fd_add_flags(fd, O_NONBLOCK | O_CLOEXEC);

	if (ai->ai_family == AF_INET6) {
		// I vaguely remember there being some reason we always want this.
		const int off = 0;
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off)) < 0) {
			aem_logf_ctx(AEM_LOG_ERROR, "setsockopt(%d, IPV6_V6ONLY, off): %s", fd, strerror(errno));
			goto fail;
		}
	}

	const int on = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		aem_logf_ctx(AEM_LOG_ERROR, "setsockopt(%d, SO_REUSEADDR, on): %s", fd, strerror(errno));
		goto fail;
	}

	evt->fd = fd;

	sock->rd_open = 0;
	sock->wr_open = 0;

	return 0;

fail:
	if (close(fd) < 0)
		aem_logf_ctx(AEM_LOG_BUG, "close(%d): %s", evt->fd, strerror(errno));
fail_noclose:
	return -1;
}

int aem_net_bind(struct aem_net_sock *sock, struct addrinfo *ai)
{
	aem_assert(sock);
	aem_assert(ai);

	struct aem_poll_event *evt = &sock->evt;

	if (evt->fd < 0) {
		aem_logf_ctx(AEM_LOG_BUG, "Invalid fd");
		return -1;
	}

	aem_assert(ai->ai_addr);
	if (bind(evt->fd, ai->ai_addr, ai->ai_addrlen) < 0) {
		aem_logf_ctx(AEM_LOG_ERROR, "bind(%d): %s", evt->fd, strerror(errno));
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
		aem_logf_ctx(AEM_LOG_ERROR, "getaddrinfo(): %s", gai_strerror(gai_rc));
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
		aem_logf_ctx(AEM_LOG_BUG, "close(%d): %s", evt->fd, strerror(errno));
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
				aem_logf_ctx(AEM_LOG_INFO, "Removing old socket %s", name.sun_path);
				unlink(name.sun_path);
			} else {
				aem_logf_ctx(AEM_LOG_ERROR, "Pre-bind: %s already exists and is not a socket", name.sun_path);
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
static void aem_net_on_rx(struct aem_stream_source *source)
{
	aem_assert(source);

	struct aem_net_conn *conn = aem_container_of(source, struct aem_net_conn, rx);
	struct aem_net_sock *sock = &conn->sock;
	struct aem_poll_event *evt = &sock->evt;

	struct aem_stream *stream = source->stream;
	aem_assert(stream);

	if (!sock->rd_open)
		return;

	if (stream->flags & AEM_STREAM_FULL)
		evt->events &= ~POLLIN;
	else
		evt->events |= POLLIN;
	aem_poll_mod(sock->poller, &sock->evt);

	if (!aem_stream_propagate_down(source, NULL))
		return;

	struct aem_stringbuf *out = aem_stream_provide_begin(source, 1);
	if (!out)
		return;

	if (out->n > 65536)
		aem_logf_ctx(AEM_LOG_BUG, "Do you really want more data when you already have %zd bytes?", out->n);

again:;
	// Make sure we have room in the buffer
	aem_stringbuf_reserve(out, 4096);
	ssize_t nread = recv(evt->fd, aem_stringbuf_end(out), aem_stringbuf_available(out), MSG_DONTWAIT);

	if (nread > 0) {
		// TODO: if (nread == reserved), reserve more next time.
		aem_logf_ctx(AEM_LOG_DEBUG, "recv(%d): got %zd bytes", evt->fd, nread);
		out->n += nread;
	} else if (nread == 0) {
		aem_logf_ctx(AEM_LOG_DEBUG, "recv(%d): EOF", evt->fd);
		stream->flags |= AEM_STREAM_FIN;
	} else {
		switch (errno) {
			//case EWOULDBLOCK:
			case EAGAIN:
				evt->events |= POLLIN;
				aem_poll_mod(sock->poller, &sock->evt);
				break;
			case EINTR:
				goto again;
			case ECONNRESET:
				aem_logf_ctx(AEM_LOG_DEBUG, "fd %d: remote closed read end of connection: %s", evt->fd, strerror(errno));
				stream->flags |= AEM_STREAM_FIN;
				sock->rd_open = 0;
				break;
			default:
				aem_logf_ctx(AEM_LOG_ERROR, "recv(%d): unexpected error, closing read end: %s", evt->fd, strerror(errno));
				stream->flags |= AEM_STREAM_FIN;
				break;
		}
	}

	aem_stream_provide_end(source);

	if (stream->flags & AEM_STREAM_FIN) {
		if (sock->rd_open) {
			if (shutdown(evt->fd, SHUT_RD) < 0 && errno != ENOTCONN) {
				aem_logf_ctx(AEM_LOG_BUG, "shutdown(%d, SHUT_RD): %s", evt->fd, strerror(errno));
			}
			sock->rd_open = 0;
		}
		evt->events &= ~POLLIN;
#ifdef HAVE_POLLRDHUP
		evt->events &= ~POLLRDHUP;
#endif
		aem_poll_mod(sock->poller, &sock->evt);
		aem_stream_source_detach(source);
		if (!sock->rd_open && !sock->wr_open)
			aem_net_sock_close(sock);
	}
}
static void aem_net_on_tx(struct aem_stream_sink *sink)
{
	aem_assert(sink);

	struct aem_net_conn *conn = aem_container_of(sink, struct aem_net_conn, tx);
	struct aem_net_sock *sock = &conn->sock;
	struct aem_poll_event *evt = &sock->evt;

	struct aem_stream *stream = sink->stream;
	if (!stream)
		return;

	struct aem_stringslice in = aem_stream_consume_begin(sink);

	if (!in.start) {
		// We don't need to call aem_stream_consume_end, since _begin failed.
		goto cancel;
	}

	if (!aem_stringslice_ok(in)) {
		// If there's nothing to send at the moment, deregister for POLLOUT and skip send.
		evt->events &= ~POLLOUT;
		aem_poll_mod(sock->poller, &sock->evt);
		aem_stream_consume_end(sink, in);
		goto cancel;
	}

again:;
	ssize_t nread = send(evt->fd, in.start, aem_stringslice_len(in), MSG_DONTWAIT | MSG_NOSIGNAL);

	if (nread > 0) {
		in.start += nread;
		aem_logf_ctx(AEM_LOG_DEBUG, "send(%d) sent %zd bytes; %zd remain", evt->fd, nread, aem_stringslice_len(in));
		// Signal FULL iff we couldn't send everything
		aem_stream_sink_set_full(sink, aem_stringslice_ok(in));
		// TODO: Signal upstream we're full *now*, not next time
	} else {
		switch (errno) {
			//case EWOULDBLOCK:
			case EAGAIN:
				// TODO: Don't shutdown() until all data is sent
				// We couldn't send anything
				stream->flags |= AEM_STREAM_FULL;
				break;
			case EINTR:
				goto again;
			case EPIPE:
			case ECONNRESET:
				aem_logf_ctx(AEM_LOG_DEBUG, "fd %d: remote closed write end of connection: %s", evt->fd, strerror(errno));
				stream->flags |= AEM_STREAM_FIN;
				sock->wr_open = 0;
				break;
			default:
				aem_logf_ctx(AEM_LOG_ERROR, "send(%d): unexpected error, closing write end: %s", evt->fd, strerror(errno));
				stream->flags |= AEM_STREAM_FIN;
				break;
		}
	}

	aem_stream_consume_end(sink, in);

	if (stream->flags & AEM_STREAM_FULL) {
		// Register for POLLOUT if we couldn't send everything.
		evt->events |= POLLOUT;
		aem_poll_mod(sock->poller, &sock->evt);
	}

cancel:
	if (stream->flags & AEM_STREAM_FIN && !aem_stringslice_ok(in)) {
		if (sock->wr_open) {
			if (shutdown(evt->fd, SHUT_WR) < 0 && errno != ENOTCONN) {
				aem_logf_ctx(AEM_LOG_BUG, "shutdown(%d, SHUT_WR): %s", evt->fd, strerror(errno));
			}
			sock->wr_open = 0;
		}
		evt->events &= ~POLLOUT;
		aem_poll_mod(sock->poller, &sock->evt);
		aem_stream_sink_detach(sink);
		if (!sock->rd_open && !sock->wr_open)
			aem_net_sock_close(sock);
	}
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
		aem_logf_ctx(AEM_LOG_DEBUG, "fd %d closed for reading", evt->fd);
		do_rx = 1;
	}
#endif
	if (aem_poll_event_check(evt, POLLHUP)) {
		aem_logf_ctx(AEM_LOG_DEBUG, "fd %d closed", evt->fd);
		aem_poll_event_check(evt, POLLERR); // Eat any POLLERR
		// Set FIN on RX so we still hang up even if it isn't a real
		// HUP that won't provide an EOF.
		if (conn->rx.stream)
			conn->rx.stream->flags |= AEM_STREAM_FIN;
		if (conn->tx.stream)
			conn->tx.stream->flags |= AEM_STREAM_FIN;
		sock->wr_open = 0;
		do_rx = 1;
		do_tx = 1;
	}
	// Valid scenario in which !(evt->events & (POLLIN | POLLOUT)):
	// - Reading side is closed,
	// - Writing side is open but currently has nothing to send.
	if (do_rx) {
		if (conn->rx.stream) {
			aem_stream_flow(conn->rx.stream);
		} else {
			aem_logf_ctx(AEM_LOG_WARN, "Removing POLLIN from fd %d due to disconnected stream", evt->fd);
			evt->events &= ~POLLIN;
			aem_poll_mod(sock->poller, &sock->evt);
		}
	}
	if (do_tx) {
		if (conn->tx.stream) {
			aem_stream_flow(conn->tx.stream);
		} else {
			aem_logf_ctx(AEM_LOG_WARN, "Removing POLLOUT from fd %d due to disconnected stream", evt->fd);
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

	if (conn->rx.stream)
		conn->rx.stream->flags |= AEM_STREAM_FIN;
	if (conn->tx.stream)
		conn->tx.stream->flags |= AEM_STREAM_FIN;
	aem_stream_source_dtor(&conn->rx);
	aem_stream_sink_dtor(&conn->tx);

	aem_net_sock_dtor(&conn->sock);
}

int aem_net_connect(struct aem_net_conn *conn, struct addrinfo *ai)
{
	aem_assert(conn);
	aem_assert(ai);

	struct aem_net_sock *sock = &conn->sock;
	struct aem_poll_event *evt = &sock->evt;

	if (evt->fd < 0) {
		aem_logf_ctx(AEM_LOG_BUG, "Invalid fd");
		return -1;
	}

	if (connect(evt->fd, ai->ai_addr, ai->ai_addrlen) < 0) {
		aem_logf_ctx(AEM_LOG_ERROR, "connect(%d): %s", evt->fd, strerror(errno));
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
		aem_logf_ctx(AEM_LOG_ERROR, "getaddrinfo(): %s", gai_strerror(gai_rc));
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
		aem_logf_ctx(AEM_LOG_BUG, "Invalid fd");
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

	aem_logf_ctx(AEM_LOG_DEBUG, "fd %d revents %hx", evt->fd, evt->revents);

	if (aem_poll_event_check(evt, POLLIN)) {
		do {
		again:;
			struct sockaddr_storage addr;
			socklen_t len = sizeof(addr);
			int fd = accept(evt->fd, (struct sockaddr*)&addr, &len);

			if (len > sizeof(addr)) {
				aem_logf_ctx(AEM_LOG_BUG, "sockaddr len %zd was smaller than required %zd!", sizeof(addr), len);
			}

			if (fd < 0) {
				switch (errno) {
					case EAGAIN:
						goto done;
					case EINTR:
						goto again;
				}
				aem_logf_ctx(AEM_LOG_ERROR, "accept(%d): %s", evt->fd, strerror(errno));
				return;
			}

			aem_assert(server->conn_new);
			struct aem_net_conn *conn = server->conn_new(server, (struct sockaddr*)&addr, len);
			if (!conn) {
				if (shutdown(fd, SHUT_RDWR) < 0 && errno != ENOTCONN)
					aem_logf_ctx(AEM_LOG_BUG, "shutdown(%d (rejected by server callback), SHUT_RDWR): %s", fd, strerror(errno));

				if (close(fd) < 0)
					aem_logf_ctx(AEM_LOG_BUG, "close(%d (rejected by server callback)): %s", fd, strerror(errno));

				continue;
			}

			conn->sock.poller = sock->poller;
			conn->sock.evt.fd = fd;
			aem_poll_add(conn->sock.poller, &conn->sock.evt);

			aem_assert(server->setup);
			// Get conn->rx and conn->tx connected somewhere
			server->setup(conn, server, (struct sockaddr*)&addr, len);

			if (!conn->rx.stream)
				aem_logf_ctx(AEM_LOG_WARN, "Probable bug: connection with no rx callback");

			aem_stream_flow(conn->tx.stream);
		} while (1);
		done:;
	}
	if (aem_poll_event_check(evt, POLLHUP)) {
		aem_net_sock_close(sock);
	}
	if (aem_poll_event_check(evt, POLLERR)) {
		aem_logf_ctx(AEM_LOG_ERROR, "fd %d: POLLERR", evt->fd);
	}
}

int aem_net_listen(struct aem_net_server *server, int backlog)
{
	aem_assert(server);

	struct aem_net_sock *sock = &server->sock;
	struct aem_poll_event *evt = &sock->evt;

	if (listen(evt->fd, backlog) < 0) {
		aem_logf_ctx(AEM_LOG_ERROR, "listen(%d): %s", evt->fd, strerror(errno));
		return -1;
	}

	evt->events = POLLIN;
	evt->on_event = aem_net_on_accept;

	aem_poll_add(sock->poller, &sock->evt);

	return 0;
}
