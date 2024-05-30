#ifndef SYSPROG24_1_PACKET_H
#define SYSPROG24_1_PACKET_H

#include <stddef.h>

#include "array.h"
#include "fifopipe.h"

struct packets
{
	struct array *packets;
};

void packets_new(struct packets **ptr);
void packets_free(struct packets *ptr);

struct array *packets_get_packets(struct packets *ptr);
struct array **packets_getptr_packets(struct packets *ptr);

void packets_pack(struct packets *ptr, struct array *src);
void packets_unpack(struct packets *ptr, struct array **dst);

void packets_send(struct packets *ptr, struct wopipe *pipe);
void packets_receive(struct packets *ptr, struct ropipe *pipe);
#endif /*SYSPROG24_1_PACKET_H*/