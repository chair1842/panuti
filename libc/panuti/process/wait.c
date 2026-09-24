#include <panuti/process.h>
#include <panuti/syscall/syscallsf.h>

int32_t wait(pid_t pid, int* ec_out) {
	return panutisysf_wait(pid, ec_out);
}