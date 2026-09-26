# Panuti Syscalls

All system calls are invoked via the `SYSENTER`/`SYSEXIT` fast syscall mechanism on i386.

The raw syscall interface is:

```c
int32_t panuti_syscall(uint32_t num, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
```

Syscall numbers are defined in `libc/include/panuti/syscall/syscallno.h`.
Convenience wrappers are declared in `libc/include/panuti/syscall/syscallsf.h`.

## Error Codes

Errors are returned with bit 31 set (i.e. negative when interpreted as signed):

```c
#define PANUTIERRORCODE(code)  ((code) + 0x80000000)
```

| Name | Value | Description |
|---|---|---|
| `PANUTIERRNO_PLAINSUCCESS` | `0x00000000` | Success |
| `PANUTIERRNO_PLAINERR` | `0x80000000` | Generic error |
| `PANUTIERRNO_INVALIDSYSCALL` | `0x80000001` | Invalid syscall number |
| `PANUTIERRNO_UNSUPPORTEDOP` | `0x80000002` | Operation not supported |
| `PANUTIERRNO_NOTFOUND` | `0x80000003` | Path or entry not found |
| `PANUTIERRNO_NOFDS` | `0x80000004` | No free file descriptors (max 32) |
| `PANUTIERRNO_BADFD` | `0x80000005` | Bad or closed file descriptor |
| `PANUTIERRNO_INVALIDADDR` | `0x80000006` | Invalid userspace address |
| `PANUTIERRNO_NOTSUPPORTED` | `0x80000007` | Filesystem type not supported |
| `PANUTIERRNO_EXISTS` | `0x80000008` | Name already exists |
| `PANUTIERRNO_BUSY` | `0x80000009` | Resource is busy (e.g. keyboard line already claimed) |

All user pointers are validated to lie within `0x08048000`--`0xC0000000`.
The lower bound is inclusive and the upper bound is exclusive, so `0xC0000000`
itself is rejected. A *range* whose end lands exactly on `0xC0000000` is accepted.

### Non-errno return values

A few syscalls can return a raw `-1` (`0xFFFFFFFF`) instead of a
`PANUTIERRNO_*` code. This value is **not** in the table above, so a test for
`PANUTIERRNO_PLAINERR` will not catch it; check for `-1` explicitly.
The syscalls affected are `ACTIVATE` (4), `MKDIR` (6), and `READDIR` (23) --
each is documented individually below.

**Note:** errors are reported *in the return value*, not through a global
`errno`. `libc` does define an `int errno`, but nothing ever assigns to it.

---

## Return value conventions

Return values are not uniform across the API. Check the shape before comparing:

| Shape | Example | Test |
|---|---|---|
| Byte count | `READ` (3), `WRITE` (0), `STREAM_READ` (19) | `>= 0` is a count; `< 0` is an error |
| Status `0` | `CLOSE` (5), `MKDIR` (6), `GETCWD` (11), `PIPE_CREATE` (17) | `== 0` is success |
| Opaque value | `GETPID` (9), `TIMESB` (10) | cannot fail; no error case |
| PID | `PROCREATE` (21) | `>= 0` is a PID; `< 0` is an error |
| Two-state | `READDIR` (23) | `0` = entry written, `1` = end of directory |
| Unobservable | `NSTREAM` (18) | see its entry -- the wrapper is `void` |

---

## Syscall Reference

### 0 -- WRITE

```c
int32_t panutisysf_write(int handle, const void* data, size_t size);
```

Write data to an open file descriptor.

**Parameters:**
- `handle` -- file descriptor index (0--31)
- `data` -- pointer to data in userspace
- `size` -- number of bytes to write

**Returns:** number of bytes written, or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `data` is not a valid userspace pointer
- `PANUTIERRNO_BADFD` -- `handle` is out of range or empty

---

### 1 -- EXIT

```c
void panutisysf_exit(uint32_t code);
```

Terminate the current process.

**Parameters:**
- `code` -- exit code, retrievable by a parent via `WAIT`

**Returns:** does not return (process is terminated immediately).

**Errors:** none.

**Note:** `code` is stored in the task and delivered to whichever process
later calls `WAIT` on this PID, which receives it via its `ec_out` argument.
It is *not* discarded. `WAIT` is single-shot per PID: the first waiter reaps
the task and frees it, so a second `WAIT` on the same PID returns
`PANUTIERRNO_NOTFOUND`.

