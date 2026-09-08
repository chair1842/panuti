#include <kernel/handle/registry.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/task.h>
#include <string.h>
#include <panuti/errno.h>
#include <kernel/syscall/handlers.h>
#include <kernel/mem/usr.h>

#define GETCWD_MAX_DEPTH 64
#define GETCWD_SCRATCH 1024

int32_t syshandler_getcwd(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a3; (void)a4;
	char* buf = (char*)a1;
	size_t len = (size_t)a2;

	if (!kernel_is_user_range(buf, len) || len == 0) {
		return PANUTIERRNO_INVALIDADDR;
	}

	task_t* t = sched_current();
	inode_t* cur = t->cwd;
	inode_t* root = registry_root();

	// the root's .. points at itself, that's the tell
	if (cur == root) {
		if (len < 2) {
			return PANUTIERRNO_INVALIDADDR;
		}
		buf[0] = '/';
		buf[1] = '\0';
		return 0;
	}

	// names get discovered innermost-first, so park them in a flat scratch
	// and remember where each one lives (the kernel stack is only 4k, no
	// 2d arrays of 256-byte names here)
	char scratch[GETCWD_SCRATCH];
	uint32_t offs[GETCWD_MAX_DEPTH];
	int ncomp = 0;
	size_t packed = 0;

	while (cur != root) {
		dirent_t* up = registry_finddirent(cur, "..", 2);
		if (!up || up->inode == cur) {
			break; // reached the root via its self-loop
		}

		inode_t* parent = up->inode;

		char* name = NULL;
		for (dirent_t* d = parent->children; d; d = d->next) {
			if (d->inode == cur) {
				name = d->name;
				break;
			}
		}

		if (!name) {
			break;
		}

		size_t nl = strlen(name);
		if (nl == 0 || nl >= REG_MAX_NAME_LEN) {
			break;
		}

		if (ncomp == GETCWD_MAX_DEPTH || packed + nl + 1 > sizeof(scratch)) {
			return PANUTIERRNO_INVALIDADDR;
		}

		offs[ncomp] = (uint32_t)packed;
		memcpy(scratch + packed, name, nl + 1);
		packed += nl + 1;
		ncomp++;

		cur = parent;
	}

	// count the final path before writing a single byte of user memory
	size_t total = 1; // leading slash
	for (int i = 0; i < ncomp; i++) {
		total += strlen(scratch + offs[i]);
		if (i > 0) {
			total += 1; // separating slash
		}
	}

	if (total >= len) {
		return PANUTIERRNO_INVALIDADDR;
	}

	size_t pos = 0;
	buf[pos++] = '/';
	for (int i = ncomp - 1; i >= 0; i--) {
		size_t nl = strlen(scratch + offs[i]);
		memcpy(buf + pos, scratch + offs[i], nl);
		pos += nl;
		if (i > 0) {
			buf[pos++] = '/';
		}
	}
	
	buf[pos] = '\0';

	return 0;
}