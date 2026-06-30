#ifndef _KERNEL_KLOG_H
#define _KERNEL_KLOG_H

enum klog_level {
	KLOG_INFO = 0,
	KLOG_WARN = 1,
};

void klog(enum klog_level level, const char *fmt, ...);

#endif