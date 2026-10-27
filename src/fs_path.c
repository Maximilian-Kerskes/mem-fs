#include "internal/fs_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

int fs_split_path(const char* path, char** parent_path, char** name) {
    if (path == NULL || parent_path == NULL || name == NULL) {
        return -EINVAL;
    }

    *parent_path = NULL;
    *name = NULL;

    size_t len = strlen(path);

    if (len == 0 || path[0] != '/') {
        return -EINVAL;
    }

    if (strcmp(path, "/") == 0) {
        return -EEXIST;
    }

    while (len > 1 && path[len - 1] == '/') {
        len--;
    }

    const char* last_slash = memrchr(path, '/', len);
    if (last_slash == NULL) {
        return -EINVAL;
    }

    size_t name_len = len - (size_t)(last_slash - path) - 1;
    if (name_len == 0) {
        return -EINVAL;
    }

    *name = strndup(last_slash + 1, name_len);
    if (*name == NULL) {
        return -ENOMEM;
    }

    if (last_slash == path) {
        *parent_path = strdup("/");
    } else {
        *parent_path = strndup(path, (size_t)(last_slash - path));
    }

    if (*parent_path == NULL) {
        free(*name);
        *name = NULL;
        return -ENOMEM;
    }

    return 0;
}

int fs_find_dir_entry(MemFs* fs, uint32_t directory_index, const char* name,
                      uint32_t* out_inode_index) {
    if (fs == NULL || name == NULL || out_inode_index == NULL) {
        return -EINVAL;
    }

    if (directory_index >= fs->inode_count) {
        return -ENOENT;
    }

    Inode* directory = &fs->inodes[directory_index];
    if (!S_ISDIR(directory->mode)) {
        return -ENOTDIR;
    }

    for (size_t i = 0; i < directory->entry_count; i++) {
        DirectoryEntry* entry = &directory->entries[i];

        if (strcmp(entry->name, name) == 0) {
            *out_inode_index = entry->inode_index;
            return 0;
        }
    }

    return -ENOENT;
}

int fs_resolve_path(MemFs* fs, const char* path, uint32_t* out_inode_index) {
    if (fs == NULL || path == NULL || out_inode_index == NULL) {
        return -EINVAL;
    }

    if (path[0] != '/') {
        return -EINVAL;
    }

    if (strcmp(path, "/") == 0) {
        *out_inode_index = fs->root_inode;
        return 0;
    }

    uint32_t current_index = fs->root_inode;
    const char* component = path + 1;

    while (*component != '\0') {
        const char* slash = strchr(component, '/');
        size_t component_len = slash ? (size_t)(slash - component) : strlen(component);

        if (component_len == 0) {
            component++;
            continue;
        }

        char* name = strndup(component, component_len);
        if (name == NULL) {
            return -ENOMEM;
        }

        uint32_t next_index;
        int result = fs_find_dir_entry(fs, current_index, name, &next_index);
        free(name);

        if (result != 0) {
            return result;
        }

        current_index = next_index;
        component += component_len;

        if (*component == '/') {
            component++;
        }

        if (*component != '\0' && !S_ISDIR(fs->inodes[current_index].mode)) {
            return -ENOTDIR;
        }
    }

    *out_inode_index = current_index;
    return 0;
}
