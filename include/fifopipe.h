#ifndef SYSPROG24_1_FIFOPIPE_H
#define SYSPROG24_1_FIFOPIPE_H

enum pipe_type {readonly, writeonly};

struct fifopipe
{
	char *name;
	int fd;
};

struct wopipe
{
	struct fifopipe *pipe;
};

struct ropipe
{
	struct fifopipe *pipe;
};

void wopipe_new(struct wopipe **ptr, const char *name);
void wopipe_free(struct wopipe *ptr);
void wopipe_write(struct wopipe *ptr, struct array *src);

void ropipe_new(struct ropipe **ptr, const char *name);
void ropipe_free(struct ropipe *ptr);
void ropipe_read(struct ropipe *ptr, struct array **dst, size_t msg_size, size_t msg_count);

#endif /*SYSPROG24_1_FIFOPIPE_H*/