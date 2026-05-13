#!/bin/sh
set -e
. ./build.sh

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub

cp sysroot/boot/panuti.kernel isodir/boot/panuti.kernel
cat > isodir/boot/grub/grub.cfg << EOF
menuentry "panuti" {
	multiboot /boot/panuti.kernel
}
EOF
grub-mkrescue -o panuti.iso isodir
