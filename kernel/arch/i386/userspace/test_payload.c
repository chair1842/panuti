#include <stdint.h>
#include <kernel/memman/memman.h>

#define TEST_PAYLOAD_VIRT 0xB0001000
#define TEMP_MAP_VIRT     0xC0500000

static const uint8_t test_payload_code[] = {
	0xB8, 0x01, 0x00, 0x00, 0x00,  /* mov eax, 1 (SYSHANDLER_EXIT) */
	0x31, 0xDB,                     /* xor ebx, ebx (a1 = 0) */
	0x89, 0xE1,                     /* mov ecx, esp */
	0xBA, 0x10, 0x10, 0x00, 0xB0,  /* mov edx, 0xB0001010 */
	0x0F, 0x34,                     /* sysenter */
	0xEB, 0xFE                      /* jmp $ (should never reach) */
};

void (*test_payload_install(addr_space_t addr_space))(void) {
	uint32_t phys = memman_alloc_frame();
	memman_map_in(addr_space, TEST_PAYLOAD_VIRT, phys, MEMMAN_PRESENT | MEMMAN_USER);

	memman_map_in(memman_get_kernel_addr_space(), TEMP_MAP_VIRT, phys, MEMMAN_PRESENT | MEMMAN_RW);
	uint8_t* tmp = (uint8_t*)TEMP_MAP_VIRT;
	
	for (unsigned i = 0; i < sizeof(test_payload_code); i++) {
		tmp[i] = test_payload_code[i];
	}
	
	memman_unmap_in(memman_get_kernel_addr_space(), TEMP_MAP_VIRT);

	return (void (*)(void))TEST_PAYLOAD_VIRT;
}