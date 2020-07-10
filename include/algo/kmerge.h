/* SPDX-License-Identifier: GPL-3.0 */

#ifndef ALGO_KMERGE_H
#define ALGO_KMERGE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

bool kmerge_merge(const char *tmpdir, size_t fcount, int32_t *buf,
		  size_t buf_nmemb, char *out);

#endif /* ALGO_KMERGE_H */
