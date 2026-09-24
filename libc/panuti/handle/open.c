#include <panuti/handle.h>
#include <panuti/syscall/syscallsf.h>

int handle_open(const char* path) {
	return panutisysf_open(path);
}