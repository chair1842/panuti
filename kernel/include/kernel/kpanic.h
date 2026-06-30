#ifndef _KERNEL_KPANIC_H
#define _KERNEL_KPANIC_H

#define kpanic(fmt, ...) kpanic_impl(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

void kpanic_impl(const char *file, int line, const char *fmt, ...);

#endif