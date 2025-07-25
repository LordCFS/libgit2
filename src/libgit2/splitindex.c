/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "splitindex.h"

#include "index.h"
#include "futils.h"
#include "path.h"
#include "oid.h"
#include "git2/index.h"

/* Bit manipulation helpers for bitmaps */
#define BITMAP_ENTRY_BYTES 4
#define BITMAP_BITS_PER_ENTRY (BITMAP_ENTRY_BYTES * 8)

static int splitindex_entry_dup(git_index_entry **out, const git_index_entry *src)
{
	git_index_entry *entry;
	size_t path_len;

	GIT_ASSERT_ARG(out);
	GIT_ASSERT_ARG(src);

	path_len = strlen(src->path);
	entry = git__calloc(1, sizeof(git_index_entry) + path_len + 1);
	if (!entry)
		return -1;

	memcpy(entry, src, sizeof(git_index_entry));
	entry->path = (char *)entry + sizeof(git_index_entry);
	memcpy((char *)entry->path, src->path, path_len + 1);

	*out = entry;
	return 0;
}

static bool bitmap_get_bit(const uint32_t *bitmap, size_t index)
{
	size_t byte_idx = index / BITMAP_BITS_PER_ENTRY;
	size_t bit_idx = index % BITMAP_BITS_PER_ENTRY;
	return (bitmap[byte_idx] & (1U << bit_idx)) != 0;
}

int git_splitindex_init(git_splitindex *splitindex)
{
	GIT_ASSERT_ARG(splitindex);

	memset(splitindex, 0, sizeof(git_splitindex));
	
	if (git_vector_init(&splitindex->delete_bitmap, 0, NULL) < 0 ||
	    git_vector_init(&splitindex->replace_bitmap, 0, NULL) < 0 ||
	    git_vector_init(&splitindex->added_entries, 0, NULL) < 0) {
		git_splitindex_free(splitindex);
		return -1;
	}

	return 0;
}

void git_splitindex_free(git_splitindex *splitindex)
{
	if (!splitindex)
		return;

	git__free(splitindex->shared_index_path);
	git_vector_dispose(&splitindex->delete_bitmap);
	git_vector_dispose(&splitindex->replace_bitmap);
	git_vector_dispose(&splitindex->added_entries);
	
	memset(splitindex, 0, sizeof(git_splitindex));
}

int git_splitindex_read_link(
	git_splitindex *splitindex,
	git_oid_t oid_type,
	const char *buffer,
	size_t size)
{
	const char *buffer_end = buffer + size;
	size_t checksum_size = git_hash_size(git_oid_algorithm(oid_type));
	uint32_t delete_bitmap_size, replace_bitmap_size;
	size_t expected_size, i;
	
	GIT_ASSERT_ARG(splitindex);
	GIT_ASSERT_ARG(buffer);

	if (size < checksum_size + 8) {
		git_error_set(GIT_ERROR_INDEX, "link extension is too small");
		return -1;
	}

	/* Read shared index checksum */
	git_oid_fromraw(&splitindex->shared_checksum, (const unsigned char *)buffer);
	buffer += checksum_size;

	/* Read bitmap sizes */
	delete_bitmap_size = ntohl(*((uint32_t *)buffer));
	buffer += 4;
	replace_bitmap_size = ntohl(*((uint32_t *)buffer));
	buffer += 4;

	/* Verify we have enough data */
	expected_size = checksum_size + 8 +
	                      (delete_bitmap_size * BITMAP_ENTRY_BYTES) +
	                      (replace_bitmap_size * BITMAP_ENTRY_BYTES);
	
	if (size < expected_size) {
		git_error_set(GIT_ERROR_INDEX, "link extension data is truncated");
		return -1;
	}

	/* Read delete bitmap */
	if (delete_bitmap_size > 0) {
		if (git_vector_init(&splitindex->delete_bitmap, delete_bitmap_size, NULL) < 0)
			return -1;
			
		for (i = 0; i < delete_bitmap_size; i++) {
			uint32_t entry;
			if (buffer + BITMAP_ENTRY_BYTES > buffer_end) {
				git_error_set(GIT_ERROR_INDEX, "delete bitmap is truncated");
				return -1;
			}
			entry = ntohl(*((uint32_t *)buffer));
			git_vector_insert(&splitindex->delete_bitmap, (void *)(uintptr_t)entry);
			buffer += BITMAP_ENTRY_BYTES;
		}
	}

	/* Read replace bitmap */
	if (replace_bitmap_size > 0) {
		if (git_vector_init(&splitindex->replace_bitmap, replace_bitmap_size, NULL) < 0)
			return -1;
			
		for (i = 0; i < replace_bitmap_size; i++) {
			uint32_t entry;
			if (buffer + BITMAP_ENTRY_BYTES > buffer_end) {
				git_error_set(GIT_ERROR_INDEX, "replace bitmap is truncated");
				return -1;
			}
			entry = ntohl(*((uint32_t *)buffer));
			git_vector_insert(&splitindex->replace_bitmap, (void *)(uintptr_t)entry);
			buffer += BITMAP_ENTRY_BYTES;
		}
	}

	splitindex->is_split = true;
	return 0;
}

