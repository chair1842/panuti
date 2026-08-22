# Building

## You need these packages to build panuti:

- the i686-panuti binutils and gcc toolchain, look at [toolchain.md] for build instructions. required to build everything here
- GRUB, you dont need it as your bootloader, you just need it installed. requierd for `./build.sh` and `./iso.sh`
- xorriso, for making the panuti iso. required for `./iso.sh`
- qemu, or more specific qemu-system-i386. used for emulation and testing. required for `./qemu.sh`

## Actually building

for most cases, you should be fine with ./qemu.sh for both building-then-testing.

`./qemu.sh` runs `./iso.sh` which runs `./build.sh`, so you dont have to run `./build.sh` at all ig.