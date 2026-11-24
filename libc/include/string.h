/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _STRING_H
#define _STRING_H 1

#include <sys/cdefs.h>

#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

int memcmp(const void*, const void*, size_t);
void* memcpy(void* __restrict, const void* __restrict, size_t);
void* memmove(void*, const void*, size_t);
void* memset(void*, int, size_t);
void* memchr(const void* buf, int ch, size_t n);
void* mempcpy(void* __restrict, const void* __restrict, size_t);

size_t strlen(const char*);
size_t strnlen(const char*, size_t);
char* strcpy(char* __restrict, const char* __restrict);
char* strncpy(char* __restrict, const char* __restrict, size_t);
char* stpcpy(char* __restrict, const char* __restrict);
char* stpncpy(char* __restrict, const char* __restrict, size_t);
char* strcat(char* __restrict, const char* __restrict);
char* strncat(char* __restrict, const char* __restrict, size_t);
int strcmp(const char*, const char*);
int strncmp(const char*, const char*, size_t);
int strcasecmp(const char*, const char*);
int strncasecmp(const char*, const char*, size_t);
char* strchr(const char*, int);
char* strrchr(const char*, int);
char* strstr(const char*, const char*);
char* strpbrk(const char*, const char*);
size_t strspn(const char*, const char*);
size_t strcspn(const char*, const char*);
char* strtok(char* __restrict, const char* __restrict);
char* strtok_r(char* __restrict, const char* __restrict, char** __restrict);
char* strdup(const char*);

#if defined(__cplusplus)
}
#endif

#endif
