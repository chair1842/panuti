#include <kernel/mem/usr.h>
#include <stdint.h>

#define USER_SPACE_BASE 0x08048000u
#define USER_SPACE_END  0xC0000000u

bool kernel_is_user_ptr(const void* ptr) {
	uint32_t addr = (uint32_t)ptr;
	return addr >= USER_SPACE_BASE && addr < USER_SPACE_END;
}

bool kernel_is_user_range(const void* buf, size_t len) {
	if (len == 0) {
		return true;
	}
	uint32_t start = (uint32_t)buf;
	uint32_t end = start + (uint32_t)len;
	return start >= USER_SPACE_BASE && end >= start && end <= USER_SPACE_END;
}