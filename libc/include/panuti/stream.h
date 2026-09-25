#ifndef _PANUTI_STREAM_H
#define _PANUTI_STREAM_H

#include <stddef.h>

// read len bytes from instream_no to data_out
int stream_read(int instream_no, void* data_out, size_t len);

// write len bytes of data to outstream_no
int stream_write(int outstream_no, const void* data, size_t len);

// output how many streams are there of in and out to a array of size 2.
// in streams are in out[0], out streams are in out[1]
void nstream(int out[2]);

#endif