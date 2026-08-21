#ifndef _STDIO_H
#define _STDIO_H 1

#include <sys/cdefs.h>
#include <stdarg.h>
#include <stddef.h>

#define EOF (-1)
typedef struct { int unused; } FILE;

#define SEEK_SET 0

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
int vfprintf(FILE* __restrict, const char* __restrict, va_list);
int sprintf(char* __restrict, const char* __restrict, ...);

FILE* fopen(const char* __restrict, const char* __restrict);
FILE* fdopen(int, const char*);
int fclose(FILE*);
int feof(FILE*);
size_t fread(void* __restrict, size_t, size_t, FILE* __restrict);
size_t fwrite(const void* __restrict, size_t, size_t, FILE* __restrict);
int fseek(FILE*, long, int);
long ftell(FILE*);
void setbuf(FILE* __restrict, char* __restrict);

#ifdef __cplusplus
}
#endif

#endif