int git_splitindex_write_link(
	git_str *out,
	const git_splitindex *splitindex)
{
	uint32_t delete_bitmap_size, replace_bitmap_size;
	size_t checksum_size;
	size_t i;
	
	GIT_ASSERT_ARG(out);
	GIT_ASSERT_ARG(splitindex);

	if (!splitindex->is_split)
		return 0;

	/* Write shared index checksum */
	checksum_size = git_hash_size(git_oid_algorithm(GIT_OID_SHA1)); /* For now, assume SHA1 */
	if (git_str_put(out, (const char *)splitindex->shared_checksum.id, checksum_size) < 0)
		return -1;

	/* Write bitmap sizes */
	delete_bitmap_size = htonl((uint32_t)splitindex->delete_bitmap.length);
	replace_bitmap_size = htonl((uint32_t)splitindex->replace_bitmap.length);
	
	if (git_str_put(out, (const char *)&delete_bitmap_size, 4) < 0 ||
	    git_str_put(out, (const char *)&replace_bitmap_size, 4) < 0)
		return -1;

	/* Write delete bitmap */
	for (i = 0; i < splitindex->delete_bitmap.length; i++) {
		uint32_t entry = htonl((uint32_t)(uintptr_t)splitindex->delete_bitmap.contents[i]);
		if (git_str_put(out, (const char *)&entry, 4) < 0)
			return -1;
	}

	/* Write replace bitmap */
	for (i = 0; i < splitindex->replace_bitmap.length; i++) {
		uint32_t entry = htonl((uint32_t)(uintptr_t)splitindex->replace_bitmap.contents[i]);
		if (git_str_put(out, (const char *)&entry, 4) < 0)
			return -1;
	}

	return 0;
}

static int construct_shared_index_path(
	git_str *path,
	const char *index_path,
	const git_oid *shared_checksum)
{
	char checksum_str[GIT_OID_MAX_HEXSIZE + 1];
	git_str dir_path = GIT_STR_INIT;
	git_str filename = GIT_STR_INIT;
	int error = 0;

	/* Get the directory containing the index file */
	if ((error = git_fs_path_dirname_r(&dir_path, index_path)) < 0)
		goto cleanup;

	/* Convert checksum to hex string */
	git_oid_tostr(checksum_str, sizeof(checksum_str), shared_checksum);

	/* Construct shared index filename: sharedindex.<checksum> */
	if ((error = git_str_printf(&filename, "sharedindex.%s", checksum_str)) < 0)
		goto cleanup;
		
	error = git_str_joinpath(path, dir_path.ptr, filename.ptr);

cleanup:
	git_str_dispose(&dir_path);
	git_str_dispose(&filename);
	return error;
}

