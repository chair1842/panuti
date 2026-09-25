#include <panuti/stream.h>
#include <panuti/syscall/syscallsf.h>

int stream_write(int outstream_no, const void* data, size_t len) {
	return panutisysf_stream_write(outstream_no, data, len);
};