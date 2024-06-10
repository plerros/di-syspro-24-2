#include <fcntl.h>
#include <linux/limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "configuration.h"

#include "helper.h"
#include "task.h"
#include "taskboard.h"

struct task *task_get(struct array *ptr, size_t tid)
{
	struct task **ret = array_get(ptr, tid);
	if (ret == NULL)
		return NULL;

	return (*ret);
}

void taskboard_addnow(struct taskboard *ptr);

void taskboard_new(struct taskboard **ptr)
{

	OPTIONAL_ASSERT(ptr != NULL);
	OPTIONAL_ASSERT(*ptr == NULL);

	if (ptr == NULL)
		return;

	struct taskboard *new = malloc(sizeof(struct taskboard));
	if (new == NULL)
		abort();

	new->tasks = NULL;
	new->addlater = NULL;
	// Dummy job to take up the job_id 0
	llnode_new(&(new->addlater), sizeof(struct task *), NULL);
	struct task *dummy_job_0 = NULL;

	llnode_add(&(new->addlater), &dummy_job_0);

	*ptr = new;
}

void taskboard_free(struct taskboard *ptr)
{
	if (ptr == NULL)
		return;

	size_t size = array_get_size(ptr->tasks);

	for (size_t i = 0; i < size; i++)
		taskboard_free_tid(ptr, i);

	array_free(ptr->tasks);
	ptr->tasks = NULL;

	taskboard_addnow(ptr);

	size = array_get_size(ptr->tasks);
	for (size_t i = 0; i < size; i++)
		taskboard_free_tid(ptr, i);

	array_free(ptr->tasks);

	free(ptr);
}

size_t taskboard_add(
	struct taskboard *ptr,
	struct array *command,
	struct wopipe *to_cmd,
	struct array **reply)
{
	OPTIONAL_ASSERT(ptr != NULL);
	size_t ret = 0;

	if (ptr == NULL)
		goto out;

	if (command == NULL)
		goto out;

	sigset_t oldmask;
	block_sigchild(&oldmask);

	if (ptr->addlater == NULL)
		llnode_new(&(ptr->addlater), sizeof(struct task *), NULL);

	size_t position = array_get_size(ptr->tasks) + llnode_getsize(ptr->addlater);

	struct task *tmp = NULL;
	task_new(&tmp, command, position, to_cmd);
	llnode_add(&(ptr->addlater), &tmp);

	sigprocmask(SIG_SETMASK, &oldmask, NULL);
	ret = position;

out:
	// Form reply
	char str[100];
	str[0] = '\0';

	if (ret != 0)
		sprintf(str, "JOB_%lu SUBMITTED", tmp->taskid);
	else
		printf("Can't insert job\n");

	struct llnode *ll = NULL;
	llnode_new(&ll, sizeof(char), NULL);

	for (size_t i = 0; i < strlen(str) + 1; i++)
		llnode_add(&ll, &(str[i]));

	if (reply != NULL)
		array_new(reply, ll);

	llnode_free(ll);

	return ret;
}

void taskboard_addnow(struct taskboard *ptr)
{
	if (ptr->addlater == NULL)
		return;

	// Copy array data to an llnode
	struct llnode *ll = NULL;
	llnode_new(&ll, sizeof(struct task *), NULL);

	size_t size = array_get_size(ptr->tasks);
	for (size_t i = 0; i < size; i++)
		llnode_add(&ll, array_get(ptr->tasks, i));

	// Merge the two llnodes
	llnode_link(&(ptr->addlater), &ll);

	array_free(ptr->tasks);
	ptr->tasks = NULL;
	array_new(&(ptr->tasks), ptr->addlater);
	llnode_free(ptr->addlater);
	ptr->addlater = NULL;
}

