#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "configuration.h"
#include "configuration_adv.h"

#include "helper.h"
#include "queue.h"
#include "llnode.h"
#include "array.h"
#include "netpipe.h"
#include "packet.h"
#include "command.h"
#include "taskboard.h"
#include "executor_data.h"
#include "thread_controller.h"

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

void processcmd(struct controller_data_t *ptr, struct array *command)
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
			size_t task_id = taskboard_add(ptr->exd->tboard, stripped, ptr->to_cmd);
			queue_push(&(ptr->exd->waiting), task_id, -1);
			break;
		}

		case cmd_setConcurrency:
			update_concurrency(&(ptr->exd->concurrency), stripped);
			printf("New concurrency: %u\n", ptr->exd->concurrency);
			break;

		case cmd_stop: {
			size_t task_id = array_to_u(stripped);
			queue_find_pop_optional(&(ptr->exd->waiting), task_id, -1);
			queue_find_pop_optional(&(ptr->exd->running), task_id, -1);
			taskboard_remove_tid(ptr->exd->tboard, task_id, &reply);
			break;
		}

		case cmd_pollrunning:
			taskboard_get_running(ptr->exd->tboard, &reply);
			break;

		case cmd_pollqueued:
			taskboard_get_waiting(ptr->exd->tboard, &reply);
			break;

		case cmd_exit: {
			ptr->exd->exit_flag = true;
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
	packets_send(p, ptr->to_cmd);

	if (command_recognize(command) == cmd_issueJob)
		ptr->to_cmd = NULL;
skip:
	packets_free(p);
	array_free(reply);
	array_free(stripped);
	sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

void controller_data_new(
	struct controller_data_t **ptr,
	struct executor_data *exd,
	struct array **command
)
{
	OPTIONAL_ASSERT(ptr != NULL);
	OPTIONAL_ASSERT(*ptr == NULL);

	struct controller_data_t *new = malloc(sizeof(struct controller_data_t));
	if (new == NULL)
		abort();

	new->exd = exd;
	new->from_cmd = exd->from_cmd;
	new->to_cmd   = exd->to_cmd;
	new->command  = *command;

	exd->from_cmd = NULL;
	exd->to_cmd   = NULL;
	*command      = NULL;
	*ptr = new;
}

void controller_data_free(struct controller_data_t *ptr)
{
	if (ptr == NULL)
		return;

	ropipe_free(ptr->from_cmd);
	wopipe_free(ptr->to_cmd);
	array_free(ptr->command);
	free(ptr);
}

void *controller_fn(void *void_args)
{
	struct controller_data_t *data = (struct controller_data_t *)void_args;

	ed_enter_write(data->exd);
	processcmd(data, data->command);
	ed_exit_write(data->exd, NULL);
	return NULL;
}
