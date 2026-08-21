#ifndef _UNISTD_H
#define _UNISTD_H 1

#include <sys/cdefs.h>

#include <sys/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define F_OK 0

pid_t fork(void);
int execv(const char*, char* const[]);
int execve(const char*, char* const[], char* const[]);
int execvp(const char*, char* const[]);
pid_t getpid(void);
int close(int);
int access(const char*, int);

#ifdef __cplusplus
}
#endif

#endif
