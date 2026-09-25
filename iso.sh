#!/bin/sh
set -e
. ./build.sh

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub
mkdir -p isodir/usr/bin

cp sysroot/boot/panuti_kern isodir/boot/panuti_kern
cp sysroot/boot/pint isodir/boot/pint
cp sysroot/usr/bin/* isodir/usr/bin/
cat > isodir/boot/grub/grub.cfg << EOF
set timeout=2
set default=0
menuentry "panuti" {
	multiboot /boot/panuti_kern
	module /boot/pint
}
EOF
grub-mkrescue -o panuti.iso isodir
