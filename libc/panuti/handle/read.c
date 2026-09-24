#include <panuti/handle.h>
#include <panuti/syscall/syscallsf.h>

int handle_read(int fd, void* data_out, size_t len) {
	return panutisysf_read(fd, data_out, len);
}