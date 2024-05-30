#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>


#include "configuration.h"

#include "helper.h"
#include "queue.h"
#include "llnode.h"
#include "array.h"
#include "fifopipe.h"
#include "packet.h"
#include "command.h"
#include "taskboard.h"

void create_txt()
{
	FILE *txt;
	txt = fopen(TXT_NAME, "w");
	fprintf(txt, "%d", getpid());
	fclose(txt);
}

struct executor_data
{
	struct ropipe *handshake;
	struct ropipe *from_cmd;
	struct wopipe *to_cmd;
	bool exit_flag;
	struct taskboard *tboard;
	struct queue *waiting;
	struct queue *running;
	unsigned int concurrency;
};

unsigned int array_to_u(struct array *stripped)
{
	long long tmp = strtol((char *)array_get(stripped, 0), NULL, 10);
	if (tmp < 0)
		return 0;

	if (tmp > UINT_MAX)
		tmp = UINT_MAX;

	return ((unsigned int)tmp);
}

void update_concurrency(unsigned int *concurrency, struct array *stripped)
{
	long long tmp = strtol((char *)array_get(stripped, 0), NULL, 10);
	if (tmp < 0)
		return;

	if (tmp > UINT_MAX)
		tmp = UINT_MAX;

	*concurrency = (unsigned int)tmp;
}

struct executor_data *global_data = NULL;

void sigchld_handler(__attribute__((unused))int sig)
{
	pid_t pid;
	while((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
		size_t task_id =  queue_find_pop(&(global_data->running), 0, pid);
		if (task_id != 0)
			taskboard_remove_tid(global_data->tboard, task_id, NULL);
	}
}

void executor_processcmd(struct executor_data *exd, struct array *command)
{
	if (command == NULL)
		return;

	sigset_t oldmask;
	block_sigchild(&oldmask);

	struct array *stripped = NULL;
	command_strip(command, &stripped);

#ifdef DEBUG
	array_print_str(command);
	array_print_str(stripped);
#endif

	struct array *reply = NULL;

	switch (command_recognize(command)) {
		case cmd_empty:
			goto skip;

		case cmd_invalid:
			fprintf(stderr, "Invalid Command\n");
			break;

		case cmd_issueJob: {
			size_t task_id = taskboard_add(exd->tboard, stripped);
			queue_push(&(exd->waiting), task_id, -1);
			break;
		}

		case cmd_setConcurrency:
			update_concurrency(&(exd->concurrency), stripped);
			printf("New concurrency: %u\n", exd->concurrency);
			break;

		case cmd_stop: {
			size_t task_id = array_to_u(stripped);
#if (QUEUE_CLEAR == true)
			queue_find_pop(&(exd->waiting), task_id, -1);
			queue_find_pop(&(exd->running), task_id, -1);
#endif
			taskboard_remove_tid(exd->tboard, task_id, &reply);
			break;
		}

		case cmd_pollrunning:
			taskboard_get_running(exd->tboard, &reply);
			break;

		case cmd_pollqueued:
			taskboard_get_waiting(exd->tboard, &reply);
			break;

		case cmd_exit: {
			exd->exit_flag = true;
			struct llnode *ll = NULL;
			char terminated[] = "jobExecutorServer terminated";
			llnode_new(&ll, sizeof(char), NULL);

			for (size_t i = 0; i < strlen(terminated) + 1; i++)
				llnode_add(&ll, &(terminated[i]));

			array_new(&reply, ll);
			llnode_free(ll);
			break;
		}

		default:
			abort();
	}
	if (array_get(reply, 0) == NULL) {
		array_free(reply);
		reply = NULL;

		struct llnode *ll = NULL;
		char ack[] = "ack";
		llnode_new(&ll, sizeof(char), NULL);

		for (size_t i = 0; i < strlen(ack) + 1; i++)
			llnode_add(&ll, &(ack[i]));

		array_new(&reply, ll);
		llnode_free(ll);
	}

	struct packets *p = NULL;
	packets_new(&p);
	packets_pack(p, reply);
	packets_send(p, exd->to_cmd);
	packets_free(p);
	array_free(reply);

skip:
	array_free(stripped);
	sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

void mkfifo_werr(char *str)
{
	if (strlen(str) == 0)
		return;

	int rc = mkfifo(str, 0600);
	if (rc == -1){
		perror("ERROR");
		exit(1);
	}
}

void assign_work(struct executor_data *exd)
{
	sigset_t oldmask;
	block_sigchild(&oldmask);

	if (taskboard_get_running(exd->tboard, NULL) < exd->concurrency) {
		size_t task_id = queue_pop(&(exd->waiting));
		pid_t pid = taskboard_run(exd->tboard);
		if (task_id != 0)
			queue_push(&(exd->running), task_id, pid);
	}

	sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

int main()
{
	// Set up signal handling
	struct sigaction sa;
	{ // SIGCHLD
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = sigchld_handler;
		sigaction(SIGCHLD, &sa, NULL);
	}
	signal(SIGPIPE, SIG_IGN);

	create_txt();

	struct executor_data exd;
	global_data = &exd;
	exd.handshake = NULL;

	// Initialize named pipes
	mkfifo_werr(HANDSHAKE);
	ropipe_new(&(exd.handshake), HANDSHAKE);

	exd.tboard = NULL;
	taskboard_new(&(exd.tboard));

	exd.waiting = NULL;
	exd.running = NULL;
	exd.concurrency = 1;

	exd.exit_flag = false;
	while (!exd.exit_flag) {
		struct packets *p = NULL;
		packets_new(&p);
		packets_receive(p, exd.handshake);

		struct array *arr = NULL;
		packets_unpack(p, &arr);
		packets_free(p);

		char pid[100];
		char tocmd_name[200];
		char fromcmd_name[200];
		pid[0] = '\0';
		tocmd_name[0] = '\0';
		fromcmd_name[0] = '\0';

		for (size_t i = 0; i < array_get_size(arr); i++)
			pid[i] = *((char *)array_get(arr, i));

		array_free(arr);

		if (pid[0] != '\0') {
			sprintf(tocmd_name, "pipes/%s.tocmd", pid);
			sprintf(fromcmd_name, "pipes/%s.toexec", pid);
		}

		mkfifo_werr(fromcmd_name);
		mkfifo_werr(tocmd_name);

		exd.from_cmd = NULL;
		exd.to_cmd   = NULL;
		ropipe_new(&(exd.from_cmd), fromcmd_name);
		wopipe_new(&(exd.to_cmd), tocmd_name);

		p = NULL;
		packets_new(&p);
		packets_receive(p, exd.from_cmd);

		struct array *command = NULL;
		packets_unpack(p, &command);
		packets_free(p);

		executor_processcmd(&exd, command);

		assign_work(&exd);

		array_free(command);

		ropipe_free(exd.from_cmd);
		wopipe_free(exd.to_cmd);
	}

	global_data = NULL;

	while (exd.waiting != NULL)
		queue_pop(&(exd.waiting));
	while (exd.running != NULL)
		queue_pop(&(exd.running));

	block_sigchild(NULL); // Protect Taskboard_free
	taskboard_free(exd.tboard);
	ropipe_free(exd.handshake);
	remove(HANDSHAKE);
	remove(TXT_NAME);
	return 0;
}
