/* SPDX-License-Identifier: GPL-3.0 */

#ifndef SORT_H
#define SORT_H

#include <stddef.h>
#include <stdbool.h>

struct sort;

struct sort *sort_create(const char *fpath, size_t buf_size, size_t thr_count);
void sort_destroy(struct sort *obj);
bool sort_sort(struct sort *obj);

#endif /* SORT_H */
