// SPDX-License-Identifier: GPL-3.0
/*
 * (C) Copyright 2019 Malith Jayaweera
 * (C) Copyright 2020 Sam Protsenko <joe.skb7@gmail.com>
 */

/**
 * @file
 *
 * Parallel Merge Sort.
 *
 * Taken from [1] and reworked further:
 *   - to conform with C89 standard
 *   - to be uniform with the rest of the project
 *   - fixed corner cases (array length = 1, num_threads > len)
 *   - added "fast path" for 1-thread mode
 *
 * [1] https://malithjayaweera.com/2019/02/parallel-merge-sort/
 */

#include <algo/pmsort.h>
#include <tools.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

struct pmsort {
	int32_t *arr;		/* array to sort (shared pointer) */
	size_t len;		/* array length */
	size_t num_threads;	/* thread count to use for sorting */
	size_t npt;		/* numbers per thread */
	size_t offset;		/* additional elements to sort with last thr */
};

static struct pmsort obj; /* singleton */

/* Merge function */
static void pmsort_merge(int32_t *arr, size_t left, size_t middle, size_t right)
{
	size_t i = 0, j = 0, k = 0;
	size_t left_length = middle - left + 1;
	size_t right_length = right - middle;
	int32_t *left_array, *right_array;

	left_array = xmalloc(left_length * sizeof(int32_t));
	right_array = xmalloc(right_length * sizeof(int32_t));

	/* Copy values to left array */
	for (i = 0; i < left_length; ++i)
		left_array[i] = arr[left + i];

	/* Copy values to right array */
	for (j = 0; j < right_length; ++j)
		right_array[j] = arr[middle + 1 + j];

	/* Chose from right and left arrays and copy */
	i = 0;
	j = 0;
	while (i < left_length && j < right_length) {
		if (left_array[i] <= right_array[j]) {
			arr[left + k] = left_array[i];
			i++;
		} else {
			arr[left + k] = right_array[j];
			j++;
		}
		k++;
	}

	/* Copy the remaining values to the array */
	while (i < left_length) {
		arr[left + k] = left_array[i];
		k++;
		i++;
	}
	while (j < right_length) {
		arr[left + k] = right_array[j];
		k++;
		j++;
	}

	free(left_array);
	free(right_array);
}

/* Perform merge sort */
static void pmsort_merge_sort(int32_t *arr, size_t left, size_t right)
{
	if (left < right) {
		size_t middle = left + (right - left) / 2;

		pmsort_merge_sort(arr, left, middle);
		pmsort_merge_sort(arr, middle + 1, right);
		pmsort_merge(arr, left, middle, right);
	}
}

/* Merge locally sorted sections */
static void pmsort_merge_array_sections(int32_t *arr, size_t number,
					size_t aggregation)
{
	size_t i;

	for (i = 0; i < number; i += 2) {
		size_t left = i * (obj.npt * aggregation);
		size_t right = ((i + 2) * obj.npt * aggregation) - 1;
		size_t middle = left + (obj.npt * aggregation) - 1;

		if (right >= obj.len)
			right = obj.len - 1;
		pmsort_merge(arr, left, middle, right);
	}

	if (number / 2 >= 1)
		pmsort_merge_array_sections(arr, number / 2, aggregation * 2);
}

/* Assign work to each thread to perform merge sort */
static void *pmsort_thread_merge_sort(void *arg)
{
	size_t thread_id = (size_t)arg;
	size_t left = thread_id * (obj.npt);
	size_t right = (thread_id + 1) * (obj.npt) - 1;
	size_t middle;

	if (thread_id == obj.num_threads - 1)
		right += obj.offset;

	middle = left + (right - left) / 2;
	if (left < right) {
		pmsort_merge_sort(obj.arr, left, right);
		pmsort_merge_sort(obj.arr, left + 1, right);
		pmsort_merge(obj.arr, left, middle, right);
	}

	return NULL;
}

/**
 * Sort specified array using multi-threaded merge sort.
 *
 * Array will be sorted in ascending order. In case of critical errors (e.g.
 * inability to allocated memory or create thread) the program will be
 * terminated. This routine is synchronous (waiting for all threads to join).
 *
 * @param arr Array to sort
 * @param len Elements count in array
 * @param num_threads Number of threads to use for sorting
 */
void pmsort_sort(int32_t *arr, size_t len, size_t num_threads)
{
	assert(arr != NULL);
	assert(len > 0);
	assert(num_threads > 0);

	if (len == 1)
		return;

	if (num_threads > len)
		num_threads = len;

	obj.arr		= arr;
	obj.len		= len;
	obj.num_threads	= num_threads;
	obj.npt		= obj.len / obj.num_threads;
	obj.offset	= len % num_threads;

	if (num_threads == 1) {
		pmsort_thread_merge_sort((void *)0);
	} else {
		pthread_t *threads = xmalloc(num_threads * sizeof(*threads));
		size_t i;

		/* Create threads */
		for (i = 0; i < num_threads; ++i) {
			int err = pthread_create(&threads[i], NULL,
					pmsort_thread_merge_sort, (void *)i);
			if (err) {
				fprintf(stderr, "Error: Can't create thread: "
					"%d\n", err);
				exit(EXIT_FAILURE);
			}
		}

		for (i = 0; i < num_threads; ++i)
			pthread_join(threads[i], NULL);

		free(threads);
	}

	pmsort_merge_array_sections(arr, num_threads, 1);
}
