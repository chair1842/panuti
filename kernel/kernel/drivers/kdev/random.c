/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/timer.h>
#include <stddef.h>
#include <stdint.h>
#include "kdev.h"

static uint32_t rng_state = 0x853C49E1u;
static bool rng_seeded = false;

static uint32_t rng_next(void) {
	uint32_t x = rng_state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	rng_state = x;
	return x;
}

static void rng_seed(void) {
	if (rng_seeded) {
		return;
	}

	// there is no real entropy in the kernel yet; the PIT tick low bits
	// at least vary from boot to boot
	rng_state ^= (uint32_t)timer_get_ticks();
	rng_seeded = true;
}

static int random_read(void* impl, void* buf, size_t len) {
	(void)impl;
	rng_seed();

	uint8_t* p = (uint8_t*)buf;
	for (size_t i = 0; i < len; i++) {
		p[i] = (uint8_t)(rng_next() >> 24);
	}

	return (int)len;
}

// writes fold whatever the caller drops in into the stream
static int random_write(void* impl, const void* buf, size_t len) {
	(void)impl;
	rng_seed();

	const uint8_t* p = (const uint8_t*)buf;
	for (size_t i = 0; i < len; i++) {
		rng_state ^= (uint32_t)p[i] << ((i & 3) * 8);
		rng_next();
	}

	return (int)len;
}

static const handle_ops_t random_ops = {
	.read = random_read,
	.write = random_write,
	.activate = op_not_supported_act,
	.seek = op_seek_ignore,
	.ready = op_not_supported_rdy,
	.close = op_not_supported_close,
};

void kdev_random_register() {
	registry_add("/dvc/random", INODE_FILE, nullptr, &random_ops);
}