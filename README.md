# Panuti

a unix-inspired hobbyist OS

## Gifs

<img width="49%" alt="conway of life running on panuti" src="https://github.com/user-attachments/assets/8bc1d011-ece4-4487-a12a-779d88c42489" /> <img width="49%" alt="really slow editing on nib" src="https://github.com/user-attachments/assets/7fc04482-d8b3-457b-930c-872cb8978654" />

## Features

- Multitasking I386 kernel (no multiprocessing yet)
- N-ary standard streams
- VGA text mode, PC Speaker, Serial UART supported
- PATA and CD-ROM support
- ISO9660 and Ext2 filesystems
- Has it's very own coreutils, vi/vim clone, and shell
- Rename `/dev` to `/dvc` because why not

## How to try out

You can use panuti on any virtual machine (ofc)

These instructions will use QEMU.

```
qemu-system-i386 -cdrom panuti.iso
```

if you want to try out hard disks,  
get a blank hard disk image and supply it to qemu:

```
qemu-system-i386 -cdrom panuti.iso -hda DISKIMG
```

then in panuti, format the disk to ext2 before mounting it:

```
ffs-ext2 /dvc/pata/0
```

after formatting, you can mount the disk:

```
mkdir mnt
mount mnt ext2 /dvc/pata/0
```

make sure to unmount before shutting down

## Planned for 0.2

- ~~Make registry inodes and dirents dynamic~~
- ~~Make tasks dynamic~~
- ~~add time (read from RTC at boot to sync) (`/dvc/time/clock`)~~
- ~~add support for the [PC Speaker](https://en.wikipedia.org/wiki/PC_speaker) (`/dvc/beepr`)~~
- ~~add formatting ext2~~
- ~~add sleep functionality to the kernel + syscall~~
- add motions and operators to nib

## Current limitations

- Pur and Pint both assume unconditionally that the OS is being booted from a cd:
  - Pint always mounts `/dvc/cdrom/0` to `/cd` and launches pur from there
  - Pur hardcodes its PATH at `/cd/usr/bin`
- mounting at places that alr have registry nodes (such as root) will hide those registry nodes
- mounting at root will hide `/dvc` which,,, yk.
- mounting on a subdir of a mount is not allowed. so no `/mnt1/mnt2`

that's all i can think about :)

## Projects

- [kernel](kernel/README.md)

    the core of the OS

- [libc](libc/README.md)

    🤷 i dunno how to explain this

- [pint](pint/README.md)

    an init system if you can call it one today

- [pur](pur/README.md)

    a shell

- [pcrutils](pcrutils/README.md)

    a group of basic small and composable tools

- [nib](nib/README.md)

    a vim/vi clone

- [calc](calc/README.md)

    a console calculator

- [conway](conway/README.md)

    conway's game of life

- [fstools](fstools/README.md)

    tools for maintaining filesystems

## Building

go to the build instructions over here:  
👇

[building.md](doc/building.md)

## Making programs

[link here](doc/mkapps.md)
