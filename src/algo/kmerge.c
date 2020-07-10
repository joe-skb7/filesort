// SPDX-License-Identifier: GPL-3.0-only
/*
 * (C) Copyright 2020 Sam Protsenko <joe.skb7@gmail.com>
 */

/**
 * @file
 *
 * External K-way merge implementation (for files).
 *
 * Single-threaded, as it's I/O bound, CPU is not a bottleneck here.
 */

#include <algo/kmerge.h>
#include <algo/heap.h>
#include <tools.h>
#include <assert.h>
#include <math.h>
#include <string.h>

/* "K" in "K-way merge" */
#define NMERGE		16UL

struct merge {
	const char *tmpdir;	/* tmp directory path (where input files are) */
	size_t fcount;		/* input files count (on 0th merge stage) */
	int32_t *buf;		/* RAM buffer for K-way merge; shared pointer */
	size_t buf_nmemb;	/* number of members in 'buf' array */
	char *out;		/* merged file nam (shared pointer) */
	struct heap *queue;	/* priority queue for K-way merge */
};

struct merge_block {
	int32_t *buf;		/* start address of block */
	size_t size;		/* max elements (int32_t) count in buffer */
	size_t count;		/* actual elements (int32_t) count in buffer */
	size_t pos;		/* current position in this block */
};

static struct merge obj;	/* singleton */

static size_t kmerge_calc_stages(void)
{
	return (size_t)ceil(logn(obj.fcount, NMERGE));
}

static size_t kmerge_calc_stage_files(size_t stage)
{
	if (stage == 0)
		return obj.fcount;

	return (size_t)ceil((double)obj.fcount / pow(NMERGE, stage));
}

/**
 * Merge input blocks to output block.
 *
 * Priority queue already have first elements from each block pushed into it.
 *
 * @param blocks Input blocks and output block (has [NMERGE] index)
 * @param fs Input files array
 * @param fout Output file
 * @return true on success or false on failure
 */
static bool kmerge_merge_blocks(struct merge_block *blocks, FILE **fs,
				FILE *fout)
{
	struct merge_block *out = &blocks[NMERGE];
	struct merge_block *b; /* alias */
	struct heap_el el;

	while (!heap_empty(obj.queue)) {
		/* Populate output buffer with minimal elements from queue */
		heap_pop(obj.queue, &el);
		out->buf[out->pos++] = el.key;

		/* Output buffer is full; store into the file */
		if (out->pos == out->size) {
			size_t nwrite;

			nwrite = fwrite(out->buf, sizeof(int32_t), out->pos,
					fout);
			if (nwrite != out->pos) {
				fprintf(stderr, "Error: Can't write output\n");
				return false;
			}
			out->pos = 0;
		}

		/* Add next element to queue from the same buffer we popped */
		b = &blocks[el.idx];
		if (b->count == 0) {
			/* File read complete */
			continue;
		} else if (b->pos < b->count) {
			el.key = b->buf[b->pos++];
			heap_insert(obj.queue, &el);
		} else {
			/* This block is exhausted; read next one */
			b->count = fread(b->buf, sizeof(int32_t), b->size,
					 fs[el.idx]);
			b->pos = 0;
			if (b->count > 0) {
				/* And push first element to the queue */
				el.key = b->buf[b->pos++];
				heap_insert(obj.queue, &el);
			}
		}
	}

	/* Remainder */
	if (out->pos != 0) {
		size_t nwrite;

		nwrite = fwrite(out->buf, sizeof(int32_t), out->pos, fout);
		if (nwrite != out->pos) {
			fprintf(stderr, "Error: Can't write output\n");
			return false;
		}
		out->pos = 0;
	}

	return true;
}

/**
 * Copy from file to file using internal buffer.
 *
 * @param from File to copy data from
 * @param to File to copy data to
 * @return true on succes or false on failure
 */
static bool kmerge_copy(FILE *from, FILE *to)
{
	/* Read 'from' by max chunks (buf size) and write to 'to' */
	for (;;) {
		size_t nread, nwrite;

		nread = fread(obj.buf, sizeof(int32_t), obj.buf_nmemb, from);
		if (nread == 0)
			break;
		nwrite = fwrite(obj.buf, sizeof(int32_t), nread, to);
		if (nwrite != nread) {
			fprintf(stderr, "Error: Can't write output\n");
			return false;
		}
	}

	return true;
}

/**
 * Merge input files into output file.
 *
 * All files are open by caller. This function closes all those files in the
 * end.
 *
 * Output file name will be in "S_N" format, where S = stage + 1, N = outn.
 *
 * @param fs Input files
 * @param fn Input files count; [1..NMERGE]
 * @param stage Current stage number
 * @param outn Output file number
 */
