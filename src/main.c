#include "mem-fs/fs.h"
#include "mem-fs/fuse_ops.h"

int main(int argc, char* argv[]) {
    MemFs* fs = fs_create();

    if (fs == NULL) {
        return -1;
    }

    int result = fuse_main(argc, argv, &memfs_operations, fs);

    fs_destroy(fs);
    return result;
}
