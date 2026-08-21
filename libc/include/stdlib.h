#ifndef _STDLIB_H
#define _STDLIB_H 1

#include <sys/cdefs.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((__noreturn__))
void abort(void);

void free(void* ptr);
void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);

int atexit(void (*func)(void));
int atoi(const char*);
char* getenv(const char*);
int abs(int);

#ifdef __cplusplus
}
#endif

#endif
