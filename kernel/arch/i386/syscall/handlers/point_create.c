#include <stdint.h>
#include <kernel/handle/point.h>
#include <panuti/errno.h>

int32_t syshandler_point_create(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
	(void)a2; (void)a3; (void)a4;
	const char* path = (const char*)a1;

	// TODO: same unvalidated-user-pointer caveat as open(), path
	// crosses the ring-3 boundary with no bounds/mapping check yet.

	if (point_create(path) != 0) {
		return PANUTIERRNO_PLAINERR; // name taken, registry full, or point pool exhausted
	}
	
	return PANUTIERRNO_PLAINSUCCESS;
}