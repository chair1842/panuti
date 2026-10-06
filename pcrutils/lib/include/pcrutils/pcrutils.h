/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PCRUTILS_PCRUTILS_H
#define _PCRUTILS_PCRUTILS_H

#include <stddef.h>

int pcr_write_all(int stream, const char* buf, size_t len);

int pcr_output(int no_streams, const char* buf, size_t len, int only_out0);


#endif