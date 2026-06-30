#!/bin/sh
set -e
. ./headers.sh

if command -v bear &> /dev/null; then
  for PROJECT in $PROJECTS; do
    (cd $PROJECT && DESTDIR="$SYSROOT" bear -- $MAKE install)
  done
else
  echo "If you are using Zed (clang), please install bear."
  for PROJECT in $PROJECTS; do
    (cd $PROJECT && DESTDIR="$SYSROOT" $MAKE install)
  done
fi