/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/proc/proc.h>
#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/memman/slab.h>
#include <kernel/sched/task.h>
#include <panuti/errno.h>
#include <panuti/inode_type.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef enum {
	PROC_STATE,
	PROC_EXIT_CODE,
	PROC_WAITING_ON,
	PROC_WAKE_AT,
	PROC_IN_STREAMS,
	PROC_OUT_STREAMS,
	PROC_FDS,
	PROC_NAME,
	PROC_CPID,
	PROC_CMDLINE,
} proc_kind_t;

typedef struct {
	char text[TASK_CMDLINE_MAX + 8];
	size_t len;
	size_t pos;
} proc_handle_t;

typedef struct {
	char* buf;
	size_t cap;
	size_t len;
} proc_sink_t;

static void proc_putchar(char c, void* extra) {
	proc_sink_t* s = (proc_sink_t*)extra;

	if (s->len + 1 < s->cap) {
		s->buf[s->len] = c;
		s->len++;
		s->buf[s->len] = '\0';
	}
}

static size_t proc_format(char* buf, size_t cap, const char* fmt, ...) {
	proc_sink_t s = { buf, cap, 0 };

	if (cap == 0) {
		return 0;
	}

	buf[0] = '\0';

	va_list ap;
	va_start(ap, fmt);
	vfctprintf(proc_putchar, &s, fmt, ap);
	va_end(ap);

	return s.len;
}

static const char* proc_state_name(task_state_t state) {
	switch (state) {
	case TASK_READY:
		return "ready";
	case TASK_RUNNING:
		return "running";
	case TASK_BLOCKED:
		return "blocked";
	case TASK_TERMINATED:
		return "terminated";
	default:
		return "unknown";
	}
}

static size_t proc_render(char* buf, size_t cap, proc_kind_t kind, const task_info_t* info) {
	switch (kind) {
		case PROC_STATE:
			return proc_format(buf, cap, "%s\n", proc_state_name(info->state));
		case PROC_EXIT_CODE:
			return proc_format(buf, cap, "%u\n", info->state == TASK_TERMINATED ? info->exit_code : 0);
		case PROC_WAITING_ON:
			return proc_format(buf, cap, "%u\n", info->state == TASK_BLOCKED ? info->pid_waiting_on : 0);
		case PROC_WAKE_AT:
			return proc_format(buf, cap, "%lld\n", info->state == TASK_BLOCKED ? (long long)info->wake_at : 0LL);
		case PROC_IN_STREAMS:
			return proc_format(buf, cap, "%d\n", info->no_in_streams);
		case PROC_OUT_STREAMS:
			return proc_format(buf, cap, "%d\n", info->no_out_streams);
		case PROC_FDS:
			return proc_format(buf, cap, "%u\n", info->open_fds);
		case PROC_NAME:
			return proc_format(buf, cap, "%s\n", info->name);
		case PROC_CPID:
			return proc_format(buf, cap, "%u\n", info->creatorpid);
		case PROC_CMDLINE:
			return proc_format(buf, cap, "%s\n", info->cmdline);
	}

	return 0;
}

static int proc_open_common(void* impl, proc_kind_t kind, void** out_impl) {
	pid_t pid = (pid_t)(uintptr_t)impl;
	task_info_t info;

	if (task_get_info(pid, &info) != 0) {
		return PANUTIERRNO_NOTFOUND;
	}

	proc_handle_t* h = kmalloc(sizeof(*h), alignof(proc_handle_t));
	if (!h) {
		return PANUTIERRNO_PLAINERR;
	}

	h->pos = 0;
	h->len = proc_render(h->text, sizeof(h->text), kind, &info);

	*out_impl = h;
	return 0;
}

static int proc_read(void* impl, void* buf, size_t len) {
	proc_handle_t* h = (proc_handle_t*)impl;

	if (h->pos >= h->len) {
		return 0;
	}

	size_t avail = h->len - h->pos;
	size_t n = (avail < len) ? avail : len;
	memcpy(buf, h->text + h->pos, n);
	h->pos += n;

	return (int)n;
}

static int proc_close(void* impl, struct task* self) {
	(void)self;
	kfree(impl);
	return 0;
}

#define PROC_OPS(name, kind) \
	static int name##_open(void* impl, void** out_impl) { \
		return proc_open_common(impl, kind, out_impl); \
	} \
	static const handle_ops_t name##_ops = { \
		.open = name##_open, \
		.read = proc_read, \
		.write = op_not_supported_w, \
		.activate = op_not_supported_act, \
		.ready = op_not_supported_rdy, \
		.seek = op_seek_ignore, \
		.close = proc_close, \
	}

PROC_OPS(proc_state, PROC_STATE);
PROC_OPS(proc_exit_code, PROC_EXIT_CODE);
PROC_OPS(proc_waiting_on, PROC_WAITING_ON);
PROC_OPS(proc_wake_at, PROC_WAKE_AT);
PROC_OPS(proc_in_streams, PROC_IN_STREAMS);
PROC_OPS(proc_out_streams, PROC_OUT_STREAMS);
PROC_OPS(proc_fds, PROC_FDS);
PROC_OPS(proc_name, PROC_NAME);
PROC_OPS(proc_cpid, PROC_CPID);
PROC_OPS(proc_cmdline, PROC_CMDLINE);

typedef struct {
	const char* name;
	const handle_ops_t* ops;
} proc_file_t;

static const proc_file_t proc_files[] = {
	{"state", &proc_state_ops},
	{"exit_code", &proc_exit_code_ops},
	{"waiting_on", &proc_waiting_on_ops},
	{"wake_at", &proc_wake_at_ops},
	{"in_streams", &proc_in_streams_ops},
	{"out_streams", &proc_out_streams_ops},
	{"fds", &proc_fds_ops},
	{"name", &proc_name_ops},
	{"cpid", &proc_cpid_ops},
	{"cmdline", &proc_cmdline_ops},
};

void proc_init(void) {
	registry_mkdir("/sys/pid");
}

void proc_task_add(const task_t* task) {
	if (!task || task->pid == 0) {
		return;
	}

	char dir[48];
	proc_format(dir, sizeof(dir), "/sys/pid/%u", task->pid);

	if (registry_mkdir(dir) != 0) {
		return;
	}

	for (size_t i = 0; i < sizeof(proc_files) / sizeof(proc_files[0]); i++) {
		char path[80];
		proc_format(path, sizeof(path), "/sys/pid/%u/%s", task->pid, proc_files[i].name);
		registry_add(path, INODE_DVC, (void*)(uintptr_t)task->pid, proc_files[i].ops);
	}
}

void proc_task_remove(pid_t pid) {
	if (pid == 0) {
		return;
	}

	char dir[48];
	proc_format(dir, sizeof(dir), "/sys/pid/%u", pid);

	inode_t* node = registry_resolve(registry_root(), dir);
	if (node) {
		for (size_t i = 0; i < sizeof(proc_files) / sizeof(proc_files[0]); i++) {
			registry_unlink(node, proc_files[i].name, strlen(proc_files[i].name));
		}
	}

	inode_t* parent;
	const char* name;
	size_t namelen;

	if (registry_splitpath(registry_root(), dir, &parent, &name, &namelen) == 0) {
		registry_unlink(parent, name, namelen);
	}
}
