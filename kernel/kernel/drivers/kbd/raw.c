/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <kernel/kbd/core.h>
#include <kernel/handle/handle.h>
#include <kernel/handle/registry.h>
#include <kernel/kbd/dvc.h>
#include <panuti/kbd.h>
#include <string.h>

static int kbd_raw_read(void* impl, void* buf, size_t len) {
	(void)impl;

	if (len < sizeof(keypacket_t)) {
		return 0;
	}

	size_t n = 0;
	while (n + sizeof(keypacket_t) <= len) {
		keypacket_t pkt;
		if (kbd_core_read_raw_queue(&pkt) != 0) {
			return -1;
		}

		memcpy((char*)buf + n, &pkt, sizeof(pkt));
		n += sizeof(pkt);
	}

	return (int)n;
}

static const handle_ops_t kbd_raw_ops = {
	.read = kbd_raw_read,
	.write = op_not_supported_w,
	.activate = op_not_supported_act,
	.ready = op_not_supported_rdy,
	.close = op_not_supported_close,
};

void kbd_raw_init(void) {
	registry_add("/dvc/kbd/raw", INODE_FILE, nullptr, &kbd_raw_ops);
}