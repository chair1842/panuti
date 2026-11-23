/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdio.h>
#include <panuti/mount.h>
#include <panuti/process.h>
#include <sys/types.h>
#include <panuti/syscall/syscallsf.h>
#include <panuti/registry.h>
#include <panuti/stat.h>
#include <panuti/pwerop.h>

#define DKRPATH "/dkr"
#define TARGETDIRPATH "/dkr/pda/pint/targets"

typedef struct dkr_candidate {
	const char* dev;
	const char* fstype;
} dkr_candidate_t;

static const dkr_candidate_t candidates[] = {
	{"/dvc/pata/0p1", "ext2"},
	{"/dvc/pata/0", "ext2"},
	{"/dvc/cdrom/0", "isofs"},
};

int dkrmount_run() {
	mkdir("/dkr");
	for (int i = 0; i < (sizeof(candidates) / sizeof(*candidates)); i++) {
		const dkr_candidate_t* c = &candidates[i];
		if (mount(DKRPATH, c->fstype, c->dev)) {
			continue;
		}

		dirent_entry_t d;
		if (stat(TARGETDIRPATH, &d) == 0) {
			if (d.type == INODE_DIR) {
				return 0;
			}
		}

		unmount(DKRPATH);
	}

	return -1;
}

void purtarg_run() {
	int delay = 100;
	while (true) {
		char* argv = "pur"; // "name" of the program
		pid_t pid = procreate("/dkr/usr/bin/pur", &argv, 1, NULL, 0, NULL, 0);
		if ((int32_t)pid < 0) {
			sleep(delay);
			if (delay < 6000) {
				delay *= 2;
			}

			continue;
		}

		int ec = 0;
		wait(pid, &ec);
	}
}

int main(int argc, char** argv) {
	if (dkrmount_run() != 0) {
		printf("pint: dkrmount failed\n");
		printf("in 30 seconds the machine will poweroff\n");
		sleep(3000);
		pwerop_shutdown();
	}

	purtarg_run();

	return -1; //how
} 