void taskboard_remove_tid(struct taskboard *ptr, size_t tid, struct array **reply)
{
	if (ptr == NULL)
		return;

	if (tid >= array_get_size(ptr->tasks))
		return;

	sigset_t oldmask;
	block_sigchild(&oldmask);

	taskboard_addnow(ptr);

	struct task *tmp = task_get(ptr->tasks, tid);
	if (tmp == NULL)
		abort();

	// Form reply
	char str[100];
	str[0] = '\0';

	if (task_iswaiting(tmp))
		sprintf(str, "job_%lu removed", tmp->taskid);

	if (task_isrunning(tmp))
		sprintf(str, "job_%lu terminated", tmp->taskid);

	struct llnode *ll = NULL;
	llnode_new(&ll, sizeof(char), NULL);

	for (size_t i = 0; i < strlen(str) + 1; i++)
		llnode_add(&ll, &(str[i]));

	if (reply != NULL)
		array_new(reply, ll);

	llnode_free(ll);

	task_end(tmp);

	sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

void taskboard_free_tid(struct taskboard *ptr, size_t tid)
{
	if (ptr == NULL)
		return;

	if (tid >= array_get_size(ptr->tasks))
		return;

	sigset_t oldmask;
	block_sigchild(&oldmask);

	taskboard_addnow(ptr);

	struct task **tmp = (struct task **)array_get(ptr->tasks, tid);
	if (tmp == NULL)
		abort();

	task_free(*tmp);
	*tmp = NULL;

	sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

static void concat(char **str1, char *str2)
{
	if (*str1 == NULL && str2 == NULL) {
		return;
	}

	if (*str1 != NULL && str2 == NULL) {
		return;
	}

	size_t len1 = 0;
	size_t len2 = 0;

	if (*str1 != NULL)
		len1 = strlen(*str1);
	if (str2 != NULL)
		len2 = strlen(str2);

	if (*str1 == NULL && str2 != NULL) {
		char *new = malloc(len1 + len2 + 1);
		if (new == NULL)
			abort();
		strcpy(new, str2);
		*str1 = new;
		return;
	}

	if (*str1 != NULL && str2 != NULL) {
		char *new = malloc(len1 + len2 + 1);
		if (new == NULL)
			abort();

		strcpy(new, *str1);
		free(*str1);
		strcat(new, str2);
		*str1 = new;
		return;
	}
}

static void concat_task(struct task *t, char **str, size_t queuePosition)
{
	char jobstr[1024];
	sprintf(jobstr, "job_%lu, ", t->taskid);
	concat(str, jobstr);
	concat(str, array_get(t->command, 0));
	sprintf(jobstr, ", %lu\n", queuePosition);
	concat(str, jobstr);
}

size_t taskboard_get_waiting(struct taskboard *ptr, struct array **waiting)
{
	if (ptr == NULL)
		return 0;

	if (ptr->tasks == NULL && ptr->addlater == NULL)
		return 0;

	sigset_t oldmask;
	block_sigchild(&oldmask);

	taskboard_addnow(ptr);

	size_t size = array_get_size(ptr->tasks);
	struct task *tmp = NULL;

	char *str = NULL;

	size_t waiting_position = 0;

	for (size_t i = 0; i < size; i++) {

		tmp = task_get(ptr->tasks, i);
		if (tmp == NULL)
			continue;

		if (task_iswaiting(tmp)) {
			concat_task(tmp, &str, waiting_position);
			waiting_position++;
		}

	}

	struct llnode *ll = NULL;
	llnode_new(&ll, sizeof(char), NULL);

	if (str != NULL) {
		for(size_t i = 0; i < strlen(str); i++)
			llnode_add(&ll, &(str[i]));
		free(str);
		char end = '\0';
		llnode_add(&ll, &(end));
	}

	if (waiting != NULL)
		array_new(waiting, ll);

	llnode_free(ll);

	sigprocmask(SIG_SETMASK, &oldmask, NULL);
	return waiting_position;
}

size_t taskboard_get_running(struct taskboard *ptr, struct array **running)
{
	if (ptr == NULL)
		return 0;

	if (ptr->tasks == NULL && ptr->addlater == NULL)
		return 0;

	sigset_t oldmask;
	block_sigchild(&oldmask);

	taskboard_addnow(ptr);

	size_t size = array_get_size(ptr->tasks);
	struct task *tmp = NULL;

	char *str = NULL;
	size_t running_position = 0;

	for (size_t i = 0; i < size; i++) {
		tmp = task_get(ptr->tasks, i);
		if (task_isrunning(tmp)) {
			concat_task(tmp, &str, running_position);
			running_position++;
		}
	}

	struct llnode *ll = NULL;
	llnode_new(&ll, sizeof(char), NULL);

	if (str != NULL) {
		for(size_t i = 0; i < strlen(str); i++)
			llnode_add(&ll, &(str[i]));
		free(str);
		char end = '\0';
		llnode_add(&ll, &(end));
	}

	if (running != NULL)
		array_new(running, ll);

	llnode_free(ll);

	sigprocmask(SIG_SETMASK, &oldmask, NULL);
	return running_position;
}

pid_t taskboard_run(struct taskboard *ptr)
{
	if (ptr == NULL)
		return -1;

	if (ptr->tasks == NULL && ptr->addlater == NULL)
		return -1;

	sigset_t oldmask;
	block_sigchild(&oldmask);

	taskboard_addnow(ptr);

	size_t size = array_get_size(ptr->tasks);
	struct task *tmp = NULL;

	for (size_t i = 0; i < size; i++) {
		tmp = task_get(ptr->tasks, i);
		if (task_iswaiting(tmp))
			break;
	}

	pid_t ret = -1;

	if (task_iswaiting(tmp)) {
		pid_t pid = fork();
		if (pid < 0)
			abort();

		int rc = 0;
		if (pid == 0) {
			char outname[26 + 1 + 6];
			sprintf(outname, "%d.output", getpid());
			int fout = open(outname, O_WRONLY | O_CREAT, 0666);
			dup2(fout, STDOUT_FILENO);
			fclose(stderr);

			// Add current working directory to path
			char *path = getenv("PATH");
			char cwd[PATH_MAX];
			getcwd(cwd, sizeof(cwd));
			char separator[] = ":";

			char *newpath = malloc(strlen(path) + strlen(separator) + strlen(cwd) + 1);

			strcpy(newpath, path);
			strcat(newpath, separator);
			strcat(newpath, cwd);

			setenv("PATH", newpath, 1);
			rc = execlp("bash", "bash", "-c", (char *)array_get(tmp->command, 0), NULL);
		}
		else {
			tmp->pid = pid;
			ret = pid;
		}

		if (rc == -1) {
			printf("COMMAND FAILED: %s\n", (char *)array_get(tmp->command, 0));
			perror("ERROR");
			exit(1);
		}
	}

	sigprocmask(SIG_SETMASK, &oldmask, NULL);
	return ret;
}
