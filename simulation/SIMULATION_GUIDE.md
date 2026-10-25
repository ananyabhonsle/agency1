# AGENCY-1 OS Prototype & Simulation: Presentation & Demo Guide

This guide is designed for your project presentation and viva evaluation. It cuts out the low-level x86 hardware/assembly complexity and focuses strictly on **operating system concepts**, **architecture**, and a **flawless live demonstration**.

---

## 1. The 30-Second Elevator Pitch (What to Say First)

> *"Our project is **AGENCY-1**, a prototype and simulation of an **Agent-Native Operating System**.*  
> *Traditional operating systems like Linux or Windows expose POSIX system calls and text shells designed for humans. AGENCY-1 explores a new OS paradigm where the kernel's native interface is **structured, type-safe JSON-RPC** over standard communication channels, specifically optimized for autonomous software agents and automated pipelines.*  
> *In this prototype, we simulate the 4 fundamental pillars of an operating system: **an in-memory filesystem (RAMFS) with an inode table**, **an arena memory manager**, **a system call dispatch loop**, and **an atomic multi-step execution pipeline**."*

---

## 2. Core OS Architecture Components (To Explain on the Board/Slides)

### A. In-Memory File System (RAMFS) with Inode Table
- **Data Structures:** 
  - `struct inode`: Stores `id`, `type` (`N_DIR` or `N_FILE`), `name` (up to 64 chars), `parent_id` (hierarchical link), `offset` in data pool, and `bytes`.
  - Contiguous data pool (`g_ramfs_content`, 256 KiB) holding file contents.
- **Path Resolution (`ramfs_resolve`):** Tokenizes paths (e.g., `/docs/readme.txt`), walks the directory tree using parent-child inode relationships, and handles relative paths (`.`, `..`, and current working directory `cwd`).
- **File Operations:**
  - `ls`: Lists directory entries, showing subdirectories first, then regular files.
  - `cat`: Resolves inode, reads contents from data pool, and escapes control characters.
  - `write`: Supports both file creation (allocating a new inode and data block) and overwriting existing files.
  - `grep`: Line-by-line substring search across directory trees.

### B. Memory Management (Arena / Bump Allocator)
- **Concept:** Rather than standard fragmented heap allocation (`malloc`/`free`), the OS maintains a dedicated 4 MiB memory arena (`g_arena_storage`).
- **Mechanism:** Allocation uses an 8-byte aligned bump pointer (`arena_alloc`), and the memory is safely reclaimed at request boundaries (`arena_reset`).
- **Telemetry:** Live tracking of `arena_used_bytes` and `ramfs_used` exposed via `sysinfo`.

### C. System Call Dispatch & Error Handling
- **Request Envelope:** Enforces version (`"v": 1`), transaction tracking (`"id": int`), operation name (`"op"`), and arguments (`"args"`).
- **Self-Describing Error Model:** If a command fails (e.g., `E_NOENT`, `E_NOTDIR`, `E_ARG_MISSING`), the OS returns not only an error code and explanation, but a `suggested_next_call` to guide the caller toward recovery.

### D. Multi-Step Execution Pipeline (`pipe`)
- **Concept:** Analogous to UNIX pipes (`cat file | grep pattern`), but operates over structured JSON objects.
- **Atomic Batching:** Executes up to 8 operations in a single atomic transaction without client round-trips.
- **Output Chaining (`$prev`):** Automatically injects the output of step $N$ into the arguments of step $N+1$.

---

## 3. How to Run the Demo Live (Pick Any of These)

