#!/bin/sh
set -e
. ./headers.sh

if command -v bear >/dev/null 2>&1; then
  MAKE="bear -- $MAKE"
fi

for PROJECT in $PROJECTS; do
  (cd $PROJECT && DESTDIR="$SYSROOT" $MAKE install)
done
