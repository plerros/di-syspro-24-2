#ifndef SYSPROG24_2_HANDSHAKE_H
#define SYSPROG24_2_HANDSHAKE_H

#include "llnode.h"

struct handshake_t
{
	char ip[16];
	char port[8];
};

void handshake_init(struct handshake_t *ptr, char *ip, uint16_t port);
void handshake_to_llnode(struct handshake_t *ptr, struct llnode **dst);

void handshake_print(__attribute__((unused)) struct handshake_t *ptr);
#endif /* SYSPROG24_2_HANDSHAKE_H */