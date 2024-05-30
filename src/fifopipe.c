#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "configuration.h"
#include "configuration_adv.h"

#include "helper.h"
#include "llnode.h"
#include "array.h"
#include "fifopipe.h"
#include "packet.h"

// INTERNAL PIPE FUNCTIONS

static void pipe_new(struct fifopipe **ptr, const char *name)
{
	OPTIONAL_ASSERT(ptr != NULL);
	OPTIONAL_ASSERT(*ptr == NULL);

	if (ptr == NULL)
		return;

	struct fifopipe *new = malloc(sizeof(struct fifopipe));
	if (new == NULL)
		abort();

	// Copy pipe name
	new->name = NULL;
	if (name == NULL)
		goto skip_name;

	size_t len = strlen(name) + 1;
	if (len <= 1)
		goto skip_name;

	new->name = malloc(sizeof(char) * len);
	if (new->name == NULL)
		abort();

	void *rp = strcpy(new->name, name);
	if (rp != new->name)
		abort();

skip_name:
	new->fd = -1;

	*ptr = new;
}

static void pipe_free(struct fifopipe *ptr)
{
	if (ptr == NULL)
		return;

	if (ptr->fd != -1)
		close(ptr->fd);

	if (ptr->name != NULL) {
		free(ptr->name);
	}

	free(ptr);
}

static void pipe_open(struct fifopipe *ptr, int flags)
{
	OPTIONAL_ASSERT(ptr != NULL);

	if (ptr == NULL)
		return;
	if (ptr->name == NULL)
		return;
	if (strlen(ptr->name) == 0) {
		abort();
		return;
	}

	if (ptr->fd == -1)
		ptr->fd = open(ptr->name, flags);
	if (ptr->fd == -1)
		abort();
}

ssize_t write_werr(int fd, void *buf, size_t count)
{
	ssize_t rc = write(fd, buf, count);
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

static void pipe_write(struct fifopipe *ptr, struct array *src)
{
	OPTIONAL_ASSERT(ptr != NULL);
	pipe_open(ptr, O_WRONLY);

	int fd = -1;
	if (ptr != NULL) {
		fd = ptr->fd;
	}

	char *data = (char *)array_get(src, 0);
	size_t element_size = array_get_elementsize(src);

	for (size_t i = 0; i < array_get_size(src); i += 1) {
		size_t sum = 0;
		while (sum < element_size) {
			ssize_t rc = write_werr(fd, &(data[i * element_size]), element_size);
			if (rc >= 0)
				sum += (size_t)rc;
		}
	}
}

/*
 * read_werr()
 *
 * read with errors
 * auto-handle some errors to reduce clutter on parent function
 */

ssize_t read_werr(int fd, void *buf, size_t count)
{
	if (fd == -1)
		return 0;

	ssize_t rc = read(fd, buf, count);
	if (rc != -1)
		return rc;

	switch (errno) {
		case EAGAIN:
			break;
		default:
			perror("ERROR");
			exit(1);
	}
	return rc;
}

void msg_print(
	__attribute__((unused)) size_t msg_size,
	__attribute__((unused)) ssize_t rc,
	__attribute__((unused)) char *str)
{
	if (rc < 0)
		return;

#ifdef DEBUG
	fprintf(stderr, "%ld / %ld: ", rc, msg_size);
	for (size_t j = 0; j < msg_size; j++) {
		if (isprint(str[j]))
			fprintf(stderr, "%c", str[j]);
		else
			fprintf(stderr, ".");
	}
	fprintf(stderr, "\n");
#endif
}

static void pipe_read(struct fifopipe *ptr, struct array **dst, size_t msg_size, size_t msg_count)
{
	OPTIONAL_ASSERT(ptr != NULL);
	OPTIONAL_ASSERT(dst != NULL);
	OPTIONAL_ASSERT(*dst == NULL);

	char handshake[] = "handshake";

	int flags = O_RDONLY;
	if (ptr->name != NULL && strcmp(ptr->name, handshake) == 0)
		flags |= O_NONBLOCK;

	pipe_open(ptr, flags);

	int fd = -1;
	if (ptr != NULL)
		fd = ptr->fd;

	struct llnode *ll = NULL;
	llnode_new(&ll, msg_size, NULL);

	char *tmp = malloc(sizeof(char) * msg_size);
	if (tmp == NULL && (msg_size > 0))
		abort();

	for (size_t i = 0; i < msg_count; i++) {
		ssize_t rc = read_werr(fd, tmp, msg_size);
		if ((size_t) rc != msg_size)
			break;

		msg_print(msg_size, rc, tmp);
		llnode_add(&ll, tmp);
	}

	free(tmp);
	array_new(dst, ll);
	llnode_free(ll);
}

// WRITE ONLY PIPE

void wopipe_new(struct wopipe **ptr, const char *name)
{
	OPTIONAL_ASSERT(ptr != NULL);
	OPTIONAL_ASSERT(*ptr == NULL);

	if (ptr == NULL)
		return;

	struct wopipe *new = malloc(sizeof(struct wopipe));
	if (new == NULL)
		abort();

	new->pipe = NULL;

	pipe_new(&(new->pipe), name);
	*ptr = new;
}

void wopipe_free(struct wopipe *ptr)
{
	if (ptr == NULL)
		return;

	if (ptr->pipe != NULL)
		pipe_free(ptr->pipe);

	free(ptr);
}

void wopipe_write(struct wopipe *ptr, struct array *src)
{
	OPTIONAL_ASSERT(ptr != NULL);
	pipe_write(ptr->pipe, src);
}

// READ ONLY PIPE

void ropipe_new(struct ropipe **ptr, const char *name)
{
	OPTIONAL_ASSERT(ptr != NULL);
	OPTIONAL_ASSERT(*ptr == NULL);

	if (ptr == NULL)
		return;

	struct ropipe *new = malloc(sizeof(struct ropipe));
	if (new == NULL)
		abort();

	new->pipe = NULL;
	pipe_new(&(new->pipe), name);
	*ptr = new;
}

void ropipe_free(struct ropipe *ptr)
{
	if (ptr == NULL)
		return;

	char *name = NULL;
	if (ptr->pipe != NULL) {
		name = ptr->pipe->name;
		ptr->pipe->name = NULL;
	}

	pipe_free(ptr->pipe);

	if (name != NULL)
		free(name);

	free(ptr);
}

void ropipe_read(struct ropipe *ptr, struct array **dst, size_t msg_size, size_t msg_count)
{
	OPTIONAL_ASSERT(ptr != NULL);
	if (ptr == NULL)
		return;

	pipe_read(ptr->pipe, dst, msg_size, msg_count);
}