#include <panuti/syscall/syscallsf.h>

int main(void) {
	panutisysf_vga('H');
	panutisysf_vga('i');
	panutisysf_vga('!');
	return 42;
}