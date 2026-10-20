# Panuti

a hobbyist os

## Gifs

<img width="49%" alt="conway of life running on panuti" src="https://github.com/user-attachments/assets/8bc1d011-ece4-4487-a12a-779d88c42489" /> <img width="49%" alt="really slow editing on nib" src="https://github.com/user-attachments/assets/7fc04482-d8b3-457b-930c-872cb8978654" />


## How to try out

You can use panuti on any virtual machine (the type of virt machine doesn't make a difference)

These instructions will use QEMU.

```
qemu-system-i386 -cdrom panuti.iso
```

if you want to try out hard disks,
get an already formatted to ext2 disk image and use it.
Panuti does not have a utility to format yet.

```
qemu-system-i386 -cdrom panuti.iso -hda disk.img
```

to mount your formatted disk, type these commands out on panuti:

```
mkdir mnt
mount mnt ext2 /dvc/pata/0
```

make sure to unmount before shutting down

## Current limitations

- Pur and Pint both assume unconditionally that the OS is being booted from a cd
- Pur hardcodes its PATH at `/cd/usr/bin`
- only 1024 inodes can exist at a time.
- only 2048 dirents can exist at a time
- only 64 tasks at a time.

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

## Building

go to the build instructions over here:  
👇

[building.md](doc/building.md)

## Making programs

[link here](doc/mkapps.md)
