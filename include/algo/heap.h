/* SPDX-License-Identifier: GPL-3.0 */

#ifndef ALGO_HEAP_H
#define ALGO_HEAP_H

#include <stddef.h>
#include <stdbool.h>

struct heap;

struct heap_el {
	int key;
	int idx;	/* array index from which this key came */
};

struct heap *heap_create(size_t capacity);
void heap_destroy(struct heap *obj);
bool heap_empty(struct heap *obj);
void heap_insert(struct heap *obj, const struct heap_el *el);
void heap_pop(struct heap *obj, struct heap_el *el);

#endif /* ALGO_HEAP_H */
