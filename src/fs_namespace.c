#include "internal/fs_internal.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int create_node(MemFs* fs, const char* path, mode_t mode) {
    char* parent_path;
    char* name;

    int result = fs_split_path(path, &parent_path, &name);
    if (result != 0) {
        return result;
    }

    uint32_t parent_index;
    result = fs_resolve_path(fs, parent_path, &parent_index);
    free(parent_path);

    if (result != 0) {
        free(name);
        return result;
    }

    if (!S_ISDIR(fs->inodes[parent_index].mode)) {
        free(name);
        return -ENOTDIR;
    }

    fs->inodes[parent_index].mtime = fs_current_time();
    fs->inodes[parent_index].ctime = fs_current_time();

    uint32_t existing_index;
    result = fs_find_dir_entry(fs, parent_index, name, &existing_index);
    if (result == 0) {
        free(name);
        return -EEXIST;
    }
    if (result != -ENOENT) {
        free(name);
        return result;
    }

    uint32_t file_index;
    result = fs_create_inode(fs, mode, &file_index);
    if (result == 0) {
        result = fs_add_dir_entry(fs, parent_index, name, file_index);
    }

    free(name);
    return result;
}

int fs_create_file(MemFs* fs, const char* path) {
    if (fs == NULL) {
        return -EINVAL;
    }
    return create_node(fs, path, S_IFREG | 0644);
}

int fs_create_dir(MemFs* fs, const char* path) {
    if (fs == NULL) {
        return -EINVAL;
    }
    return create_node(fs, path, S_IFDIR | 0755);
}

int fs_getattr(MemFs* fs, const char* path, struct stat* stbuf) {
    if (fs == NULL || path == NULL || stbuf == NULL) {
        return -EINVAL;
    }

    uint32_t inode_index;
    int result = fs_resolve_path(fs, path, &inode_index);
    if (result != 0) {
        return result;
    }

    Inode* inode = &fs->inodes[inode_index];
    memset(stbuf, 0, sizeof(*stbuf));
    stbuf->st_ino = inode->inode_number;
    stbuf->st_mode = inode->mode;
    stbuf->st_size = inode->size;
    stbuf->st_atim = inode->atime;
    stbuf->st_mtim = inode->mtime;
    stbuf->st_ctim = inode->ctime;
    stbuf->st_nlink = S_ISREG(inode->mode) ? 1 : S_ISDIR(inode->mode) ? 2 : 0;

    return 0;
}

int fs_readdir(MemFs* fs, const char* path, FsDirEntryCallback callback, void* context) {
    if (fs == NULL || path == NULL || callback == NULL) {
        return -EINVAL;
    }

    uint32_t inode_index;
    int result = fs_resolve_path(fs, path, &inode_index);
    if (result != 0) {
        return result;
    }

    Inode* directory = &fs->inodes[inode_index];
    if (!S_ISDIR(directory->mode)) {
        return -ENOTDIR;
    }

    for (size_t i = 0; i < directory->entry_count; i++) {
        if (callback(directory->entries[i].name, context) != 0) {
            return 0;
        }
    }

    return 0;
}

int fs_unlink(MemFs* fs, const char* path) {
    if (fs == NULL || path == NULL) {
        return -EINVAL;
    }

    char* parent_path;
    char* name;
    int result = fs_split_path(path, &parent_path, &name);
    if (result != 0) {
        return result;
    }

    uint32_t parent_index;
    result = fs_resolve_path(fs, parent_path, &parent_index);
    free(parent_path);
    if (result != 0) {
        free(name);
        return result;
    }

    uint32_t target_index;
    result = fs_find_dir_entry(fs, parent_index, name, &target_index);
    free(name);
    if (result != 0) {
        return result;
    }

    Inode* parent = &fs->inodes[parent_index];
    Inode* target = &fs->inodes[target_index];
    if (S_ISDIR(target->mode)) {
        return -EISDIR;
    }
    if (!S_ISREG(target->mode)) {
        return -ENOENT;
    }

    result = fs_remove_dir_entry(fs, parent_index, target_index);
    if (result != 0) {
        return result;
    }

    free(target->data);
    target->data = NULL;
    target->size = 0;
    parent->mtime = fs_current_time();
    parent->ctime = fs_current_time();
    return 0;
}

int fs_rmdir(MemFs* fs, const char* path) {
    if (fs == NULL || path == NULL) {
        return -EINVAL;
    }

    char* parent_path;
    char* name;
    int result = fs_split_path(path, &parent_path, &name);
    if (result != 0) {
        return result;
    }

    uint32_t parent_index;
    result = fs_resolve_path(fs, parent_path, &parent_index);
    free(parent_path);
    if (result != 0) {
        free(name);
        return result;
    }

    uint32_t target_index;
    result = fs_find_dir_entry(fs, parent_index, name, &target_index);
    free(name);
    if (result != 0) {
        return result;
    }

    Inode* parent = &fs->inodes[parent_index];
    Inode* target = &fs->inodes[target_index];
    if (!S_ISDIR(target->mode)) {
        return -ENOTDIR;
    }
    if (target->entry_count != 0) {
        return -ENOTEMPTY;
    }

    result = fs_remove_dir_entry(fs, parent_index, target_index);
    if (result != 0) {
        return result;
    }

    parent->mtime = fs_current_time();
    parent->ctime = fs_current_time();
    free(target->entries);
    target->entries = NULL;
    target->entry_count = 0;
    target->entry_capacity = 0;
    return 0;
}

