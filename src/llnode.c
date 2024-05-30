#include "configuration.h"
#include "configuration_adv.h"
#include "helper.h"

#include <stdlib.h>
#include <string.h>
#include "llnode.h"


void llnode_new(struct llnode **ptr, size_t element_size, struct llnode *next)
{
	OPTIONAL_ASSERT(ptr != NULL);
	OPTIONAL_ASSERT(*ptr == NULL);

	if (ptr == NULL)
		return;

	struct llnode *new = malloc(sizeof(struct llnode));
	if (new == NULL)
		abort();

	new->data = malloc(element_size * LINK_SIZE);
	if (new->data == NULL && (element_size * LINK_SIZE > 0))
		abort();

	new->element_size = element_size;
	new->logical_size = 0;
	new->next = next;
	*ptr = new;
}

void llnode_free(struct llnode *node)
{
	if (node == NULL)
		return;

	struct llnode *tmp = node;
	for (struct llnode *i = tmp; tmp != NULL; i = tmp) {
		tmp = tmp->next;
		if (i->data != NULL)
			free(i->data);
		free(i);
	}
}

void llnode_add(struct llnode **ptr, void *value)
{
	OPTIONAL_ASSERT(ptr != NULL);
	OPTIONAL_ASSERT(*ptr != NULL);

	if (ptr == NULL)
		return;
	if (value == NULL)
		return;

	if ((*ptr)->logical_size >= LINK_SIZE) {
		struct llnode *new = NULL;
		llnode_new(&new, (*ptr)->element_size, *ptr);
		*ptr = new;
	}
	size_t offset = (*ptr)->logical_size * (*ptr)->element_size;
	void *destination = (*ptr)->data + offset;

	memcpy(destination, value, (*ptr)->element_size);
	(*ptr)->logical_size += 1;
}

size_t llnode_getsize(struct llnode *ptr)
{
	if (ptr == NULL)
		return 0;

	size_t size = 0;
	for (struct llnode *i = ptr; i != NULL; i = i->next)
		size += i->logical_size;

	return size;
}

void *llnode_get(struct llnode *ptr, size_t index)
{
	OPTIONAL_ASSERT(ptr != NULL);
	if (ptr == NULL)
		return NULL;

	if (index < ptr->logical_size) {
		size_t offset = index * ptr->element_size;
		return (ptr->data + offset);
	}
	if (ptr->next == NULL)
		return NULL;
	return (llnode_get(ptr->next, index - ptr->logical_size));
}

void llnode_link(struct llnode **ptr, struct llnode **next)
{
	if (ptr == NULL)
		*ptr = *next;

	struct llnode *curr = *ptr;
	while (curr->next != NULL)
		curr = curr->next;

	curr->next = *next;
	*next = NULL;
}
