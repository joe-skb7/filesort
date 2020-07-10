/* SPDX-License-Identifier: GPL-3.0 */

#ifndef PROFILE_H
#define PROFILE_H

#include <config.h>
#include <tools.h>
#include <stddef.h>
#include <stdio.h>

/* Benchmark types */
enum profile_bench {
	PROFILE_READ,	/* reading+parsing input file */
	PROFILE_SORT,	/* small files sorting */
	PROFILE_MERGE,	/* K-way merge of files */
	PROFILE_WRITE,	/* writing the output file */
	PROFILE_TOTAL,	/* the whole app execution time */
	/* --- */
	PROFILE_MAX
};

#ifdef CONFIG_PROFILE

void profile_start(enum profile_bench bench);
void profile_stop(enum profile_bench bench);
void profile_print(void);

#else

static inline void profile_start(enum profile_bench bench) { UNUSED(bench); }
static inline void profile_stop(enum profile_bench bench) { UNUSED(bench); }
static inline void profile_print(void) { }

#endif /* CONFIG_PROFILE */

#endif /* PROFILE_H */
