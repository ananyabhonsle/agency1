README.md
# AGENCY-1

![Freestanding](https://img.shields.io/badge/C-freestanding-168aad)
![NASM](https://img.shields.io/badge/NASM-x86__32-orange)
![QEMU](https://img.shields.io/badge/QEMU-i386-red)
![Status](https://img.shields.io/badge/Status-Phase%20M0%2BM1-yellow)

*An agent-native operating system where system calls are JSON tool calls
--- eliminating the translation layer between LLM agent frameworks and
the kernel ABI.*

------------------------------------------------------------------------

## Current Status

-   **Phase 1 / M0 (Complete):** 512-byte BIOS boot sector (CHS disk
    read, A20 enable, GDT load), protected-mode transition, 16550 UART
    driver (115200 8N1, polled), GDT/IDT/PIC/PIT init, bump allocator,
    runtime RAMFS builder. Kernel boots and prints banner:

    ``` json
    {"v":1,"ok":true,"data":{"banner":"AGENCY-1","version":"1.0"}}
    ```

-   **Phase 2 / M1 (Complete):** Hand-rolled JSON parser (no libc,
    freestanding C), operation registry (single source of truth for
    kernel dispatch + model tool schemas), `ping` op verified: receives
    JSON request → responds `{"pong":true}`. Self-describing errors with
    `suggested_next_call` recovery plans. `tokens_saved` metric computed
    per response.

-   **Phase 3 / M2--M4 (In Progress):** `sys.info`, `help`, `pwd`, `cd`,
    `ls`, `cat`, `grep`, `stat`, `write` --- code complete in `ops.c`,
    pending integration testing. `ramfs` with 13 inodes seeded at build
    time.

-   **Next: Phase 4 / M5:** Pipeline primitive (pipe op with $prev substitution) collapses 4-step agent rituals into 1 round trip, projecting ~62% token savings. Then empirical tokens_saved measurement vs POSIX shell and a Linux daemon variant.

------------------------------------------------------------------------

## Boot Path Verification

*Debug prints in `serial.log`*

  Stage               Code            Meaning
  ------------------- --------------- ------------------------------------------
  Boot sector start   `B`             BIOS handed control to boot sector
  Kernel loaded       `K`             Disk read succeeded, kernel at `0x10000`
  Protected mode      `P`             GDT loaded, segments set, 32-bit active
  `_start` entry      `X`             Kernel's first instruction executed
  Stack ready         `Y`             ESP/EBP set to `0x90000`
  GDT init            `1`             Flat GDT (null + code + data) loaded
  IDT init            `2`             IDT installed (exception stubs)
  PIC remap           `3`             PIC remapped to vectors `0x20–0x2F`
  PIT init            `a/b/c/d`       Channel 0 programmed at 100 Hz
  Arena init          `5`             Bump allocator at `0x300000` (12 MiB)
  Serial init         `6`             16550 UART at 115200 8N1
  RAMFS init          `7`             Filesystem built at `0x200000`
  Root resolve        `8`             `cwd` set to `/`
  Banner sent         `{"v":1,...}`   Dispatch loop entered

The full sequence `BKPX...` plus the banner JSON appears in
`serial.log`, confirming that the entire boot path executes correctly.

------------------------------------------------------------------------

## Tech Stack

-   **Language:** Freestanding C (`gcc -m32`), NASM x86 assembly
-   **Platform:** QEMU i386 (16 MiB RAM, BIOS boot)
-   **ABI:** JSON-over-serial (16550 UART, 115200 8N1)
-   **Build:** `gcc` + `ld` + `objcopy` + `nasm` + `python3`
-   **Size budget:** \~1500 lines of C/ASM (under 3000-line budget)

------------------------------------------------------------------------

## Build & Run

### Install toolchain (Debian/Ubuntu)

``` bash
sudo apt-get install -y gcc-multilib nasm qemu-system-x86 python3
```

### Build the image

``` bash
make
```

### Boot interactively in QEMU

``` bash
make boot
```

### Run acceptance tests

``` bash
make test
```

### Print kernel line count (budget: 3000)

``` bash
make stats
```

------------------------------------------------------------------------

## Token Economics

*Tokens are the new bytes. Classic OS design optimizes for CPU cycles
and memory bytes. AGENCY-1 treats token cost as a first-class kernel
design metric.*

Every syscall response includes a `tokens_saved` field computed against
the human-shell equivalent:

``` json
{
  "v": 1,
  "id": 17,
  "ok": true,
  "data": {
    "matches": ["..."],
    "match_count": 1,
    "files_searched": 3
  },
  "stats": {
    "us": 312,
    "bytes_out": 204,
    "tokens_saved": 41
  }
}
```

Round trips, not bytes, are the expensive unit of agentic computing ---
each round trip re-enters the model with its full context window. The
pipeline primitive (M5, planned for v1.1) collapses 4 round trips into
1, projecting \~62% token savings on a worked example.

------------------------------------------------------------------------

## Design Rationale: Interface Locus

The core contribution is not replacing POSIX, but redefining the
**interface locus**. In existing stacks, the model speaks tool calls and
the machine speaks POSIX, requiring a lossy translation layer. AGENCY-1
deletes this layer by making the syscall interface and the model
interface the same artifact: one registry simultaneously generates the
kernel dispatch table and the tool schemas the model consumes. Kernel
and model can no longer drift apart, because they are compiled from the
same table.

The bare-metal constraint is intentional --- it proves the ABI needs
nothing from the legacy stack: no process model, no libc, no drivers
beyond a UART.

------------------------------------------------------------------------

## Repository Layout

```text
agency1/
├── boot/
│   └── boot.asm          # 512-byte BIOS boot sector
├── kernel/
│   ├── main.c            # _start entry + dispatch loop
│   ├── serial.c          # 16550 UART driver
│   ├── gdt.c             # GDT/IDT/PIC/PIT init
│   ├── alloc.c           # Bump allocator
│   ├── json.c            # Hand-rolled JSON parser
│   ├── reply.c           # Response envelope + tokens_saved
│   ├── registry.c        # Op table (single source of truth)
│   ├── ramfs.c           # Filesystem + path resolver
│   ├── ops.c             # All 11 op handlers
│   └── ramfs_data.c      # Runtime ramfs builder
├── host/
│   └── genfs.py          # Seed filesystem generator
├── tests/
│   └── accept.py         # Acceptance harness
├── include/
│   └── agency1.h         # Central header
├── CONTRACT.md           # Frozen ABI contract
├── linker.ld             # Linker script
├── Makefile              # Build system
└── README.md
