#ifndef MEM_FS_FUSE_OPS_H
#define MEM_FS_FUSE_OPS_H

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>

extern struct fuse_operations memfs_operations;

#endif
