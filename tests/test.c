#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

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

static void test_path_resolution_and_validation(void) {
    MemFs* fs = fs_create();
    assert(fs != NULL);

    assert(fs_create_dir(fs, "/one") == 0);
    assert(fs_create_dir(fs, "/one/two") == 0);
    assert(fs_create_file(fs, "/one/two/file") == 0);

    struct stat st;
    assert(fs_getattr(fs, "/one//two///file", &st) == 0);
    assert(S_ISREG(st.st_mode));
    assert(fs_getattr(fs, "/one/two/file///", &st) == -ENOTDIR);
    assert(fs_create_file(fs, "/one/two///another///") == 0);
    assert(fs_getattr(fs, "/one/two/another", &st) == 0);

    assert(fs_getattr(fs, NULL, &st) == -EINVAL);
    assert(fs_getattr(fs, "relative", &st) == -EINVAL);
    assert(fs_getattr(fs, "/missing", &st) == -ENOENT);
    assert(fs_readdir(fs, NULL, collect_directory_entry, NULL) == -EINVAL);
    assert(fs_readdir(fs, "/one", NULL, NULL) == -EINVAL);
    assert(fs_write(fs, "/one/two/file", NULL, 0, 0) == -EINVAL);
    assert(fs_read(fs, "/one/two/file", NULL, 1, 0) == -EINVAL);

    fs_destroy(fs);
}

static void test_rename_and_utimens(void) {
    MemFs* fs = fs_create();
    assert(fs != NULL);
    assert(fs_create_dir(fs, "/left") == 0);
    assert(fs_create_dir(fs, "/right") == 0);
    assert(fs_create_file(fs, "/left/file") == 0);
    assert(fs_write(fs, "/left/file", "contents", 8, 0) == 8);

    struct stat before;
    struct stat after;
    assert(fs_getattr(fs, "/left/file", &before) == 0);
    assert(fs_rename(fs, "/left/file", "/left/renamed") == 0);
    assert(fs_getattr(fs, "/left/file", &after) == -ENOENT);
    assert(fs_getattr(fs, "/left/renamed", &after) == 0);
    assert(after.st_ino == before.st_ino);

    char buffer[16] = {0};
    assert(fs_read(fs, "/left/renamed", buffer, sizeof(buffer), 0) == 8);
    assert(strcmp(buffer, "contents") == 0);

    assert(fs_rename(fs, "/left/renamed", "/right/moved") == 0);
    assert(fs_getattr(fs, "/left/renamed", &after) == -ENOENT);
    assert(fs_getattr(fs, "/right/moved", &after) == 0);
    assert(after.st_ino == before.st_ino);

    assert(fs_create_file(fs, "/right/replacement") == 0);
    assert(fs_write(fs, "/right/replacement", "old", 3, 0) == 3);
    assert(fs_rename(fs, "/right/moved", "/right/replacement") == 0);
    assert(fs_getattr(fs, "/right/moved", &after) == -ENOENT);
    assert(fs_read(fs, "/right/replacement", buffer, sizeof(buffer), 0) == 8);
    assert(strcmp(buffer, "contents") == 0);

    assert(fs_rename(fs, "/right/replacement", "/right/replacement") == 0);
    assert(fs_create_dir(fs, "/left/child") == 0);
    assert(fs_rename(fs, "/left", "/left/child/moved") == -EINVAL);
    assert(fs_rename(fs, "/left/child", "/right/replacement") == -ENOTDIR);
    assert(fs_rename(fs, "/missing", "/right/nope") == -ENOENT);

    struct timespec times[2] = {{.tv_sec = 123, .tv_nsec = 456},
                                {.tv_sec = 789, .tv_nsec = 987}};
    assert(fs_utimens(fs, "/right/replacement", times) == 0);
    assert(fs_getattr(fs, "/right/replacement", &after) == 0);
    assert(after.st_atim.tv_sec == times[0].tv_sec);
    assert(after.st_atim.tv_nsec == times[0].tv_nsec);
    assert(after.st_mtim.tv_sec == times[1].tv_sec);
    assert(after.st_mtim.tv_nsec == times[1].tv_nsec);

    struct timespec omit[2] = {{.tv_sec = 0, .tv_nsec = UTIME_OMIT},
                               {.tv_sec = 0, .tv_nsec = UTIME_OMIT}};
    assert(fs_utimens(fs, "/right/replacement", omit) == 0);
    assert(fs_utimens(fs, "/missing", times) == -ENOENT);

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
    assert(fs_utimens(NULL, "/file", NULL) == -EINVAL);
    assert(fs_rename(NULL, "/file", "/other") == -EINVAL);

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
    assert(memfs_operations.utimens != NULL);
    assert(memfs_operations.rename != NULL);
}

int main(void) {
    test_create_and_metadata();
    test_read_write_and_sparse_files();
    test_readdir_callback_and_capacity_growth();
    test_unlink_and_rmdir();
    test_path_resolution_and_validation();
    test_rename_and_utimens();
    test_null_arguments_and_operations_table();
    return 0;
}
