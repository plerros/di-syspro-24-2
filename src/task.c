#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

#include "configuration.h"

#include "array.h"
#include "task.h"

void task_new(struct task **ptr, struct array *command, size_t taskid)
{
	OPTIONAL_ASSERT(ptr != NULL);
	OPTIONAL_ASSERT(*ptr == NULL);

	if (ptr == NULL)
		return;

	struct task *new = malloc(sizeof(struct task));
	if (new == NULL)
		abort();

	new->taskid = taskid;
	new->pid = -1;
	new->command = NULL;
	array_copy(command, &(new->command));

	*ptr = new;
}

void task_free(struct task *ptr)
{
	if (ptr == NULL)
		return;

	if (ptr->pid != -1)
		kill(ptr->pid, SIGKILL);

	array_free(ptr->command);
	free(ptr);
}

bool task_isfinished(struct task *ptr)
{
	if (ptr == NULL)
		return true;

	return (array_get(ptr->command, 0) == NULL);
}

bool task_isrunning(struct task *ptr)
{
	if (task_isfinished(ptr))
		return false;

	if (ptr->pid == -1)
		return false;

	return true;
}

bool task_iswaiting(struct task *ptr)
{
	if (task_isfinished(ptr))
		return false;

	if (task_isrunning(ptr))
		return false;

	return true;
}

void task_run(struct task *ptr)
{
	if (ptr == NULL)
		return;

	if (!task_iswaiting(ptr))
		return;

	char *command = (char *)array_get(ptr->command, 0);

	pid_t pid = fork();
	if (pid < 0)
		abort();

	int rc = 0;
	if (pid == 0)
		rc = execl(command, command, NULL);
	else
		ptr->pid = pid;

	if (rc == -1) {
		perror("ERROR");
		exit(1);
	}
}

/*
 * Note to future self:
 * 
 * This function must free all pointers & set them to NULL.
 */

void task_end(struct task *ptr)
{
	if (ptr == NULL)
		return;

	if (ptr->pid != -1)
		kill(ptr->pid, SIGKILL);

	ptr->pid = -1;
	array_free(ptr->command);
	ptr->command = NULL;
}