#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "llnode.h"
#include "array.h"
#include "netpipe.h"
#include "helper.h"

__attribute__((unused))
static void set_nonblock(int fd)
{
	if (fd == -1)
		return;

	int flags = fcntl(fd, F_GETFL);
	flags = flags | O_NONBLOCK;

	int rc = fcntl(fd, F_SETFL, flags);
	if (rc >= 0)
		return;
	
	perror("ERROR: fcntl");
	exit(1);
}

static int netpipe_get_fdacc(struct netpipe_t *ptr)
{
	if (ptr == NULL)
		return -1;
	
	return (ptr->fd_acc);
}

static int netpipe_get_fd(struct netpipe_t *ptr)
{
	if (ptr == NULL)
		return -1;
	
	return (ptr->fd);
}

static void netpipe_set_fd(struct netpipe_t *ptr, int fd)
{
	OPTIONAL_ASSERT(ptr != NULL);
	if (ptr == NULL)
		return;
	
	ptr->fd = fd;
}

void netpipe_new(struct netpipe_t **ptr)
{
	struct netpipe_t *new = malloc(sizeof(struct netpipe_t));
	if (new == NULL)
		abort();

	struct addrinfo hints = {
		.ai_flags = 0,
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = 0
	};
	new->hints = hints;

	int domain   = new->hints.ai_family;
	int type     = new->hints.ai_socktype;
	int protocol = 0;
	new->fd_acc = socket(domain, type, protocol);
	new->fd = -1;

	*ptr = new;
}

void netpipe_free(struct netpipe_t *ptr)
{
	if (ptr == NULL)
		return;

	if (netpipe_get_fdacc(ptr) == netpipe_get_fd(ptr))
		netpipe_set_fd(ptr, -1);

	if (netpipe_get_fdacc(ptr) != -1)
		close(netpipe_get_fdacc(ptr));

	if (netpipe_get_fd(ptr) != -1)
		close(netpipe_get_fd(ptr));

	free(ptr);
}

void netpipe_write(struct netpipe_t *ptr, struct array *src)
{
	OPTIONAL_ASSERT(ptr != NULL);
	if (ptr == NULL)
		return;

	if (netpipe_get_fd(ptr) == -1)
		return;

	char *buf = (char *)array_get(src, 0);
	size_t size = array_get_size(src) * array_get_elementsize(src);

	size_t sum = 0;
	while (sum < size) {
		ssize_t rc = write_werr(netpipe_get_fd(ptr), buf, size - sum);
		if (rc >= 0)
			sum += (size_t)rc;

		msg_print(size, rc, buf);
	}
}

void netpipe_read(
	struct netpipe_t *ptr,
	struct array **dst,
	size_t msg_size,
	size_t msg_count)
{
	OPTIONAL_ASSERT(ptr != NULL);
	if (ptr == NULL)
		return;

	if (netpipe_get_fd(ptr) == -1)
		return;

	struct llnode *ll = NULL;
	llnode_new(&ll, msg_size, NULL);

	char *buf = malloc(msg_size);
	if (buf == NULL && msg_size > 0)
		abort();

	for (size_t i = 0; i < msg_count; i++) {
		ssize_t rc = read_werr(netpipe_get_fd(ptr), buf, msg_size);
		if ((size_t) rc != msg_size)
			break;

		msg_print(msg_size, rc, buf);
		llnode_add(&ll, buf);
	}

	free(buf);
	array_new(dst, ll);
	llnode_free(ll);
}

void wopipe_new(struct wopipe **ptr, char *name, char *port)
{
	struct wopipe *new = malloc(sizeof(struct wopipe));
	if (new == NULL)
		abort();

	new->pipe = NULL;
	netpipe_new(&(new->pipe));
	netpipe_set_fd(new->pipe, netpipe_get_fdacc(new->pipe));

	getaddrinfo(name, port, &(new->pipe->hints), &(new->addr));

	int fd = netpipe_get_fd(new->pipe);
	connect(fd, new->addr->ai_addr, new->addr->ai_addrlen);
	*ptr = new;
}

