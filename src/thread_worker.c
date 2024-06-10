#include <stdlib.h>
#include <sys/wait.h>

#include "helper.h"
#include "executor_data.h"
#include "thread_worker.h"

void worker_data_new(struct worker_data_t **ptr, struct executor_data *exd)
{
	struct worker_data_t *new = malloc(sizeof(struct worker_data_t));
	if (new == NULL)
		abort();

	new->exd = exd;
	new->exit_flag = false;
	*ptr = new;
}

void worker_data_free(struct worker_data_t *ptr)
{
	if (ptr == NULL)
		return;

	free(ptr);
}

void *worker_fn(void *void_args)
{
	struct worker_data_t *data = (struct worker_data_t *)void_args;

	while (!(data->exit_flag)) {
		sigset_t oldmask;
		block_sigchild(&oldmask);
		ed_enter_write(data->exd);
		assign_work(data->exd);
		ed_exit_write(data->exd, &(data->exit_flag));
		sigprocmask(SIG_SETMASK, &oldmask, NULL);
	}

	worker_data_free(data);
	return NULL;
}