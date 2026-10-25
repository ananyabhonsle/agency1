# AGENCY-1 ABI Specification Contract (v1.0 Frozen)

This document formalizes the Application Binary Interface (ABI) between AI agents and the AGENCY-1 bare-metal operating system kernel.

---

## 1. Physical & Transport Layer

- **Interface:** 16550 UART Serial Port (`COM1`, I/O port `0x3F8`)
- **Baud Rate:** 115200 bps
- **Data Bits:** 8
- **Parity:** None
- **Stop Bits:** 1 (`115200 8N1`)
- **Framing:** Plain text JSON Lines terminated with newline (`\n`, `0x0A`). Carriage returns (`\r`) are ignored.
- **Maximum Line Length:** 4096 bytes (`MAX_LINE_LEN`). Lines exceeding 4096 bytes trigger `E_TOO_LONG`.
- **Serial Timeout:** 250 ms (`SERIAL_TIMEOUT_MS`). Incomplete or stalled transmissions trigger `E_BUSY`.

---

## 2. Wire Protocol

All communication is strictly synchronous request-response. Every request issued by the agent receives exactly one corresponding response from the kernel.

### 2.1 Request Envelope

Every incoming request must be a JSON object containing four mandatory top-level fields:

```json
{
  "v": 1,
  "id": 42,
  "op": "string",
  "args": {}
}
```

| Field | Type | Required | Description |
| :--- | :--- | :--- | :--- |
| `v` | integer | Yes | Protocol version. Must be `1`. |
| `id` | integer | Yes | Correlation identifier chosen by the agent, returned in response. |
| `op` | string | Yes | Name of the kernel operation to execute. |
| `args` | object | Yes | Key-value arguments passed to the operation handler. |

---

### 2.2 Response Envelope

Every response emitted by the kernel conforms to one of two structures:

#### Success Response
```json
{
  "v": 1,
  "id": 42,
  "ok": true,
  "data": { ... },
  "stats": {
    "us": 120,
    "bytes_out": 98,
    "tokens_saved": 25
  }
}
```

#### Error Response
```json
{
  "v": 1,
  "id": 42,
  "ok": false,
  "error": {
    "code": "E_NOENT",
    "meaning": "named path does not exist",
    "suggested_next_call": {
      "op": "ls",
      "args": {"path": "/"}
    }
  },
  "stats": {
    "us": 10,
    "bytes_out": 174,
    "tokens_saved": 0
  }
}
```

| Field | Type | Description |
| :--- | :--- | :--- |
| `v` | integer | Protocol version (`1`). |
| `id` | integer | Matches the `id` provided in the request. |
| `ok` | boolean | `true` if operation succeeded; `false` on failure. |
| `data` | any | Operation-specific JSON payload (present when `ok: true`). |
| `error` | object | Error descriptor object (present when `ok: false`). |
| `stats.us` | integer | Microseconds elapsed during kernel dispatch. |
| `stats.bytes_out` | integer | Number of bytes written to the serial port for this reply. |
| `stats.tokens_saved` | integer | Estimated token savings versus equivalent human POSIX terminal session. |

---

## 3. Standard Error Codes

All kernel errors are self-describing and include a recovery recommendation (`suggested_next_call`) whenever applicable.

| Code | Meaning | Recovery Strategy (`suggested_next_call`) |
| :--- | :--- | :--- |
| `E_NOENT` | Named path or file does not exist | Run `ls` on parent or root directory |
| `E_NOTDIR` | Path component is not a directory | Inspect node metadata via `stat` |
| `E_ISDIR` | Path is a directory when a file was expected | Inspect directory via `ls` |
| `E_ARG_MISSING` | Required argument is absent | Check operation schema via `help` |
| `E_ARG_UNKNOWN` | Unexpected argument provided | Query schema via `help` |
| `E_ARG_TYPE` | Argument type mismatch | Consult `help` for required types |
| `E_UNKNOWN_OP` | Operation name not found in registry | Call `help` to list registered operations |
| `E_TOO_LONG` | Payload or pipeline exceeds limits | Shorten request buffer or pipeline steps |
| `E_BUSY` | Reception timeout on serial line | Resend complete request with terminal newline |
| `E_PARSE` | Request string is not valid JSON | Re-encode valid JSON object |
| `E_FAULT` | Internal kernel or filesystem fault | Inspect state via `sys.info` |

