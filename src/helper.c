#include <ctype.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
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

void gethost(char *host)
{
	struct ifaddrs *ifap;
	int rc = getifaddrs(&ifap);
	if (rc == -1) {
		perror("ERROR");
		exit(1);
	}

	struct ifaddrs *i = ifap;
	host[0] = '\0';

	for (; i != NULL; i = i->ifa_next) {
		// Ignore loopback
		if (strcmp(i->ifa_name, "lo") == 0)
			continue;

		int family = i->ifa_addr->sa_family;

		if (family != AF_INET)
			continue;

		int s = getnameinfo(i->ifa_addr, sizeof(struct sockaddr_in),
			host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
		if (s != 0) {
			printf("getnameinfo() failed: %s\n", gai_strerror(s));
			exit(1);
		}
#ifdef DEBUG
		printf("<Interface>: %s \t <Address> %s\n", i->ifa_name, host);
#endif
		break;

	}
	free(ifap);
	if (host[0] == '\0')
		sprintf(host, "127.0.0.1");
}