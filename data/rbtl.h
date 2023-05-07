/** Binary tree.  Red-black tree.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include "list.h"
#include <ffbase/rbtree.h>

/** Node which holds pointers to its sibling nodes having the same key. */
typedef struct ffrbtl_node {
	ffrbt_node *left
		, *right
		, *parent;
	uint color;
	ffrbtkey key;

	fflist_item sib;
} ffrbtl_node;

/** Get RBT node by the pointer to its list item. */
#define ffrbtl_nodebylist(item)  FF_GETPTR(ffrbtl_node, sib, item)

/** Walk through sibling nodes.
The last node in row points to the first one - break the loop after this. */
#define FFRBTL_FOR_SIB(node, iter) \
for (iter = node \
	; iter != NULL \
	; ({ \
		iter = ffrbtl_nodebylist(iter->sib.next); \
		if (iter == node) \
			iter = NULL; \
		}) \
	)

/** Insert a new node or list-item. */
void ffrbtl_insert(ffrbtree *tr, ffrbtl_node *k)
{
	ffrbt_node *n = tr->root;
	ffrbtl_node *found = (ffrbtl_node*)fftree_findnode(k->key, (fftree_node**)&n, &tr->sentl);

	if (found == NULL)
		ffrbtl_insert3(tr, k, n);
	else {
		ffchain_append(&k->sib, &found->sib);
		tr->len++;
	}
}

static inline void ffrbtl_insert_withhash(ffrbtree *tr, ffrbtl_node *n, ffrbtkey hash)
{
	n->key = hash;
	ffrbtl_insert(tr, n);
}

#define ffrbtl_insert3(tr, nod, parent) \
do { \
	ffrbt_insert(tr, (ffrbt_node*)(nod), parent); \
	(nod)->sib.next = (nod)->sib.prev = &(nod)->sib; \
} while (0)
