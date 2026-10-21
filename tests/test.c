#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include "mem-fs/fuse_ops.h"

static const char hello_content[] = "Hello from mem-fs!\n";

struct directory_entries {
    const char* names[3];
    size_t count;
};

static int collect_directory_entry(void* data, const char* name, const struct stat* st,
                                   off_t offset, enum fuse_fill_dir_flags flags) {
    (void)st;
    (void)offset;
    (void)flags;

    struct directory_entries* entries = data;
    assert(entries->count < 3);
    entries->names[entries->count++] = name;
    return 0;
}

static void test_operations_table(void) {
    /* The public API is the operations table, not its individual callbacks. */
    assert(memfs_operations.getattr != NULL);
    assert(memfs_operations.readdir != NULL);
    assert(memfs_operations.open != NULL);
    assert(memfs_operations.read != NULL);
}

static void test_getattr(void) {
    struct stat st;

    assert(memfs_operations.getattr("/", &st, NULL) == 0);
    assert(S_ISDIR(st.st_mode));
    assert((st.st_mode & 0777) == 0755);
    assert(st.st_nlink == 2);

    assert(memfs_operations.getattr("/hello", &st, NULL) == 0);
    assert(S_ISREG(st.st_mode));
    assert((st.st_mode & 0777) == 0444);
    assert(st.st_size == (off_t)strlen(hello_content));

    assert(memfs_operations.getattr("/missing", &st, NULL) == -ENOENT);
}

static void test_readdir(void) {
    struct directory_entries entries = {0};

    assert(memfs_operations.readdir("/", &entries, collect_directory_entry, 0, NULL, 0) == 0);
    assert(entries.count == 3);
    assert(strcmp(entries.names[0], ".") == 0);
    assert(strcmp(entries.names[1], "..") == 0);
    assert(strcmp(entries.names[2], "hello") == 0);

    assert(memfs_operations.readdir("/missing", &entries, collect_directory_entry, 0, NULL, 0) ==
           -ENOENT);
}

static void test_open(void) {
    assert(memfs_operations.open("/hello", NULL) == 0);
    assert(memfs_operations.open("/missing", NULL) == -ENOENT);
}

static void test_read(void) {
    char buffer[sizeof(hello_content)] = {0};

    assert(memfs_operations.read("/hello", buffer, 5, 0, NULL) == 5);
    assert(memcmp(buffer, "Hello", 5) == 0);

    memset(buffer, 0, sizeof(buffer));
    assert(memfs_operations.read("/hello", buffer, sizeof(buffer), 6, NULL) ==
           (int)(strlen(hello_content) - 6));
    assert(memcmp(buffer, hello_content + 6, strlen(hello_content) - 6) == 0);

    assert(memfs_operations.read("/hello", buffer, sizeof(buffer), strlen(hello_content), NULL) ==
           0);
    assert(memfs_operations.read("/hello", buffer, sizeof(buffer), -1, NULL) == -EINVAL);
    assert(memfs_operations.read("/missing", buffer, sizeof(buffer), 0, NULL) == -ENOENT);
}

int main(void) {
    test_operations_table();
    test_getattr();
    test_readdir();
    test_open();
    test_read();

    return 0;
}
