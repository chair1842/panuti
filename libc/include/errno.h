#ifndef _ERRNO_H
#define _ERRNO_H

extern int errno;

#define EDOM   1
#define ERANGE 2
#define EILSEQ 3

#define EPERM   4
#define ENOENT  5
#define ESRCH   6
#define EINTR   7
#define EIO     8
#define ENXIO   9
#define E2BIG   10
#define ENOEXEC 11
#define EBADF   12
#define ECHILD  13
#define EAGAIN  14
#define ENOMEM  15
#define EACCES  16
#define EFAULT  17
#define ENOTBLK 18
#define EBUSY   19
#define EEXIST  20
#define EXDEV   21
#define ENODEV  22
#define ENOTDIR 23
#define EISDIR  24
#define EINVAL  25
#define ENFILE  26
#define EMFILE  27
#define ENOTTY  28
#define EFBIG   29
#define ENOSPC  30
#define ESPIPE  31
#define EROFS   32
#define EMLINK  33
#define EPIPE   34
#define ENOSYS  35
#define ENOTEMPTY 36
#define ENAMETOOLONG 37

#endif