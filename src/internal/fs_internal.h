#ifndef MEM_FS_INTERNAL_H
#define MEM_FS_INTERNAL_H

#include "mem-fs/fs.h"

#include <stdint.h>

typedef struct {
    char* name;
    uint32_t inode_index;
} DirectoryEntry;

typedef struct {
    uint32_t inode_number;

    mode_t mode;
    off_t size;

    struct timespec atime;
    struct timespec mtime;
    struct timespec ctime;

    union {
        char* data;

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

struct timespec fs_current_time(void);

int fs_split_path(const char* path, char** parent_path, char** name);
int fs_find_dir_entry(MemFs* fs, uint32_t directory_index, const char* name,
                      uint32_t* out_inode_index);
int fs_resolve_path(MemFs* fs, const char* path, uint32_t* out_inode_index);

int fs_create_inode(MemFs* fs, mode_t mode, uint32_t* out_inode_index);
int fs_add_dir_entry(MemFs* fs, uint32_t directory_index, const char* name, uint32_t inode_index);
int fs_remove_dir_entry(MemFs* fs, uint32_t directory_index, uint32_t inode_index);
DirectoryEntry* fs_find_entry_by_inode(Inode* directory, uint32_t inode_index);
bool fs_directory_contains_inode(MemFs* fs, uint32_t directory_index, uint32_t wanted_index);
#endif
