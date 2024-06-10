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

#include "executor_data.h"
#include "thread_controller.h"
#include "thread_worker.h"

void create_txt()
{
	FILE *txt;
	txt = fopen(TXT_NAME, "w");
	fprintf(txt, "%d", getpid());
	fclose(txt);
}

struct executor_data *global_data = NULL;

void sigchld_handler(__attribute__((unused))int sig)
{
	pid_t pid;
	while((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
		ed_enter_write(global_data);

		size_t task_id =  queue_find_pop(&(global_data->running), 0, pid);
		if (task_id == 0)
			continue;

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

		task_end(task_get(taskboard_get_tasks(global_data->tboard), task_id));
		ed_exit_write(global_data, NULL);
	}
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

	struct executor_data *exd = NULL;
	executor_data_new(&exd, argc, argv);

	for (int i = 0; i < exd->threadPoolSize; i++) {
		struct worker_data_t *tmp = NULL;
		worker_data_new(&tmp, exd);
		int rc = pthread_create(&(exd->worker_threads[i]), NULL, worker_fn, (void *)(tmp));
		assert(rc == 0);
	}

	global_data = exd;

	char host[NI_MAXHOST];
	gethost(host);

	// Initialize named pipes
	bool exit_flag = false;
	while (!exd->exit_flag) {
		struct packets *p = NULL;
		packets_new(&p);
		packets_receive(p, exd->handshake);

		struct array *arr = NULL;
		packets_unpack(p, &arr);
		packets_free(p);

		exd->from_cmd = NULL;
		exd->to_cmd   = NULL;
		{
			struct handshake_t *client_data = array_get(arr, 0);
			if (client_data != NULL) {
				handshake_print(client_data);
				wopipe_new(&(exd->to_cmd), client_data->ip, client_data->port);
			}
		}
		array_free(arr);

		{
			ropipe_new(&(exd->from_cmd), NULL);
			struct handshake_t server_data;
			handshake_init(&server_data, host, ropipe_get_port(exd->from_cmd));

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
		packets_send(p, exd->to_cmd);
		packets_free(p);

		p = NULL;
		packets_new(&p);
		packets_receive(p, exd->from_cmd);

		struct array *command = NULL;
		packets_unpack(p, &command);
		packets_free(p);

		if (array_get(command, 0) != NULL) {
			struct controller_data_t *controller_data = NULL;
			controller_data_new(&controller_data, exd, &command);

			pthread_t tmp;

			int rc = pthread_create(&tmp, NULL, controller_fn, (void *)(controller_data));
			assert(rc == 0);

			llnode_add(&(exd->controller_threads), &tmp);
		}

		array_free(command);

		ropipe_free(exd->from_cmd);
		wopipe_free(exd->to_cmd);
		ropipe_close(exd->handshake);

		// Update the exit flag.
		// TODO make less janky.
		ed_enter_write(exd);
		ed_exit_write(exd, &exit_flag);
	}

	for (int i = 0; i < exd->threadPoolSize; i++) {
		pthread_join(exd->worker_threads[i], 0);
	}

	struct array *controller_threads = NULL;
	array_new(&controller_threads, exd->controller_threads);
	llnode_free(exd->controller_threads);

	for (size_t i = 0; i < array_get_size(controller_threads); i++) {
		pthread_t *tmp = array_get(controller_threads, i);
		pthread_join(*tmp, 0);
	}
	array_free(controller_threads);

	global_data = NULL;
	block_sigchild(NULL); // Protect Taskboard_free
	executor_data_free(exd);
	return 0;
}
