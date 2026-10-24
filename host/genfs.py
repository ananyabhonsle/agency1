#!/usr/bin/env python3
"""AGENCY-1 Seed Filesystem Generator
Generates fs/seed_image.bin containing inode table + content blocks.
"""

import argparse
import struct
import os

MAX_INODES = 64
INODE_SIZE = 48
BLOCK_SIZE = 512
RAMFS_SIZE = 256 * 1024


class Inode:
    DIR = 0
    FILE = 1

    def __init__(self, id, type, name, parent, offset=0, bytes_=0):
        self.id = id
        self.type = type
        self.name = name[:31]
        self.parent = parent
        self.offset = offset
        self.bytes = bytes_

    def pack(self):
        name_bytes = self.name.encode('utf-8')[:31]
        name_bytes += b'\0' * (32 - len(name_bytes))
        return struct.pack('<II32sIII',
                           self.id, self.type, name_bytes,
                           self.parent, self.offset, self.bytes)


SEED_FILES = {
    "/bin/agency1.txt": "AGENCY-1 v1.0\n",
    "/docs/readme.txt": "AGENCY-1 - Agent-Native Operating System\n\nA bare-metal kernel whose syscalls are JSON tool calls over serial.\n",
    "/docs/abi.txt": "AGENCY-1 ABI Reference\n\nRequest: {\"v\":1,\"id\":int,\"op\":\"string\",\"args\":{...}}\n",
    "/logs/boot.log": "[ 0.000] AGENCY-1 v1.0 booting\n[ 0.412] kernel panic in ahci_poll: timeout\n[ 0.907] kernel panic resumed: retrying\n[ 1.334] kernel panic cleared after reset\n",
    "/logs/serial.log": "[ 1.001] RX: ping\n[ 1.002] TX: pong\n",
    "/src/main.c": "// AGENCY-1 kernel main - dispatch loop, _start entry, banner\n",
    "/src/serial.c": "// AGENCY-1 16550 UART driver - polled, no interrupts\n",
    "/src/ramfs.c": "// AGENCY-1 in-memory filesystem - inode table + content blocks\n",
}


def build_seed():
    inodes = []
    content_blocks = bytearray()

    inodes.append(Inode(0, Inode.DIR, "/", 0))

    dirs = ["/bin", "/docs", "/logs", "/src"]
    for i, d in enumerate(dirs, start=1):
        inodes.append(Inode(i, Inode.DIR, d.lstrip('/'), 0))

    next_id = len(inodes)
    for path, content in SEED_FILES.items():
        parent_path = os.path.dirname(path)
        parent_id = 0
        for ino in inodes:
            if ino.type == Inode.DIR:
                if parent_path == "/":
                    parent_id = 0
                    break
                elif parent_path.lstrip('/') == ino.name:
                    parent_id = ino.id
                    break

        name = os.path.basename(path)
        content_bytes = content.encode('utf-8')
        offset = len(content_blocks)
        inodes.append(Inode(next_id, Inode.FILE, name, parent_id, offset, len(content_bytes)))
        content_blocks += content_bytes
        pad = (BLOCK_SIZE - (len(content_bytes) % BLOCK_SIZE)) % BLOCK_SIZE
        content_blocks += b'\0' * pad
        next_id += 1

    return inodes, bytes(content_blocks)


def write_image(inodes, content, output_path):
    inode_table = bytearray()
    for ino in inodes:
        inode_table += ino.pack()
    empty = Inode(0xFFFFFFFF, 0, "", 0, 0, 0).pack()
    while len(inode_table) < MAX_INODES * INODE_SIZE:
        inode_table += empty

    image = bytes(inode_table) + content
    if len(image) < RAMFS_SIZE:
        image += b'\0' * (RAMFS_SIZE - len(image))

    os.makedirs(os.path.dirname(output_path) or '.', exist_ok=True)
    with open(output_path, 'wb') as f:
        f.write(image)

    print(f"Generated {output_path} ({len(image)} bytes)")
    print(f"  Inodes: {len(inodes)} used / {MAX_INODES} max")
    print(f"  Content: {len(content)} bytes")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="fs/seed_image.bin")
    args = parser.parse_args()
    inodes, content = build_seed()
    write_image(inodes, content, args.output)
