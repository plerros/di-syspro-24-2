#ifndef SYSPROG24_1_LLNODE_H
#define SYSPROG24_1_LLNODE_H

struct llnode
{
	void *data;
	size_t element_size;
	size_t logical_size; // The first unoccupied element.
	struct llnode *next;
};
void llnode_new(struct llnode **ptr, size_t element_size, struct llnode *next);
void llnode_free(struct llnode *list);
void llnode_add(struct llnode **ptr, void *value);
size_t llnode_getsize(struct llnode *ptr);
void *llnode_get(struct llnode *ptr, size_t index);
void llnode_link(struct llnode **ptr, struct llnode **next);

#endif /* SYSPROG24_1_LLNODE_H */