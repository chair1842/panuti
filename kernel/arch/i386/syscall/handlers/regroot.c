#include <kernel/handle/registry.h>
#include <panuti/errno.h>

int32_t syshandler_regroot(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a1; (void)a2; (void)a3; (void)a4;
	inode_t* result = registry_root();
	if (!result) return PANUTIERRNO_PLAINERR;
	return (int32_t)(uintptr_t)result;
}
