#ifndef AEM_NET_H
#define AEM_NET_H

#include <netdb.h>
#include <sys/socket.h>

#include <aem/poll.h>
#include <aem/stream.h>

/// Generic socket
struct aem_net_sock {
	struct aem_poll_event evt;

	struct aem_poll *poller;

	char rd_open : 1;
	char wr_open : 1;
};

// You must call sock->poller yourself before calling this.
struct aem_net_sock *aem_net_sock_init(struct aem_net_sock *sock);
void aem_net_sock_dtor(struct aem_net_sock *sock);

void aem_net_sock_close(struct aem_net_sock *sock);

struct addrinfo;
int aem_net_socket(struct aem_net_sock *sock, struct addrinfo *ai);
int aem_net_bind(struct aem_net_sock *sock, struct addrinfo *ai);

int aem_net_sock_inet(struct aem_net_sock *sock, const char *node, const char *service, int do_bind);
int aem_net_sock_path(struct aem_net_sock *sock, const char *path, int msg);


/// Stream connection
struct aem_net_conn {
	struct aem_net_sock sock;

	struct aem_stream_source rx;
	struct aem_stream_sink tx;
};

struct aem_net_conn *aem_net_conn_init(struct aem_net_conn *conn);
void aem_net_conn_dtor(struct aem_net_conn *conn);

int aem_net_connect(struct aem_net_conn *conn, struct addrinfo *ai);
int aem_net_connect_inet(struct aem_net_conn *conn, const char *node, const char *service);
int aem_net_connect_path(struct aem_net_conn *conn, const char *path, int msg);


/// TCP server listener socket
struct aem_net_server {
	struct aem_net_sock sock;

	// Callback to connect conn->rx and conn->tx somewhere.
	// Return a negative number to drop the connection, in which case conn
	// will be freed for you.
	struct aem_net_conn *(*conn_new)(struct aem_net_server *server, struct sockaddr *addr, socklen_t len);
	void (*setup)(struct aem_net_conn *conn, struct aem_net_server *server, struct sockaddr *addr, socklen_t len);
};

struct aem_net_server *aem_net_server_init(struct aem_net_server *server);
void aem_net_server_dtor(struct aem_net_server *server);

// You must set srv->setup to set up conn->rx and conn->tx before calling this.
int aem_net_listen(struct aem_net_server *srv, int backlog);

#endif /* AEM_NET_H */
