#ifndef SYSPROG24_2_NETPIPE_H
#define SYSPROG24_2_NETPIPE_H

#include <netdb.h>

struct netpipe_t
{
	struct addrinfo hints;
	int fd_acc;
	int fd;
};

struct wopipe
{
	struct netpipe_t *pipe;
	struct addrinfo *addr;
};

void wopipe_new(struct wopipe **ptr, char *name, char *port);
void wopipe_free(struct wopipe *ptr);
void wopipe_write(struct wopipe *ptr, struct array *src);

struct ropipe
{
	struct netpipe_t *pipe;
	uint16_t port;
};

void ropipe_new(struct ropipe **ptr, char *port);
void ropipe_free(struct ropipe *ptr);
uint16_t ropipe_get_port(struct ropipe *ptr);
void ropipe_read(struct ropipe *ptr, struct array **dst, size_t msg_size, size_t msg_count);
void ropipe_close(struct ropipe *ptr);

#endif /* SYSPROG24_2_NETPIPE_H */