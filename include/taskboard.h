#ifndef SYSPROG24_2_TASKBOARD_H
#define SYSPROG24_2_TASKBOARD_H

#include <unistd.h>

#include "configuration.h"

#include "llnode.h"
#include "array.h"

struct taskboard
{
	struct array *tasks;
	struct llnode *addlater;
};

void taskboard_new(struct taskboard **ptr);
void taskboard_free(struct taskboard *ptr);
size_t taskboard_add(struct taskboard *ptr, struct array *command);
void taskboard_remove_tid(struct taskboard *ptr, size_t tid, struct array **reply);
size_t taskboard_get_waiting(struct taskboard *ptr, struct array **waiting);
size_t taskboard_get_running(struct taskboard *ptr, struct array **running);
pid_t taskboard_run(struct taskboard *ptr);
#endif /*SYSPROG24_2_TASKBOARD_H*/