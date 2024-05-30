#include <stdbool.h>
#include <stdlib.h>

#include "queue.h"

static struct queue *gnext(struct queue *ptr)
{
	if (ptr == NULL)
		return NULL;

	return ptr->next;
}

static struct queue *gprev(struct queue *ptr)
{
	if (ptr == NULL)
		return NULL;

	return ptr->prev;
}

static void snext(struct queue *ptr, struct queue *next)
{
	if (ptr == NULL)
		return;

	ptr->next = next;
}

static void sprev(struct queue *ptr, struct queue *prev)
{
	if (ptr == NULL)
		return;

	ptr->prev = prev;
}

static size_t queue_gettaskid(struct queue *ptr)
{
	if (ptr == NULL)
		return 0;
	return ptr->task_id;
}

static pid_t queue_getpid(struct queue *ptr)
{
	if (ptr == NULL)
		return -1;
	return ptr->pid;
}

void queue_push(struct queue **ptr, size_t task_id, pid_t pid)
{
	if (ptr == NULL)
		return;

	struct queue *new = malloc(sizeof(struct queue));
	if (new == NULL)
		abort();

	new->task_id = task_id;
	new->pid = pid;

	snext(new, new);
	sprev(new, new);

	if (*ptr != NULL) {
		snext(new, *ptr);
		sprev(new, gprev(*ptr));

		snext(gprev(new), new);
		sprev(gnext(new), new);
	}

	*ptr = new;
}


size_t queue_pop(struct queue **ptr)
{
	if (ptr == NULL)
		return 0;

	struct queue *rm = gprev(*ptr);

	sprev(gnext(rm), gprev(rm));
	snext(gprev(rm), gnext(rm));

	*ptr = gnext(rm);

	if (gnext(rm) == rm && gprev(rm) == rm)
		*ptr = NULL;

	size_t ret = 0;
	if (rm != NULL)
		ret = rm->task_id;

	free(rm);
	return ret;
}

size_t queue_find_pop(struct queue **ptr, size_t task_id, pid_t pid)
{
	bool pid_matters = (pid != -1);
	bool tid_matters = (task_id != 0);

	if (!pid_matters && !tid_matters)
		return 0;

	struct queue *to_remove = NULL;
	for (struct queue *i = gnext(*ptr); i != *ptr; i = gnext(i)) {
		if (pid_matters && queue_getpid(i) == pid) {
			to_remove = i;
			break;
		}
		if (pid_matters && queue_gettaskid(i) == task_id) {
			to_remove = i;
			break;
		}
	}
	if (to_remove == NULL)
		return 0;

	to_remove = gnext(to_remove);

	size_t ret = 0;
	if (to_remove == *ptr)
		ret = queue_pop(ptr);
	else
		ret = queue_pop(&to_remove);

	return ret;
}