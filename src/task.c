#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

#include "configuration.h"

#include "array.h"
#include "task.h"
#include "netpipe.h"

#include "packet.h"

void task_new(
	struct task **ptr,
	struct array *command,
	size_t taskid,
	struct wopipe *to_cmd)
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
	new->to_cmd = to_cmd;
	array_copy(command, &(new->command));

	*ptr = new;
}

void task_free(struct task *ptr)
{
	if (ptr == NULL)
		return;

	if (ptr->pid != -1) {
		wait(ptr->pid);
		char outname[26 + 1 + 6];
		sprintf(outname, "%d.output", ptr->pid);
		FILE *fp = fopen(outname, "r");

		struct llnode *ll = NULL;
		llnode_new(&ll, sizeof(char), NULL);

		while (1) {
			int ch = fgetc(fp);
			if (feof(fp))
				break;
			if (ferror(fp))
				break;
			llnode_add(&ll, &ch);
		}
		fclose(fp);
		remove(outname);

		char end = '\0';
		llnode_add(&ll, &end);

		struct array *output = NULL;
		array_new(&output, ll);
		llnode_free(ll);

		struct packets *p = NULL;
		packets_new(&p);
		packets_pack(p, output);
		array_free(output);

		packets_send(p, ptr->to_cmd);
		packets_free(p);
	}

	array_free(ptr->command);
	wopipe_free(ptr->to_cmd);
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