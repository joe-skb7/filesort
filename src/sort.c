// SPDX-License-Identifier: GPL-3.0-only
/*
 * (C) Copyright 2020 Sam Protsenko <joe.skb7@gmail.com>
 */

/**
 * @file
 *
 * Sort module.
 *
 * Sort the file containing integers (int32_t) in ascending order. File can be
 * of any size (even bigger than RAM). To overcome RAM limitation, "External
 * sorting" algorithm is used. This module allows to use multi-threaded sorting
 * and to specify buffer size (i.e. how much RAM to use for sorting).
 *
 * Quick sort is used to sort one chunk of file. To merge all the sorted chunks
 * in the final file, K-way merge algorithm is used.
 */

#define _POSIX_C_SOURCE	200809L
#define _XOPEN_SOURCE	500L

#include <sort.h>
#include <algo/kmerge.h>
#include <algo/pmsort.h>
#include <config.h>
#include <tools.h>
#include <profile.h>
#include <assert.h>
#include <errno.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Try to use /tmp by default */
#define TMP_TEMPLATE1	"/tmp/tmpdir.XXXXXX"
/* Fallback option: tmpdir in current dir */
#define TMP_TEMPLATE2	"tmpdir.XXXXXX"

struct sort {
	const char *fpath;	/* input file path (shared pointer) */
	int32_t *buf;		/* current buffer */
	size_t buf_nmemb;	/* max number of members in 'buf' array */
	size_t thr_count;	/* thread count */
	size_t fcount;		/* number of buffers (or tmp files) */
	char tmpdir[19];
};

#ifdef CONFIG_USE_QSORT
/* Comparator for qsort() */
static int sort_cmp(const void *p1, const void *p2)
{
	int32_t l = *(int32_t *)p1;
	int32_t r = *(int32_t *)p2;

	return (l < r) ? -1 : (l > r);
}
#endif

static int sort_remove_callback(const char *fpath, const struct stat *sb,
				int typeflag, struct FTW *ftwbuf)
{
	int err;

	UNUSED(sb);
	UNUSED(typeflag);
	UNUSED(ftwbuf);

	err = remove(fpath);
	if (err)
		perror(fpath);

	return err;
}

static bool sort_create_tmp_dir(struct sort *obj)
{
	char *dir;

	strcpy(obj->tmpdir, TMP_TEMPLATE1);
	dir = mkdtemp(obj->tmpdir);
	if (dir == NULL) {
		errno = 0;
		strcpy(obj->tmpdir, TMP_TEMPLATE2);
		dir = mkdtemp(obj->tmpdir);
		if (dir == NULL) {
			perror("Error: Can't create tmp directory");
			return false;
		}
	}

	pr_debug("### %s(): tmpdir = %s\n", __func__, obj->tmpdir);
	return true;
}

static void sort_remove_tmp_dir(struct sort *obj)
{
	if (obj->tmpdir[0] == '\0')
		return;

	if (nftw(obj->tmpdir, sort_remove_callback, FOPEN_MAX,
			FTW_DEPTH | FTW_MOUNT | FTW_PHYS) == -1) {
		perror("Warning: Can't remove tmpdir");
		errno = 0;
	}

	obj->tmpdir[0] = '\0';
}

/**
 * Sort current buffer and write it into temporary file.
 *
 * Later these temporary files will be merge-sorted and stored to the final
 * file. File name for the temprorary file is composed using @p bufn, using
 * next template:
 *
 *     $tmpdir/0_N
 *
 * where N is current buffer index @p bufn, and '0' means "0th merge stage".
 *
 * @param obj Sort object
 * @param bufn Index of current buffer
 * @param count Actual elements count in the buffer to handle
 * @return true on success or false on failure
 */
static bool sort_handle_buf(struct sort *obj, size_t bufn, size_t count)
{
	char fname[FNAME_SIZE];
	FILE *stream;
	size_t res;
	bool ret = true;

	profile_start(PROFILE_SORT);
#ifdef CONFIG_USE_QSORT
	qsort(obj->buf, count, sizeof(int32_t), sort_cmp);
#else
	pmsort_sort(obj->buf, count, obj->thr_count);
#endif
	profile_stop(PROFILE_SORT);

	format_tmp_fname(fname, obj->tmpdir, 0, bufn);
	pr_debug("### %s(): %s\n", __func__, fname);

	stream = xfopen(fname, "w");
	res = fwrite(obj->buf, sizeof(int32_t), count, stream);
	if (res != count) {
		fprintf(stderr, "Error: Failed to write %s file\n", fname);
		ret = false;
	}
	fclose(stream);

	return ret;
}

