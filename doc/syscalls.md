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
A zero-length range bypasses the bounds check entirely and is always accepted.

### Non-errno return values

**Assume any return value may be a negative number outside the table above.**
Several syscalls delegate to a filesystem or driver op that signals failure with
a plain `-1` rather than a `PANUTIERRNO_*` code, and the kernel forwards it
untouched. Testing only against the codes in the table is therefore unsound;
treat *any* negative result as an error.

The syscalls that can return an out-of-table value, and what they return:

| # | Syscall | Out-of-table value | Cause |
|---|---|---|---|
| 0 | `WRITE` | `-1` | writing to a directory, a pipe's read end, or any IsoFS file |
| 3 | `READ` | `-1` | reading a directory, a pipe's write end, or on IsoFS I/O failure |
| 4 | `ACTIVATE` | `-1` | all handle types except console and kbd |
| 6 | `MKDIR` | `-1` | every registry-side failure mode (see below); a create-capable mount returns its own result instead |
| 15 | `MOUNT` | `-1`, `-2` | IsoFS volume scan hits an invalid block or an I/O error |
| 19 | `STREAM_READ` | `-1` | the bound stream is a directory, pipe, or IsoFS file |
| 20 | `STREAM_WRITE` | `-1` | likewise |
| 23 | `READDIR` | `-1` | an IsoFS directory record could not be read |
| 29 | `MKFILE` | `-1` | the parent is not on a create-capable mount (any registry-tree path, or an isofs/fatfs mount); a create-capable mount returns its own result instead |

`MOUNT` is the one syscall that can return a value other than `-1`: the
IsoFS mount path returns the block layer's `BLOCK_ERR_IO` (`-2`) on an I/O
error. This is reachable in practice -- mounting a small RAM block device as
IsoFS walks the PVD scan off the end of the device and returns `-1`.

**Note:** errors are reported *in the return value*, not through a global
`errno`. `libc` does define an `int errno`, but nothing ever assigns to it.

---

## Return value conventions

Return values are not uniform across the API. Check the shape before comparing:

| Shape | Syscalls | Test |
|---|---|---|
| Byte count | `WRITE` (0), `READ` (3), `STREAM_READ` (19), `STREAM_WRITE` (20) | `> 0` is a count; `<= 0` is an error or EOF -- see below |
| Status `0` | `CLOSE` (5), `MKDIR` (6), `GETCWD` (11), `PIPE_CREATE` (17), `RESIZE` (28), `MKFILE` (29) | `== 0` is success |
| Opaque value | `GETPID` (9) | cannot fail; no error case |
| PID | `PROCREATE` (21) | `>= 0` is a PID; `< 0` is an error |
| Multi-valued | `READDIR` (23) | `0` = entry, `1` = end of directory, negative = error |
| Unobservable | `NSTREAM` (18) | the wrapper is `void`; see its entry |

**Caveats on the byte-count shape:**

- `0` is genuinely ambiguous. For a pipe it means EOF; for IsoFS it also means
  EOF; but for a **block device** it means the read or write *failed* -- the
  block ops return `0` on allocation and I/O errors, so a failed transfer is
  indistinguishable from an empty one.
- Because a failed op may surface as `-1` rather than a `PANUTIERRNO_*` code,
  test `< 0` for errors rather than comparing against a specific code.

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
- `-1` -- the target rejected the write: it is a directory, the read end of a
  pipe, or a file on a mounted filesystem. Raw `-1`, not an errno code.

**Note:** on a **block device** a return of `0` means the write *failed*
(allocation or I/O error), not that zero bytes were written. A zero-length
write is a no-op and also returns `0`.

---

### 1 -- EXIT

```c
void panutisysf_exit(uint32_t code);
```

Terminate the current process.

**Parameters:**
- `code` -- exit code, retrievable by any task that later calls `WAIT` on this PID

**Returns:** does not return (process is terminated immediately).

**Errors:** none.

**Note:** `code` is stored in the task and delivered via the `ec_out` argument
of a later `WAIT` on this PID. It is *not* discarded. Panuti has no notion of a
parent or of process ownership: `WAIT` performs no ownership check, so any task
may wait on -- and reap -- any PID except its own.

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
- `-1` -- the target rejected the read: it is a directory, the write end of a
  pipe, or a file on a mounted filesystem whose block read failed. Raw `-1`,
  not an errno code.

**Note:** as with `WRITE`, a `0` return on a **block device** means the read
*failed*, not that end-of-file was reached. `0` means EOF only for pipes and
IsoFS files.

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

