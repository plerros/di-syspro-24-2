#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#include "llnode.h"
#include "handshake.h"

void handshake_init(struct handshake_t *ptr, char *ip, uint16_t port)
{
	if (ptr == NULL)
		return;

	struct handshake_t tmp;
	for (size_t i = 0; i < sizeof(tmp.ip); i++)
		ptr->ip[i] = '\0';

	for (size_t i = 0; i < sizeof(tmp.port); i++)
		ptr->port[i] = '\0';

	sprintf(ptr->ip, "%s", ip);
	sprintf(ptr->port, "%u", port);
}

void handshake_to_llnode(struct handshake_t *ptr, struct llnode **dst)
{
	if (dst == NULL)
		return;

	struct handshake_t tmp;
	if (*dst == NULL)
		llnode_new(dst, sizeof(char), NULL);

	for (size_t i = 0; i < sizeof(tmp.ip); i++)
		llnode_add(dst, &(ptr->ip[i]));

	for (size_t i = 0; i < sizeof(tmp.port); i++)
		llnode_add(dst, &(ptr->port[i]));
}

void handshake_print(__attribute__((unused)) struct handshake_t *ptr)
{
#ifdef DEBUG
	printf("%s:%s\n", ptr->ip, ptr->port);
#endif
}