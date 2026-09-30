/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_ATA_ATA_H
#define _KERNEL_ATA_ATA_H

// scan every probed drive slot for plain ata disks, register each one as
// /dvc/diskN and publish its mbr partitions as /dvc/diskNpM. call once, after
// ide_init() has probed the buses and after /dvc exists
void ata_init(void);

#endif
