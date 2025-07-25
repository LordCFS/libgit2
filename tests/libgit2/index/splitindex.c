#include "clar_libgit2.h"
#include "index.h"
#include "git2/sys/index.h"
#include "splitindex.h"

static git_repository *g_repo;

void test_index_splitindex__initialize(void)
{
	g_repo = cl_git_sandbox_init("splitindex");
}

void test_index_splitindex__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_index_splitindex__can_open_splitindex(void)
{
	git_index *idx;
	
	/* Split-index is now supported, so this should succeed */
	cl_git_pass(git_repository_index(&idx, g_repo));
	
	/* The test repository has an empty split-index, so 0 entries is expected */
	cl_assert(git_index_entrycount(idx) == 0);
	
	git_index_free(idx);
}

void test_index_splitindex__has_splitindex_extension(void)
{
	git_index *idx;
	
	cl_git_pass(git_repository_index(&idx, g_repo));
	
	/* Check that the index is recognized as having split-index */
	/* This verifies our extension reading is working */
	cl_assert(((git_index *)idx)->splitindex != NULL);
	
	git_index_free(idx);
}

void test_index_splitindex__splitindex_is_marked_as_split(void)
{
	git_index *idx;
	git_splitindex *splitindex;
	
	cl_git_pass(git_repository_index(&idx, g_repo));
	
	splitindex = ((git_index *)idx)->splitindex;
	cl_assert(splitindex != NULL);
	cl_assert(splitindex->is_split == true);
	
	git_index_free(idx);
}

void test_index_splitindex__shared_checksum_is_valid(void)
{
	git_index *idx;
	git_splitindex *splitindex;
	char checksum_str[GIT_OID_MAX_HEXSIZE + 1];
	
	cl_git_pass(git_repository_index(&idx, g_repo));
	
	splitindex = ((git_index *)idx)->splitindex;
	cl_assert(splitindex != NULL);
	
	/* Convert checksum to string and verify it's not empty */
	git_oid_tostr(checksum_str, sizeof(checksum_str), &splitindex->shared_checksum);
	cl_assert(strlen(checksum_str) > 0);
	
	/* The expected checksum from our test data */
	cl_assert_equal_s("39d890139ee5356c7ef572216cebcd27aa41f9df", checksum_str);
	
	git_index_free(idx);
}

void test_index_splitindex__bitmap_vectors_are_initialized(void)
{
	git_index *idx;
	git_splitindex *splitindex;
	
	cl_git_pass(git_repository_index(&idx, g_repo));
	
	splitindex = ((git_index *)idx)->splitindex;
	cl_assert(splitindex != NULL);
	
	/* Bitmap vectors should be initialized (even if empty) */
	cl_assert(splitindex->delete_bitmap.contents != NULL || splitindex->delete_bitmap.length == 0);
	cl_assert(splitindex->replace_bitmap.contents != NULL || splitindex->replace_bitmap.length == 0);
	cl_assert(splitindex->added_entries.contents != NULL || splitindex->added_entries.length == 0);
	
	git_index_free(idx);
}

void test_index_splitindex__can_read_index_without_errors(void)
{
	git_index *idx;
	
	/* This test ensures that split-index files can be read without errors */
	cl_git_pass(git_repository_index(&idx, g_repo));
	
	/* Verify basic index operations work */
	cl_assert(git_index_entrycount(idx) == git_index_entrycount(idx)); /* Tautology to avoid >= 0 warning */
	cl_assert(git_index_caps(idx) >= 0);
	
	git_index_free(idx);
}

void test_index_splitindex__can_iterate_entries(void)
{
	git_index *idx;
	size_t entry_count;
	size_t i;
	
	cl_git_pass(git_repository_index(&idx, g_repo));
	
	entry_count = git_index_entrycount(idx);
	
	/* Iterate through all entries (should work even if count is 0) */
	for (i = 0; i < entry_count; i++) {
		const git_index_entry *entry = git_index_get_byindex(idx, i);
		cl_assert(entry != NULL);
		cl_assert(entry->path != NULL);
		cl_assert(strlen(entry->path) > 0);
	}
	
	git_index_free(idx);
}

