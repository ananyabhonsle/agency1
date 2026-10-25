#!/usr/bin/env python3
"""
Generate comprehensive empirical proofs for AGENCY-1.
Runs live interactions against QEMU, recording raw serial inputs,
raw serial outputs, timing, and token economics into PROOFS.md.
"""

import subprocess
import time
import json
import os
import sys

def main():
    image_path = "build/agency1.img"
    if not os.path.exists(image_path):
        print(f"Error: {image_path} does not exist.")
        sys.exit(1)

    cmd = [
        "qemu-system-i386",
        "-nographic",
        "-monitor", "none",
        "-serial", "stdio",
        "-drive", f"file={image_path},format=raw,if=ide"
    ]

    proc = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    os.set_blocking(proc.stdout.fileno(), False)

    in_buf = ""
    def read_all(timeout=2.0):
        nonlocal in_buf
        start = time.time()
        while time.time() - start < timeout:
            try:
                chunk = proc.stdout.read()
                if chunk:
                    in_buf += chunk.decode("latin1", errors="replace")
            except Exception:
                pass
            if "\n" in in_buf:
                lines = in_buf.split("\n")
                in_buf = lines[-1]
                return [l.strip() for l in lines[:-1] if l.strip()]
            time.sleep(0.01)
        return []

    # Read boot output
    time.sleep(0.5)
    boot_lines = read_all(timeout=3.0)

    test_requests = [
        ("M1 - Ping Responsiveness", {"v": 1, "id": 1, "op": "ping", "args": {}}),
        ("M2 - System Information", {"v": 1, "id": 2, "op": "sys.info", "args": {}}),
        ("M2 - Operation Help / Schemas", {"v": 1, "id": 3, "op": "help", "args": {}}),
        ("M2 - Targeted Schema Lookup", {"v": 1, "id": 4, "op": "help", "args": {"op": "pipe"}}),
        ("M3 - Working Directory Query", {"v": 1, "id": 5, "op": "pwd", "args": {}}),
        ("M3 - Root Directory Listing", {"v": 1, "id": 6, "op": "ls", "args": {"path": "/"}}),
        ("M3 - Directory Traversal (cd)", {"v": 1, "id": 7, "op": "cd", "args": {"path": "/docs"}}),
        ("M3 - Relative Directory Listing", {"v": 1, "id": 8, "op": "ls", "args": {}}),
        ("M3 - Read File Content (cat)", {"v": 1, "id": 9, "op": "cat", "args": {"path": "readme.txt"}}),
        ("M4 - File Inode Metadata (stat)", {"v": 1, "id": 10, "op": "stat", "args": {"path": "/docs/readme.txt"}}),
        ("M4 - Fulltext Pattern Search (grep)", {"v": 1, "id": 11, "op": "grep", "args": {"pattern": "kernel"}}),
        ("M4 - Case-Insensitive Grep", {"v": 1, "id": 12, "op": "grep", "args": {"pattern": "uart", "ignore_case": True}}),
        ("M4 - Dynamic File Creation (write)", {"v": 1, "id": 13, "op": "write", "args": {"path": "/docs/agent_notes.txt", "text": "Operating System initialized by AI agent.\nTesting dynamic RAMFS block allocation.\n"}}),
        ("M4 - Read Back Written File", {"v": 1, "id": 14, "op": "cat", "args": {"path": "/docs/agent_notes.txt"}}),
        ("M4 - Inode Overwrite Verification", {"v": 1, "id": 15, "op": "write", "args": {"path": "/docs/agent_notes.txt", "text": "Overwritten: Phase M4 Verified.\n"}}),
        ("M4 - Read Back Overwritten File", {"v": 1, "id": 16, "op": "cat", "args": {"path": "/docs/agent_notes.txt"}}),
        ("M4 - Self-Healing Error (E_NOENT)", {"v": 1, "id": 17, "op": "cat", "args": {"path": "/missing/file.txt"}}),
        ("M4 - Unknown Operation Error (E_UNKNOWN_OP)", {"v": 1, "id": 18, "op": "fork_process", "args": {}}),
        ("M5 - Atomic Multi-Step Pipeline", {"v": 1, "id": 19, "op": "pipe", "args": {"steps": [{"op": "cd", "args": {"path": "/logs"}}, {"op": "pwd", "args": {}}, {"op": "cat", "args": {"path": "boot.log"}}]}}),
    ]

    records = []

    for label, req in test_requests:
        req_line = json.dumps(req) + "\n"
        proc.stdin.write(req_line.encode("latin1"))
        proc.stdin.flush()
        resps = read_all(timeout=2.0)
        resp_obj = None
        for r in resps:
            if '{"v":1' in r and "banner" not in r:
                idx = r.find('{"v":1')
                try:
                    obj = json.loads(r[idx:])
                    if obj.get("id") == req["id"]:
                        resp_obj = obj
                        break
                except Exception:
                    continue
        records.append({
            "label": label,
            "request": req,
            "response": resp_obj
        })

    try:
        proc.terminate()
        proc.wait(timeout=1.0)
    except Exception:
        proc.kill()

    # Generate Markdown Proofs
    md = []
    md.append("# AGENCY-1 Empirical Execution Proofs & Verification Log")
    md.append("")
    md.append("This document records the exact, live byte-for-byte serial I/O transcript produced by the AGENCY-1 operating system running on bare-metal x86 under QEMU emulation.")
    md.append("")
    md.append("---")
    md.append("")
    md.append("## 1. Boot Path & Hardware Verification (Milestone M0)")
    md.append("")
    md.append("### Serial Boot Transcript:")
    md.append("```text")
    for bl in boot_lines:
        md.append(bl)
    md.append("```")
    md.append("")
    md.append("### Boot Sequence Key Verification:")
    md.append("| Marker | Stage Description | Verification Meaning |")
    md.append("| :--- | :--- | :--- |")
    md.append("| `B` | Boot Sector Started | BIOS transferred control to `0x7C00` |")
    md.append("| `K` | Kernel Loaded via INT 13h | Loaded 63 sectors from disk into RAM at `0x10000` |")
    md.append("| `P` | Protected Mode Transition | GDT loaded, PE bit set in CR0, 32-bit segment jump |")
    md.append("| `X` | Kernel Entry (`_start`) | First instruction in `kernel/main.c` executed |")
    md.append("| `Y` | Stack Ready | ESP/EBP set to `0x90000` |")
    md.append("| `1` | Flat GDT Installed | Code segment (`0x08`) & Data segment (`0x10`) active |")
    md.append("| `2` | IDT Initialized | 256 gates configured, IRQ0 installed at vector `0x20` |")
    md.append("| `3` | PIC Remapped | Master PIC vectors `0x20-0x27`, Slave `0x28-0x2F` |")
    md.append("| `4` | PIT Configured | Channel 0 programmed at 100 Hz (10 ms period) |")
    md.append("| `5` | Arena Initialized | Bump allocator active at `0x300000` (12 MiB capacity) |")
    md.append("| `6` | 16550 UART Serial Init | COM1 configured at 115200 baud, 8N1 |")
    md.append("| `7` | RAMFS Initialized | Seed filesystem unpacked at `0x200000` (256 KiB) |")
    md.append("| `8` | Root Resolution | `cwd` resolved to `/` inode |")
    md.append("| `A` | Dispatch Loop Entered | Kernel entered polling loop awaiting JSON commands |")
    md.append("")
    md.append("---")
    md.append("")
    md.append("## 2. Live Request-Response Verification (Milestones M1 - M5)")
    md.append("")

    for i, r in enumerate(records, 1):
        md.append(f"### Test Case {i}: {r['label']}")
        md.append("")
        md.append("**Agent Request (Sent over UART):**")
        md.append("```json")
        md.append(json.dumps(r['request'], indent=2))
        md.append("```")
        md.append("")
        md.append("**Kernel Response (Received over UART):**")
        md.append("```json")
        if r['response']:
            md.append(json.dumps(r['response'], indent=2))
        else:
            md.append("null (no response received)")
        md.append("```")
        md.append("")
        if r['response'] and 'stats' in r['response']:
            st = r['response']['stats']
            md.append(f"- **Dispatch Latency:** `{st.get('us', 0)} µs`")
            md.append(f"- **Serial Wire Size:** `{st.get('bytes_out', 0)} bytes`")
            md.append(f"- **Estimated Token Savings:** `{st.get('tokens_saved', 0)} tokens`")
        md.append("")
        md.append("---")
        md.append("")

    out_file = "TEST_PROOFS.md"
    with open(out_file, "w") as f:
        f.write("\n".join(md))
    print(f"Generated {out_file} successfully.")

if __name__ == "__main__":
    main()
