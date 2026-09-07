#include <kernel/handle/registry.h>
#include <kernel/sched/task.h>
#include <kernel/sched/sched.h>
#include <string.h>
#include <panuti/errno.h>
#include <kernel/syscall/handlers.h>
#include <kernel/mem/usr.h>

int32_t syshandler_unlink(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	const char* path = (const char*)a1;

	if (!kernel_is_user_ptr(path)) {
		return PANUTIERRNO_INVALIDADDR;
	}

	size_t len = strlen(path);
	if (len == 0) {
		return PANUTIERRNO_NOTFOUND;
	}

	// strip trailing slash
	while (len > 1 && path[len - 1] == '/') {
		len--;
	}

	const char* last = path + len;
	while (last > path && *(last - 1) != '/') {
		last--;
	}

	size_t namelen = (size_t)((path + len) - last);
	if (namelen == 0) {
		return PANUTIERRNO_NOTFOUND;
	}

	task_t* t = sched_current();
	inode_t* parent;

	if (last == path) {
		parent = t->cwd;
	} else {
		// resolve parent (everything before last component)
		char buf[128];
		size_t plen = (size_t)(last - path);
		if (plen >= sizeof(buf)) {
			return PANUTIERRNO_INVALIDADDR;
		}
		memcpy(buf, path, plen);
		buf[plen] = '\0';
		parent = registry_resolve(t->cwd, buf);
	}

	if (!parent || parent->type != INODE_DIR) {
		return PANUTIERRNO_NOTFOUND;
	}

	dirent_t* d = registry_unlink(parent, last, namelen);
	if (!d) {
		return PANUTIERRNO_NOTFOUND;
	}

	return 0;
}