---

### 2 -- OPEN

```c
int32_t panutisysf_open(const char* path);
```

Open a file, device, block device, or directory by path and obtain a file descriptor.

**Parameters:**
- `path` -- null-terminated path string

**Returns:** non-negative file descriptor on success, or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `path` is not a valid userspace pointer
- `PANUTIERRNO_NOTFOUND` -- path does not resolve
- `PANUTIERRNO_NOFDS` -- all 32 handle slots are in use
- `PANUTIERRNO_PLAINERR` -- open failed internally (e.g. could not allocate a directory handle)

**Note:** Opening a directory returns a *directory handle*: it does not support `READ`/`WRITE`, but it can be passed to `READDIR` to iterate its entries, and to `CLOSE` afterwards.

---

### 3 -- READ

```c
int32_t panutisysf_read(int handle, void* data, size_t size);
```

Read data from an open file descriptor.

**Parameters:**
- `handle` -- file descriptor index
- `data` -- userspace buffer to receive data
- `size` -- maximum number of bytes to read

**Returns:** number of bytes actually read (0 indicates EOF for pipes), or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `data` is not a valid userspace pointer
- `PANUTIERRNO_BADFD` -- `handle` is out of range or empty

---

### 4 -- ACTIVATE

```c
int32_t panutisysf_activate(int handle);
```

Activate a device handle (device-specific operation).

**Currently a no-op: this syscall always fails.** No handle type in the tree
implements a successful activate, so there is no device-specific result to
report. The call is forwarded to the handle's `activate` op, and every
implementation is a stub.

**Parameters:**
- `handle` -- file descriptor index

**Returns:** never a success. The result depends on the handle type:

| Handle type | Result |
|---|---|
| pipe, directory, mounted-FS file, block device | `-1` (raw, not an errno code) |
| `/dvc/console`, `/dvc/kbd/line` | `PANUTIERRNO_UNSUPPORTEDOP` |

**Errors:**
- `PANUTIERRNO_BADFD` -- `handle` is out of range or empty
- `PANUTIERRNO_UNSUPPORTEDOP` -- device does not implement activate
- `-1` -- the handle types listed in the table above

---

### 5 -- CLOSE

```c
int32_t panutisysf_close(int handle);
```

Close an open file descriptor.

**Parameters:**
- `handle` -- file descriptor index

**Returns:** 0 on success, or error code.

**Errors:**
- `PANUTIERRNO_BADFD` -- `handle` is out of range or empty

**Side effects:** decrements the inode refcount; for pipes, signals EOF to readers.

---

### 6 -- MKDIR

```c
int32_t panutisysf_mkdir(const char* path);
```

Create a new directory in the in-memory VFS registry.

**Parameters:**
- `path` -- path for the new directory

**Returns:** 0 on success, or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `path` is not a valid userspace pointer
- `-1` -- the directory could not be created; this is a **raw** `-1`, not a
  `PANUTIERRNO_*` code, and covers every failure below:
  - `path` is empty
  - `path` names the root directory or resolves to an empty name
  - the parent directory does not exist or is not a directory
  - the parent path prefix is 128 bytes or longer
  - the name already exists in the parent (name collision)
  - the name is 256 bytes or longer
  - the inode table is full (1024 inodes)
  - the directory entry table is full (2048 entries)
  - the parent is inside a mounted filesystem (no filesystem implements
    directory creation, so this always fails)

---

### 7 -- CHDIR

```c
int32_t panutisysf_chdir(const char* path);
```

Change the current working directory.

**Parameters:**
- `path` -- new working directory path

**Returns:** 0 on success, or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `path` is not a valid userspace pointer
- `PANUTIERRNO_NOTFOUND` -- path does not resolve
- `PANUTIERRNO_UNSUPPORTEDOP` -- resolved path is not a directory

---

### 8 -- UNLINK

```c
int32_t panutisysf_unlink(const char* path);
```

Remove a directory entry from its parent directory.

**Parameters:**
- `path` -- path of the entry to remove

**Returns:** 0 on success, or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `path` is not a valid userspace pointer
- `PANUTIERRNO_NOTFOUND` -- path does not resolve or name is empty

