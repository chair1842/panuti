/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/serial/core.h>
#include <kernel/serial/dvc.h>
#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/irq.h>
#include <kernel/klog.h>
#include <kernel/memman/slab.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/task.h>
#include <panuti/errno.h>
#include <panuti/inode_type.h>
#include <stdio.h>
#include <string.h>

#include "../../../arch/i386/drivers/serial/uart.h"

typedef struct {
	char* buf;
	size_t cap;
	size_t len;
} str_sink_t;

static void str_sink_putchar(char c, void* extra) {
	str_sink_t* s = (str_sink_t*)extra;
	if (s->len + 1 < s->cap) {
		s->buf[s->len] = c;
		s->len++;
		s->buf[s->len] = '\0';
	}
}

static size_t render_snprintf(char* buf, size_t cap, const char* fmt, ...) {
	str_sink_t s = { buf, cap, 0 };
	if (cap == 0) {
		return 0;
	}
	
	buf[0] = '\0';

	va_list ap;
	va_start(ap, fmt);
	vfctprintf(str_sink_putchar, &s, fmt, ap);
	va_end(ap);

	return s.len;
}

static uart_t serial_ports[SERIAL_MAX_PORTS];
static int serial_n_ports;

uart_t* serial_get_port(int index) {
	if (index < 0 || index >= serial_n_ports) {
		return nullptr;
	}
	
	return &serial_ports[index];
}

int serial_port_count(void) {
	return serial_n_ports;
}

// ring helpers

static bool rx_ring_empty(uart_t* u) {
	return u->rx_head == u->rx_tail;
}

static void rx_ring_push(uart_t* u, uint8_t byte) {
	size_t next = (u->rx_head + 1) % SERIAL_RING_SIZE;

	if (next == u->rx_tail) {
		u->rx_tail = (u->rx_tail + 1) % SERIAL_RING_SIZE;
		u->overruns++;
	}

	u->rx_ring[u->rx_head] = byte;
	u->rx_head = next;
}

static bool rx_ring_pop(uart_t* u, uint8_t* out) {
	if (rx_ring_empty(u)) {
		return false;
	}

	*out = u->rx_ring[u->rx_tail];
	u->rx_tail = (u->rx_tail + 1) % SERIAL_RING_SIZE;
	return true;
}

static bool tx_ring_empty(uart_t* u) {
	return u->tx_head == u->tx_tail;
}

static void tx_ring_push(uart_t* u, uint8_t byte) {
	size_t next = (u->tx_head + 1) % SERIAL_RING_SIZE;

	if (next == u->tx_tail) {
		u->tx_tail = (u->tx_tail + 1) % SERIAL_RING_SIZE;
	}

	u->tx_ring[u->tx_head] = byte;
	u->tx_head = next;
}

int serial_tx_pull(uart_t* u, uint8_t* out) {
	if (tx_ring_empty(u)) {
		return -1;
	}

	*out = u->tx_ring[u->tx_tail];
	u->tx_tail = (u->tx_tail + 1) % SERIAL_RING_SIZE;
	return 0;
}

void serial_rx_push(uart_t* u, uint8_t byte) {
	rx_ring_push(u, byte);
	u->rx_bytes++;

	for (int i = 0; i < u->rx_n_waiters; i++) {
		task_wake((task_t*)u->rx_waiters[i]);
	}
	
	u->rx_n_waiters = 0;
}

void serial_rx_error(uart_t* u) {
	u->overruns++;
}

void serial_register(void) {
	serial_n_ports = 0;

	struct {
		uint16_t base;
		uint32_t vector;
	} candidates[SERIAL_MAX_PORTS] = {
		{UART_COM1_BASE, 0x24},
		{UART_COM2_BASE, 0x23},
		{UART_COM3_BASE, 0x24},
		{UART_COM4_BASE, 0x23},
	};

	for (int i = 0; i < SERIAL_MAX_PORTS; i++) {
		serial_chip_t chip;
		if (!uart_probe(candidates[i].base, &chip)) {
			continue;
		}

		uart_t* u = &serial_ports[serial_n_ports];
		memset(u, 0, sizeof(*u));
		u->base = candidates[i].base;
		u->irq_vector = candidates[i].vector;
		u->index = serial_n_ports;
		u->chip = chip;
		u->present = true;
		u->baud = 115200;

		uart_init(u->base, u->baud);

		const char* name = (chip == SERIAL_CHIP_16550A) ? "16550A" : "8250";
		klog(KLOG_INFO, "serial: COM%d at 0x%x is a %s\n", serial_n_ports + 1, u->base, name);

		if (uart_loopback_test(u->base)) {
			klog(KLOG_INFO, "serial: COM%d loopback pass\n", serial_n_ports + 1);
		} else {
			klog(KLOG_WARN, "serial: COM%d loopback FAIL\n", serial_n_ports + 1);
		}

		serial_isa_attach(u);

		serial_n_ports++;
	}

	if (serial_n_ports == 0) {
		klog(KLOG_WARN, "serial: no ports found\n");
	}
}

