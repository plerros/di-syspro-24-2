#ifndef SYSPROG24_2_THREAD_WORKER_H
#define SYSPROG24_2_THREAD_WORKER_H

#include "executor_data.h"

struct executor_data;

struct worker_data_t
{
	struct executor_data *exd;
	bool exit_flag;
};

void worker_data_new(struct worker_data_t **ptr, struct executor_data *exd);
void worker_data_free(struct worker_data_t *ptr);
void *worker_fn(void *void_args);

#endif /* SYSPROG24_2_THREAD_WORKER_H */