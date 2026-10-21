# mem-fs

A mountable, in-memory filesystem loosely based on ext2 written in C.

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
