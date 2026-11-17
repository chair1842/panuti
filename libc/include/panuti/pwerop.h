/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PANUTI_PWEROP_H
#define _PANUTI_PWEROP_H

#if defined(__cplusplus)
extern "C" {
#endif

// these operations just do them, no preperating. just cold servitude
// so don't use these naively

// reboot/restart/reset the system
void pwerop_reboot();

// poweroff the system
void pwerop_shutdown();

#if defined(__cplusplus)
}
#endif

#endif