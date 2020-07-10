/* SPDX-License-Identifier: GPL-3.0 */

#ifndef ALGO_PMSORT_H
#define ALGO_PMSORT_H

#include <stddef.h>
#include <stdint.h>

void pmsort_sort(int32_t *arr, size_t len, size_t num_threads);

#endif /* ALGO_PMSORT_H */