int git_splitindex_load_shared_entries(
	git_vector *entries,
	const char *shared_index_path)
{
	git_index *shared_index = NULL;
	int error = 0;
	size_t i;

	GIT_ASSERT_ARG(entries);
	GIT_ASSERT_ARG(shared_index_path);

	/* Open and read the shared index file */
	if ((error = git_index_open(&shared_index, shared_index_path)) < 0) {
		git_error_set(GIT_ERROR_INDEX, "failed to open shared index file '%s'", shared_index_path);
		goto cleanup;
	}

	/* Copy entries from the shared index */
	if ((error = git_vector_init(entries, shared_index->entries.length, NULL)) < 0)
		goto cleanup;

	for (i = 0; i < shared_index->entries.length; i++) {
		git_index_entry *shared_entry = shared_index->entries.contents[i];
		git_index_entry *entry_copy = NULL;
		
		/* Create a copy of the entry */
		if ((error = splitindex_entry_dup(&entry_copy, shared_entry)) < 0)
			goto cleanup;
			
		if ((error = git_vector_insert(entries, entry_copy)) < 0) {
			git__free(entry_copy);
			goto cleanup;
		}
	}

cleanup:
	if (error < 0 && entries->contents) {
		git_index_entry *entry;
		git_vector_foreach(entries, i, entry) {
			git__free(entry);
		}
		git_vector_dispose(entries);
	}
	
	git_index_free(shared_index);
	return error;
}

int git_splitindex_merge_entries(
	struct git_index *index,
	git_splitindex *splitindex)
{
	git_vector shared_entries = GIT_VECTOR_INIT;
	git_str shared_path = GIT_STR_INIT;
	int error = 0;
	size_t i, j;

	GIT_ASSERT_ARG(index);
	GIT_ASSERT_ARG(splitindex);

	if (!splitindex->is_split)
		return 0;

	/* Construct path to shared index file */
	if ((error = construct_shared_index_path(&shared_path, 
	                                        index->index_file_path,
	                                        &splitindex->shared_checksum)) < 0)
		goto cleanup;

	/* Load entries from shared index */
	if ((error = git_splitindex_load_shared_entries(&shared_entries, 
	                                               shared_path.ptr)) < 0)
		goto cleanup;

	/* Clear the current entries in the index */
	git_index_clear(index);

	/* Start with shared entries */
	for (i = 0; i < shared_entries.length; i++) {
		git_index_entry *shared_entry = shared_entries.contents[i];
		bool deleted = false;
		bool replaced = false;

		/* Check if this entry is marked for deletion */
		for (j = 0; j < splitindex->delete_bitmap.length; j++) {
			uint32_t bitmap_entry = (uint32_t)(uintptr_t)splitindex->delete_bitmap.contents[j];
			if (bitmap_get_bit(&bitmap_entry, i)) {
				deleted = true;
				break;
			}
		}

		/* Check if this entry is marked for replacement */
		for (j = 0; j < splitindex->replace_bitmap.length; j++) {
			uint32_t bitmap_entry = (uint32_t)(uintptr_t)splitindex->replace_bitmap.contents[j];
			if (bitmap_get_bit(&bitmap_entry, i)) {
				replaced = true;
				break;
			}
		}

		/* Skip deleted entries and replaced entries (they'll be added from split index) */
		if (!deleted && !replaced) {
			git_index_entry *entry_copy = NULL;
			if ((error = splitindex_entry_dup(&entry_copy, shared_entry)) < 0)
				goto cleanup;
				
			if ((error = git_vector_insert(&index->entries, entry_copy)) < 0) {
				git__free(entry_copy);
				goto cleanup;
			}
			
			if ((error = git_index_entrymap_put(&index->entries_map, entry_copy)) < 0)
				goto cleanup;
		}
	}

	/* Add new entries from the split index (these are already in index->entries from parsing) */
	/* The split index entries were parsed during the normal index reading process */

cleanup:
	if (shared_entries.contents) {
		git_index_entry *entry;
		git_vector_foreach(&shared_entries, i, entry) {
			git__free(entry);
		}
		git_vector_dispose(&shared_entries);
	}
	git_str_dispose(&shared_path);
	return error;
}
