#include <panuti/syscall/syscallsf.h>

int main(void) {
	int pid = panutisysf_getpid();
	int console = panutisysf_open("/dvc/console");

	if (pid == 2) {
		// waiter
		panutisysf_point_create("/dev/testpoint");
		int fd = panutisysf_open("/dev/testpoint");
		panutisysf_write(console, "waiter: waiting\n", 17);
		panutisysf_wait(fd);
		panutisysf_write(console, "waiter: woke up!\n", 18);
	} else {
		// activator (pid 1) — crude delay to give the waiter a chance
		// to run first and actually block before we activate. See note below.
		for (volatile int i = 0; i < 5000000; i++) {}

		int fd = panutisysf_open("/dev/testpoint");
		panutisysf_write(console, "activator: activating\n", 23);
		panutisysf_activate(fd);
	}

	return 0;
}