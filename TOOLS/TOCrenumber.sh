#!/bin/sh
#
# Very simple tool to convert numeric TOC links to alphabetic (for translators,
# it is already done in english DOCS)
#
# Usage:
#  cd DOCS/French
#  ...
#  TOCrenumber.sh 2.3.1.2.1 xv_3dfx
#  ...
#
# by Gabucino
#

for i in *html; do
  cat $i | sed s/#$1\"/#$2\"/ > $i.new
  mv -f $i.new $i
  cat $i | sed s/NAME="$1"\>/NAME="$2"\>/ > $i.new
  mv -f $i.new $i
done
