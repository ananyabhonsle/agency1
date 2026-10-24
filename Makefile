# AGENCY-1 Makefile
CC      = gcc
LD      = ld
NASM    = nasm
QEMU    = qemu-system-i386
PYTHON  = python3
OBJCOPY = objcopy

CFLAGS  = -m32 -ffreestanding -fno-pic -fno-stack-protector -fno-builtin -nostdlib -Wall -Wextra -O2 -Iinclude
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

KERNEL_SRCS = kernel/main.c kernel/serial.c kernel/gdt.c kernel/json.c kernel/registry.c kernel/ramfs.c kernel/ops.c kernel/reply.c kernel/alloc.c kernel/ramfs_data.c
KERNEL_OBJS = $(KERNEL_SRCS:.c=.o)

BOOT_BIN    = boot/boot.bin
IMAGE       = build/agency1.img

.PHONY: all boot test stats clean

all: $(IMAGE)

boot/boot.bin: boot/boot.asm
	$(NASM) -f bin -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/kernel.elf: $(KERNEL_OBJS) linker.ld
	@mkdir -p build
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS)

build/kernel.bin: build/kernel.elf
	$(OBJCOPY) -O binary build/kernel.elf build/kernel.bin

 $(IMAGE): build/kernel.bin boot/boot.bin
	@mkdir -p build
	cp boot/boot.bin $@
	cat build/kernel.bin >> $@
	# Pad image to 32 KiB (64 sectors) so the boot sector's read doesn't fail
	@SZ=$$(stat -c%s $@); \
	if [ $$SZ -lt 32768 ]; then \
	  dd if=/dev/zero bs=1 count=$$((32768 - SZ)) >> $@ 2>/dev/null; \
	fi
	@echo "Built $(IMAGE) ($$(stat -c%s $@) bytes)"

boot: $(IMAGE)
	$(QEMU) -nographic -serial mon:stdio -drive file=$(IMAGE),format=raw,if=ide

test: $(IMAGE)
	$(PYTHON) tests/accept.py

stats:
	@echo "Kernel C/ASM line count:"
	@wc -l $(KERNEL_SRCS) boot/boot.asm | tail -1

clean:
	rm -f $(KERNEL_OBJS) build/kernel.elf build/kernel.bin $(IMAGE) serial.log
	rm -rf build
