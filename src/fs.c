#include "internal/fs_internal.h"

#include <stdlib.h>
#include <time.h>

struct timespec fs_current_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts;
}

MemFs* fs_create(void) {
    MemFs* fs = calloc(1, sizeof(*fs));
    if (fs == NULL) {
        return NULL;
    }

    fs->inode_capacity = 16;
    fs->inodes = calloc(fs->inode_capacity, sizeof(*fs->inodes));
    if (fs->inodes == NULL) {
        free(fs);
        return NULL;
    }

    fs->inode_count = 1;
    fs->root_inode = 0;

    Inode* root = &fs->inodes[fs->root_inode];
    struct timespec now = fs_current_time();
    root->inode_number = 1;
    root->mode = S_IFDIR | 0755;
    root->size = 0;
    root->atime = now;
    root->mtime = now;
    root->ctime = now;

    return fs;
}

void fs_destroy(MemFs* fs) {
    if (fs == NULL) {
        return;
    }

    for (size_t i = 0; i < fs->inode_count; i++) {
        Inode* inode = &fs->inodes[i];
        if (S_ISREG(inode->mode)) {
            free(inode->data);
        } else if (S_ISDIR(inode->mode)) {
            for (size_t j = 0; j < inode->entry_count; j++) {
                free(inode->entries[j].name);
            }
            free(inode->entries);
        }
    }

    free(fs->inodes);
    free(fs);
}
