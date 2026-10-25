#!/usr/bin/env python3
"""
AGENCY-1 Acceptance Test Harness
Verifies Milestones M0 through M5 end-to-end against bare-metal QEMU.
"""

import subprocess
import time
import json
import sys
import os

class Agency1Client:
    def __init__(self, image_path="build/agency1.img"):
        if not os.path.exists(image_path):
            raise FileNotFoundError(f"Image not found at {image_path}")

        cmd = [
            "qemu-system-i386",
            "-nographic",
            "-monitor", "none",
            "-serial", "stdio",
            "-drive", f"file={image_path},format=raw,if=ide"
        ]
        self.proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        os.set_blocking(self.proc.stdout.fileno(), False)
        self.in_buf = ""
        self.banner = None
        self._wait_for_banner()

    def _read_available(self):
        try:
            chunk = self.proc.stdout.read()
            if chunk:
                self.in_buf += chunk.decode("latin1", errors="replace")
        except Exception:
            pass

    def _wait_for_banner(self, timeout=3.0):
        start = time.time()
        while time.time() - start < timeout:
            self._read_available()
            if "\n" in self.in_buf:
                lines = self.in_buf.split("\n")
                self.in_buf = lines[-1]
                for line in lines[:-1]:
                    if '{"v":1' in line and "banner" in line:
                        idx = line.find('{"v":1')
                        try:
                            self.banner = json.loads(line[idx:])
                            return
                        except json.JSONDecodeError:
                            continue
            time.sleep(0.02)
        raise TimeoutError(f"Kernel banner not received within {timeout}s. Read: {repr(self.in_buf)}")

    def request(self, req_id, op, args=None, timeout=2.0):
        if args is None:
            args = {}
        payload = {"v": 1, "id": req_id, "op": op, "args": args}
        raw_msg = json.dumps(payload) + "\n"
        self.proc.stdin.write(raw_msg.encode("latin1"))
        self.proc.stdin.flush()

        start = time.time()
        while time.time() - start < timeout:
            self._read_available()
            if "\n" in self.in_buf:
                lines = self.in_buf.split("\n")
                self.in_buf = lines[-1]
                for line in lines[:-1]:
                    if '{"v":1' in line and "banner" not in line:
                        idx = line.find('{"v":1')
                        try:
                            resp = json.loads(line[idx:])
                            if resp.get("id") == req_id:
                                return resp
                        except json.JSONDecodeError:
                            continue
            time.sleep(0.01)
        raise TimeoutError(f"Timeout waiting for response to id={req_id} op={op}. Read: {repr(self.in_buf)}")

    def close(self):
        try:
            self.proc.terminate()
            self.proc.wait(timeout=1.0)
        except Exception:
            self.proc.kill()