/**
 * Read input file by chunks, sort these chunks and store them into tmp files.
 *
 * @param obj "Sort" object
 * @return true on success or false on failure
 */
static bool sort_read_chunks(struct sort *obj)
{
	FILE *stream;
	char *line = NULL;
	size_t len = 0;
	ssize_t nread;
	bool ret = true;
	size_t buf_idx = 0;	/* index in buffer */
	size_t bufn = 0;	/* buffer number */

	profile_start(PROFILE_READ);
	stream = xfopen(obj->fpath, "r");
	while ((nread = getline(&line, &len, stream)) != -1) {
		char *pos;
		int val, err;

		/* Remove trailing newline */
		if ((pos = strchr(line, '\n')) != NULL)
			*pos = '\0';

		err = str2int(&val, line, 10);
		if (err) {
			fprintf(stderr, "Error: Invalid line %s\n", line);
			ret = false;
			goto err;
		}

		obj->buf[buf_idx++] = val;
		if (buf_idx == obj->buf_nmemb) {
			profile_stop(PROFILE_READ);
			ret = sort_handle_buf(obj, bufn, buf_idx);
			if (!ret)
				goto err;
			profile_start(PROFILE_READ);
			buf_idx = 0;
			++bufn;
		}
	}
	profile_stop(PROFILE_READ);

	/* Remainder */
	if (buf_idx != 0) {
		ret = sort_handle_buf(obj, bufn, buf_idx);
		if (!ret)
			goto err;
		++bufn;
	}

	obj->fcount = bufn;

err:
	free(line);
	fclose(stream);
	return ret;
}

/**
 * Serialize merged file (binary) to the input file (text).
 *
 * @param obj Sort object
 * @param fname_merged Merged file path
 */
static void sort_write_output(struct sort *obj, const char *fname_merged)
{
	FILE *fmerged, *fout;
	size_t n, i;

	fmerged = xfopen(fname_merged, "r");
	fout = xfopen(obj->fpath, "w");
	do {
		n = fread(obj->buf, sizeof(int32_t), obj->buf_nmemb, fmerged);
		for (i = 0; i < n; ++i)
			fprintf(fout, "%d\n", obj->buf[i]);
	} while (n != 0);

	fclose(fout);
	fclose(fmerged);
}

/**
 * Constructor for "sort" object.
 *
 * @param fpath Path to file to be sorted; will be stored as a reference
 * @param buf_size Size of one chunk, in bytes
 * @param thr_count Number of threads to use for sorting
 * @return Pointer to constructed object or NULL on error
 */
struct sort *sort_create(const char *fpath, size_t buf_size, size_t thr_count)
{
	struct sort *obj;

	assert(fpath != NULL);
	assert(buf_size > 0);
	assert((buf_size % sizeof(int32_t)) == 0);
	assert(thr_count > 0);

	obj = malloc(sizeof(*obj));
	if (!obj)
		goto err1;

	memset(obj, 0, sizeof(*obj));
	obj->fpath = fpath;
	obj->buf_nmemb = buf_size / sizeof(int32_t);
	obj->thr_count = thr_count;
	obj->buf = malloc(buf_size);
	if (!obj->buf)
		goto err2;

	return obj;

err2:
	free(obj);
err1:
	fprintf(stderr, "Error: Unable to allocate memory in %s()\n", __func__);
	return NULL;
}

/**
 * Destructor for "sort" object.
 *
 * @param obj "Sort" object to destroy
 */
void sort_destroy(struct sort *obj)
{
	assert(obj != NULL);

	sort_remove_tmp_dir(obj);
	free(obj->buf);
	free(obj);
}

/**
 * Sort the file specified in constructor.
 */
bool sort_sort(struct sort *obj)
{
	char fname_merged[FNAME_SIZE];
	bool res, ret = true;

	res = sort_create_tmp_dir(obj);
	if (!res)
		return false;

	res = sort_read_chunks(obj);
	if (!res) {
		ret = false;
		goto exit;
	}

	profile_start(PROFILE_MERGE);
	res = kmerge_merge(obj->tmpdir, obj->fcount, obj->buf, obj->buf_nmemb,
			   fname_merged);
	profile_stop(PROFILE_MERGE);
	if (!res) {
		ret = false;
		goto exit;
	}

	profile_start(PROFILE_WRITE);
	sort_write_output(obj, fname_merged);
	profile_stop(PROFILE_WRITE);

exit:
	sort_remove_tmp_dir(obj);
	return ret;
}