void test_index_splitindex__index_operations_are_safe(void)
{
	git_index *idx;
	
	cl_git_pass(git_repository_index(&idx, g_repo));
	
	/* Test that basic index operations don't crash with split-index */
	cl_git_pass(git_index_read(idx, 0));
	
	/* Verify the index is still valid after operations */
	cl_assert(((git_index *)idx)->splitindex != NULL);
	cl_assert(((git_index *)idx)->splitindex->is_split == true);
	
	git_index_free(idx);
}

void test_index_splitindex__can_get_index_path(void)
{
	git_index *idx;
	const char *path;
	
	cl_git_pass(git_repository_index(&idx, g_repo));
	
	path = git_index_path(idx);
	cl_assert(path != NULL);
	cl_assert(strlen(path) > 0);
	
	/* Path should contain "index" */
	cl_assert(strstr(path, "index") != NULL);
	
	git_index_free(idx);
}

void test_index_splitindex__memory_management_works(void)
{
	git_index *idx;
	git_splitindex *splitindex;
	int i;
	
	/* Test multiple open/close cycles to verify memory management */
	for (i = 0; i < 3; i++) {
		cl_git_pass(git_repository_index(&idx, g_repo));
		
		splitindex = ((git_index *)idx)->splitindex;
		cl_assert(splitindex != NULL);
		cl_assert(splitindex->is_split == true);
		
		git_index_free(idx);
	}
}

/* Helper function to create a split-index with entries for testing */
static void create_test_index_with_entries(git_repository *repo)
{
	git_index *idx;
	
	/* This test is for future expansion when we have test data with actual entries */
	cl_git_pass(git_repository_index(&idx, repo));
	
	/* For now, just verify we can get the index */
	cl_assert(idx != NULL);
	
	git_index_free(idx);
}

void test_index_splitindex__handles_empty_repository(void)
{
	/* Test that split-index works with empty repositories */
	create_test_index_with_entries(g_repo);
}

void test_index_splitindex__extension_parsing_robust(void)
{
	git_index *idx;
	git_splitindex *splitindex;
	
	cl_git_pass(git_repository_index(&idx, g_repo));
	
	splitindex = ((git_index *)idx)->splitindex;
	cl_assert(splitindex != NULL);
	
	/* Verify that the extension was parsed correctly */
	cl_assert(splitindex->is_split == true);
	
	/* Verify the shared checksum is not all zeros */
	cl_assert(!git_oid_is_zero(&splitindex->shared_checksum));
	
	git_index_free(idx);
}

void test_index_splitindex__shared_index_path_construction(void)
{
	git_index *idx;
	git_splitindex *splitindex;
	char checksum_str[GIT_OID_MAX_HEXSIZE + 1];
	const char *index_path;
	
	cl_git_pass(git_repository_index(&idx, g_repo));
	
	splitindex = ((git_index *)idx)->splitindex;
	index_path = git_index_path(idx);
	
	cl_assert(splitindex != NULL);
	cl_assert(index_path != NULL);
	
	/* Verify checksum can be converted to string */
	git_oid_tostr(checksum_str, sizeof(checksum_str), &splitindex->shared_checksum);
	cl_assert(strlen(checksum_str) == 40); /* SHA1 hex length */
	
	git_index_free(idx);
}

/* Advanced tests for split-index functionality */

void test_index_splitindex__init_and_free_splitindex_struct(void)
{
	git_splitindex splitindex;
	
	/* Test splitindex initialization */
	cl_git_pass(git_splitindex_init(&splitindex));
	
	/* Verify initial state */
	cl_assert(splitindex.is_split == false);
	cl_assert(git_oid_is_zero(&splitindex.shared_checksum));
	cl_assert(splitindex.delete_bitmap.length == 0);
	cl_assert(splitindex.replace_bitmap.length == 0);
	cl_assert(splitindex.added_entries.length == 0);
	
	/* Test cleanup */
	git_splitindex_free(&splitindex);
}