int fs_utimens(MemFs* fs, const char* path, const struct timespec tv[2]) {
    if (fs == NULL || path == NULL || tv == NULL) {
        return -EINVAL;
    }

    uint32_t inode_index;
    int result = fs_resolve_path(fs, path, &inode_index);
    if (result != 0) {
        return result;
    }

    Inode* inode = &fs->inodes[inode_index];
    if (tv[0].tv_nsec != UTIME_OMIT) {
        inode->atime = tv[0].tv_nsec == UTIME_NOW ? fs_current_time() : tv[0];
    }
    if (tv[1].tv_nsec != UTIME_OMIT) {
        inode->mtime = tv[1].tv_nsec == UTIME_NOW ? fs_current_time() : tv[1];
    }
    inode->ctime = fs_current_time();
    return 0;
}

int fs_rename(MemFs* fs, const char* from, const char* to) {
    if (fs == NULL || from == NULL || to == NULL) {
        return -EINVAL;
    }

    char* from_parent_path = NULL;
    char* from_name = NULL;
    char* to_parent_path = NULL;
    char* to_name = NULL;

    int result = fs_split_path(from, &from_parent_path, &from_name);
    if (result != 0) {
        goto cleanup;
    }

    result = fs_split_path(to, &to_parent_path, &to_name);
    if (result != 0) {
        goto cleanup;
    }

    uint32_t from_parent_index;
    uint32_t to_parent_index;

    result = fs_resolve_path(fs, from_parent_path, &from_parent_index);
    if (result != 0) {
        goto cleanup;
    }

    result = fs_resolve_path(fs, to_parent_path, &to_parent_index);
    if (result != 0) {
        goto cleanup;
    }

    Inode* from_parent = &fs->inodes[from_parent_index];
    Inode* to_parent = &fs->inodes[to_parent_index];

    if (!S_ISDIR(from_parent->mode) || !S_ISDIR(to_parent->mode)) {
        result = -ENOTDIR;
        goto cleanup;
    }

    uint32_t source_index;
    result = fs_find_dir_entry(fs, from_parent_index, from_name, &source_index);
    if (result != 0) {
        goto cleanup;
    }

    Inode* source = &fs->inodes[source_index];

    if (S_ISDIR(source->mode) && fs_directory_contains_inode(fs, source_index, to_parent_index)) {
        result = -EINVAL;
        goto cleanup;
    }

    uint32_t destination_index;
    result = fs_find_dir_entry(fs, to_parent_index, to_name, &destination_index);

    if (result == 0) {
        if (destination_index == source_index) {
            result = 0;
            goto cleanup;
        }

        Inode* destination = &fs->inodes[destination_index];

        if (S_ISDIR(source->mode) && !S_ISDIR(destination->mode)) {
            result = -ENOTDIR;
            goto cleanup;
        }

        if (!S_ISDIR(source->mode) && S_ISDIR(destination->mode)) {
            result = -EISDIR;
            goto cleanup;
        }

        if (S_ISDIR(destination->mode) && destination->entry_count != 0) {
            result = -ENOTEMPTY;
            goto cleanup;
        }

        result = fs_remove_dir_entry(fs, to_parent_index, destination_index);
        if (result != 0) {
            goto cleanup;
        }
    } else if (result != -ENOENT) {
        goto cleanup;
    }

    if (from_parent_index == to_parent_index) {
        DirectoryEntry* entry = fs_find_entry_by_inode(from_parent, source_index);

        char* new_name = strdup(to_name);
        if (new_name == NULL) {
            result = -ENOMEM;
            goto cleanup;
        }

        free(entry->name);
        entry->name = new_name;
    } else {
        result = fs_add_dir_entry(fs, to_parent_index, to_name, source_index);
        if (result != 0) {
            goto cleanup;
        }

        result = fs_remove_dir_entry(fs, from_parent_index, source_index);
        if (result != 0) {
            goto cleanup;
        }
    }

    source->ctime = fs_current_time();
    from_parent->mtime = fs_current_time();
    from_parent->ctime = fs_current_time();
    to_parent->mtime = fs_current_time();
    to_parent->ctime = fs_current_time();

    result = 0;

cleanup:
    free(from_parent_path);
    free(from_name);
    free(to_parent_path);
    free(to_name);
    return result;
}
