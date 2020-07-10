// SPDX-License-Identifier: GPL-3.0-only
/*
 * (C) Copyright 2020 Sam Protsenko <joe.skb7@gmail.com>
 */

#include <sort.h>
#include <tools.h>
#include <profile.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#define BUF_MIN		1UL	/* MiB */
#define BUF_MAX		1024UL	/* MiB */
#define BUF_DEF		128UL	/* MiB */
#define THR_MIN		1
#define THR_MAX		1024

struct params {
	const char *fpath;	/* file path */
	int buf_size;		/* buffer size, in MiB */
	int thr_count;		/* thread count */
};

static const char * const help_str =
	"Program sorts integers (int32_t) in specified file using limited RAM\n"
	"specified by BUFFER_SIZE by multiple THREADS threads.\n\n"
	"Optional arguments:\n"
	"  -b BUFFER_SIZE   in MiB; by default 128 MiB\n"
	"  -t THREADS       by default all threads\n";

static void print_usage(const char *app)
{
	printf("Usage: %s FILENAME [-b BUFFER_SIZE] [-t THREADS]\n\n%s", app,
		help_str);
}

static bool parse_args(int argc, char *argv[], struct params *p)
{
	int c, err;

	if (argc == 2 && !strcmp(argv[1], "--help")) {
		print_usage(argv[0]);
		exit(EXIT_SUCCESS);
	}

	if (argc < 2 || argc > 6) {
		fprintf(stderr, "Error: Invalid argument count\n");
		print_usage(argv[0]);
		return false;
	}

	/* Default values */
	memset(p, 0, sizeof(*p));
	p->buf_size = BUF_DEF;
	p->thr_count = get_cpus();

	/* Parse and sanity check optional parameters */
	while ((c = getopt(argc, argv, "b:t:")) != -1) {
		switch (c) {
		case 'b':
			err = str2int(&p->buf_size, optarg, 10);
			if (err) {
				fprintf(stderr, "Error: Wrong buffer size\n");
				print_usage(argv[0]);
				return false;
			}
			/* Must fit in size_t when in bytes */
			if (p->buf_size > BUF_MAX) {
				fprintf(stderr,
					"Error: Buffer size must be <= %lu MiB"
					"\n", BUF_MAX);
				print_usage(argv[0]);
				return false;
			}
			break;
		case 't':
			err = str2int(&p->thr_count, optarg, 10);
			if (err) {
				fprintf(stderr, "Error: Wrong thread count\n");
				print_usage(argv[0]);
				return false;
			}
			break;
		default: /* ? */
			fprintf(stderr, "Error: Invalid option\n");
			print_usage(argv[0]);
			return false;
		}
	}

	/* Parse file name */
	if (argc - optind != 1 || argv[optind] == NULL) {
		fprintf(stderr, "Error: File name not specified\n");
		print_usage(argv[0]);
		return false;
	}
	p->fpath = argv[optind];

	pr_debug("### %s() results:\n", __func__);
	pr_debug("  p->fpath     = %s\n", p->fpath);
	pr_debug("  p->buf_size  = %d MiB\n", p->buf_size);
	pr_debug("  p->thr_count = %d\n\n", p->thr_count);

	return true;
}

static bool validate_args(const struct params *p)
{
	if (!file_exist(p->fpath)) {
		fprintf(stderr, "Error: File does not exist\n");
		return false;
	}

	if (file_size(p->fpath) < 1) {
		/* File is empty, there is nothing to sort */
		exit(EXIT_SUCCESS);
	}

	if (p->buf_size < BUF_MIN || p->buf_size > BUF_MAX) {
		fprintf(stderr, "Error: Buffer size must be %lu..%lu MiB\n",
			BUF_MIN, BUF_MAX);
		return false;
	}

	if (p->thr_count < THR_MIN || p->thr_count > THR_MAX) {
		fprintf(stderr, "Error: Threads cunt must be %d..%d\n",
			THR_MIN, THR_MAX);
		return false;
	}

	return true;
}

int main(int argc, char *argv[])
{
	bool res;
	int ret = EXIT_SUCCESS;
	struct params p;
	struct sort *s;

	profile_start(PROFILE_TOTAL);

	res = parse_args(argc, argv, &p);
	if (!res)
		return EXIT_FAILURE;

	res = validate_args(&p);
	if (!res)
		return EXIT_FAILURE;

	s = sort_create(p.fpath, p.buf_size << 20, p.thr_count);
	if (!s)
		return EXIT_FAILURE;

	res = sort_sort(s);
	if (!res) {
		ret = EXIT_FAILURE;
		goto exit;
	}

	profile_stop(PROFILE_TOTAL);
	profile_print();

exit:
	sort_destroy(s);
	return ret;
}
