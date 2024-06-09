#ifndef SYSPROG24_2_THREAD_CONTROLLER_H
#define SYSPROG24_2_THREAD_CONTROLLER_H

#include "executor_data.h"

struct controller_data_t
{
	struct executor_data *exd;
	struct ropipe *from_cmd;
	struct wopipe *to_cmd;
	struct array *command;
};

void controller_data_new(
	struct controller_data_t **ptr,
	struct executor_data *exd,
	struct array **command);

void controller_data_free(struct controller_data_t *ptr);
void *controller_fn(void *void_args);

#endif /* SYSPROG24_2_THREAD_CONTROLLER_H */