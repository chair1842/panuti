#ifndef _SYS_STAT_H
#define _SYS_STAT_H 1

#include <sys/cdefs.h>

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define S_IRUSR 0400
#define S_IWUSR 0200

int mkdir(const char*, mode_t);

#ifdef __cplusplus
}
#endif

#endif
