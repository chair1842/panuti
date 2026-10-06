/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PCRUTILS_PCRUTILS_H
#define _PCRUTILS_PCRUTILS_H

#include <stddef.h>

typedef struct {
	char** v;
	size_t n;
	size_t cap;
} pcr_lines_t;

typedef struct {
	char* data;
	size_t len;
	size_t cap;
} pcr_buf_t;

int pcr_write_all(int stream, const char* buf, size_t len);

int pcr_output(int no_streams, const char* buf, size_t len, int only_out0);

const char* pcr_strchr(const char* s, char c);

bool pcr_help_wanted(int argc, char** argv, const char* help);

bool pcr_option_provided(int argc, char** argv, const char* option);

int pcr_buf_append(pcr_buf_t* b, const char* data, size_t n);

void pcr_buf_free(pcr_buf_t* b);

int pcr_stream_lines(int stream, pcr_lines_t* lines);

void pcr_lines_free(pcr_lines_t* lines);

void pcr_strs_sort(char** v, size_t n, bool reverse, bool numeric);

long pcr_parse_long(const char* s, bool* ok);

int pcr_uformat(unsigned long v, int base, char* out);

bool pcr_glob_match(const char* pat, const char* s);

char* pcr_path_join(const char* base, const char* name);

#endif