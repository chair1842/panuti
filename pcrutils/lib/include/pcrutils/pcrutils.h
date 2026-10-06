/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PCRUTILS_PCRUTILS_H
#define _PCRUTILS_PCRUTILS_H

#include <stddef.h>

int pcr_write_all(int stream, const char* buf, size_t len);

int pcr_output(int no_streams, const char* buf, size_t len, int only_out0);

bool pcr_help_wanted(int argc, char** argv, const char* help);

bool pcr_option_provided(int argc, char** argv, const char* option);

#endif