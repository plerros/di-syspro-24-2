#ifndef SYSPROG24_1_QUEUE_H
#define SYSPROG24_1_QUEUE_H

#include <unistd.h>

struct queue
{
	size_t task_id;
	pid_t pid;
	struct queue *next;
	struct queue *prev;
};

void queue_push(struct queue **ptr, size_t task_id, pid_t pid);
size_t queue_pop(struct queue **ptr);
size_t queue_find_pop(struct queue **ptr, size_t task_id, pid_t pid);

#endif /*SYSPROG24_1_QUEUE_H*/