### Option 1: Interactive Menu-Driven Mode (Direct D: Drive Access)
Simply double-click **`run_menu.bat`** (or run `.\agency_sim.exe`).  
You are presented with a clean, numbered menu operating directly on your **real D: drive**:
```text
========================================================================
     AGENCY-1: AGENT-NATIVE OPERATING SYSTEM (LIVE HOST D: DRIVE)       
========================================================================
  [Live Path] Current Directory: D:/TYstuff/OS/fastlearner
  [Hardware]  Process Uptime:    25 ms
------------------------------------------------------------------------
  [1]  System Telemetry & Host Status (sysinfo)
  [2]  List Directory Contents on Disk (ls)
  [3]  Read Real File Contents on Disk (cat)
  [4]  Create / Write Real File on Disk (write)
  [5]  Search Text Across Real Files on Disk (grep)
  [6]  Inspect Real File / Inode Metadata on Disk (stat)
  [7]  Change Working Directory on Disk (cd)
  [8]  Execute Atomic Multi-Step Pipeline (pipe)
  [9]  Kernel Health Check (ping)
  [10] Run 17-Test Automated Verification Suite
  [11] Run 5-Step Guided Demonstration on Real Files
  [12] Execute Raw JSON-RPC Syscall (Advanced)
  [13] Switch to Interactive Command-Line Shell (CLI)
  [0]  Exit Simulation
========================================================================
Select an option [0-13]: 
```
*You can now type real Windows paths like `D:\TYstuff\OS\fastlearner\AGENCY-1-Handoff-Documentation.pdf`!*

---

### Option 2: Automated 5-Step Live Walkthrough
Double-click **`run_demo.bat`** or run:
```powershell
cd D:\TYstuff\OS\fastlearner\agency1\simulation
.\agency_sim.exe --demo
```
**What happens:** It executes 5 core operations sequentially on screen.

---

### Option 3: Automated Test Suite (100% Pass Rate)
Run the test runner binary to show the evaluator all 17 tests passing with their raw buffer inputs and outputs:
```powershell
cd D:\TYstuff\OS\fastlearner\agency1\simulation
.\test_runner.exe
```
**Output:**
```text
========================================================================
  AGENCY-1: Automated Verification Test Suite (Host Disk I/O)
  Validating Real File I/O, Inodes, Directory Walking, Syscalls & Pipe
========================================================================

  [PASS] Test 01: ping health check
         REQ: {"v":1,"id":1,"op":"ping","args":{}}
         REP: {"v":1,"id":1,"ok":true,"data":{"pong":true},"stats":{"us":12,"bytes_out":13,"tokens_saved":12}}

  [PASS] Test 02: sys.info telemetry
         REQ: {"v":1,"id":2,"op":"sys.info","args":{}}
         REP: {"v":1,"id":2,"ok":true,"data":{"version":"1.0 (Live Host Filesystem)","platform":"Windows x86_64 (Real Host Filesystem)","uptime_ms":0,"arena_used":456,"cwd":"D:/TYstuff/OS/fastlearner","ops":["ping","help","sys.info","pwd","cd","ls","cat","grep","stat","write","pipe"]},"stats":{"us":12,"bytes_out":240,"tokens_saved":52}}

  ...

========================================================================
  TEST VERIFICATION: 17 / 17 PASSED (100% VERIFIED)
========================================================================
```

---

## 4. Viva / Examiner Q&A Cheat Sheet

| Question | Your Answer |
| :--- | :--- |
| **Q: Why is this written in pure C without external libraries?** | *"Operating system kernels cannot depend on userspace third-party libraries. Writing the RAMFS, arena allocator, and JSON parser in pure C demonstrates standard OS primitives from first principles."* |
| **Q: How does the file system represent directories?** | *"Like UNIX inodes, our `struct inode` has a `type` field (`N_DIR` vs `N_FILE`) and a `parent_id`. A directory is just an inode whose children reference its ID as their `parent_id`."* |
| **Q: What is the purpose of the Memory Arena?** | *"Standard `malloc`/`free` leads to heap fragmentation over time. The arena bump-allocator pre-allocates a contiguous pool and resets pointers at request boundaries, guaranteeing $O(1)$ allocation and zero memory leaks."* |
| **Q: How does the multi-step `pipe` feature work?** | *"It allows multiple OS commands to execute sequentially inside the kernel in a single round-trip. The output of each step is captured and can be substituted into the next step using the `$prev` variable."* |
| **Q: How is error recovery handled?** | *"Every error returns a structured code (`E_NOENT`, `E_NOTDIR`, `E_ARG_MISSING`) along with a `suggested_next_call` indicating the next logical system call to recover from the error."* |
| **Q: How is `tokens_saved` calculated?** | *"Traditional OS kernels optimize for CPU cycles and I/O time, but an Agent-Native OS optimizes for LLM context window cost. The kernel subtracts its compact JSON byte size from the equivalent verbose human terminal shell output (`(human_equiv_bytes - bytes_out) / 4`), assuming ~4 bytes per LLM token."* |
