/* SPDX-License-Identifier: GPL-3.0 */

#ifndef TOOLS_H
#define TOOLS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#define UNUSED(v)	((void)v)
#define BIT(n)		(1 << (n))
#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(a[0]))

/* File name max size; see format_tmp_fname() */
#define FNAME_SIZE	80UL

/*
 * Dummy printf for disabled debugging statements to use whilst maintaining
 * gcc's format and side-effect checking.
 */
#define no_printf(...)							\
({									\
	if (0)								\
		printf(__VA_ARGS__);					\
	0;								\
})

/* pr_debug() should produce zero code unless DEBUG is defined */
#ifdef DEBUG
#define pr_debug(...) printf(__VA_ARGS__)
#else
#define pr_debug(...) no_printf(__VA_ARGS__)
#endif

size_t get_cpus(void);
int str2int(int *out, char *s, int base);
bool file_exist(const char *path);
long file_size(const char *path);
double logn(double x, double base);
void format_tmp_fname(char *fname, const char *dir, size_t stage, size_t num);
FILE *xfopen(const char *pathname, const char *mode);
void *xmalloc(size_t size);

#endif /* TOOLS_H */
