#!/bin/sh

echo "#define VERSION \"0.17cvs-"`date -r CVS/Entries +%y%m%d-%H:%M`"\"" >version.h

