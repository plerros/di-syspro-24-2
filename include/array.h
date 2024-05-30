#ifndef SYSPROG24_1_ARRAY_H
#define SYSPROG24_1_ARRAY_H

#include "configuration_adv.h"
#include "llnode.h"

struct array
{
	void *data;
	size_t element_size;
	size_t size;
};

void array_free(struct array *ptr);
void array_new(struct array **ptr, struct llnode *ll);
void *array_get(struct array *ptr, size_t pos);
size_t array_get_size(struct array *ptr);
size_t array_get_elementsize(struct array *ptr);
void array_print_str(struct array *arr);
void array_copy(struct array *src, struct array **dst);
#endif /*SYSPROG24_1_ARRAY_H*/