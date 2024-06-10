#include <limits.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "helper.h"
#include "queue.h"
#include "netpipe.h"
#include "taskboard.h"
#include "executor_data.h"

void executor_data_new(struct executor_data **ptr, int argc, char *argv[])
{
	OPTIONAL_ASSERT(ptr != NULL);
	OPTIONAL_ASSERT(*ptr == NULL);

	struct executor_data *new = malloc(sizeof(struct executor_data));

	// Input
	new->port = NULL;
	new->bufferSize = 0;
	new->threadPoolSize = 0;

	if (argc > 1)
		new->port = argv[1];
	if (argc > 2)
		new->bufferSize = atoi(argv[2]);
	if (argc > 3)
		new->threadPoolSize = atoi(argv[3]);

	//Multithreading
	new->controller_threads = NULL;
	llnode_new(&(new->controller_threads), sizeof(pthread_t), NULL);

	new->worker_threads = malloc(sizeof(pthread_t) * new->threadPoolSize);
	if (new->worker_threads == NULL)
		abort();

	new->mtx = malloc(sizeof(pthread_mutex_t));
	if (new->mtx == NULL)
		abort();
	pthread_mutex_init(new->mtx, NULL);

	new->readers = 0;
	new->read_cond = malloc(sizeof(pthread_cond_t));
	if (new->read_cond == NULL)
		abort();
	pthread_cond_init(new->read_cond, NULL);

	new->writers = 0;
	new->write_cond = malloc(sizeof(pthread_cond_t));
	if (new->write_cond == NULL)
		abort();
	pthread_cond_init(new->write_cond, NULL);

	//Other
	new->handshake = NULL;
	ropipe_new(&(new->handshake), new->port);
	if (ropipe_get_port(new->handshake) != atoi(new->port)) {
		printf("Couldn't allocate port %s\n", new->port);
		exit(1);
	}

	new->tboard = NULL;
	taskboard_new(&(new->tboard));

	new->waiting = NULL;
	new->running = NULL;
	new->concurrency = 1;
	new->exit_flag = false;
	*ptr = new;
}

void executor_data_free(struct executor_data *ptr)
{
	if (ptr == NULL)
		return;

	free(ptr->worker_threads);
	pthread_mutex_destroy(ptr->mtx);
	free(ptr->mtx);

	pthread_cond_destroy(ptr->read_cond);
	free(ptr->read_cond);
	pthread_cond_destroy(ptr->write_cond);
	free(ptr->write_cond);

	while (ptr->waiting != NULL)
		queue_pop(&(ptr->waiting));
	while (ptr->running != NULL)
		queue_pop(&(ptr->running));

	taskboard_free(ptr->tboard);
	ropipe_free(ptr->handshake);
	free(ptr);
}

void assign_work(struct executor_data *ptr)
{
	sigset_t oldmask;
	block_sigchild(&oldmask);

	if (taskboard_get_running(ptr->tboard, NULL) < ptr->concurrency) {
		size_t task_id = queue_pop(&(ptr->waiting));
		pid_t pid = taskboard_run(ptr->tboard);
		if (task_id != 0)
			queue_push(&(ptr->running), task_id, pid);
	}

	sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

void ed_enter_write(struct executor_data *ptr)
{
	pthread_mutex_lock(ptr->mtx);
	{
		while (ptr->writers > 0) {
			pthread_cond_wait(ptr->write_cond, ptr->mtx);
		}
		ptr->writers += 1;
	}
	pthread_mutex_unlock(ptr->mtx);
}

void ed_exit_write(struct executor_data *ptr, bool *exit_flag)
{
	pthread_mutex_lock(ptr->mtx);
	{
		ptr->writers -= 1;
		pthread_cond_broadcast(ptr->read_cond);
		pthread_cond_broadcast(ptr->write_cond);

		if (ptr->exit_flag == true && exit_flag != NULL)
			*exit_flag = true;
	}
	pthread_mutex_unlock(ptr->mtx);
}