void test_index_splitindex__read_link_extension_basic(void)
{
	git_splitindex splitindex;
	git_oid expected_checksum;
	char buffer[100];
	char *pos = buffer;
	
	/* Initialize splitindex */
	cl_git_pass(git_splitindex_init(&splitindex));
	
	/* Create a test checksum */
	cl_git_pass(git_oid_fromstr(&expected_checksum, "39d890139ee5356c7ef572216cebcd27aa41f9df"));
	
	/* Construct link extension data manually */
	/* 20 bytes checksum + 4 bytes delete_bitmap_size + 4 bytes replace_bitmap_size */
	memcpy(pos, expected_checksum.id, 20);
	pos += 20;
	
	/* No delete bitmap entries */
	*((uint32_t*)pos) = htonl(0);
	pos += 4;
	
	/* No replace bitmap entries */
	*((uint32_t*)pos) = htonl(0);
	pos += 4;
	
	/* Test reading the extension */
	cl_git_pass(git_splitindex_read_link(&splitindex, GIT_OID_SHA1, buffer, pos - buffer));
	
	/* Verify the data was read correctly */
	cl_assert(splitindex.is_split == true);
	cl_assert(git_oid_equal(&splitindex.shared_checksum, &expected_checksum));
	cl_assert(splitindex.delete_bitmap.length == 0);
	cl_assert(splitindex.replace_bitmap.length == 0);
	
	git_splitindex_free(&splitindex);
}

void test_index_splitindex__read_link_extension_error_handling(void)
{
	git_splitindex splitindex;
	char buffer[10]; /* Too small for checksum + sizes */
	
	cl_git_pass(git_splitindex_init(&splitindex));
	
	/* Test error handling with insufficient data */
	cl_git_fail(git_splitindex_read_link(&splitindex, GIT_OID_SHA1, buffer, sizeof(buffer)));
	
	/* Test null pointer safety */
	cl_git_fail(git_splitindex_read_link(NULL, GIT_OID_SHA1, "test", 4));
	cl_git_fail(git_splitindex_read_link(&splitindex, GIT_OID_SHA1, NULL, 4));
	
	git_splitindex_free(&splitindex);
}

void test_index_splitindex__write_link_extension_basic(void)
{
	git_splitindex splitindex;
	git_str output = GIT_STR_INIT;
	git_oid test_checksum;
	
	cl_git_pass(git_splitindex_init(&splitindex));
	cl_git_pass(git_oid_fromstr(&test_checksum, "39d890139ee5356c7ef572216cebcd27aa41f9df"));
	
	/* Set up splitindex for writing */
	git_oid_cpy(&splitindex.shared_checksum, &test_checksum);
	splitindex.is_split = true;
	
	/* Write the extension */
	cl_git_pass(git_splitindex_write_link(&output, &splitindex));
	
	/* Verify output size (20 bytes checksum + 4 bytes + 4 bytes for bitmap sizes) */
	cl_assert(output.size == 28);
	
	/* Verify checksum is at the beginning */
	cl_assert(memcmp(output.ptr, test_checksum.id, 20) == 0);
	
	/* Verify bitmap sizes are 0 */
	{
		uint32_t delete_size = ntohl(*((uint32_t*)(output.ptr + 20)));
		uint32_t replace_size = ntohl(*((uint32_t*)(output.ptr + 24)));
		cl_assert(delete_size == 0);
		cl_assert(replace_size == 0);
	}
	
	git_str_dispose(&output);
	git_splitindex_free(&splitindex);
}

void test_index_splitindex__multiple_index_operations(void)
{
	git_index *idx;
	size_t initial_count, final_count;
	git_splitindex *splitindex;
	git_oid initial_checksum, final_checksum;
	int i;
	
	/* Get initial state */
	cl_git_pass(git_repository_index(&idx, g_repo));
	initial_count = git_index_entrycount(idx);
	splitindex = ((git_index *)idx)->splitindex;
	git_oid_cpy(&initial_checksum, &splitindex->shared_checksum);
	git_index_free(idx);
	
	/* Perform multiple operations */
	for (i = 0; i < 5; i++) {
		cl_git_pass(git_repository_index(&idx, g_repo));
		cl_git_pass(git_index_read(idx, 0));
		
		/* Verify state remains consistent */
		cl_assert(git_index_entrycount(idx) == initial_count);
		cl_assert(((git_index *)idx)->splitindex != NULL);
		cl_assert(((git_index *)idx)->splitindex->is_split == true);
		
		git_index_free(idx);
	}
	
	/* Verify checksum is still the same */
	cl_git_pass(git_repository_index(&idx, g_repo));
	splitindex = ((git_index *)idx)->splitindex;
	git_oid_cpy(&final_checksum, &splitindex->shared_checksum);
	final_count = git_index_entrycount(idx);
	
	cl_assert(git_oid_equal(&initial_checksum, &final_checksum));
	cl_assert(initial_count == final_count);
	
	git_index_free(idx);
}

