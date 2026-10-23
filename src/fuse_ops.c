#include "mem-fs/fuse_ops.h"
#include "mem-fs/fs.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>

static MemFs* get_fs(void) { return fuse_get_context()->private_data; }

static const char hello_content[] = "Hello from mem-fs!\n";

static int memfs_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    (void)fi;

    return fs_getattr(get_fs(), path, stbuf);
}

static int add_fuse_dir_entry(const char* name, void* context) {
    struct {
        void* buf;
        fuse_fill_dir_t filler;
    }* state = context;

    return state->filler(state->buf, name, NULL, 0, 0);
}

static int memfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset,
                         struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;

    struct {
        void* buf;
        fuse_fill_dir_t filler;
    } state = {.buf = buf, .filler = filler};

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    return fs_readdir(get_fs(), path, add_fuse_dir_entry, &state);
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

static int memfs_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    (void)mode;
    (void)fi;

    return fs_create_file(get_fs(), path);
}

static int memfs_mkdir(const char* path, mode_t mode) {
    (void)mode;

    return fs_create_dir(get_fs(), path);
}

struct fuse_operations memfs_operations = {
    .getattr = memfs_getattr,
    .readdir = memfs_readdir,
    .read = memfs_read,
    .create = memfs_create,
    .mkdir = memfs_mkdir,
};