static bool kmerge_merge_files(FILE **fs, size_t fn, size_t stage, size_t outn)
{
	const size_t bs = obj.buf_nmemb / (NMERGE + 1); /* block size, int32_t*/
	struct merge_block blocks[NMERGE + 1]; /* in blocks + out block */
	struct merge_block *b; /* alias */
	struct heap_el el;
	FILE *fout;
	char fname[FNAME_SIZE];
	bool ret = true;
	size_t i;

	/* Initialize blocks */
	memset(blocks, 0, sizeof(blocks));
	for (i = 0; i <= NMERGE; ++i) {
		b = &blocks[i];
		b->buf = obj.buf + i * bs;
		b->size = bs;
	}

	/* Read first blocks from input files into buf */
	for (i = 0; i < fn; ++i) {
		b = &blocks[i];
		b->count = fread(b->buf, sizeof(int32_t), bs, fs[i]);
		b->pos = 0;

		/* Add one element from each input buffer to priority queue */
		el.idx = i;
		el.key = b->buf[b->pos++];
		heap_insert(obj.queue, &el);
	}

	/* Open output file */
	format_tmp_fname(fname, obj.tmpdir, stage + 1, outn);
	pr_debug("### %s(): %s\n", __func__, fname);
	fout = xfopen(fname, "w");

	/* K-way merge */
	ret = kmerge_merge_blocks(blocks, fs, fout);

	/* Close input and output files */
	for (i = 0; i < fn; ++i)
		fclose(fs[i]);
	fclose(fout);
	return ret;
}

/**
 * Merge files on current stage.
 *
 * Input files for current stage have name "S_N", where "S" is current stage
 * number, and "N" is file number (starting from 0). Input files reside in
 * @p tmpdir.
 *
 * @param stage Current stage number (0 is first phase)
 * @param fcount Files count to merge on current stage
 * @return true on success or false on failure
 */
static bool kmerge_merge_stage(size_t stage, size_t fcount)
{
	FILE *fs[NMERGE];
	size_t i, fn = 0;
	bool ret = true;

	for (i = 0; i < fcount; ++i) {
		char fname[FNAME_SIZE];

		format_tmp_fname(fname, obj.tmpdir, stage, i);
		fs[fn] = xfopen(fname, "r");
		fn++;
		if (fn == NMERGE) {
			ret = kmerge_merge_files(fs, fn, stage, i / NMERGE);
			if (!ret)
				return false;
			fn = 0;
		}
	}

	/* Remainder */
	if (fn == 1) {
		/* Fast path */
		char fname[FNAME_SIZE];
		FILE *fout;

		pr_debug("### %s(): remainder = 1 (copy case)\n", __func__);

		format_tmp_fname(fname, obj.tmpdir, stage + 1, i / NMERGE);
		pr_debug("### %s(): %s\n", __func__, fname);
		fout = xfopen(fname, "w");
		ret = kmerge_copy(fs[0], fout);
		fclose(fout);
		fclose(fs[0]);
	} else if (fn != 0) {
		ret = kmerge_merge_files(fs, fn, stage, i / NMERGE);
	}

	return ret;
}

static bool kmerge_merge_all(void)
{
	size_t stages; /* merge stages */
	size_t i;

	stages = kmerge_calc_stages();
	format_tmp_fname(obj.out, obj.tmpdir, stages, 0);
	for (i = 0; i < stages; ++i) {
		size_t stage_fcount = kmerge_calc_stage_files(i);
		bool res;

		res = kmerge_merge_stage(i, stage_fcount);
		if (!res)
			return false;
	}

	return true;
}

/**
 * Perform K-way merge.
 *
 * Input files have names "0_N", where 0 mean "0th merge stage" and "N" is a
 * file number (starting from 0). Input files reside in @p tmpdir.
 *
 * Output (merged) file path will be stored in @p out.
 *
 * @note Not re-entrant (not thread-safe), as it uses internal global var.
 *
 * @param tmpdir Temp directory path (where input files reside)
 * @param fcount Input files count
 * @param buf RAM buffer (allocated) for K-way merge
 * @param buf_nmemb Elements count in @p buf; must be > 16 (NMERGE)
 * @param[out] out Merged file name (file path); must be allocated for 80+ bytes
 * @return true on success or false on failure
 */
bool kmerge_merge(const char *tmpdir, size_t fcount, int32_t *buf,
		  size_t buf_nmemb, char *out)
{
	bool res;

	assert(tmpdir != NULL);
	assert(fcount > 0);
	assert(buf != NULL);
	assert(buf_nmemb > NMERGE);
	assert(out != NULL);

	obj.tmpdir	= tmpdir;
	obj.fcount	= fcount;
	obj.buf		= buf;
	obj.buf_nmemb	= buf_nmemb;
	obj.out		= out;
	obj.queue	= heap_create(NMERGE);
	if (!obj.queue)
		return false;

	res = kmerge_merge_all();
	heap_destroy(obj.queue);

	return res;
}
