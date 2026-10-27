#include "internal/fs_internal.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

ssize_t fs_read(MemFs* fs, const char* path, char* buffer, size_t size, off_t offset) {
    if (fs == NULL || path == NULL || (buffer == NULL && size > 0)) {
        return -EINVAL;
    }
    if (offset < 0) {
        return -EINVAL;
    }

    uint32_t inode_index;
    int result = fs_resolve_path(fs, path, &inode_index);
    if (result != 0) {
        return result;
    }

    Inode* inode = &fs->inodes[inode_index];
    if (S_ISDIR(inode->mode)) {
        return -EISDIR;
    }
    if (!S_ISREG(inode->mode)) {
        return -EINVAL;
    }
    if (size == 0 || offset >= inode->size) {
        return 0;
    }

    size_t remaining = (size_t)(inode->size - offset);
    if (size > remaining) {
        size = remaining;
    }

    memcpy(buffer, inode->data + (size_t)offset, size);
    inode->atime = fs_current_time();
    return (ssize_t)size;
}

ssize_t fs_write(MemFs* fs, const char* path, const char* buffer, size_t size, off_t offset) {
    if (fs == NULL || path == NULL || buffer == NULL) {
        return -EINVAL;
    }
    if (offset < 0) {
        return -EINVAL;
    }

    uint32_t inode_index;
    int result = fs_resolve_path(fs, path, &inode_index);
    if (result != 0) {
        return result;
    }

    Inode* inode = &fs->inodes[inode_index];
    if (S_ISDIR(inode->mode)) {
        return -EISDIR;
    }
    if (!S_ISREG(inode->mode)) {
        return -EINVAL;
    }
    if (size == 0) {
        return 0;
    }
    if ((uintmax_t)offset > SIZE_MAX || size > SIZE_MAX - (size_t)offset) {
        return -EFBIG;
    }

    size_t end = (size_t)offset + size;
    size_t old_size = (size_t)inode->size;

    if (end > old_size) {
        char* new_data = realloc(inode->data, end);
        if (new_data == NULL) {
            return -ENOMEM;
        }

        inode->data = new_data;
        if ((size_t)offset > old_size) {
            memset(inode->data + old_size, 0, (size_t)offset - old_size);
        }
        inode->size = (off_t)end;
    }

    memcpy(inode->data + (size_t)offset, buffer, size);
    inode->mtime = fs_current_time();
    inode->ctime = fs_current_time();
    return (ssize_t)size;
}

int fs_truncate(MemFs* fs, const char* path, off_t size) {
    if (fs == NULL || path == NULL) {
        return -EINVAL;
    }
    if (size < 0) {
        return -EINVAL;
    }

    uint32_t inode_index;
    int result = fs_resolve_path(fs, path, &inode_index);
    if (result != 0) {
        return result;
    }

    Inode* inode = &fs->inodes[inode_index];
    if (S_ISDIR(inode->mode)) {
        return -EISDIR;
    }
    if (!S_ISREG(inode->mode)) {
        return -EINVAL;
    }
    if ((size_t)size > SIZE_MAX) {
        return -EFBIG;
    }

    size_t new_size = (size_t)size;
    size_t old_size = (size_t)inode->size;
    if (new_size == old_size) {
        return 0;
    }

    if (new_size == 0) {
        free(inode->data);
        inode->data = NULL;
        inode->size = 0;
        return 0;
    }

    char* new_data = realloc(inode->data, new_size);
    if (new_data == NULL) {
        return -ENOMEM;
    }

    if (new_size > old_size) {
        memset(new_data + old_size, 0, new_size - old_size);
    }

    inode->data = new_data;
    inode->size = size;
    inode->mtime = fs_current_time();
    inode->ctime = fs_current_time();
    return 0;
}
