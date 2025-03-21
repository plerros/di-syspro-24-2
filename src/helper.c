#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "configuration_adv.h"
#include "helper.h"

void no_args() {};

void block_sigchild(sigset_t *oldmask)
{
	sigset_t mask;

	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);

	int rc = sigprocmask(SIG_BLOCK, &mask, oldmask);

	if (rc == -1) {
		perror("ERROR");
		exit(1);
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