#define SERIAL_BAUD_DEFAULT 115200

typedef struct {
	uart_t* port;
	bool in_use;
	task_t* owner;
} serial_claim_t;

static serial_claim_t serial_claims[SERIAL_MAX_PORTS];

typedef struct {
	bool in_use;
	task_t* owner;
} serial_data_state_t;

static serial_data_state_t serial_data_states[SERIAL_MAX_PORTS];

static int claim_port(uart_t* u) {
	serial_claim_t* c = &serial_claims[u->index];
	uint32_t flags = irq_save_disable();

	if (c->in_use) {
		irq_restore(flags);
		return PANUTIERRNO_BUSY;
	}

	c->in_use = true;
	c->owner = sched_current();

	irq_restore(flags);
	return 0;
}

static void release_port(uart_t* u) {
	serial_claim_t* c = &serial_claims[u->index];
	uint32_t flags = irq_save_disable();
	c->in_use = false;
	c->owner = nullptr;
	irq_restore(flags);
}

static int serial_data_open(void* impl, void** out_impl) {
	uart_t* u = (uart_t*)impl;
	(void)out_impl;

	int rc = claim_port(u);
	if (rc != 0) {
		return rc;
	}

	serial_data_states[u->index] = (serial_data_state_t){ .in_use = true, .owner = sched_current() };
	return 0;
}

static int serial_data_read(void* impl, void* buf, size_t len) {
	uart_t* u = (uart_t*)impl;
	uint8_t* out = (uint8_t*)buf;

	if (len == 0) {
		return 0;
	}

	size_t n = 0;

	uint32_t flags = irq_save_disable();

	while (n < len) {
		uint8_t byte;
		if (rx_ring_pop(u, &byte)) {
			out[n++] = byte;
			continue;
		}

		if (n > 0) {
			break;
		}

		if (u->rx_n_waiters < SERIAL_MAX_WAITERS) {
			u->rx_waiters[u->rx_n_waiters++] = sched_current();
		}
		
		task_block(sched_current());

		irq_restore(flags);
		flags = irq_save_disable();
	}

	irq_restore(flags);

	return (int)n;
}

static int serial_data_write(void* impl, const void* buf, size_t len) {
	uart_t* u = (uart_t*)impl;
	const uint8_t* in = (const uint8_t*)buf;

	uint32_t flags = irq_save_disable();

	for (size_t i = 0; i < len; i++) {
		tx_ring_push(u, in[i]);
		u->tx_bytes++;
	}

	while (!tx_ring_empty(u)) {
		if (uart_tx_ready(u->base)) {
			uint8_t byte = 0;
			serial_tx_pull(u, &byte);
			uart_putchar_raw(u->base, byte);
		}
	}

	irq_restore(flags);

	return (int)len;
}

static int serial_data_close(void* impl, struct task* self) {
	uart_t* u = (uart_t*)impl;

	uint32_t flags = irq_save_disable();
	serial_data_state_t* s = &serial_data_states[u->index];
	if (s->in_use && s->owner == self) {
		s->in_use = false;
		s->owner = nullptr;
	}
	
	irq_restore(flags);

	release_port(u);
	return 0;
}

static const handle_ops_t serial_data_ops = {
	.open = serial_data_open,
	.read = serial_data_read,
	.write = serial_data_write,
	.activate = op_not_supported_act,
	.ready = op_not_supported_rdy,
	.seek = op_seek_ignore,
	.close = serial_data_close,
};

typedef struct {
	uart_t* port;
	char snapshot[256];
	size_t len;
	size_t pos;
} serial_status_handle_t;

static int serial_status_open(void* impl, void** out_impl) {
	uart_t* u = (uart_t*)impl;

	serial_status_handle_t* h = kmalloc(sizeof(*h), alignof(serial_status_handle_t));
	if (!h) {
		return PANUTIERRNO_PLAINERR;
	}

	h->port = u;
	h->pos = 0;

	const char* chip = (u->chip == SERIAL_CHIP_16550A) ? "16550A" : "8250";

	h->len = render_snprintf(h->snapshot, sizeof(h->snapshot),
		"port=COM%d\n"
		"base=0x%x\n"
		"chip=%s\n"
		"baud=%u\n"
		"rx_bytes=%u\n"
		"tx_bytes=%u\n"
		"overruns=%u\n",
		u->index + 1, u->base, chip, u->baud,
		u->rx_bytes, u->tx_bytes, u->overruns);

	*out_impl = h;
	return 0;
}

