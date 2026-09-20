#ifndef _KERNEL_KLOG_H
#define _KERNEL_KLOG_H

#include <stdint.h>

typedef enum klog_level {
	KLOG_INFO = 0,
	KLOG_WARN = 1,
} klog_level_t;

void klog(klog_level_t level, const char *fmt, ...);

#endif