void wopipe_free(struct wopipe *ptr)
{
	if (ptr == NULL)
		return;

	netpipe_free(ptr->pipe);
	freeaddrinfo(ptr->addr);
	free(ptr);
}
void wopipe_write(struct wopipe *ptr, struct array *src)
{
	if (ptr == NULL)
		return;

	netpipe_write(ptr->pipe, src);
}


int bind_werr(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	if (sockfd == -1)
		return 0;

	int rc = bind(sockfd, addr, addrlen);
	if (rc != -1)
		return rc;

	switch (errno) {
		case EACCES:
			break;
		case EADDRINUSE:
			break;		
		default:
			perror("ERROR: bind");
			exit(1);
	}
	return rc;
}

uint16_t ropipe_new(struct ropipe **ptr, char *port)
{
	struct ropipe *new = malloc(sizeof(struct wopipe));
	if (new == NULL)
		abort();

	netpipe_new(&(new->pipe));

	uint16_t port_u16 = 1;

	if (port != NULL)
		port_u16 = atoi(port);

	for (; port_u16 < UINT16_MAX; port_u16++) {
		struct sockaddr_in addr = {
			.sin_family = new->pipe->hints.ai_family,
			.sin_addr.s_addr = htonl(INADDR_ANY),
			.sin_port = htons(port_u16)
		};
		int rc = bind_werr(
			netpipe_get_fdacc(new->pipe),
			(struct sockaddr *)&(addr),
			sizeof(addr));

		if (rc == 0)
			break;
	}
	listen(netpipe_get_fdacc(new->pipe), 5);
	*ptr = new;

	return port_u16;
}

void ropipe_free(struct ropipe *ptr)
{
	if (ptr == NULL)
		return;

	netpipe_free(ptr->pipe);
	free(ptr);
}

static int accept_werr(int fd, struct sockaddr *addr, socklen_t *len)
{
	int rc = accept(fd, NULL, NULL);
	if (rc != -1)
		return rc;

	switch (errno) {
		case EAGAIN:
			break;
		case EPIPE:
			break;
		default:
			perror("ERROR: accept");
			exit(1);
	}
	return rc;
}

static void ropipe_accept(struct ropipe *ptr)
{
	if (ptr == NULL)
		return;

	if (netpipe_get_fd(ptr->pipe) != -1)
		return;

	struct sockaddr addr;
	socklen_t len;

	while (netpipe_get_fd(ptr->pipe) == -1)
		netpipe_set_fd(ptr->pipe, accept_werr(netpipe_get_fdacc(ptr->pipe), &addr, &len));
}

bool pollin_check(int fd, int timeout)
{
	if (fd == -1)
		return false;

	struct pollfd pfd = {
		.fd = fd,
		.events = POLLIN | POLLERR | POLLHUP,
		.revents = POLLIN | POLLERR | POLLHUP
	};

	int rc = poll(&pfd, 1, timeout);
	if (rc < 0) {
		perror("ERROR: poll");
		exit(1);
	}

	if (pfd.revents & POLLIN) {
		printf("Received Data\n");
		return true;
	}

	if (pfd.revents & (POLLERR | POLLHUP))
		printf("Socket was closed\n");
	else
		printf("NoData\n");

	return false;
}

void ropipe_read(struct ropipe *ptr, struct array **dst, size_t msg_size, size_t msg_count)
{
	sigset_t oldmask;
	block_sigchild(&oldmask);

	int fd_acc = netpipe_get_fdacc(ptr->pipe);
	int fd     = netpipe_get_fd(ptr->pipe);
	bool fd_acc_pollin = false;
	bool fd_pollin     = false;

	// POLL fd_acc
	if (fd_acc != -1 && fd == -1)
		fd_acc_pollin = pollin_check(fd_acc, 1000);

	if (fd_acc_pollin)
		ropipe_accept(ptr);

	fd = netpipe_get_fd(ptr->pipe);
	if (fd != -1)
		fd_pollin = pollin_check(fd, 1000);

	if (fd_pollin)
		netpipe_read(ptr->pipe, dst, msg_size, msg_count);

	sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

void ropipe_close(struct ropipe *ptr)
{
	if (netpipe_get_fd(ptr->pipe) == -1)
		return;

	close(netpipe_get_fd(ptr->pipe));
	netpipe_set_fd(ptr->pipe ,-1);
}