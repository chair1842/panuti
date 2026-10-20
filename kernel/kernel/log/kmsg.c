/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/irq.h>
#include <kernel/klog.h>
#include <kernel/kmsg.h>
#include <kernel/memman/slab.h>
#include <panuti/errno.h>
#include <panuti/inode_type.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KMSG_BUFFER_SIZE 8192

#define KMSG_TAG 0x01
#define KMSG_HEADER_LEN 4
#define KMSG_LABEL_LEN 7
#define KMSG_MAX_RECORD 65535

static char kmsg_buffer[KMSG_BUFFER_SIZE];
static size_t kmsg_head;
static size_t kmsg_tail;

typedef struct kmsg_stream {
	int level;
	bool labeled;
} kmsg_stream_t;

typedef struct kmsg_handle {
	const kmsg_stream_t* stream;
	size_t cursor;
	size_t pending;
} kmsg_handle_t;

static const kmsg_stream_t kmsg_all_stream = {-1, true };
static const kmsg_stream_t kmsg_log_stream = { KLOG_INFO, false };
static const kmsg_stream_t kmsg_warn_stream = { KLOG_WARN, false };

static void kmsg_push_byte(char c) {
	size_t next = (kmsg_head + 1) % KMSG_BUFFER_SIZE;

	if (next == kmsg_tail) {
		kmsg_tail = (kmsg_tail + 1) % KMSG_BUFFER_SIZE;
	}

	kmsg_buffer[kmsg_head] = c;
	kmsg_head = next;
}

void kmsg_push(int level, const char* data, size_t len) {
	if (level != KLOG_INFO && level != KLOG_WARN) {
		level = KLOG_INFO;
	}

	if (len > KMSG_MAX_RECORD) {
		len = KMSG_MAX_RECORD;
	}

	uint32_t flags = irq_save_disable();

	kmsg_push_byte(KMSG_TAG);
	kmsg_push_byte((char)level);
	kmsg_push_byte((char)(len & 0xff));
	kmsg_push_byte((char)((len >> 8) & 0xff));

	for (size_t i = 0; i < len; i++) {
		kmsg_push_byte(data[i]);
	}

	irq_restore(flags);
}

static size_t kmsg_resync(size_t from) {
	size_t head = kmsg_head;
	size_t cursor = from;

	while (cursor != head) {
		if (kmsg_buffer[cursor] != KMSG_TAG) {
			cursor = (cursor + 1) % KMSG_BUFFER_SIZE;
			continue;
		}

		if ((head - cursor + KMSG_BUFFER_SIZE) % KMSG_BUFFER_SIZE < KMSG_HEADER_LEN) {
			break;
		}

		int level = (unsigned char)kmsg_buffer[(cursor + 1) % KMSG_BUFFER_SIZE];
		if (level == KLOG_INFO || level == KLOG_WARN) {
			return cursor;
		}

		cursor = (cursor + 1) % KMSG_BUFFER_SIZE;
	}

	return cursor;
}

static int kmsg_open(void* impl, void** out_impl) {
	struct kmsg_handle* h = kmalloc(sizeof(*h), alignof(struct kmsg_handle));
	if (!h) {
		return PANUTIERRNO_PLAINERR;
	}

	h->stream = (const struct kmsg_stream*)impl;
	h->pending = 0;

	uint32_t flags = irq_save_disable();
	h->cursor = kmsg_resync(kmsg_tail);
	irq_restore(flags);

	*out_impl = h;
	return 0;
}

static int kmsg_read(void* impl, void* buf, size_t len) {
	struct kmsg_handle* h = (struct kmsg_handle*)impl;
	const struct kmsg_stream* s = h->stream;
	char* out = (char*)buf;

	uint32_t flags = irq_save_disable();

	size_t tail = kmsg_tail;
	size_t head = kmsg_head;
	size_t cursor = h->cursor;

	if (cursor < tail || cursor > head) {
		cursor = kmsg_resync(tail);
		h->pending = 0;
	}

	size_t written = 0;

	while (h->pending > 0 && written < len) {
		out[written++] = kmsg_buffer[cursor];
		cursor = (cursor + 1) % KMSG_BUFFER_SIZE;
		h->pending--;
	}

	while (h->pending == 0 && cursor != head && written < len) {
		char c = kmsg_buffer[cursor];
		size_t record_start = cursor;

		if (c != KMSG_TAG) {
			cursor = (cursor + 1) % KMSG_BUFFER_SIZE;
			out[written++] = c;
			continue;
		}

		if ((head - cursor + KMSG_BUFFER_SIZE) % KMSG_BUFFER_SIZE < KMSG_HEADER_LEN) {
			break;
		}

		cursor = (cursor + 1) % KMSG_BUFFER_SIZE;
		
		int level = (unsigned char)kmsg_buffer[cursor];
		cursor = (cursor + 1) % KMSG_BUFFER_SIZE;
		
		size_t body_len = (unsigned char)kmsg_buffer[cursor];
		cursor = (cursor + 1) % KMSG_BUFFER_SIZE;
		
		body_len |= (size_t)(unsigned char)kmsg_buffer[cursor] << 8;
		cursor = (cursor + 1) % KMSG_BUFFER_SIZE;

		if (level != KLOG_INFO && level != KLOG_WARN) {
			level = KLOG_INFO;
		}

		if ((head - cursor + KMSG_BUFFER_SIZE) % KMSG_BUFFER_SIZE < body_len) {
			break;
		}

		bool keep = (s->level < 0) || (s->level == level);

		if (!keep) {
			cursor = (cursor + body_len) % KMSG_BUFFER_SIZE;
			continue;
		}

		if (s->labeled && written + KMSG_LABEL_LEN > len) {
			cursor = record_start;
			break;
		}

		if (s->labeled) {
			const char* label = (level == KLOG_WARN) ? "[WARN] " : "[INFO] ";
			for (int i = 0; i < KMSG_LABEL_LEN; i++) {
				out[written++] = label[i];
			}
		}

		size_t take = body_len;
		if (take > len - written) {
			take = len - written;
		}

		for (size_t i = 0; i < take; i++) {
			out[written++] = kmsg_buffer[cursor];
			cursor = (cursor + 1) % KMSG_BUFFER_SIZE;
		}

		if (take < body_len) {
			h->pending = body_len - take;
			break;
		}
	}

	h->cursor = cursor;

	irq_restore(flags);

	return (int)written;
}

static int kmsg_close(void* impl, struct task* self) {
	(void)self;
	kfree(impl);
	return 0;
}

static const handle_ops_t kmsg_ops = {
	.open = kmsg_open,
	.read = kmsg_read,
	.write = op_not_supported_w,
	.activate = op_not_supported_act,
	.seek = op_seek_ignore,
	.ready = op_not_supported_rdy,
	.close = kmsg_close,
};

void kmsg_init() {
	registry_mkdir("/dvc/kmsg");

	registry_add("/dvc/kmsg/all", INODE_DVC, (void*)&kmsg_all_stream, &kmsg_ops);
	registry_add("/dvc/kmsg/log", INODE_DVC, (void*)&kmsg_log_stream, &kmsg_ops);
	registry_add("/dvc/kmsg/warn", INODE_DVC, (void*)&kmsg_warn_stream, &kmsg_ops);
}
