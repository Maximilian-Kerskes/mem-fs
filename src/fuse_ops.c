#include "mem-fs/fuse_ops.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>

static const char hello_content[] = "Hello from mem-fs!\n";

static int memfs_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    (void)fi;

    memset(stbuf, 0, sizeof(*stbuf));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    if (strcmp(path, "/hello") == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = (off_t)strlen(hello_content);
        return 0;
    }

    return -ENOENT;
}

static int memfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset,
                         struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;

    if (strcmp(path, "/") != 0) {
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    filler(buf, "hello", NULL, 0, 0);
    return 0;
}

static int memfs_open(const char* path, struct fuse_file_info* fi) {
    (void)fi;

    if (strcmp(path, "/hello") != 0) {
        return -ENOENT;
    }

    return 0;
}

static int memfs_read(const char* path, char* buf, size_t size, off_t offset,
                      struct fuse_file_info* fi) {
    (void)fi;

    if (strcmp(path, "/hello") != 0) {
        return -ENOENT;
    }
    if (offset < 0) {
        return -EINVAL;
    }

    size_t content_size = strlen(hello_content);
    if ((size_t)offset >= content_size) {
        return 0;
    }

    size_t remaining = content_size - (size_t)offset;
    if (size > remaining) {
        size = remaining;
    }

    memcpy(buf, hello_content + offset, size);
    return (int)size;
}

struct fuse_operations memfs_operations = {
    .getattr = memfs_getattr,
    .readdir = memfs_readdir,
    .open = memfs_open,
    .read = memfs_read,
};
