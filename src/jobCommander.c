#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "configuration.h"

#include "llnode.h"
#include "array.h"
#include "netpipe.h"
#include "packet.h"
#include "command.h"
#include "handshake.h"
#include "helper.h"

void print_txtreadretry(int retries, int retries_max)
{
	switch (retries) {
		case 0:
			break;
		case 1:
			printf("Waiting for %s\n", TXT_NAME);
			break;
		default:
			printf("%d / %d\n", retries, retries_max);
	}
}

void wait_for_txt(int n)
{
	for (int i = 0; i < n; i++) {
		if (access(TXT_NAME, F_OK) == 0)
			return;

		print_txtreadretry(i, n);
		sleep(1);
	}
}

void reply_receive(struct ropipe *from_exec)
{
	struct array *reply = NULL;
	int retry = 0;
	do {
		array_free(reply);
		reply = NULL;

		struct packets *p = NULL;
		packets_new(&p);
		packets_receive(p, from_exec);
		packets_unpack(p, &reply);
		packets_free(p);
		retry++;
		if (retry % 100 == 0)
			sleep(1);
	} while (array_get(reply, 0) == NULL && retry < 10000);

	char *str = (char *) array_get(reply, 0);
	char str2[] = "ack";
	if (str != NULL && !(strcmp(str, str2) == 0))
		printf("%s\n", str);

	array_free(reply);
}

int main(int argc, char *argv[])
{
	if (argc < 4)
		return 1;

	// Initialize handshake pipe
	struct wopipe *handshake = NULL;
	wopipe_new(&handshake, argv[1], argv[2]);

	struct ropipe *from_exec = NULL;
	uint16_t port_input = ropipe_new(&from_exec, NULL);

	// Handshake
	char host[NI_MAXHOST];
	gethost(host);

	struct handshake_t hs_data;
	handshake_init(&hs_data, host, port_input); //TODO not static

	struct llnode *ll = NULL;
	handshake_to_llnode(&hs_data, &ll);

	struct array *arr = NULL;
	array_new(&arr, ll);
	llnode_free(ll);

	struct packets *p = NULL;
	packets_new(&p);
	packets_pack(p, arr);
	array_free(arr);
	packets_send(p, handshake);
	packets_free(p);

	// Receive Handshake
	struct wopipe *to_exec = NULL;
	{
		struct handshake_t *server_data = NULL;
		while (1) {
			p = NULL;
			packets_new(&p);
			packets_receive(p, from_exec);

			arr = NULL;
			packets_unpack(p, &arr);
			packets_free(p);
	
			server_data = array_get(arr, 0);
			if (server_data != NULL)
				break;

			array_free(arr);
		}
		
		handshake_print(server_data);
		wopipe_new(&to_exec, server_data->ip, server_data->port);
	}
	array_free(arr);
	p = NULL;

	// Initialize pipes

	// Parse args
	ll = NULL;
	llnode_new(&ll, sizeof(char), NULL);

	for (int i = 3; i < argc; i++) {
		for(size_t j = 0; j < strlen(argv[i]); j++) {
			llnode_add(&ll, &(argv[i][j]));
		}
		// Add back space character between arguments
		if (i + 1 < argc) {
			char tmp = ' ';
			llnode_add(&ll, &tmp);
		}
	}
	// Add back null character to denote end of string
	{
		char tmp = '\0';
		llnode_add(&ll, &tmp);
	}

	arr = NULL;
	array_new(&arr, ll);
	llnode_free(ll);

	if (command_recognize(arr) == cmd_invalid)
		fprintf(stderr, "Invalid Command\n");

	p = NULL;
	packets_new(&p);
	packets_pack(p, arr);

	// Send
	packets_send(p, to_exec);
	packets_free(p);

	// Receive reply
	reply_receive(from_exec);

	// If we issued a Job, wait for the stdout to be returned
	if (command_recognize(arr) == cmd_issueJob)
		reply_receive(from_exec);

	array_free(arr);

	wopipe_free(to_exec);
	ropipe_free(from_exec);
	wopipe_free(handshake);
	return 0;
}