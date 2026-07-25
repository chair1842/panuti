#include <kernel/handle/registry.h>
#include <panuti/errno.h>
#include "handlers.h"

int32_t syshandler_mkdir(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	const char* path = (const char*)a1;

	if (!is_user_ptr(path)) {
		return PANUTIERRNO_INVALIDADDR;
	}

	return registry_mkdir(path);
}
