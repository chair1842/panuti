#!/bin/sh
set -e
. ./build.sh

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub

cp sysroot/boot/panuti_kern isodir/boot/panuti_kern
cp sysroot/boot/pint.elf isodir/boot/pint.elf
cat > isodir/boot/grub/grub.cfg << EOF
menuentry "panuti" {
	multiboot /boot/panuti_kern
	module /boot/pint.elf
}
EOF
grub-mkrescue -o panuti.iso isodir
