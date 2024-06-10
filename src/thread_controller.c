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
			size_t task_id = taskboard_add(ptr->exd->tboard, stripped, ptr->to_cmd, &reply);
			queue_push(&(ptr->exd->waiting), task_id, -1);
			break;
		}

		case cmd_setConcurrency: {
			update_concurrency(&(ptr->exd->concurrency), stripped);
			printf("New concurrency: %u\n", ptr->exd->concurrency);
			char str[100] = "\0";
			sprintf(str, "CONCURRENCY_SET_AT %u", ptr->exd->concurrency);
			form_reply(&reply, str);
			break;
		}

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
			char terminated[] = "SERVER_TERMINATED";
			form_reply(&reply, terminated);
			break;
		}

		default:
			abort();
	}
	if (array_get(reply, 0) == NULL) {
		array_free(reply);
		reply = NULL;
		char ack[] = "ack";
		form_reply(&reply, ack);
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

	bool command_submitted = false;
	bool exit_flag = false;

	bool issueJob = false;
	if (command_recognize(data->command) == cmd_issueJob) {
		issueJob = true;
	}

	while (!command_submitted && !exit_flag) {
		sigset_t oldmask;
		block_sigchild(&oldmask);
		ed_enter_write(data->exd);

		size_t jobs = 0;
		if (issueJob) {
			jobs += taskboard_get_waiting(data->exd->tboard, NULL);
			jobs += taskboard_get_running(data->exd->tboard, NULL);
			printf("%lu >= %lu\n", jobs, data->exd->bufferSize);
		}

		if (!issueJob || jobs < data->exd->bufferSize) {
			processcmd(data, data->command);
			command_submitted = true;
		}

		ed_exit_write(data->exd, &exit_flag);
		sigprocmask(SIG_SETMASK, &oldmask, NULL);
		sleep(1);
	}

	if (exit_flag && !command_submitted) {
		char str1[] = "SERVER TERMINATED BEFORE EXECUTION";
		char str2[] = "ack";

		struct array *reply = NULL;
		struct packets *p = NULL;

		form_reply(&reply, str1);
		packets_new(&p);
		packets_pack(p, reply);
		array_free(reply);
		reply = NULL;

		packets_send(p, data->to_cmd);
		packets_free(p);
		p = NULL;

		if (issueJob) {
			form_reply(&reply, str2);
			packets_new(&p);
			packets_pack(p, reply);
			array_free(reply);
			reply = NULL;

			packets_send(p, data->to_cmd);
			packets_free(p);
		}
	}

	controller_data_free(data);
	return NULL;
}
