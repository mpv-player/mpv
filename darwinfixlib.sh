#!/bin/sh
if [ `uname -s` = 'Darwin' ]; then
  echo "Fixing libs with ranlib for Darwin (MacOSX)"
  for i in $* ; do
    if (echo $i | grep \\.a) >/dev/null 2>&1; then
      echo "ranlib $i"
      (ranlib $i) >/dev/null 2>&1
    fi
  done
fi
exit 0
