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

All user pointers are validated to lie within `0x08048000`--`0xC0000000`.

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
int32_t panutisysf_exit(uint32_t code);
```

Terminate the current process.

**Parameters:**
- `code` -- exit code (reserved, currently unused)

**Returns:** does not return (process is terminated immediately).

**Errors:** none.

---

### 2 -- OPEN

```c
int32_t panutisysf_open(const char* path);
```

Open a file, device, or block device by path and obtain a file descriptor.

**Parameters:**
- `path` -- null-terminated path string

**Returns:** non-negative file descriptor on success, or error code.

**Errors:**
- `PANUTIERRNO_INVALIDADDR` -- `path` is not a valid userspace pointer
- `PANUTIERRNO_NOTFOUND` -- path does not resolve
- `PANUTIERRNO_UNSUPPORTEDOP` -- path is a directory
- `PANUTIERRNO_NOFDS` -- all 32 handle slots are in use
- `PANUTIERRNO_PLAINERR` -- open failed internally

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

**Parameters:**
- `handle` -- file descriptor index

**Returns:** device-specific result, or error code.

**Errors:**
- `PANUTIERRNO_BADFD` -- `handle` is out of range or empty

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
- `-1` -- name collision during path walk

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
int32_t panutisysf_yield(void);
```

Voluntarily give up the CPU so the scheduler can run another task.

**Parameters:** none.

**Returns:** 0 (always succeeds).

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
- `fstype` -- filesystem type string (`"iso9660"` or `"vfat"`)
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

**Returns:** 0 on success (intended), or error code.

**Intended errors:**
- `PANUTIERRNO_INVALIDADDR` -- either pointer is not a valid userspace pointer
- `PANUTIERRNO_NOFDS` -- not enough free handle slots (needs 2)
- `PANUTIERRNO_PLAINERR` -- memory allocation failure

**Intended behavior:** Allocates a 4096-byte circular pipe buffer and two handles.
Pipe reads block if the buffer is empty and return 0 on EOF.
Pipe writes block if the buffer is full.
Closing the write end sends EOF to readers.

---

## Quick Reference

| # | Name | Registered |
|---|---|---|
| 0 | `WRITE` | Yes |
| 1 | `EXIT` | Yes |
| 2 | `OPEN` | Yes |
| 3 | `READ` | Yes |
| 4 | `ACTIVATE` | Yes |
| 5 | `CLOSE` | Yes |
| 6 | `MKDIR` | Yes |
| 7 | `CHDIR` | Yes |
| 8 | `UNLINK` | Yes |
| 9 | `GETPID` | Yes |
| 10 | `TIMESB` | Yes |
| 11 | `GETCWD` | Yes |
| 12 | `YIELD` | Yes |
| 13 | `RENAME` | Yes |
| 14 | `LINK` | Yes |
| 15 | `MOUNT` | Yes |
| 16 | `UNMOUNT` | Yes |
| 17 | `PIPE_CREATE` | **No** (bug) |

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
