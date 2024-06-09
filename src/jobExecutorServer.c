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
#include "netpipe.h"
#include "packet.h"
#include "command.h"
#include "task.h"
#include "taskboard.h"
#include "handshake.h"

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
		if (task_id != 0) {
			char outname[26 + 1 + 6];
			sprintf(outname, "%d.output", pid);
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

			struct task *tmp = task_get(global_data->tboard->tasks, task_id);
			packets_send(p, tmp->to_cmd);
			packets_free(p);

			taskboard_remove_tid(global_data->tboard, task_id, NULL);
		}	
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

	array_print_str(command);
	array_print_str(stripped);

	struct array *reply = NULL;
	struct packets *p = NULL;
	packets_new(&p);

	switch (command_recognize(command)) {
		case cmd_NULL:
			goto skip;

		case cmd_invalid:
			fprintf(stderr, "Invalid Command\n");
			break;

		case cmd_issueJob: {
			size_t task_id = taskboard_add(exd->tboard, stripped, exd->to_cmd);
			queue_push(&(exd->waiting), task_id, -1);
			break;
		}

		case cmd_setConcurrency:
			update_concurrency(&(exd->concurrency), stripped);
			printf("New concurrency: %u\n", exd->concurrency);
			break;

		case cmd_stop: {
			size_t task_id = array_to_u(stripped);
			queue_find_pop_optional(&(exd->waiting), task_id, -1);
			queue_find_pop_optional(&(exd->running), task_id, -1);
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

	packets_pack(p, reply);
	packets_send(p, exd->to_cmd);

	if (command_recognize(command) == cmd_issueJob)
		exd->to_cmd = NULL;
skip:
	packets_free(p);
	array_free(reply);
	array_free(stripped);
	sigprocmask(SIG_SETMASK, &oldmask, NULL);
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

int main(int argc, char *argv[])
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

	if (argc != 4)
		return 1;

	struct executor_data exd;
	global_data = &exd;
	exd.handshake = NULL;

	// Initialize named pipes
	ropipe_new(&(exd.handshake), argv[1]);
	if (ropipe_get_port(exd.handshake) != atoi(argv[1])) {
		printf("Couldn't allocate port %s\n", argv[1]);
		return 1;
	}

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

		exd.from_cmd = NULL;
		exd.to_cmd   = NULL;
		{	
			struct handshake_t *client_data = array_get(arr, 0);
			if (client_data != NULL) {
				handshake_print(client_data);
				wopipe_new(&(exd.to_cmd), client_data->ip, client_data->port);
			}
		}
		array_free(arr);

		{
			char host[NI_MAXHOST];
			gethost(host);

			ropipe_new(&(exd.from_cmd), NULL);
			struct handshake_t server_data;
			handshake_init(&server_data, host, ropipe_get_port(exd.from_cmd));

			struct llnode *ll = NULL;
			handshake_to_llnode(&server_data, &ll);

			arr = NULL;
			array_new(&arr, ll);
			llnode_free(ll);
		}

		p = NULL;
		packets_new(&p);
		packets_pack(p, arr);
		array_free(arr);
		packets_send(p, exd.to_cmd);
		packets_free(p);

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
		ropipe_close(exd.handshake);
	}

	global_data = NULL;

	while (exd.waiting != NULL)
		queue_pop(&(exd.waiting));
	while (exd.running != NULL)
		queue_pop(&(exd.running));

	block_sigchild(NULL); // Protect Taskboard_free
	taskboard_free(exd.tboard);
	ropipe_close(exd.handshake);
	ropipe_free(exd.handshake);
	remove(HANDSHAKE);
	remove(TXT_NAME);
	return 0;
}
