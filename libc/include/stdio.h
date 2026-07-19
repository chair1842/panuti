#ifndef _STDIO_H
#define _STDIO_H 1

#include <sys/cdefs.h>
#include <stdarg.h>

#define EOF (-1)
typedef struct { int unused; } FILE;

#ifdef __cplusplus
extern "C" {
#endif

extern FILE* stderr;
#define stderr stderr

int printf(const char* __restrict, ...);
int putchar(int);
int puts(const char*);
int fctprintf(void (*out)(char c, void* extra_arg), void* extra_arg, const char* format, ...);
int vfctprintf(void (*out)(char c, void* extra_arg), void* extra_arg, const char* format, va_list arg);
int vprintf(const char* format, va_list arg);
int fflush(FILE*);
int fprintf(FILE*, const char*, ...);

#ifdef __cplusplus
}
#endif

#endif