---

## 4. Kernel Operation Registry

The operation registry in `kernel/registry.c` is the single source of truth for both kernel dispatch and LLM tool definitions.

### 4.1 `ping`
- **Purpose:** Health check and connection verification.
- **Args:** `{}`
- **Data:** `{"pong": true}`

### 4.2 `help`
- **Purpose:** Describe kernel tool schemas for dynamic model introspection.
- **Args:** `op: string?`
- **Data (all):** `{"ops": [{"name": string, "schema": string}, ...]}`
- **Data (single):** `{"name": string, "schema": string}`

### 4.3 `sys.info`
- **Purpose:** Return operating system state, memory utilization, and uptime.
- **Args:** `{}`
- **Data:** 
  ```json
  {
    "version": "1.0",
    "uptime_ms": 1250,
    "mem": {
      "ramfs_used": 3902,
      "arena_used": 160
    },
    "cwd": "/",
    "ops": ["ping", "help", "sys.info", "pwd", "cd", "ls", "cat", "grep", "stat", "write", "pipe"]
  }
  ```

### 4.4 `pwd`
- **Purpose:** Return current working directory path.
- **Args:** `{}`
- **Data:** `{"cwd": "/docs"}`

### 4.5 `cd`
- **Purpose:** Change current working directory.
- **Args:** `path: string`
- **Data:** `{"cwd": "/docs"}`

### 4.6 `ls`
- **Purpose:** List directory contents (directories first, then files).
- **Args:** `path: string?` (defaults to `cwd`)
- **Data:** `[{"name": string, "type": "dir"|"file", "bytes": int}, ...]`

### 4.7 `cat`
- **Purpose:** Read entire file contents.
- **Args:** `path: string`
- **Data:** `{"path": string, "bytes": int, "text": string, "truncated": false}`

### 4.8 `grep`
- **Purpose:** Search for substring pattern across in-memory files.
- **Args:** `pattern: string`, `path: string?`, `ignore_case: bool?`
- **Data:**
  ```json
  {
    "matches": [
      {
        "file": "/logs/boot.log",
        "line": 1,
        "col": 10,
        "excerpt": "AGENCY-1 v1.0 booting"
      }
    ],
    "match_count": 1,
    "files_searched": 8
  }
  ```

### 4.9 `stat`
- **Purpose:** Inspect file or directory metadata.
- **Args:** `path: string?` (defaults to `cwd`)
- **Data:** `{"name": string, "type": "dir"|"file", "bytes": int, "inode": int, "children"?: [...]}`

### 4.10 `write`
- **Purpose:** Create a new file or overwrite an existing file in RAMFS.
- **Args:** `path: string`, `text: string`
- **Data:** `{"path": string, "bytes": int, "created": bool, "overwritten": bool}`

### 4.11 `pipe`
- **Purpose:** Atomic multi-step execution pipeline with `$prev` substitution. Collapses multiple round trips into a single request.
- **Args:** `steps: array` (maximum 8 steps)
  ```json
  {
    "steps": [
      {"op": "cd", "args": {"path": "/logs"}},
      {"op": "pwd", "args": {}},
      {"op": "cat", "args": {"path": "boot.log"}}
    ]
  }
  ```
- **Data:**
  ```json
  {
    "steps_completed": 3,
    "results": [
      {"op": "cd", "ok": true, "data": {"cwd": "/logs"}},
      {"op": "pwd", "ok": true, "data": {"cwd": "/logs"}},
      {"op": "cat", "ok": true, "data": {"path": "boot.log", "bytes": 164, ...}}
    ]
  }
  ```
