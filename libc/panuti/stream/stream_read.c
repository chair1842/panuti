#include <panuti/stream.h>
#include <panuti/syscall/syscallsf.h>

int stream_read(int instream_no, void* data_out, size_t len) {
	return panutisysf_stream_read(instream_no, data_out, len);
}