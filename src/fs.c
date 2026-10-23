#include "mem-fs/fs.h"
#include <asm-generic/errno-base.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct {
    char* name;
    uint32_t inode_index;
} DirectoryEntry;

typedef struct {
    uint32_t inode_number;

    mode_t mode;
    off_t size;

    union {
        struct {
            char* data;
        };

        struct {
            DirectoryEntry* entries;
            size_t entry_count;
            size_t entry_capacity;
        };
    };
} Inode;

struct MemFs {
    Inode* inodes;
    size_t inode_count;
    size_t inode_capacity;

    size_t root_inode;
};

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

    root->inode_number = 1;
    root->mode = S_IFDIR | 0755;
    root->size = 0;

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

static int split_path(const char* path, char** parent_path, char** name) {
    if (path == NULL || parent_path == NULL || name == NULL) {
        return -1;
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

static int find_dir_entry(MemFs* fs, uint32_t directory_index, const char* name,
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

static int resolve_path(MemFs* fs, const char* path, uint32_t* out_inode_index) {
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

        int result = find_dir_entry(fs, current_index, name, &next_index);

        free(name);

        if (result != 0) {
            return result;
        }

        current_index = next_index;

        component += component_len;

        if (*component == '/') {
            component++;
        }

        if (*component != '\0') {
            if (!S_ISDIR(fs->inodes[current_index].mode)) {
                return -ENOTDIR;
            }
        }
    }

    *out_inode_index = current_index;
    return 0;
}

static int create_inode(MemFs* fs, mode_t mode, uint32_t* out_inode_index) {
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

    fs->inodes[index] = (Inode){
        .inode_number = index + 1,
        .mode = mode,
        .size = 0,
    };

    fs->inode_count++;
    *out_inode_index = index;
    return 0;
}

static int add_dir_entry(MemFs* fs, uint32_t directory_index, const char* name,
                         uint32_t inode_index) {
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

    int result = find_dir_entry(fs, directory_index, name, &existing_index);

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

static int create_node(MemFs* fs, const char* path, mode_t mode) {
    char* parent_path;
    char* name;

    int result = split_path(path, &parent_path, &name);

    if (result != 0) {
        return result;
    }

    uint32_t parent_index;

    result = resolve_path(fs, parent_path, &parent_index);

    free(parent_path);

    if (result != 0) {
        free(name);
        return result;
    }

    if (!S_ISDIR(fs->inodes[parent_index].mode)) {
        free(name);
        return -ENOTDIR;
    }

    uint32_t existing_index;

    result = find_dir_entry(fs, parent_index, name, &existing_index);

    if (result == 0) {
        free(name);
        return -EEXIST;
    }

    if (result != -ENOENT) {
        free(name);
        return result;
    }

    uint32_t file_index;

    result = create_inode(fs, mode, &file_index);

    if (result != 0) {
        free(name);
        return result;
    }

    result = add_dir_entry(fs, parent_index, name, file_index);
    free(name);

    return result;
}

int fs_create_file(MemFs* fs, const char* path) { return create_node(fs, path, S_IFREG | 0644); }

int fs_create_dir(MemFs* fs, const char* path) { return create_node(fs, path, S_IFDIR | 0755); }

int fs_getattr(MemFs* fs, const char* path, struct stat* stbuf) {
    if (fs == NULL || path == NULL || stbuf == NULL) {
        return -EINVAL;
    }

    uint32_t inode_index;
    int result = resolve_path(fs, path, &inode_index);

    if (result != 0) {
        return result;
    }

    Inode* inode = &fs->inodes[inode_index];

    memset(stbuf, 0, sizeof(*stbuf));

    stbuf->st_ino = inode->inode_number;
    stbuf->st_mode = inode->mode;
    stbuf->st_size = inode->size;

    if (S_ISREG(inode->mode)) {
        stbuf->st_nlink = 1;
    } else if (S_ISDIR(inode->mode)) {
        stbuf->st_nlink = 2;
    }

    return 0;
}

int fs_readdir(MemFs* fs, const char* path, FsDirEntryCallback callback, void* context) {
    if (fs == NULL || path == NULL || callback == NULL) {
        return -EINVAL;
    }

    uint32_t inode_index;
    int result = resolve_path(fs, path, &inode_index);

    if (result != 0) {
        return result;
    }

    Inode* directory = &fs->inodes[inode_index];

    if (!S_ISDIR(directory->mode)) {
        return -ENOTDIR;
    }

    for (size_t i = 0; i < directory->entry_count; i++) {
        DirectoryEntry* entry = &directory->entries[i];

        if (callback(entry->name, context) != 0) {
            return 0;
        }
    }

    return 0;
}
