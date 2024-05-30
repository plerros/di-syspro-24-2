#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "configuration.h"
#include "configuration_adv.h"

#include "helper.h"
#include "llnode.h"
#include "array.h"
#include "command.h"

struct cmd
{
	char name[16];
	int enumval;
	bool exact_length;
};

struct cmd commands[] = {
	{"",                cmd_invalid,        false},
	{"",                cmd_empty,          true},
	{"issueJob ",       cmd_issueJob,       false},
	{"setConcurrency ", cmd_setConcurrency, false},
	{"stop job_",       cmd_stop,           false},
	{"poll running",    cmd_pollrunning,    true},
	{"poll queued",     cmd_pollqueued,     true},
	{"exit",            cmd_exit,           true}
};
#include <stdlib.h>

int command_recognize(struct array *arr)
{
	size_t arr_total_len = array_get_elementsize(arr) * array_get_size(arr);

	char empty[] = "\0";

	char *input = array_get(arr, 0);
	if (input == NULL && arr_total_len == 0) {
		input = &empty[0];
		arr_total_len = 1;
	}

	for (size_t i = cmd_empty; i <= cmd_exit; i++) {
		size_t len = strlen(commands[i].name);
		if (commands[i].exact_length)
			len++;

		if (len > arr_total_len)
			continue;

		if (commands[i].exact_length && len != arr_total_len)
			continue;

		int rc = memcmp(commands[i].name, input, len);
		if (rc == 0)
			return commands[i].enumval;
	}

	return cmd_invalid;
}

void command_strip(struct array *arr, struct array **ret)
{
	OPTIONAL_ASSERT(arr != NULL);
	OPTIONAL_ASSERT(ret != NULL);
	OPTIONAL_ASSERT(*ret == NULL);

	size_t command_length = 0;

	switch(command_recognize(arr)) {
		case cmd_empty:
			break;
		case cmd_invalid:
			break;
		default:
			command_length = strlen(commands[command_recognize(arr)].name);
	}

	size_t skip = command_length;
	while (1) {
		char *tmp = array_get(arr, skip);
		if (tmp == NULL)
			break;

		if (!isspace(*tmp))
			break;

		skip++;
	}

	struct llnode *ll = NULL;
	llnode_new(&ll, sizeof(char), NULL);

	for (size_t i = skip; 1; i++) {
		char *tmp = array_get(arr, i);
		if (tmp == NULL)
			break;

		llnode_add(&ll, tmp);
	}

	array_new(ret, ll);
	llnode_free(ll);
}