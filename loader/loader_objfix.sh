#!/bin/sh
# This script fixes up symbol mangling in GNU as code of stubs.s.
# This file is licensed under the GPL, more info at http://www.fsf.org/
for i in "export_names" \
       "printf" \
       "exp_EH_prolog" \
       "unk_exp1"
do
echo "fixing: $i=_$i"
objcopy --redefine-sym "$i=_$i" stubs.o
done
