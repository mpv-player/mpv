#!/bin/sh
# This script fixes up symbol mangling in GNU as code of mp3lib.
# (c)2001-2002 by Felix Buenemann <atmosfear at users.sourceforge.net>A
# This file is licensed under the GPL, more info at http://www.fsf.org/
for i in \
	"CpuDetect" \
	"ipentium" \
	"a3dnow" \
	"isse"
do
echo "fixing: $i=_$i"
objcopy --redefine-sym "$i=_$i" libMP3.a
done