**Note:** The underlying inode is not destroyed if its refcount > 0 (e.g. still open via a handle).

---

### 9 -- GETPID

```c
uint32_t panutisysf_getpid(void);
```

Get the process ID of the calling process.

**Parameters:** none.

**Returns:** PID (always succeeds).

---

### 10 -- TIMESB

```c
int32_t panutisysf_timesb(void);
```

Get the time in ticks (centiseconds) since system boot.

**Parameters:** none.

**Returns:** number of PIT ticks since boot (always succeeds). The PIT runs at 100 Hz, so each tick is 10 ms.

---

### 11 -- GETCWD

```c
int32_t panutisysf_getcwd(char* buf, size_t len);
```

Retrieve the absolute path of the current working directory.

**Parameters:**
- `buf` -- userspace buffer to receive the path
- `len` -- size of the buffer in bytes

**Returns:** 0 on success, or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- buffer is invalid, `len` is 0, or buffer too small

**Limits:** max 64 path components, max 1024 bytes total.

---

### 12 -- YIELD

```c
void panutisysf_yield(void);
```

Voluntarily give up the CPU so the scheduler can run another task.

**Parameters:** none.

**Returns:** no return value (always succeeds).

---

### 13 -- RENAME

```c
int32_t panutisysf_rename(const char* oldpath, const char* newpath);
```

Move a directory entry from one path to another (same inode, new name/location).

**Parameters:**
- `oldpath` -- current path of the entry
- `newpath` -- desired new path

**Returns:** 0 on success, or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- either pointer is not a valid userspace pointer
- `PANUTIERRNO_NOTFOUND` -- source or destination parent does not exist, or source entry not found
- `PANUTIERRNO_EXISTS` -- destination name already exists
- `PANUTIERRNO_UNSUPPORTEDOP` -- rename involves a mounted filesystem

**Note:** self-rename (same path) is a no-op.

---

### 14 -- LINK

```c
int32_t panutisysf_link(const char* target, const char* newpath);
```

Create a hard link -- a second directory entry pointing to the same inode.

**Parameters:**
- `target` -- path of the existing inode to link
- `newpath` -- path where the new link is created

**Returns:** 0 on success, or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- either pointer is not a valid userspace pointer
- `PANUTIERRNO_NOTFOUND` -- target does not exist, or newpath's parent does not exist
- `PANUTIERRNO_UNSUPPORTEDOP` -- target is a directory (hard links to directories are forbidden)
- `PANUTIERRNO_EXISTS` -- destination name already exists

---

### 15 -- MOUNT

```c
int32_t panutisysf_mount(const char* mountp, const char* fstype, const char* blkdev);
```

Mount a filesystem from a block device onto a mountpoint directory.

**Parameters:**
- `mountp` -- path to an existing directory to serve as mountpoint
- `fstype` -- filesystem type string (`"isofs"` or `"fatfs"`)
- `blkdev` -- path to a block device

**Returns:** 0 on success, or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- any pointer is not a valid userspace pointer
- `PANUTIERRNO_NOTSUPPORTED` -- `fstype` is not recognized
- `PANUTIERRNO_NOTFOUND` -- block device or mountpoint path does not resolve

---

### 16 -- UNMOUNT

```c
int32_t panutisysf_unmount(const char* mountp);
```

Unmount a filesystem from a mountpoint.

**Parameters:**
- `mountp` -- path of the mountpoint to unmount

**Returns:** 0 on success, or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `mountp` is not a valid userspace pointer
- `PANUTIERRNO_NOTFOUND` -- path does not resolve or no filesystem mounted there

---

### 17 -- PIPE_CREATE

```c
int32_t panutisysf_pipe_create(const int* read_fd, const int* write_fd);
```

Create an anonymous pipe pair for inter-process communication.

**Parameters:**
- `read_fd` -- userspace pointer to `int` where the read-end fd is written
- `write_fd` -- userspace pointer to `int` where the write-end fd is written

**Returns:** 0 on success, or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- either pointer is not a valid userspace pointer
- `PANUTIERRNO_NOFDS` -- not enough free handle slots (needs 2)
- `PANUTIERRNO_PLAINERR` -- memory allocation failure

