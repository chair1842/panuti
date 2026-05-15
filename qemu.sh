#!/bin/sh
set -e
. ./iso.sh

qemu-system-$(./target-triplet-to-arch.sh $HOST) -cdrom panuti.iso -no-reboot -serial stdio -no-shutdown -d int -D qemu.log
