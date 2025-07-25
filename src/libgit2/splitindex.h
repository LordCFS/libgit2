/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_splitindex_h__
#define INCLUDE_splitindex_h__

#include "common.h"
#include "vector.h"
#include "git2/oid.h"

/* Forward declaration */
struct git_index;

/** Split-index link extension signature */
#define INDEX_EXT_LINK_SIG_STR "link"

/** Split-index link extension data structure */
struct index_link_extension {
	git_oid shared_checksum;   /* SHA-1/SHA-256 of the shared index */
	uint32_t delete_bitmap;    /* Number of bitmap entries for deleted files */
	uint32_t replace_bitmap;   /* Number of bitmap entries for replaced files */
	/* Followed by variable-length bitmaps and entry data */
};

/** Split-index data stored in the main index */
typedef struct git_splitindex {
	git_oid shared_checksum;     /* Checksum of the shared index file */
	char *shared_index_path;     /* Path to the shared index file */
	git_vector delete_bitmap;    /* Bitmap of deleted entries */
	git_vector replace_bitmap;   /* Bitmap of replaced entries */
	git_vector added_entries;    /* New entries added to split index */
	bool is_split;               /* Whether this index uses split-index */
} git_splitindex;

/**
 * Initialize a split-index structure
 *
 * @param splitindex The split-index structure to initialize
 * @return 0 on success, negative error code on failure
 */
int git_splitindex_init(git_splitindex *splitindex);

/**
 * Free resources used by a split-index structure
 *
 * @param splitindex The split-index structure to free
 */
void git_splitindex_free(git_splitindex *splitindex);

/**
 * Read the link extension from an index buffer
 *
 * @param splitindex The split-index structure to populate
 * @param oid_type The OID type (SHA1 or SHA256)
 * @param buffer The buffer containing the link extension data
 * @param size The size of the buffer
 * @return 0 on success, negative error code on failure
 */
int git_splitindex_read_link(
	git_splitindex *splitindex,
	git_oid_t oid_type,
	const char *buffer,
	size_t size);

/**
 * Write the link extension to a buffer
 *
 * @param out The buffer to write to
 * @param splitindex The split-index structure to write
 * @return 0 on success, negative error code on failure
 */
int git_splitindex_write_link(
	git_str *out,
	const git_splitindex *splitindex);

/**
 * Load entries from a shared index file
 *
 * @param entries Vector to store the loaded entries
 * @param shared_index_path Path to the shared index file
 * @return 0 on success, negative error code on failure
 */
int git_splitindex_load_shared_entries(
	git_vector *entries,
	const char *shared_index_path);

/**
 * Merge split-index entries with shared index entries
 *
 * @param index The main index to merge entries into
 * @param splitindex The split-index data
 * @return 0 on success, negative error code on failure
 */
int git_splitindex_merge_entries(
	struct git_index *index,
	git_splitindex *splitindex);

#endif
