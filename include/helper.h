#ifndef SYSPROG24_2_HELPER_H
#define SYSPROG24_2_HELPER_H

#include <signal.h>
#include <unistd.h>

#include "configuration_adv.h"

void no_args();
void block_sigchild(sigset_t *oldmask);

ssize_t read_werr(int fd, void *buf, size_t count);
ssize_t write_werr(int fd, void *buf, size_t count);

void msg_print(
	__attribute__((unused)) size_t msg_size,
	__attribute__((unused)) ssize_t rc,
	__attribute__((unused)) char *str);

void gethost(char *host);
#endif /*SYSPROG24_2_HELPER_H*/