def run_tests():
    print("=" * 65)
    print("        AGENCY-1 KERNEL ACCEPTANCE TEST SUITE")
    print("=" * 65)

    passed = 0
    total = 0

    def check(milestone, name, condition, details=""):
        nonlocal passed, total
        total += 1
        status = "PASS" if condition else "FAIL"
        if condition:
            passed += 1
        print(f"[{status}] {milestone:<4} | {name:<35} | {details}")
        if not condition:
            print(f"       >>> Assertion failed for {name}!")

    client = None
    try:
        client = Agency1Client()
        banner = client.banner

        # M0 - Boot and Banner
        check("M0", "Kernel Boot & Startup Banner",
              banner is not None and banner.get("ok") is True and
              banner.get("data", {}).get("banner") == "AGENCY-1",
              f"Version: {banner.get('data', {}).get('version')}")

        # M1 - Ping
        resp = client.request(1, "ping")
        check("M1", "Ping Request / Pong Response",
              resp.get("ok") is True and resp.get("data", {}).get("pong") is True and
              resp.get("id") == 1 and "stats" in resp,
              f"bytes_out={resp.get('stats', {}).get('bytes_out')}")

        # M2 - Sys.info & Help
        resp = client.request(2, "sys.info")
        data = resp.get("data", {})
        check("M2", "System Info Metadata & Uptime",
              resp.get("ok") is True and data.get("version") == "1.0" and
              data.get("uptime_ms", 0) > 0 and "mem" in data,
              f"Uptime: {data.get('uptime_ms')} ms, Mem: {data.get('mem')}")

        resp = client.request(3, "help")
        ops_list = resp.get("data", {}).get("ops", [])
        check("M2", "Operation Registry Schemas",
              resp.get("ok") is True and len(ops_list) >= 11,
              f"{len(ops_list)} ops registered in kernel")

        resp = client.request(4, "help", {"op": "cat"})
        check("M2", "Individual Op Schema Help",
              resp.get("ok") is True and resp.get("data", {}).get("name") == "cat",
              f"Schema: {resp.get('data', {}).get('schema')}")

        # M3 - Filesystem operations
        resp = client.request(5, "pwd")
        check("M3", "Current Working Directory (pwd)",
              resp.get("ok") is True and resp.get("data", {}).get("cwd") == "/",
              f"cwd={resp.get('data', {}).get('cwd')}")

        resp = client.request(6, "ls", {"path": "/"})
        entries = [e.get("name") for e in resp.get("data", [])]
        check("M3", "Root Directory Listing (ls /)",
              resp.get("ok") is True and "docs" in entries and "bin" in entries,
              f"Entries: {entries}")

        resp = client.request(7, "cd", {"path": "/docs"})
        check("M3", "Change Working Directory (cd /docs)",
              resp.get("ok") is True and resp.get("data", {}).get("cwd") == "/docs",
              f"cwd={resp.get('data', {}).get('cwd')}")

        resp = client.request(8, "cat", {"path": "readme.txt"})
        text = resp.get("data", {}).get("text", "")
        check("M3", "Read File Content (cat)",
              resp.get("ok") is True and "AGENCY-1" in text and
              resp.get("data", {}).get("truncated") is False,
              f"Read {resp.get('data', {}).get('bytes')} bytes")

        # M4 - Search, Metadata, Writing, Errors
        resp = client.request(9, "stat", {"path": "/docs/readme.txt"})
        check("M4", "Inode Metadata (stat)",
              resp.get("ok") is True and resp.get("data", {}).get("type") == "file" and
              resp.get("data", {}).get("bytes", 0) > 0,
              f"inode={resp.get('data', {}).get('inode')}, size={resp.get('data', {}).get('bytes')}")

        resp = client.request(10, "grep", {"pattern": "kernel"})
        check("M4", "Pattern Search Across Files (grep)",
              resp.get("ok") is True and resp.get("data", {}).get("match_count", 0) > 0,
              f"matches={resp.get('data', {}).get('match_count')}, searched={resp.get('data', {}).get('files_searched')}")

        resp = client.request(11, "write", {"path": "/docs/test.txt", "text": "Created by AI Agent\n"})
        check("M4", "Create New File (write)",
              resp.get("ok") is True and resp.get("data", {}).get("created") is True,
              f"Created /docs/test.txt ({resp.get('data', {}).get('bytes')} bytes)")

        resp = client.request(12, "cat", {"path": "/docs/test.txt"})
        check("M4", "Verify Written Content",
              resp.get("ok") is True and "Created by AI Agent" in resp.get("data", {}).get("text", ""),
              f"Content: {repr(resp.get('data', {}).get('text'))}")

        resp = client.request(13, "write", {"path": "/docs/test.txt", "text": "Overwritten by AI Agent\n"})
        check("M4", "Overwrite Existing File",
              resp.get("ok") is True and resp.get("data", {}).get("overwritten") is True,
              "File updated successfully")

        resp = client.request(14, "cat", {"path": "/nonexistent.log"})
        err = resp.get("error", {})
        check("M4", "Self-Describing Error (E_NOENT)",
              resp.get("ok") is False and err.get("code") == "E_NOENT" and
              "suggested_next_call" in err,
              f"suggested: {err.get('suggested_next_call')}")

        resp = client.request(15, "unknown_sys_op")
        check("M4", "Unknown Op Error (E_UNKNOWN_OP)",
              resp.get("ok") is False and resp.get("error", {}).get("code") == "E_UNKNOWN_OP",
              f"code: {resp.get('error', {}).get('code')}")

        # M5 - Multi-Operation Pipeline
        resp = client.request(16, "pipe", {
            "steps": [
                {"op": "cd", "args": {"path": "/logs"}},
                {"op": "pwd", "args": {}},
                {"op": "cat", "args": {"path": "boot.log"}}
            ]
        })
        pipe_data = resp.get("data", {})
        check("M5", "Multi-Step Pipeline (pipe op)",
              resp.get("ok") is True and pipe_data.get("steps_completed") == 3 and
              len(pipe_data.get("results", [])) == 3,
              f"Completed 3 steps. tokens_saved={resp.get('stats', {}).get('tokens_saved')}")

    except Exception as e:
        print(f"\n[FATAL] Test harness exception: {e}")
        import traceback
        traceback.print_exc()
        return 1
    finally:
        if client:
            client.close()

    print("=" * 65)
    print(f"FINAL SUMMARY: {passed} / {total} tests passed ({int(passed/total*100)}%)")
    print("=" * 65)
    return 0 if passed == total else 1

if __name__ == "__main__":
    sys.exit(run_tests())