void test_index_splitindex__shared_index_file_access(void)
{
	git_index *idx, *shared_index = NULL;
	git_splitindex *splitindex;
	char checksum_str[GIT_OID_MAX_HEXSIZE + 1];
	git_str shared_path = GIT_STR_INIT;
	git_str index_dir = GIT_STR_INIT;
	git_str filename = GIT_STR_INIT;
	const char *index_path;
	char *dir_path;
	
	cl_git_pass(git_repository_index(&idx, g_repo));
	splitindex = ((git_index *)idx)->splitindex;
	cl_assert(splitindex != NULL);
	
	/* Construct expected shared index path using simpler method */
	index_path = git_index_path(idx);
	dir_path = git_fs_path_dirname(index_path);
	cl_assert(dir_path != NULL);
	git_oid_tostr(checksum_str, sizeof(checksum_str), &splitindex->shared_checksum);
	cl_git_pass(git_str_printf(&filename, "sharedindex.%s", checksum_str));
	cl_git_pass(git_str_joinpath(&shared_path, dir_path, filename.ptr));
	
	/* Verify the shared index file exists */
	cl_assert(git_fs_path_exists(shared_path.ptr));
	
	/* Try to open the shared index directly */
	cl_git_pass(git_index_open(&shared_index, shared_path.ptr));
	
	/* Verify it's a valid index and not marked as split */
	cl_assert(shared_index != NULL);
	cl_assert(((git_index *)shared_index)->splitindex == NULL);
	
	git__free(dir_path);
	git_index_free(shared_index);
	git_index_free(idx);
	git_str_dispose(&shared_path);
	git_str_dispose(&index_dir);
	git_str_dispose(&filename);
}

void test_index_splitindex__repository_status_operations(void)
{
	git_status_list *status_list = NULL;
	const char *workdir;
	
	/* Test that workdir operations work with split-index */
	workdir = git_repository_workdir(g_repo);
	cl_assert(workdir != NULL);
	
	/* Get repository status (this exercises the index) */
	cl_git_pass(git_status_list_new(&status_list, g_repo, NULL));
	
	/* Should succeed */
	cl_assert(status_list != NULL);
	
	git_status_list_free(status_list);
}

void test_index_splitindex__concurrent_access_simulation(void)
{
	git_index *first_index = NULL, *second_index = NULL;
	
	/* Test opening the same index multiple times */
	cl_git_pass(git_repository_index(&first_index, g_repo));
	cl_git_pass(git_repository_index(&second_index, g_repo));
	
	/* Both should be valid and have split-index */
	cl_assert(((git_index *)first_index)->splitindex != NULL);
	cl_assert(((git_index *)second_index)->splitindex != NULL);
	cl_assert(((git_index *)first_index)->splitindex->is_split == true);
	cl_assert(((git_index *)second_index)->splitindex->is_split == true);
	
	/* Both should have the same shared checksum */
	cl_assert(git_oid_equal(
		&((git_index *)first_index)->splitindex->shared_checksum,
		&((git_index *)second_index)->splitindex->shared_checksum
	));
	
	git_index_free(first_index);
	git_index_free(second_index);
}

void test_index_splitindex__error_resilience(void)
{
	git_index *idx = NULL;
	
	/* Test with corrupted index content that has a proper header signature 
	 * but invalid data to trigger parsing failure */
	cl_git_mkfile("tmp_corrupted_index", "DIRC\x00\x00\x00\x02INVALID_INDEX_CONTENT_THAT_SHOULD_FAIL_PARSING");
	cl_git_fail(git_index_open(&idx, "tmp_corrupted_index"));
	cl_assert(idx == NULL);
	
	/* Test with completely invalid header signature */
	cl_git_mkfile("tmp_invalid_index", "XXXX\x00\x00\x00\x02INVALID");
	cl_git_fail(git_index_open(&idx, "tmp_invalid_index"));
	cl_assert(idx == NULL);
	
	p_unlink("tmp_corrupted_index");
	p_unlink("tmp_invalid_index");
}
