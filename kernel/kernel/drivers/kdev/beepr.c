/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/beepr.h>
#include <panuti/errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "kdev.h"

static bool beepr_isspace(char c) {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int beepr_write(void* impl, const void* buf, size_t len) {
	(void)impl;

	const char* in = (const char*)buf;
	size_t i = 0;

	while (i < len && beepr_isspace(in[i])) {
		i++;
	}

	if (i >= len || in[i] < '0' || in[i] > '9') {
		return PANUTIERRNO_INVALIDARG;
	}

	uint32_t frequency = 0;

	while (i < len && in[i] >= '0' && in[i] <= '9') {
		uint32_t digit = (uint32_t)(in[i] - '0');

		if (frequency > (UINT32_MAX - digit) / 10) {
			return PANUTIERRNO_INVALIDARG;
		}

		frequency = frequency * 10 + digit;
		i++;
	}

	while (i < len && beepr_isspace(in[i])) {
		i++;
	}

	if (i != len) {
		return PANUTIERRNO_INVALIDARG;
	}

	if (beepr_play(frequency) != 0) {
		return PANUTIERRNO_INVALIDARG;
	}

	return (int)len;
}

static int beepr_close(void* impl, struct task* self) {
	(void)impl;
	(void)self;

	beepr_stop();
	return 0;
}

static const handle_ops_t beepr_ops = {
	.read = op_not_supported_rw,
	.write = beepr_write,
	.activate = op_not_supported_act,
	.seek = op_seek_ignore,
	.ready = op_not_supported_rdy,
	.close = beepr_close,
};

void kdev_beepr_register() {
	registry_add("/dvc/beepr", INODE_DVC, nullptr, &beepr_ops);
}
