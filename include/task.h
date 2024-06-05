#ifndef SYSPROG24_2_TASK_H
#define SYSPROG24_2_TASK_H

#include <unistd.h>

#include "configuration.h"

#include "array.h"
#include "netpipe.h"

struct task
{
	struct array *command;
	size_t taskid;
	pid_t pid;
	struct wopipe *to_cmd;
};

void task_new(
	struct task **ptr,
	struct array *command,
	size_t taskid,
	struct wopipe *to_cmd);
void task_free(struct task *ptr);

bool task_isfinished(struct task *ptr);
bool task_isrunning(struct task *ptr);
bool task_iswaiting(struct task *ptr);

void task_run(struct task *ptr);
void task_end(struct task *ptr);

#endif /*SYSPROG24_2_TASK_H*/