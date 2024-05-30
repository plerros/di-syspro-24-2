#ifndef SYSPROG24_1_HELPER_H
#define SYSPROG24_1_HELPER_H

#include <signal.h>

#include "configuration_adv.h"

void no_args();
void block_sigchild(sigset_t *oldmask);

#endif /*SYSPROG24_1_HELPER_H*/