/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PANUTI_STREAM_H
#define _PANUTI_STREAM_H

#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct nstreams {
	int in;
	int out;
} nstreams_t;

// read len bytes from instream_no to data_out
int stream_read(int instream_no, void* data_out, size_t len);

// write len bytes of data to outstream_no
int stream_write(int outstream_no, const void* data, size_t len);

// output how many streams are there of in and out to a nstreams_t
nstreams_t nstream();

#if defined(__cplusplus)
}
#endif

#endif
