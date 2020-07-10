// SPDX-License-Identifier: GPL-3.0-only
/*
 * (C) Copyright 2020 Sam Protsenko <joe.skb7@gmail.com>
 */

/**
 * @file
 *
 * Simple Min-Heap implementation, using 'int' as a value type.
 *
 * Heap = binary search tree. This one is Min-Heap, means root node contains
 * minimal value.
 *
 * Can be used to implement priority queue (where value = priority).
 */

#include <algo/heap.h>
#include <assert.h>
#include <stdlib.h>

struct heap {
	size_t capacity;	/* max possible nodes number in the tree */
	size_t count;		/* current nodes number in the tree */
	struct heap_el *arr;	/* array representation of binary heap */
};

/* Calculate parent node index (in array) for child element with index i */
static inline int heap_parent(int i)
{
	return (i - 1) / 2;
}

/* Calculate left child node index (in array) of parent element with index i */
static inline int heap_left(int i)
{
	return (i * 2) + 1;
}

/* Calculate right child node index (in array) of parent element with index i */
static inline int heap_right(int i)
{
	return (i * 2) + 2;
}

static inline void heap_swap(struct heap_el *x, struct heap_el *y)
{
	struct heap_el tmp;

	tmp = *x;
	*x = *y;
	*y = tmp;
}

/**
 * Heapify a subtree with root at specified node.
 *
 * Adapted from "Introduction to Algorithms, 3rd edition" (Cormen), p.154.
 *
 * Complexity: O(log(N)), if tree is balanced.
 *
 * @param obj Heap object
 * @param i Index of subtree root node in array
 */
static void heap_heapify_min(struct heap *obj, int i)
{
	int l = heap_left(i);
	int r = heap_right(i);
	int min = i;

	/* Checking for the smallest element */
	if (l < obj->count && obj->arr[l].key < obj->arr[min].key)
		min = l;
	if (r < obj->count && obj->arr[r].key < obj->arr[min].key)
		min = r;

	/* Update the min-heap tree */
	if (min != i) {
		heap_swap(&obj->arr[i], &obj->arr[min]);
		heap_heapify_min(obj, min);
	}
}

/**
 * Construct heap object.
 *
 * @param capacity Max nodes in the tree
 * @return Constructed object or NULL on failure
 */
struct heap *heap_create(size_t capacity)
{
	struct heap *obj;

	assert(capacity > 0);

	obj = malloc(sizeof(*obj));
	if (!obj)
		return NULL;

	obj->capacity = capacity;
	obj->count = 0;
	obj->arr = malloc(sizeof(struct heap_el) * capacity);
	if (!obj->arr)
		goto err;

	return obj;

err:
	free(obj);
	return NULL;
}

/**
 * Destructor for heap object.
 *
 * @param obj Heap object
 */
void heap_destroy(struct heap *obj)
{
	assert(obj != NULL);
	free(obj->arr);
	free(obj);
}

/**
 * Check if heap is empty.
 *
 * @param obj Heap object
 * @return true if heap is empty or false otherwise
 */
bool heap_empty(struct heap *obj)
{
	return obj->count == 0;
}

/**
 * Insert new item into the heap and rebuild it if needed.
 *
 * Complexity: O(log(N)), if tree is balanced.
 *
 * @param obj Heap object
 * @param[in] el Item to insert
 * @return true on success or false on overflow
 */
void heap_insert(struct heap *obj, const struct heap_el *el)
{
	size_t i;

	/* Check for overflow */
	assert(obj->count < obj->capacity);

	/* Insert new key */
	obj->count++;
	i = obj->count - 1;
	obj->arr[i] = *el;

	/* Fix Min-Heap property if it's violated */
	while (i != 0 && obj->arr[heap_parent(i)].key > obj->arr[i].key) {
		heap_swap(&obj->arr[i], &obj->arr[heap_parent(i)]);
		i = heap_parent(i);
	}
}

/**
 * Get the minimal element, remove it and rebuild the tree.
 *
 * Complexity: O(log(N)), if tree is balanced.
 *
 * @note Please make sure the heap is empty before running this function.
 *
 * @param obj Heap object
 * @param[out] el Minimal element
 * @return true on success or false on underflow
 */
void heap_pop(struct heap *obj, struct heap_el *el)
{
	/* Check for underflow */
	assert(obj->count > 0);

	*el = obj->arr[0]; /* min item is root node in Min-Heap */

	/* Remove root node and rebuild the whole tree */
	obj->arr[0] = obj->arr[obj->count - 1];
	obj->count--;
	heap_heapify_min(obj, 0);
}
