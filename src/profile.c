// SPDX-License-Identifier: GPL-3.0-only
/*
 * (C) Copyright 2020 Sam Protsenko <joe.skb7@gmail.com>
 */

#include <profile.h>
#include <config.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/resource.h>

#ifdef CONFIG_PROFILE

struct profile {
	struct rusage before[PROFILE_MAX];	/* start timestamps */
	struct rusage after[PROFILE_MAX];	/* stop timestamps */
	double time[PROFILE_MAX];		/* benchmarks */
};

static const char * const bench_str[PROFILE_MAX] = {
	"reading",
	"sorting",
	"merging",
	"writing",
	"TOTAL"
};

static struct profile obj;

static void profile_calc_time(enum profile_bench bench)
{
	struct rusage *a = &obj.after[bench];
	struct rusage *b = &obj.before[bench];
	double time;

	time = ((((a->ru_utime.tv_sec * 1000000 + a->ru_utime.tv_usec) -
		  (b->ru_utime.tv_sec * 1000000 + b->ru_utime.tv_usec)) +
		 ((a->ru_stime.tv_sec * 1000000 + a->ru_stime.tv_usec) -
		  (b->ru_stime.tv_sec * 1000000 + b->ru_stime.tv_usec)))
		/ 1000000.0);

	obj.time[bench] += time;
}

void profile_start(enum profile_bench bench)
{
	assert(bench < PROFILE_MAX);
	getrusage(RUSAGE_SELF, &obj.before[bench]);
}

void profile_stop(enum profile_bench bench)
{
	assert(bench < PROFILE_MAX);
	getrusage(RUSAGE_SELF, &obj.after[bench]);
	profile_calc_time(bench);
}

void profile_print(void)
{
	size_t i;

	printf("### Profiling results:\n");
	for (i = 0; i < PROFILE_MAX; ++i)
		printf("TIME IN %*s: %.2f s\n", 10, bench_str[i], obj.time[i]);
}

#endif /* CONFIG_PROFILE */
