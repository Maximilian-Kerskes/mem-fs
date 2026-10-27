# mem-fs

A mountable, in-memory filesystem loosely based on ext2 written in C.

## Current Status

### Currently Supported Fuse Operations:

- `getattr`
- `readdir`
- `open`
- `read`
- `write`
- `create`
- `mkdir`
- `truncate`
- `unlink`
- `rmdir`
- `utimens`
- `rename`

### TODO Features

- Replace dynamically alloced bufers with fixed-size memory blocks.
- Implement block alloc and dealloc.
- Update ops(read, write, truncate) to support blocks.
- Tracking and reusing free inodes.

## Quickstart

### Dependencies

```bash
sudo apt-get install libfuse3-dev fuse3 pkg-config
```

### Building and Testing:

```bash
make
./build/mem-fs -f ./tests/resources/mem-fs-mount
```