Create a new directory -- either a registry node, or a directory on the
mounted filesystem the parent lives in.

**Parameters:**
- `path` -- path for the new directory

**Returns:** 0 on success, or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `path` is not a valid userspace pointer
- `-1` -- the directory could not be created; this is a **raw** `-1`, not a
  `PANUTIERRNO_*` code, and covers every failure before any filesystem is
  asked to act:
  - `path` is empty
  - `path` names the root directory or resolves to an empty name
  - the parent directory does not exist or is not a directory
  - the parent path prefix is 128 bytes or longer
  - the name already exists in the parent (name collision)
  - the name is 256 bytes or longer
  - the inode table is full (1024 inodes)
  - the directory entry table is full (2048 entries)
  - the parent is inside a mounted filesystem and the filesystem does not
    implement directory creation there. Only ext2 can create today (for both
    directories and files); on isofs and fatfs mounts this path always fails
    and returns `-1`.
- the filesystem's own result is returned unchanged for a mount that does
  implement creation. ext2 contributes `PANUTIERRNO_EXISTS` (name already
  in the directory), `PANUTIERRNO_PLAINERR` (no free inode or the block I/O
  failed), `PANUTIERRNO_NOTSUPPORTED` (read-only volume), and
  `PANUTIERRNO_INVALIDARG` / `PANUTIERRNO_UNSUPPORTEDOP` (malformed request
  or a name of `"."` / `".."`).

**Note:** one failure is not reported. If the directory entry table fills up
after the new entry itself has been linked, the `"."` and `".."` links fail
silently and `MKDIR` still returns `0`, leaving a half-initialised directory.

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

**Note:** The underlying inode is not destroyed if its refcount > 0 (e.g. still open via a handle). The directory entry is likewise not destroyed while something still references it, and its name is stored on the heap. The entry is released once it is no longer in its parent directory *and* no `READDIR` cursor is positioned on it, so removing an entry that is being streamed does not pull the name out from under the reader.

---

### 9 -- GETPID

```c
uint32_t panutisysf_getpid(void);
```

Get the process ID of the calling process.

**Parameters:** none.

**Returns:** PID (always succeeds).

---

### 10 -- (retired: TIMESB)

Syscall `10` was a "time since boot in ticks (centiseconds)" readout. It has
been removed; the kernel clock is now exposed as a device instead:

- `/dvc/uptime` -- reading it returns the time since boot in centiseconds as a
  newline-terminated decimal string (the PIT runs at 100 Hz, so each tick is
  10 ms).

Number `10` is left unassigned to keep the other numbers stable.

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
- `-1` / `-2` -- the IsoFS volume scan read an invalid block (`-1`) or hit an
  I/O error (`-2`). These are raw block-layer codes, not `PANUTIERRNO_*` ones,
  and are returned unchanged. Reachable in practice: the scan reads 2 KiB per
  LBA and walks off the end of a small block device rather than reporting a
  filesystem error.

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
- `-1` -- the bound stream rejected the read (it is a directory, the write end
  of a pipe, or a file on a mounted filesystem). Raw `-1`, not an errno code.

**Note:** The result is produced by the underlying device's read operation, so its value is device-specific. The same caveat as `READ` applies: on a block device a `0` return signals a failed transfer, not EOF.

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
- `-1` -- the bound stream rejected the write (it is a directory, the read end
  of a pipe, or a file on a mounted filesystem). Raw `-1`, not an errno code.

**Note:** The result is produced by the underlying device's write operation, so its value is device-specific. As with `WRITE`, a `0` return on a **block device** means the transfer failed.

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
- `PANUTIERRNO_NOTFOUND` -- target process does not exist, or has already been
  reaped by an earlier `WAIT`

**Note:** the caller blocks until the target process terminates. If the target is still running, the caller is put to sleep and retried when the target exits. The `ec_out` value is the exit code passed to `EXIT` (1).

**Note:** `WAIT` is single-shot per PID. The successful waiter reaps the task and
its slot is recycled, so any subsequent `WAIT` on the same PID returns
`PANUTIERRNO_NOTFOUND`. If several tasks wait on the same PID they are all woken
on exit, but exactly one receives the exit code and the rest get
`PANUTIERRNO_NOTFOUND` -- a waiter cannot tell that it lost the race. (PIDs are
never reused, so the guarantee holds for the lifetime of the system.)