**Behavior:** Allocates a 4096-byte circular pipe buffer and two handles.
Pipe reads block if the buffer is empty and return 0 on EOF.
Pipe writes block if the buffer is full.
Closing the write end sends EOF to readers.

---

### 18 -- NSTREAM

```c
void panutisysf_nstream(int out[2]);
```

Report the number of input and output streams attached to the calling task.

Every task carries a fixed set of stream slots in each direction; only the entries that were successfully bound at task creation count as valid streams.

**Parameters:**
- `out` -- userspace pointer to an array of 2 `int`s used to return the stream counts

**Returns:** nothing. The wrapper is declared `void` and discards the
handler's result, so a caller **cannot distinguish success from failure**: if
`out` is an invalid pointer, the buffer is simply left untouched and no error
is reported. Validate the pointer yourself, or call `panuti_syscall` directly
to observe the return value.

**Errors:** (reachable only via a direct `panuti_syscall` call)
- `PANUTIERRNO_INVALIDADDR` -- `out` is not a valid userspace pointer

**Note:** On success, `out[0]` receives the number of input streams and `out[1]` the number of output streams. At task creation the kernel attempts to bind output stream 0 to the console device (`/dvc/console`) and input stream 0 to the keyboard line discipline (`/dvc/kbd/line`); a slot is only registered if the device is available.

---

### 19 -- STREAM_READ

```c
int32_t panutisysf_stream_read(int stream_no, void* buf, size_t len);
```

Read data from one of the calling task's input streams.

**Parameters:**
- `stream_no` -- input stream index (0 to N-1, where N is the input-stream count reported by `NSTREAM`)
- `buf` -- userspace buffer to receive the data
- `len` -- maximum number of bytes to read

**Returns:** number of bytes actually read, or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `buf`/`len` is not a valid userspace range
- `PANUTIERRNO_BADFD` -- `stream_no` is out of range for the task's input streams
- `PANUTIERRNO_UNSUPPORTEDOP` -- the underlying stream does not support reading

**Note:** The result is produced by the underlying device's read operation, so its value is device-specific.

---

### 20 -- STREAM_WRITE

```c
int32_t panutisysf_stream_write(int stream_no, const void* buf, size_t len);
```

Write data to one of the calling task's output streams.

**Parameters:**
- `stream_no` -- output stream index (0 to N-1, where N is the output-stream count reported by `NSTREAM`)
- `buf` -- userspace buffer of data to write
- `len` -- number of bytes to write

**Returns:** number of bytes actually written, or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `buf`/`len` is not a valid userspace range
- `PANUTIERRNO_BADFD` -- `stream_no` is out of range for the task's output streams
- `PANUTIERRNO_UNSUPPORTEDOP` -- the underlying stream does not support writing

**Note:** The result is produced by the underlying device's write operation, so its value is device-specific.

---

### 21 -- PROCREATE

```c
pid_t panutisysf_procreate(const procreate_args_t* args);
```

Create a new user process by loading an ELF executable.

**Parameters:**
- `args` -- pointer to a `procreate_args_t` struct (see below)

**Returns:** PID of the new process, or error code.

The `procreate_args_t` struct is defined in `libc/include/panuti/syscall/procreate.h`:

```c
typedef struct procreate_args {
    const char* path;       // path to ELF executable
    char** argv;            // argument string pointers
    int argc;               // number of arguments (max 32)
    int* in_streams;        // array of input stream handle indices
    int no_in_streams;      // number of input streams (max 16)
    int* out_streams;       // array of output stream handle indices
    int no_out_streams;     // number of output streams (max 16)
} procreate_args_t;
```

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `args` or any pointer inside the struct is not a valid userspace pointer
- `PANUTIERRNO_PLAINERR` -- `argc`, `no_in_streams`, or `no_out_streams` is out of range

---

### 22 -- WAIT

```c
int32_t panutisysf_wait(pid_t pid, int* ec_out);
```

Block until a target process exits and retrieve its exit code.

**Parameters:**
- `pid` -- PID of the process to wait for
- `ec_out` -- userspace pointer to `int` where the exit code is written

**Returns:** 0 on success, or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `ec_out` is not a valid userspace pointer
- `PANUTIERRNO_PLAINERR` -- attempting to wait on the calling process itself
- `PANUTIERRNO_NOTFOUND` -- target process does not exist

