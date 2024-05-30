#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

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
