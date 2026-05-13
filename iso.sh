#!/bin/sh
set -e
. ./build.sh

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub

cp sysroot/boot/panuti_kern isodir/boot/panuti_kern
cat > isodir/boot/grub/grub.cfg << EOF
menuentry "panuti" {
	multiboot /boot/panuti_kern
}
EOF
grub-mkrescue -o panuti.iso isodir
