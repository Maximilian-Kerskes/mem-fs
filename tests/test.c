#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "mem-fs/fs.h"
#include "mem-fs/fuse_ops.h"

struct directory_entries {
    const char* names[64];
    size_t count;
    size_t stop_after;
};

static int collect_directory_entry(const char* name, void* context) {
    struct directory_entries* entries = context;

    assert(entries->count < sizeof(entries->names) / sizeof(entries->names[0]));
    entries->names[entries->count++] = name;

    return entries->stop_after != 0 && entries->count >= entries->stop_after;
}

static void assert_entries(MemFs* fs, const char* path, const char* const* expected,
                           size_t expected_count) {
    struct directory_entries actual = {0};

    assert(fs_readdir(fs, path, collect_directory_entry, &actual) == 0);
    assert(actual.count == expected_count);

    for (size_t i = 0; i < expected_count; i++) {
        assert(strcmp(actual.names[i], expected[i]) == 0);
    }
}

static void test_create_and_metadata(void) {
    MemFs* fs = fs_create();
    assert(fs != NULL);

    struct stat st;
    assert(fs_getattr(fs, "/", &st) == 0);
    assert(S_ISDIR(st.st_mode));
    assert((st.st_mode & 0777) == 0755);
    assert(st.st_nlink == 2);
    assert(st.st_size == 0);

    assert(fs_create_dir(fs, "/docs") == 0);
    assert(fs_create_dir(fs, "/docs/archive") == 0);
    assert(fs_create_file(fs, "/docs/readme") == 0);

    assert(fs_getattr(fs, "/docs", &st) == 0);
    assert(S_ISDIR(st.st_mode));
    assert(fs_getattr(fs, "/docs/readme", &st) == 0);
    assert(S_ISREG(st.st_mode));
    assert((st.st_mode & 0777) == 0644);
    assert(st.st_size == 0);

    const char* root_entries[] = {"docs"};
    const char* docs_entries[] = {"archive", "readme"};
    assert_entries(fs, "/", root_entries, 1);
    assert_entries(fs, "/docs", docs_entries, 2);

    assert(fs_create_file(fs, "/docs/readme") == -EEXIST);
    assert(fs_create_dir(fs, "/missing/child") == -ENOENT);
    assert(fs_create_file(fs, "/docs/readme/child") == -ENOTDIR);
    assert(fs_create_file(fs, "relative") == -EINVAL);
    assert(fs_create_file(fs, "/") == -EEXIST);

    fs_destroy(fs);
}

static void test_read_write_and_sparse_files(void) {
    MemFs* fs = fs_create();
    assert(fs != NULL);
    assert(fs_create_file(fs, "/data") == 0);

    const char content[] = "hello world";
    assert(fs_write(fs, "/data", content, strlen(content), 0) == (ssize_t)strlen(content));

    char buffer[32] = {0};
    assert(fs_read(fs, "/data", buffer, sizeof(buffer), 0) == (ssize_t)strlen(content));
    assert(memcmp(buffer, content, strlen(content)) == 0);
    assert(fs_read(fs, "/data", buffer, 5, 6) == 5);
    assert(memcmp(buffer, "world", 5) == 0);
    assert(fs_read(fs, "/data", buffer, sizeof(buffer), strlen(content)) == 0);

    assert(fs_write(fs, "/data", "x", 1, 20) == 1);
    struct stat st;
    assert(fs_getattr(fs, "/data", &st) == 0);
    assert(st.st_size == 21);
    memset(buffer, 0xff, sizeof(buffer));
    assert(fs_read(fs, "/data", buffer, 21, 0) == 21);
    for (size_t i = strlen(content); i < 20; i++) {
        assert(buffer[i] == 0);
    }
    assert(buffer[20] == 'x');

    assert(fs_truncate(fs, "/data", 5) == 0);
    assert(fs_getattr(fs, "/data", &st) == 0);
    assert(st.st_size == 5);
    assert(fs_read(fs, "/data", buffer, sizeof(buffer), 0) == 5);
    assert(memcmp(buffer, "hello", 5) == 0);

    assert(fs_truncate(fs, "/data", 10) == 0);
    assert(fs_getattr(fs, "/data", &st) == 0);
    assert(st.st_size == 10);
    assert(fs_read(fs, "/data", buffer, 10, 0) == 10);
    for (size_t i = 5; i < 10; i++) {
        assert(buffer[i] == 0);
    }

    assert(fs_truncate(fs, "/data", 0) == 0);
    assert(fs_getattr(fs, "/data", &st) == 0);
    assert(st.st_size == 0);
    assert(fs_read(fs, "/data", buffer, sizeof(buffer), 0) == 0);

    assert(fs_read(fs, "/", buffer, sizeof(buffer), 0) == -EISDIR);
    assert(fs_write(fs, "/", "x", 1, 0) == -EISDIR);
    assert(fs_truncate(fs, "/", 0) == -EISDIR);
    assert(fs_read(fs, "/data", buffer, sizeof(buffer), -1) == -EINVAL);
    assert(fs_write(fs, "/data", "x", 1, -1) == -EINVAL);
    assert(fs_truncate(fs, "/data", -1) == -EINVAL);
    assert(fs_read(fs, "/missing", buffer, sizeof(buffer), 0) == -ENOENT);

    fs_destroy(fs);
}