**Note:** Panuti exposes no dedicated exit-notification mechanism -- no
signals, no `kill`, no process-status syscall, and `GETPID` reports only the
caller. `WAIT` is the only kernel-provided way to observe a specific task's
exit. Applications that need to notice a child's death without consuming its
exit status must arrange their own signal, e.g. by holding a pipe's write end
and watching for EOF.

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
    inode_type_t type;           // see the type discussion below
    size_t size;                 // see the note below
} dirent_entry_t;
```

`size` is the entry's length in bytes, and is only as good as the filesystem
that recorded it:

- **Native registry entries always report `0`**, from both `READDIR` and
  `STAT`. The registry records no lengths anywhere, so this is `0` even for
  entries that have real content.
- **IsoFS entries report a real length** from both. `READDIR` hands out the
  extent length recorded in the directory record; `STAT` returns the same value,
  for directories as well as files.

`dirent_out` is written **only on success**. A `READDIR` that returns `1` for
end of directory, or any error, leaves the struct untouched, so a caller that
reuses one `dirent_entry_t` across calls must not read it after a non-zero
return without zeroing it first.

`inode_type_t` values: `INODE_NONE = 0`, `INODE_DIR = 1`, `INODE_FILE = 2`,
`INODE_BLOCK = 3`, `INODE_PIPE = 4`.

For **native (registry) directories** the reported type is the entry's inode
type verbatim, so a switch on it must handle `INODE_DIR`, `INODE_FILE`, *and*
`INODE_BLOCK` -- listing `/dvc`, for example, yields `INODE_BLOCK` for block
devices. `INODE_NONE` and `INODE_PIPE` are not reachable here: pipes exist only
as handle slots and are never registered as directory entries. Entries sourced
from **IsoFS** are always `INODE_DIR` or `INODE_FILE`.

**Returns:** 0 on success (entry written to `dirent_out`), 1 on end of directory (no entry written), or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `dirent_out` is not a valid userspace pointer
- `PANUTIERRNO_BADFD` -- `fd` is out of range or empty
- `PANUTIERRNO_UNSUPPORTEDOP` -- `fd` is not a directory handle
- `-1` -- an IsoFS-backed directory could not be read. This is a **raw** `-1`,
  not a `PANUTIERRNO_*` code, propagated unchanged from the filesystem. It
  occurs when the directory's extent is invalid or runs past the end of the
  volume, when the record buffer cannot be allocated, or when the underlying
  block read fails. Malformed or truncated records do *not* cause this -- they
  are silently skipped and iteration continues.

**Note:** Native (registry) directories stream their entries in dirent-list order and include the special entries `"."` and `".."`. Directories mounted from a filesystem are streamed by the filesystem itself; IsoFS emits the on-disk directory records (also including `"."` and `".."`). Closing the handle with `CLOSE` frees the directory cursor.

Entry names live on the heap, and the cursor holds a reference to the entry it is currently positioned on, so a name stays valid for as long as the cursor needs it. Closing the handle drops that reference. The practical guarantee is that **unlinking entries while a `READDIR` walk is in progress is safe**: the entry under the cursor is not torn down out from under the reader, its name is still delivered in full, and the walk continues to the following entries. See the "Traps when looping" note below for the caveat that comes with it.

**Traps when looping:**

- On an IsoFS `-1` the cursor is left unchanged, so a naive
  `while (panutisysf_readdir(fd, &e) == 0)` loop spins forever. Break on *any*
  non-zero return, not just on `1`.
- Only IsoFS implements the `readdir` filesystem op. Mounting a FAT filesystem
  and listing the mountpoint dereferences a null op pointer and panics, so
  `READDIR` is only usable on native and IsoFS directories.
- A walk is not a snapshot. An entry unlinked partway through is still reported
  by that walk, because the cursor keeps it alive until it is released; the
  unlink is not reflected in what the walk returns. Re-`OPEN` the directory to
  observe the post-unlink contents.

---

### 24 -- STAT

```c
int32_t panutisysf_stat(const char* path, dirent_entry_t* dirent_out);
```

Resolve a path and describe the entry it names, without opening it.

**Parameters:**
- `path` -- path of the entry to describe
- `dirent_out` -- userspace pointer to a `dirent_entry_t` that receives the entry

`path` is resolved relative to the calling task's current working directory, by
exactly the same rules as `NEXIST` and every other path-taking syscall.

The reported `name` is the entry's **own** name, not the path it was reached
by, so `STAT` and `READDIR` describe the same file identically. The one
exception is a path with no final component to report, such as `"/"`, which
keeps the literal path the caller supplied.

`type` uses the same `inode_type_t` vocabulary described under `READDIR`.

`size` is `0` unless the entry came from a mounted filesystem that implements
`fs_ops->size`. The native registry records no
lengths anywhere, so **native entries always report `size == 0` even when they
have real content**. IsoFS returns the same extent length that `READDIR` hands
out for the same entry, so `STAT` and `READDIR` agree on `size`.

**Returns:** 0 on success (entry written to `dirent_out`), or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `path` is not a valid NUL-terminated userspace pointer, or `dirent_out` is not a valid userspace range of `sizeof(dirent_entry_t)` bytes
- `PANUTIERRNO_NOTFOUND` -- `path` does not resolve
- `PANUTIERRNO_PLAINERR` -- the final name component is 256 bytes or longer and so does not fit `dirent_entry_t.name`

**Note:** `STAT` resolves and describes only; it does not open the entry, so it
costs no file descriptor and cannot fail with `PANUTIERRNO_NOFDS`. There is no
permission model, so no access check is performed and `STAT` succeeds on a
directory, a block device, or any other registered entry.

libc wraps this as `int stat(const char*, dirent_entry_t*)` in
`libc/panuti/stat/stat.c`.

---

### 25 -- NEXIST

```c
int32_t panutisysf_nexist(const char* path);
```

Test whether a path resolves to something, without describing or opening it.

**Parameters:**
- `path` -- path to test

**Returns:** **1 if the path resolves, 0 if it does not.** This is deliberately
*not* an error code: every `PANUTIERRNO_*` value lives at or above `0x80000000`,
which leaves `0` and `1` free to mean "no" and "yes". A caller that treats a
non-zero result as success is correct only because nothing else can return
`1`; a caller that compares against `1` exactly is correct regardless.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `path` is not a valid NUL-terminated userspace pointer

**Note:** Resolution is identical to `STAT`'s, so anything `STAT` can find,
`NEXIST` finds, and vice versa -- including entries inside a mounted
filesystem. "Exists" says nothing about kind; use `STAT` and check
`entry.type` for that.

libc wraps this as `bool nexist(const char*)` in
`libc/panuti/stat/nexist.c`, which returns true only on an exact `1`, so a
future in-band error would surface as "does not exist" rather than as true.

---

### 26 -- MMAPAN

```c
int32_t panutisysf_mmapan(size_t len, int prot, void* addr_hint);
```

Map `len` bytes of anonymous memory and return the base address.

The whole region is backed by physical frames and **zeroed before the call
returns** -- there is no demand paging, so every page is resident from the
start. Frames are committed in chunks of 64 with interrupts masked, which bounds
the irq-off window.

`prot` is a mask of `MMAPAN_PROT_*` from `libc/include/panuti/mmap.h`:

| Bit | Name | Effect |
|---|---|---|
| `0x0` | `MMAPAN_PROT_NONE` | pages are present but supervisor-only, so ring 3 cannot touch them at all |
| `0x1` | `MMAPAN_PROT_READ` | pages become user accessible |
| `0x2` | `MMAPAN_PROT_WRITE` | pages become user accessible and writable |
| `0x4` | `MMAPAN_PROT_EXEC` | accepted, but has **no effect** |

Two consequences worth knowing before you rely on the protection bits:

- `PROT_WRITE` implies `PROT_READ`. The x86 R/W bit gates supervisor access as
  well, so there is no way to express writable-but-not-readable here.
- `PROT_EXEC` cannot be honoured because a 32-bit page table entry carries no
  NX bit; text is executable either way.

`addr_hint` is a preference, not a contract. It is rounded down to a page and
used if it currently holds a free run that large; otherwise the kernel searches
the rest of user space. The address is chosen entirely kernel-side, so there is
no user pointer to validate.

**Returns:** the base address -- a userspace address, so non-negative -- on
success, or a negative `PANUTIERRNO_*` code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `prot` has bits set outside the three known flags (which means the caller was built against a different header, so it is refused rather than silently mapped with a protection nobody asked for), or `len` is `0`, or `len` exceeds `MMAPAN_MAX`
- `PANUTIERRNO_NOMEM` -- no free run of the requested size in user space, or physical frames ran out. **Failure is all-or-nothing:** frames committed by earlier chunks are released before returning, so a failed call leaves nothing mapped.

**Note:** the kernel keeps no per-task accounting, so `MMAPAN_MAX` (16 MiB)
bounds a single call and *not* a process's total footprint; a caller can map
repeatedly up to the physical memory and address space available.

**This is the syscall `malloc` is built on.** libc's allocator takes 1 MiB
arenas from here (`libc/stdlib/malloc_util.c`), so any program that calls
`malloc` issues `MMAPAN` even though it never names it.

---

### 27 -- MUNMAP

```c
int32_t panutisysf_munmap(void* addr, size_t len);
```

Release a range previously handed out by `MMAPAN`.

`addr` must be page aligned and `len` is rounded up to a whole number of pages.
Pages in the range that are already unmapped are skipped, so a single call may
legitimately span a hole; this is not an error.

**Returns:** 0 on success, or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `len` is `0`, or `addr` is not page aligned, or `addr`..`addr`+`len` is not wholly within the userspace range (which also rejects a `len` that would wrap the 32-bit address space)

**Note:** only `MMAPAN` hands out addresses, so a `addr` that is not page
aligned means the caller's own arithmetic went wrong rather than that it meant
to clip a neighbour. The kernel rejects it instead of rounding, so a
miscomputed range fails loudly instead of unmapping the wrong page.

---

### 28 -- RESIZE

```c
int32_t panutisysf_resize(int fd, uint64_t* new_size);
```

Resize the object behind an open file descriptor, truncating or extending it.

**Parameters:**
- `fd` -- file descriptor index
- `new_size` -- userspace pointer to a `uint64_t` holding the desired size in bytes

**Returns:** 0 on success, or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `new_size` is not a valid userspace pointer
- `PANUTIERRNO_INVALIDARG` -- the size read from `*new_size` exceeds `INT64_MAX`
- `PANUTIERRNO_BADFD` -- `fd` is out of range or empty
- `PANUTIERRNO_UNSUPPORTEDOP` -- the handle has no resize op. This is the answer
  for pipes, directories, the console/kbd devices, block devices, the registry
  device files, and any mounted filesystem that does not implement resize.

**Note:** the size is passed *by pointer*, unlike every other scalar argument:
the wrapper takes `uint64_t*` and the kernel dereferences it in user space, so
a caller must keep it valid for the duration of the call.

**Note:** the result is produced by whichever handle type's resize op the kernel
dispatches to, so it is only as good as that op. Today only **ext2** implements
a working resize. On an ext2 file, shrinking to `N` frees the blocks past the
new end and updates the recorded size; extending grows the recorded size but
allocates **no new blocks**, so the newly exposed range reads as zeros until
written. The op is ext2-specific, so it adds its own failure codes on top of the
list above: `PANUTIERRNO_NOTSUPPORTED` on a read-only volume,
`PANUTIERRNO_INVALIDARG` for a size past the end of the volume,
`PANUTIERRNO_UNSUPPORTEDOP` if the file is not a regular file, and
`PANUTIERRNO_NOTFOUND`/`PANUTIERRNO_PLAINERR` if the inode cannot be read or the
block freeing fails.

---

### 29 -- MKFILE

```c
int32_t panutisysf_mkfile(const char* path);
```

Create a new, empty regular file inside a mounted filesystem.

**Parameters:**
- `path` -- path for the new file

**Returns:** 0 on success, or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `path` is not a valid userspace pointer
- `-1` -- the file could not be created; this is a **raw** `-1`, not a
  `PANUTIERRNO_*` code, and covers every failure before any filesystem is
  asked to act:
  - `path` is empty
  - `path` names the root directory or resolves to an empty name
  - the parent directory does not exist or is not a directory
  - the parent path prefix is 128 bytes or longer
  - the parent is **not on a mounted filesystem that implements creation**.
    The registry tree deliberately has no file backing, so any path whose
    parent lives in it fails here -- including an isofs or fatfs mount
    (neither implements `create`)
- the filesystem's own result is returned unchanged for a mount that does
  implement creation. ext2 contributes `PANUTIERRNO_EXISTS` (name already
  in the directory), `PANUTIERRNO_PLAINERR` (no free inode or the block I/O
  failed), `PANUTIERRNO_NOTSUPPORTED` (read-only volume),
  `PANUTIERRNO_INVALIDARG` / `PANUTIERRNO_UNSUPPORTEDOP` (malformed request
  or a name of `"."` / `".."`), and `PANUTIERRNO_PLAINERR` if the entry has
  no room in its directory block.

**Note:** unlike `MKDIR`, creation is a purely on-disk operation: no registry
node is allocated, no `"."`/`".."` links are added, and nothing is left
behind on a `-1`. A created file is a plain filesystem file and opens with the
filesystem's read/write ops, so `READ`/`WRITE` on it work as usual.

---

### 30 -- SEEK

```c
int32_t panutisysf_seek(int fd, int64_t* offset, int whence);
```

Set the byte position of an open handle, so the next `READ`/`WRITE` on it
starts from there instead of continuing where the last one left off. The
position is computed from a *base* chosen by `whence` plus the signed `offset`
passed by pointer:

| `whence` | Base |
|---|---|
| `SEEK_SET` | the start of the file/device (offset 0) |
| `SEEK_CUR` | the handle's current position |
| `SEEK_END` | the end of the file/device |

The three `whence` values are defined in
`libc/include/panuti/syscall/seek.h` (`SEEK_SET 0`, `SEEK_CUR 1`,
`SEEK_END 2`), which `<panuti/syscall/syscallsf.h>` pulls in for you.

**Parameters:**
- `fd` -- file descriptor index
- `offset` -- userspace pointer to an `int64_t` holding the signed distance in bytes from the base
- `whence` -- `SEEK_SET`, `SEEK_CUR`, or `SEEK_END`

**Returns:** 0 on success, or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `offset` is not a valid userspace pointer
- `PANUTIERRNO_INVALIDARG` -- `whence` is not one of `SEEK_SET`/`SEEK_CUR`/`SEEK_END`
- `PANUTIERRNO_BADFD` -- `fd` is out of range or empty
- `PANUTIERRNO_UNSUPPORTEDOP` -- the handle has no seek op. This is the answer
  for pipes, directories, the console/kbd devices, `/dvc/uptime`, and the
  registry device files that are not listed below.

**Note:** like `RESIZE`, the position is passed *by pointer*.

**Note:** the result is produced by whichever handle type's seek op the kernel
dispatches to. Today the real positional implementations are **files** on a
mounted filesystem and **block devices**. The computed position is clamped to
the file/device size, never going negative or past the end; a position at the
end makes the next `READ` return 0, and `SEEK_END` on a filesystem without a
size op fails with `PANUTIERRNO_UNSUPPORTEDOP`. The stateless
`/dvc/null`, `/dvc/zero`, and `/dvc/random` accept any seek as a no-op. Every
other handle type has no seek op and returns `PANUTIERRNO_UNSUPPORTEDOP`.

---

## Quick Reference

All 31 syscalls listed here are registered in the kernel dispatch table and
documented in detail above. Numbers are defined in
`libc/include/panuti/syscall/syscallno.h`.

The dispatch table has 256 slots. Any number at or above `256` returns
`PANUTIERRNO_INVALIDSYSCALL`, as does any number below `256` that is not
registered -- today that is `31` through `255`, so they are reserved rather
than permanently invalid.

| # | Name | # | Name |
|---|---|---|---|
| 0 | `WRITE` | 14 | `LINK` |
| 1 | `EXIT` | 15 | `MOUNT` |
| 2 | `OPEN` | 16 | `UNMOUNT` |
| 3 | `READ` | 17 | `PIPE_CREATE` |
| 4 | `ACTIVATE` | 18 | `NSTREAM` |
| 5 | `CLOSE` | 19 | `STREAM_READ` |
| 6 | `MKDIR` | 20 | `STREAM_WRITE` |
| 7 | `CHDIR` | 21 | `PROCREATE` |
| 8 | `UNLINK` | 22 | `WAIT` |
| 9 | `GETPID` | 23 | `READDIR` |
| 10 | _(retired)_ | 24 | `STAT` |
| 11 | `GETCWD` | 25 | `NEXIST` |
| 12 | `YIELD` | 26 | `MMAPAN` |
| 13 | `RENAME` | 27 | `MUNMAP` |
| 28 | `RESIZE` | 29 | `MKFILE` |
| 30 | `SEEK` |  |  |

## Limits

| Limit | Value |
|---|---|
| File descriptors per task | 32 |
| Total inodes | 1024 |
| Total directory entries | 2048 |
| Max path component name | 255 bytes (256-byte buffer, NUL included) |
| Max getcwd nesting | 64 components |
| Max mounted filesystems | 64 |
| Pipe buffer size | 4096 bytes |
| Max input streams per task | 16 |
| Max output streams per task | 16 |
| Max procreate argv count | 32 |
| Max single anonymous mapping | 16 MiB |
