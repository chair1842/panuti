#include <panuti/syscall/syscallsf.h>

int main(void) {
	int fd = panutisysf_open("/dvc/console");
	if (fd < 0) {
		panutisysf_exit(1);
	}
	const char* msg = "PINT\n";
	panutisysf_write(fd, msg, 5);
	return 42;
}