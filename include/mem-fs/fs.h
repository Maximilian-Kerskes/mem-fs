#ifndef MEM_FS_FS_H
#define MEM_FS_FS_H

#include <stddef.h>
#include <sys/stat.h>
#include <stdio.h>
#include <time.h>
typedef struct MemFs MemFs;

MemFs* fs_create(void);
void fs_destroy(MemFs* fs);

int fs_getattr(MemFs* fs, const char* path, struct stat* stbuf);

typedef int (*FsDirEntryCallback)(const char* name, void* context);
int fs_readdir(MemFs* fs, const char* path, FsDirEntryCallback callback, void* context);

int fs_create_file(MemFs* fs, const char* path);
int fs_create_dir(MemFs* fs, const char* path);
int fs_readdir(MemFs* fs, const char* path, FsDirEntryCallback callback, void* context);
ssize_t fs_read(MemFs* fs, const char* path, char* buffer, size_t size, off_t offset);
ssize_t fs_write(MemFs* fs, const char* path, const char* buffer, size_t size, off_t offset);
int fs_truncate(MemFs* fs, const char* path, off_t size);
int fs_unlink(MemFs* fs, const char* path);
int fs_rmdir(MemFs* fs, const char* path);
int fs_utimens(MemFs* fs, const char* path, const struct timespec tv[2]);
int fs_rename(MemFs* fs, const char* from, const char* to);
int fs_rename(MemFs* fs, const char* from, const char* to);

#endif // MEM_FS_FS_H
