#include <kernel/handle/registry.h>
#include <panuti/errno.h>
#include <kernel/syscall/handlers.h>
#include <kernel/mem/usr.h>

// unmount the filesystem currently attached at `mountp`.
int32_t syshandler_unmount(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	const char* mountp = (const char*)a1;

	if (!kernel_is_user_ptr(mountp)) {
		return PANUTIERRNO_INVALIDADDR;
	}

	if (registry_unmount(mountp) != 0) {
		return PANUTIERRNO_NOTFOUND;
	}

	return PANUTIERRNO_PLAINSUCCESS;
}
