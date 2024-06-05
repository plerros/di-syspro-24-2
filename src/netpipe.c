#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "llnode.h"
#include "array.h"
#include "netpipe.h"

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

	if (ptr->fd_acc  == ptr->fd)
		ptr->fd = -1;

	if (ptr->fd_acc != -1)
		close(ptr->fd_acc);

	if (ptr->fd != -1)
		close(ptr->fd);

	free(ptr);
}

void netpipe_write(struct netpipe_t *ptr, struct array *src)
{
	if (ptr->fd == -1)
		return;

	char *buf = (char *)array_get(src, 0);
	size_t size = array_get_size(src) * array_get_elementsize(src);

	ssize_t rc_signed   = write(ptr->fd, buf, size);
	size_t  rc_unsigned = 0;
	if (rc_signed > 0)
		rc_unsigned = (size_t) rc_signed;

	if (rc_unsigned == size)
		return;

	fprintf(stderr, "Write failed\n");
}

void netpipe_read(struct netpipe_t *ptr, struct array **dst, size_t msg_size, size_t msg_count)
{
	if (ptr == NULL)
		return;

	if (ptr->fd == -1)
		return;

	struct llnode *ll = NULL;
	llnode_new(&ll, msg_size, NULL);

	char *buf = malloc(msg_size);
	if (buf == NULL && msg_size > 0)
		abort();

	for (size_t i = 0; i < msg_count; i++) {
		read(ptr->fd, buf, msg_size);
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
	new->pipe->fd = new->pipe->fd_acc;

	getaddrinfo(name, port, &(new->pipe->hints), &(new->addr));
	connect(new->pipe->fd, new->addr->ai_addr, new->addr->ai_addrlen);
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
	netpipe_write(ptr->pipe, src);
}

void ropipe_new(struct ropipe **ptr, char *port)
{
	struct ropipe *new = malloc(sizeof(struct wopipe));
	if (new == NULL)
		abort();

	netpipe_new(&(new->pipe));

	uint16_t port_u16 = atoi(port);
	struct sockaddr_in addr = {
		.sin_family = new->pipe->hints.ai_family,
		.sin_addr.s_addr = htonl(INADDR_ANY),
		.sin_port = htons(port_u16)
	};

	bind(new->pipe->fd_acc, (struct sockaddr *)&(addr), sizeof(addr));
	listen(new->pipe->fd_acc, 5);
	*ptr = new;
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
			perror("ERROR");
			exit(1);
	}
	return rc;
}


static void ropipe_accept(struct ropipe *ptr)
{
	if (ptr == NULL)
		return;

	if (ptr->pipe->fd != -1)
		return;

	struct sockaddr addr;
	socklen_t len;

	while (ptr->pipe->fd == -1)
		ptr->pipe->fd = accept_werr(ptr->pipe->fd_acc, &addr, &len);
}

void ropipe_read(struct ropipe *ptr, struct array **dst, size_t msg_size, size_t msg_count)
{
	ropipe_accept(ptr);
	netpipe_read(ptr->pipe, dst, msg_size, msg_count);
}

void ropipe_close(struct ropipe *ptr)
{
	if (ptr->pipe->fd == -1)
		return;

	close(ptr->pipe->fd);
	ptr->pipe->fd = -1;
}