static int serial_status_read(void* impl, void* buf, size_t len) {
	serial_status_handle_t* h = (serial_status_handle_t*)impl;

	size_t avail = h->len - h->pos;
	size_t n = (avail < len) ? avail : len;
	memcpy(buf, h->snapshot + h->pos, n);
	h->pos += n;
	
	return (int)n;
}

static int serial_status_close(void* impl, struct task* self) {
	(void)self;
	kfree(impl);
	return 0;
}

static const handle_ops_t serial_status_ops = {
	.open = serial_status_open,
	.read = serial_status_read,
	.write = op_not_supported_w,
	.activate = op_not_supported_act,
	.ready = op_not_supported_rdy,
	.seek = op_seek_ignore,
	.close = serial_status_close,
};

typedef struct {
	uart_t* port;
	char render[32];
	size_t len;
	size_t pos;
} serial_baud_handle_t;

static int serial_baud_open(void* impl, void** out_impl) {
	uart_t* u = (uart_t*)impl;

	serial_baud_handle_t* h = kmalloc(sizeof(*h), alignof(serial_baud_handle_t));
	if (!h) {
		return PANUTIERRNO_PLAINERR;
	}

	h->port = u;
	h->pos = 0;
	h->len = render_snprintf(h->render, sizeof(h->render), "%u\n", u->baud);

	*out_impl = h;
	return 0;
}

static int serial_baud_read(void* impl, void* buf, size_t len) {
	serial_baud_handle_t* h = (serial_baud_handle_t*)impl;

	size_t avail = h->len - h->pos;
	size_t n = (avail < len) ? avail : len;
	memcpy(buf, h->render + h->pos, n);
	h->pos += n;
	return (int)n;
}

static int serial_baud_write(void* impl, const void* buf, size_t len) {
	serial_baud_handle_t* h = (serial_baud_handle_t*)impl;
	uart_t* u = h->port;
	const char* in = (const char*)buf;

	uint32_t rate = 0;
	size_t i = 0;

	while (i < len && (in[i] == ' ' || in[i] == '\t')) {
		i++;
	}

	if (i >= len || in[i] < '0' || in[i] > '9') {
		return PANUTIERRNO_INVALIDARG;
	}

	while (i < len && in[i] >= '0' && in[i] <= '9') {
		rate = rate * 10 + (uint32_t)(in[i] - '0');
		i++;
	}

	if (rate == 0 || UART_BASE_CLOCK % rate != 0) {
		return PANUTIERRNO_INVALIDARG;
	}

	uint16_t divisor = (uint16_t)(UART_BASE_CLOCK / rate);
	if (divisor == 0) {
		return PANUTIERRNO_INVALIDARG;
	}

	uint32_t flags = irq_save_disable();

	while (!uart_tx_ready(u->base)) {}

	uart_set_baud(u->base, rate);
	u->baud = rate;

	irq_restore(flags);

	return (int)len;
}

static int serial_baud_close(void* impl, struct task* self) {
	(void)self;
	kfree(impl);
	return 0;
}

static const handle_ops_t serial_baud_ops = {
	.open = serial_baud_open,
	.read = serial_baud_read,
	.write = serial_baud_write,
	.activate = op_not_supported_act,
	.ready = op_not_supported_rdy,
	.seek = op_seek_ignore,
	.close = serial_baud_close,
};

void serial_dvc_init(void) {
	if (serial_n_ports == 0) {
		return;
	}

	registry_mkdir("/dvc/serial");

	for (int i = 0; i < serial_n_ports; i++) {
		uart_t* u = &serial_ports[i];

		char dir[32];
		render_snprintf(dir, sizeof(dir), "/dvc/serial/%d", i);
		registry_mkdir(dir);

		char path[48];

		render_snprintf(path, sizeof(path), "/dvc/serial/%d/data", i);
		registry_add(path, INODE_DVC, u, &serial_data_ops);

		render_snprintf(path, sizeof(path), "/dvc/serial/%d/status", i);
		registry_add(path, INODE_DVC, u, &serial_status_ops);

		render_snprintf(path, sizeof(path), "/dvc/serial/%d/baud", i);
		registry_add(path, INODE_DVC, u, &serial_baud_ops);
	}
}
