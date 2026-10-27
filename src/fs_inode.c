#include "internal/fs_internal.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int fs_create_inode(MemFs* fs, mode_t mode, uint32_t* out_inode_index) {
    if (fs == NULL || out_inode_index == NULL) {
        return -EINVAL;
    }

    if (fs->inode_count >= UINT32_MAX) {
        return -ENOSPC;
    }

    if (fs->inode_count == fs->inode_capacity) {
        if (fs->inode_capacity > SIZE_MAX / 2) {
            return -ENOSPC;
        }

        size_t new_capacity = fs->inode_capacity * 2;
        if (new_capacity > SIZE_MAX / sizeof(*fs->inodes)) {
            return -ENOSPC;
        }

        Inode* new_inodes = realloc(fs->inodes, new_capacity * sizeof(*fs->inodes));
        if (new_inodes == NULL) {
            return -ENOMEM;
        }

        fs->inodes = new_inodes;
        fs->inode_capacity = new_capacity;
    }

    uint32_t index = (uint32_t)fs->inode_count;
    struct timespec now = fs_current_time();

    fs->inodes[index] = (Inode){
        .inode_number = index + 1,
        .mode = mode,
        .size = 0,
        .atime = now,
        .mtime = now,
        .ctime = now,
    };

    fs->inode_count++;
    *out_inode_index = index;
    return 0;
}

int fs_add_dir_entry(MemFs* fs, uint32_t directory_index, const char* name, uint32_t inode_index) {
    if (fs == NULL || name == NULL) {
        return -EINVAL;
    }

    if (directory_index >= fs->inode_count) {
        return -ENOENT;
    }

    Inode* directory = &fs->inodes[directory_index];
    if (!S_ISDIR(directory->mode)) {
        return -ENOTDIR;
    }

    uint32_t existing_index;
    int result = fs_find_dir_entry(fs, directory_index, name, &existing_index);
    if (result == 0) {
        return -EEXIST;
    }
    if (result != -ENOENT) {
        return result;
    }

    if (directory->entry_count >= directory->entry_capacity) {
        if (directory->entry_capacity > SIZE_MAX / 2) {
            return -ENOSPC;
        }

        size_t new_capacity = directory->entry_capacity == 0 ? 4 : directory->entry_capacity * 2;

        if (new_capacity > SIZE_MAX / sizeof(*directory->entries)) {
            return -ENOSPC;
        }

        DirectoryEntry* new_entries =
            realloc(directory->entries, new_capacity * sizeof(*directory->entries));
        if (new_entries == NULL) {
            return -ENOMEM;
        }

        directory->entries = new_entries;
        directory->entry_capacity = new_capacity;
    }

    char* name_copy = strdup(name);
    if (name_copy == NULL) {
        return -ENOMEM;
    }

    directory->entries[directory->entry_count] = (DirectoryEntry){
        .name = name_copy,
        .inode_index = inode_index,
    };
    directory->entry_count++;

    return 0;
}

int fs_remove_dir_entry(MemFs* fs, uint32_t directory_index, uint32_t inode_index) {
    if (fs == NULL || directory_index >= fs->inode_count) {
        return -EINVAL;
    }

    Inode* directory = &fs->inodes[directory_index];
    if (!S_ISDIR(directory->mode)) {
        return -ENOTDIR;
    }

    size_t entry_index = 0;
    while (entry_index < directory->entry_count &&
           directory->entries[entry_index].inode_index != inode_index) {
        entry_index++;
    }

    if (entry_index == directory->entry_count) {
        return -ENOENT;
    }

    free(directory->entries[entry_index].name);
    for (size_t i = entry_index; i + 1 < directory->entry_count; i++) {
        directory->entries[i] = directory->entries[i + 1];
    }
    directory->entry_count--;

    return 0;
}
