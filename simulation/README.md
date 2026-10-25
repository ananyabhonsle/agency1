# AGENCY-1 OS Prototype & Simulation Package (Host Filesystem & Live D: Drive Access)

This package implements the **AGENCY-1 Agent-Native Operating System** interface directly over your **real host filesystem (D: Drive)**.

---

## Key Features

- **Direct D: Drive Access:** You can inspect, list, read, write, and search real files on your Windows machine (`D:\TYstuff\OS\fastlearner\...`).
- **Zero External Dependencies:** Self-contained and portable.
- **Menu-Driven Interface:** Numbered menu covering all 11 OS operations and automated tests.
- **Dual Output:** Every operation displays both human-readable output and the exact JSON-RPC system call emitted by the kernel.

---

## Folder Contents

- **`agency_sim.c`**: Operating system kernel prototype with live disk I/O.
- **`agency_sim.exe`**: Native Windows executable.
- **`agency_sim`**: Pre-compiled 64-bit Linux executable.
- **`test_runner.c`**: Automated verification test harness.
- **`test_runner.exe`**: Native Windows test runner executable.
- **`run_menu.bat`**: Double-click to launch the main menu.
- **`run_demo.bat`**: Double-click to run the live 5-step automated walkthrough on real files.
- **`run_tests.bat`**: Double-click to run the 17-test verification suite.
- **`run_shell.bat`**: Double-click to launch the command-line prompt.
- **`SIMULATION_GUIDE.md`**: Complete presentation walkthrough and viva cheat sheet.

---

## Quick Start (How to Run)

### Method 1: Double-Click Menu Interface (Zero Typing)
- Double-click **`run_menu.bat`** (or **`agency_sim.exe`**) to open the full numbered menu interface:
  - `[1]` System Telemetry & Memory Allocation (`sysinfo`)
  - `[2]` List Directory Contents (`ls`)
  - `[3]` Read File Contents (`cat`)
  - `[4]` Create / Overwrite File in RAMFS (`write`)
  - `[5]` Search Keyword Across Files (`grep`)
  - `[6]` Inspect Inode & File Metadata (`stat`)
  - `[7]` Change Working Directory (`cd`)
  - `[8]` Execute Atomic Multi-Step Pipeline (`pipe`)
  - `[9]` Kernel Health Check (`ping`)
  - `[10]` Run 17-Test Automated Verification Suite
  - `[11]` Run 5-Step Guided Demonstration
  - `[0]` Exit Simulation

### Method 2: One-Click Shortcut Launchers
- **`run_menu.bat`**: Launches the main menu.
- **`run_demo.bat`**: Runs the live 5-step automated walkthrough.
- **`run_tests.bat`**: Runs the 17-test verification suite.
- **`run_shell.bat`**: Launches the direct command-line prompt.

### Method 3: From Terminal / PowerShell
```powershell
cd D:\TYstuff\OS\fastlearner\agency1\simulation

# 1. Run live automated demo
.\agency_sim.exe --demo

# 2. Run interactive OS shell
.\agency_sim.exe

# 3. Run automated test suite
.\test_runner.exe
```

### Method 4: Recompiling from Source in Front of Evaluators
If an evaluator asks you to compile it from scratch:
- **Windows (MinGW/GCC):**
  ```cmd
  gcc -O2 agency_sim.c -o agency_sim.exe
  gcc -O2 test_runner.c -o test_runner.exe
  ```
- **Linux / macOS:**
  ```bash
  gcc -O2 agency_sim.c -o agency_sim
  gcc -O2 test_runner.c -o test_runner
  ```

---

## Token Economics: How `tokens_saved` is Calculated

In traditional operating systems, kernels measure efficiency purely in **CPU execution cycles** and **I/O wait times**. In an **Agent-Native Operating System**, the primary operational cost and bottleneck for AI agents is **LLM context window token consumption**.

### 1. Human POSIX Shell vs. Agent-Native Kernel
When an AI agent runs shell commands on Linux or Windows, the terminal returns verbose, human-oriented formatting:
- File permissions and ownership flags (`-rwxr-xr-x 1 user staff 4096 ...`)
- ANSI color escape sequences (`\033[01;34m`)
- Shell prompts (`user@hostname:~/dir$ `)
- Redundant header lines, table formatting, and echoed inputs.

Every unnecessary character consumes LLM prompt tokens, slows down model inference, and increases API costs. In contrast, AGENCY-1 emits compact, structured JSON without visual decoration.

### 2. The Mathematical Formula
The kernel maintains a baseline table of how many bytes a standard human terminal session produces for each equivalent command (`human_equiv_bytes`):

$$\text{Tokens Saved} = \frac{\text{Human\_Equivalent\_Bytes} - \text{Kernel\_Bytes\_Out}}{4}$$

*(Standard LLM Tokenizer Rule: **~4 characters / bytes ≈ 1 LLM Token**)*

| Operation | Human Terminal Baseline (`bytes`) | Typical AGENCY-1 Output (`bytes`) | Estimated Tokens Saved |
| :--- | :--- | :--- | :--- |
| `sys.info` | ~450 bytes (from `uname`, `uptime`, `free`, `df`) | ~240 bytes (compact JSON) | **~52 tokens** |
| `stat` | ~256 bytes (from `stat` or `ls -l` with permissions) | ~150 bytes | **~26 tokens** |
| `ls` | ~512 bytes (directory listing with timestamps/modes) | ~200 bytes | **~78 tokens** |
| `pipe` | ~1024 bytes (multiple bash command round-trips) | ~300 bytes (atomic batch) | **~181 tokens** |

### 3. Concrete Example: `sysinfo`
1. **Human Equivalent:** A multi-command Linux terminal status output takes **~450 bytes**.
2. **AGENCY-1 Response:** Emits **240 bytes**.
3. **Bytes Saved:** $450 - 240 = 210 \text{ bytes}$.
4. **Tokens Saved:** $\frac{210}{4} = \mathbf{52 \text{ tokens}}$.

This metric is returned live under the `"stats"` envelope of every successful system call:
```json
"stats": {
  "us": 12,
  "bytes_out": 240,
  "tokens_saved": 52
}
```

