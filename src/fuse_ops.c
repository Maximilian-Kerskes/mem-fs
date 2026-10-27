#include "mem-fs/fuse_ops.h"
#include "mem-fs/fs.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <time.h>

static MemFs* get_fs(void) { return fuse_get_context()->private_data; }

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

static int memfs_open(const char* path, struct fuse_file_info* fi) {
    (void)fi;

    struct stat st;
    int result = fs_getattr(get_fs(), path, &st);

    if (result != 0) {
        return result;
    }

    if (!S_ISREG(st.st_mode)) {
        return -EISDIR;
    }

    return 0;
}

static int memfs_read(const char* path, char* buf, size_t size, off_t offset,
                      struct fuse_file_info* fi) {
    (void)fi;

    ssize_t result = fs_read(fuse_get_context()->private_data, path, buf, size, offset);

    if (result > INT_MAX) {
        return -EOVERFLOW;
    }

    return (int)result;
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

static int memfs_write(const char* path, const char* buf, size_t size, off_t offset,
                       struct fuse_file_info* fi) {
    (void)fi;

    ssize_t result = fs_write(fuse_get_context()->private_data, path, buf, size, offset);

    if (result > INT_MAX) {
        return -EOVERFLOW;
    }

    return (int)result;
}

static int memfs_truncate(const char* path, off_t size, struct fuse_file_info* fi) {
    (void)fi;

    return fs_truncate(get_fs(), path, size);
}

static int memfs_unlink(const char* path) { return fs_unlink(get_fs(), path); }

static int memfs_rmdir(const char* path) { return fs_rmdir(get_fs(), path); }

static int memfs_utimens(const char* path, const struct timespec tv[2], struct fuse_file_info* fi) {
    (void)fi;

    return fs_utimens(get_fs(), path, tv);
}

static int memfs_rename(const char* from, const char* to, unsigned int flags) {
    (void)flags;
    return fs_rename(get_fs(), from, to);
}

struct fuse_operations memfs_operations = {.getattr = memfs_getattr,
                                           .readdir = memfs_readdir,
                                           .open = memfs_open,
                                           .read = memfs_read,
                                           .write = memfs_write,
                                           .create = memfs_create,
                                           .mkdir = memfs_mkdir,
                                           .truncate = memfs_truncate,
                                           .unlink = memfs_unlink,
                                           .rmdir = memfs_rmdir,
                                           .utimens = memfs_utimens,
                                           .rename = memfs_rename};
