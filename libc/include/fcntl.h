#ifndef _FCNTL_H
#define _FCNTL_H 1

#include <sys/cdefs.h>

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2

#define O_CREAT 0100
#define O_TRUNC 01000

#define F_RDLCK 0
#define F_WRLCK 1
#define F_UNLCK 2

#define F_SETLKW 7

struct flock {
	short l_type;
	short l_whence;
	off_t l_start;
	off_t l_len;
	pid_t l_pid;
};

int open(const char*, int, ...);
int fcntl(int, int, ...);

#ifdef __cplusplus
}
#endif

#endif
