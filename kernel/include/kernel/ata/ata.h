/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KERNEL_ATA_ATA_H
#define _KERNEL_ATA_ATA_H

// scan every probed drive slot for plain ata disks, register each one as
// /dvc/pata/N and publish its mbr partitions as /dvc/pata/NpM. call once, after
// ide_init() has probed the buses and after /dvc/pata exists
void ata_init(void);

#endif
