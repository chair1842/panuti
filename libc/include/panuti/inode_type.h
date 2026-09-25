/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PANUTI_INODE_TYPE_H
#define _PANUTI_INODE_TYPE_H

#include <stdint.h>

typedef enum : uint8_t {
    INODE_NONE = 0,
    INODE_DIR,
    INODE_FILE,
    INODE_BLOCK,
    INODE_PIPE,
} inode_type_t;

#endif