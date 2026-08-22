# how to build the i686-panuti binutils and gcc toolchain

## dependencies

[https://osdev.wiki/wiki/GCC_Cross-Compiler#Installing_Dependencies]

install these dependencies in your system. these are required by both gcc and binutils.

## binutils

### patching

1. clone `https://sourceware.org/git/binutils-gdb.git`
2. switch your branch to the `binutils-2_47-branch` branch.
3. copy toolchain/panuti-binutils.patch to your cloned binutils-gdb repo
4. change your current directory to the cloned repo
5. run `git apply panuti-binutils.patch`

### configuring

in the cloned repo, make a `build` folder and then change your current directory to the newly created dir.

then run `../configure --disable-gdb --target=i686-panuti --prefix="$HOME/.local/cross" --with-sysroot="$HOME/panuti/sysroot" --disable-werror --disable-nls` to generate makefiles to run

$HOME/panuti is where the panuti repo is located so change that to wherever you have the repo cloned to.

$HOME/.local/cross is where the toolchain is installed to, so preferably add "your-prefix/bin" to your PATH.

### building

in the build folder, run in order:

`
make -j$(nproc)
make install -j$(nproc)
`

## gcc

you must have the panuti repo cloned in your machine already for this/

### patching

1. clone `https://gcc.gnu.org/git/gcc.git`
2. switch to the `releases/gcc-16` branch.
3. copy toolchain/panuti-gcc.patch to your cloned gcc repo
4. change your current directory to the cloned repo
5. run `git apply panuti-gcc.patch`

### configuring

in the cloned repo, make a `build` folder and then change your current directory to the newly created dir.

then run `../configure --target=i686-panuti --prefix="$HOME/.local/cross" --with-sysroot="$HOME/panuti/sysroot" --enable-languages=c,c++ --disable-nls` to generate makefiles to run

$HOME/panuti is where the panuti repo is located so change that to wherever you have the repo cloned to.

$HOME/.local/cross is where the toolchain is installed to, so preferably add "your-prefix/bin" to your PATH.

### copying headers to sysroot

This step is required for building.

if you dont do this step, you will get a bunch of annoying errors about missing headers and not build at all and then you have to do this step anyway.

in the panuti repo, run `./headers.sh` to copy headers to sysroot/ so that they can be used

### building

in the build folder, run in order (don't worry if gcc takes a long time to build, its like that):

`
make all-gcc all-target-libgcc -j$(nproc)
make install-gcc install-target-libgcc -j$(nproc)
`