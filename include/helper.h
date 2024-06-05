#ifndef SYSPROG24_2_HELPER_H
#define SYSPROG24_2_HELPER_H

#include <signal.h>
#include <unistd.h>

#include "configuration_adv.h"

void no_args();
void block_sigchild(sigset_t *oldmask);

ssize_t read_werr(int fd, void *buf, size_t count);
ssize_t write_werr(int fd, void *buf, size_t count);

#endif /*SYSPROG24_2_HELPER_H*/