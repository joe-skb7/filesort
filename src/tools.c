// SPDX-License-Identifier: GPL-3.0-only
/*
 * (C) Copyright 2020 Sam Protsenko <joe.skb7@gmail.com>
 */

#include <tools.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* Input file name max size */
#define FNAME_SIZE	80UL

static void die(const char * format, ...)
{
	va_list vargs;

	va_start(vargs, format);
	vfprintf(stderr, format, vargs);
	fprintf(stderr, "\n");
	va_end(vargs);

	fflush(stdout);
	fflush(stderr);
	exit(EXIT_FAILURE);
}

/**
 * Get the number of processors currently online (available).
 *
 * Cross-platform version (wrapper) of get_nprocs_conf(). In case if it's not
 * available, 1 will be returned.
 *
 * @return CPU count
 */
size_t get_cpus(void)
{
#ifdef _SC_NPROCESSORS_ONLN
	return sysconf(_SC_NPROCESSORS_ONLN);
#else
	return 1;
#endif
}

/**
 * Convert string to int.
 *
 * @param[out] out The converted int; cannot be NULL
 * @param[in] s Input string to be converted; cannot be NULL
 *              The format is the same as in strtol(), except that the
 *              following are inconvertible:
 *                - empty string
 *                - leading whitespace
 *                - any trailing characters that are not part of the number
 * @param[in] base Base to interpret string in. Same range as strtol (2 to 36)
 * @return 0 on success or negative value on error
 */
int str2int(int *out, char *s, int base)
{
	char *end;
	long l;

	assert(out);
	assert(s);
	assert(base >= 2 && base <= 36);

	if (s[0] == '\0' || isspace(s[0]))
		return -EINVAL;

	errno = 0;
	l = strtol(s, &end, base);

	/* Both checks are needed because INT_MAX == LONG_MAX is possible */
	if (l > INT_MAX || (errno == ERANGE && l == LONG_MAX))
		return -ERANGE;
	if (l < INT_MIN || (errno == ERANGE && l == LONG_MIN))
		return -ERANGE;
	if (*end != '\0')
		return -EINVAL;

	*out = l;
	return 0;
}

/**
 * Check if file exists.
 *
 * @param path File path to check
 * @return true if file exists
 */
bool file_exist(const char *path)
{
	return access(path, F_OK) != -1;
}

/**
 * Get file size (in bytes).
 *
 * @param path Path to file
 * @return File size in bytes or -1 on error
 */
long file_size(const char *path)
{
	struct stat st;

	if (stat(path, &st) == 0)
		return st.st_size;

	return -1;
}

/**
 * Calculate logarithm with custom base.
 *
 * @param x Value to calculate logarithm for
 * @param base Logarithm base
 * @return Calculated logarithm value
 */
double logn(double x, double base)
{
	return log(x) / log(base);
}

/**
 * Format tmp file name.
 *
 * Create string of next format:
 *
 *     $dir/$stage_$num
 *
 * @param fname String to store formatted file name; must be allocated for 80
 *              bytes or more
 * @param dir Directory where file is located
 * @param stage Current merge stage number
 * @param num File number
 */
void format_tmp_fname(char *fname, const char *dir, size_t stage, size_t num)
{
	int err;

	err = snprintf(fname, FNAME_SIZE, "%s/%zu_%zu", dir, stage, num);
	if (err >= FNAME_SIZE || err < 0)
		die("Error: Unable to format tmp file string");
}

FILE *xfopen(const char *pathname, const char *mode)
{
	FILE *f;

	f = fopen(pathname, mode);
	if (!f)
		die("Error: Unable to open file %s for \"%s\"", pathname, mode);

	return f;
}

void *xmalloc(size_t size)
{
	void *mem;

	mem = malloc(size);
	if (!mem)
		die("Error: Unable to allocate memory");

	return mem;
}