**Note:** The caller blocks until the target process terminates. If the target is still running, the caller is put to sleep and retried when the target exits. The `ec_out` value is the exit code passed to `EXIT` (1).

**Note:** `WAIT` is single-shot per PID. The first waiter to succeed reaps the
task and destroys it, so any subsequent `WAIT` on the same PID returns
`PANUTIERRNO_NOTFOUND`. There is no way to wait on a PID more than once, and
no way to detect a child exiting other than by calling `WAIT` on its PID.

---

### 23 -- READDIR

```c
int32_t panutisysf_readdir(int fd, dirent_entry_t* dirent_out);
```

Read the next directory entry from an open directory handle.

Open a directory with `OPEN` first; each `READDIR` call returns one entry and advances the directory's internal cursor, so entries are streamed in order. The caller loops until `1` is returned, indicating end of directory.

**Parameters:**
- `fd` -- file descriptor of an open directory handle
- `dirent_out` -- userspace pointer to a `dirent_entry_t` that receives the entry

The `dirent_entry_t` struct is defined in `libc/include/panuti/dirent.h`:

```c
#define DIRENT_NAME_MAX 256

typedef struct {
    char name[DIRENT_NAME_MAX];  // entry name, NUL-terminated
    inode_type_t type;           // INODE_DIR or INODE_FILE
} dirent_entry_t;
```

`inode_type_t` values: `INODE_NONE = 0`, `INODE_DIR = 1`, `INODE_FILE = 2`,
`INODE_BLOCK = 3`, `INODE_PIPE = 4`. For **native (registry) directories** the
reported type is the entry's inode type verbatim, so it may be any of the above
-- listing `/dvc`, for example, yields `INODE_BLOCK` for block devices.
Entries sourced from **IsoFS** are always `INODE_DIR` or `INODE_FILE`.

**Returns:** 0 on success (entry written to `dirent_out`), 1 on end of directory (no entry written), or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `dirent_out` is not a valid userspace pointer
- `PANUTIERRNO_BADFD` -- `fd` is out of range or empty
- `PANUTIERRNO_UNSUPPORTEDOP` -- `fd` is not a directory handle
- `-1` -- an IsoFS-backed directory could not be read (corrupt or truncated
  directory record, or a read failure against the underlying block device).
  This is a **raw** `-1`, not a `PANUTIERRNO_*` code, and is propagated
  unchanged from the filesystem.

**Note:** Native (registry) directories stream their entries in dirent-list order and include the special entries `"."` and `".."`. Directories mounted from a filesystem are streamed by the filesystem itself; IsoFS emits the on-disk directory records (also including `"."` and `".."`). Closing the handle with `CLOSE` frees the directory cursor.

---

## Quick Reference

All 24 syscalls below are registered in the kernel dispatch table. Numbers are
defined in `libc/include/panuti/syscall/syscallno.h`; any number outside this
range returns `PANUTIERRNO_INVALIDSYSCALL`.

| # | Name | # | Name |
|---|---|---|---|
| 0 | `WRITE` | 12 | `YIELD` |
| 1 | `EXIT` | 13 | `RENAME` |
| 2 | `OPEN` | 14 | `LINK` |
| 3 | `READ` | 15 | `MOUNT` |
| 4 | `ACTIVATE` | 16 | `UNMOUNT` |
| 5 | `CLOSE` | 17 | `PIPE_CREATE` |
| 6 | `MKDIR` | 18 | `NSTREAM` |
| 7 | `CHDIR` | 19 | `STREAM_READ` |
| 8 | `UNLINK` | 20 | `STREAM_WRITE` |
| 9 | `GETPID` | 21 | `PROCREATE` |
| 10 | `TIMESB` | 22 | `WAIT` |
| 11 | `GETCWD` | 23 | `READDIR` |

## Limits

| Limit | Value |
|---|---|
| File descriptors per task | 32 |
| Total inodes | 1024 |
| Total directory entries | 2048 |
| Max path component name | 256 bytes |
| Max getcwd nesting | 64 components |
| Max mounted filesystems | 64 |
| Pipe buffer size | 4096 bytes |
| Max input streams per task | 16 |
| Max output streams per task | 16 |
| Max procreate argv count | 32 |
