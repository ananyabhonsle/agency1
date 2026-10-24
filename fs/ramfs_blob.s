section .rodata
global ramfs_image
ramfs_image:
    incbin "fs/seed_image.bin"
