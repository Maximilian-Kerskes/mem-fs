#include "mem-fs/fuse_ops.h"

int main(int argc, char* argv[]) {
    return fuse_main(argc, argv, &memfs_operations, NULL);
}
