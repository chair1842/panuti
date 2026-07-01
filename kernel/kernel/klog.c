#include <kernel/klog.h>
#include <kernel/tty.h>
#include <kernel/serial.h>
#include <stdarg.h>
#include <stdio.h>

static void out_serial(char c, void *arg) {
	(void)arg;
	serial_write(&c, sizeof(c));
}

static void out_both(char c, void *arg) {
	(void)arg;
	serial_write(&c, sizeof(c));
	terminal_write(&c, sizeof(c));
}

void klog(enum klog_level level, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	void (*out)(char, void*) = (level == KLOG_INFO) ? out_serial : out_both;
	
	if (level == KLOG_WARN) {
		terminal_fsetcolor(ANSI_COLOR_LIGHT_MAGENTA, ANSI_COLOR_BLACK);
	}

	if (level == KLOG_INFO) {
		fctprintf(out, NULL, "[INFO] ");
	} else {
		fctprintf(out, NULL, "[WARN] ");
	}
	
	vfctprintf(out, NULL, fmt, args);
	va_end(args);

	terminal_fsetcolor(ANSI_COLOR_LIGHT_GREY, ANSI_COLOR_BLACK);
}