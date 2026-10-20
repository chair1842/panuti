/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/klog.h>
#include <kernel/kmsg.h>
#include <kernel/tty.h>
#include <stdarg.h>
#include <stdio.h>

#define KLOG_MSG_MAX 256

struct msg_sink {
	char buf[KLOG_MSG_MAX];
	size_t len;
};

static void msg_putchar(char c, void *arg) {
	struct msg_sink *s = (struct msg_sink *)arg;
	if (s->len < KLOG_MSG_MAX) {
		s->buf[s->len] = c;
	}
	
	s->len++;
}

void klog(enum klog_level level, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);

	struct msg_sink sink = {
		.len = 0
	};
	
	vfctprintf(msg_putchar, &sink, fmt, args);
	va_end(args);

	size_t body_len = sink.len < KLOG_MSG_MAX ? sink.len : KLOG_MSG_MAX;
	kmsg_push((int)level, sink.buf, body_len);
}