# assoofs
A simple, kernel-space, on-disk filesystem using libfs and modules

### to compile
As root:
- make clean && make
- insmod assoofs.ko && mount -t assoofs /dev/loop0 /mnt/assoofs

### to test / use (supported operations)
- mkdir
- touch
- echo x (int) > file