static void test_readdir_callback_and_capacity_growth(void) {
    MemFs* fs = fs_create();
    assert(fs != NULL);

    for (size_t i = 0; i < 20; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/file%zu", i);
        assert(fs_create_file(fs, path) == 0);
    }

    struct directory_entries first_two = {.stop_after = 2};
    assert(fs_readdir(fs, "/", collect_directory_entry, &first_two) == 0);
    assert(first_two.count == 2);
    assert(strcmp(first_two.names[0], "file0") == 0);
    assert(strcmp(first_two.names[1], "file1") == 0);

    struct directory_entries all = {0};
    assert(fs_readdir(fs, "/", collect_directory_entry, &all) == 0);
    assert(all.count == 20);

    assert(fs_readdir(fs, "/file0", collect_directory_entry, &all) == -ENOTDIR);
    assert(fs_readdir(fs, "/missing", collect_directory_entry, &all) == -ENOENT);

    fs_destroy(fs);
}

static void test_unlink_and_rmdir(void) {
    MemFs* fs = fs_create();
    assert(fs != NULL);
    assert(fs_create_dir(fs, "/dir") == 0);
    assert(fs_create_file(fs, "/dir/file") == 0);
    assert(fs_create_dir(fs, "/empty") == 0);

    assert(fs_unlink(fs, "/dir") == -EISDIR);
    assert(fs_rmdir(fs, "/dir") == -ENOTEMPTY);
    assert(fs_rmdir(fs, "/dir/file") == -ENOTDIR);
    assert(fs_unlink(fs, "/dir/file") == 0);
    assert(fs_getattr(fs, "/dir/file", &(struct stat){0}) == -ENOENT);
    assert(fs_rmdir(fs, "/dir") == 0);
    assert(fs_getattr(fs, "/dir", &(struct stat){0}) == -ENOENT);

    assert(fs_rmdir(fs, "/empty") == 0);
    assert(fs_unlink(fs, "/missing") == -ENOENT);
    assert(fs_rmdir(fs, "/") == -EEXIST);

    fs_destroy(fs);
}

static void test_null_arguments_and_operations_table(void) {
    MemFs* fs = fs_create();
    assert(fs != NULL);
    fs_destroy(fs);
    assert(fs_getattr(NULL, "/", &(struct stat){0}) == -EINVAL);
    assert(fs_getattr(NULL, "/", NULL) == -EINVAL);
    assert(fs_create_file(NULL, "/file") == -EINVAL);
    assert(fs_create_dir(NULL, "/dir") == -EINVAL);
    assert(fs_read(NULL, "/file", NULL, 0, 0) == -EINVAL);
    assert(fs_write(NULL, "/file", "x", 1, 0) == -EINVAL);
    assert(fs_truncate(NULL, "/file", 0) == -EINVAL);
    assert(fs_unlink(NULL, "/file") == -EINVAL);
    assert(fs_rmdir(NULL, "/dir") == -EINVAL);

    assert(memfs_operations.getattr != NULL);
    assert(memfs_operations.readdir != NULL);
    assert(memfs_operations.open != NULL);
    assert(memfs_operations.read != NULL);
    assert(memfs_operations.write != NULL);
    assert(memfs_operations.create != NULL);
    assert(memfs_operations.mkdir != NULL);
    assert(memfs_operations.truncate != NULL);
    assert(memfs_operations.unlink != NULL);
    assert(memfs_operations.rmdir != NULL);
}

int main(void) {
    test_create_and_metadata();
    test_read_write_and_sparse_files();
    test_readdir_callback_and_capacity_growth();
    test_unlink_and_rmdir();
    test_null_arguments_and_operations_table();
    return 0;
}
