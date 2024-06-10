#ifndef SYSPROG24_2_EXECUTOR_DATA_H
#define SYSPROG24_2_EXECUTOR_DATA_H

#include <pthread.h>
#include <stdbool.h>

#include "array.h"
#include "queue.h"
#include "taskboard.h"
#include "netpipe.h"

struct executor_data
{
	//Input
	char *port;
	size_t bufferSize;
	int threadPoolSize;

	// MT
	struct llnode *controller_threads;
	pthread_t *worker_threads;

	pthread_mutex_t *mtx;
	int readers;
	int writers;
	pthread_cond_t *read_cond;
	pthread_cond_t *write_cond;

	// Data
	struct ropipe *handshake;
	struct ropipe *from_cmd;
	struct wopipe *to_cmd;
	bool exit_flag;
	struct taskboard *tboard;
	struct queue *waiting;
	struct queue *running;
	unsigned int concurrency;
};

void executor_data_new(struct executor_data **ptr, int argc, char *argv[]);
void executor_data_free(struct executor_data *ptr);
void assign_work(struct executor_data *exd);

void ed_enter_write(struct executor_data *ptr);
void ed_exit_write(struct executor_data *ptr, bool *exit_flag);
#endif /* SYSPROG24_2_EXECUTOR_DATA_H */