KERNEL_INCLUDES = /usr/src/linux/include
INCLUDES = -I$(KERNEL_INCLUDES)
# auto install
CONFIG_SHELL=/bin/sh -c
KERNEL_VERSION=$(shell $(CONFIG_SHELL) 'uname -r')
MOD_PATH_PREFIX=
MOD_PATH=$(MOD_PATH_PREFIX)/lib/modules/$(KERNEL_VERSION)/kernel/drivers/video/

#
# standard CFLAGS
#

CPPFLAGS := -D__KERNEL__ -I$(KERNEL_INCLUDES)

CFLAGS := $(CPPFLAGS) -Wall -Wstrict-prototypes -Wno-trigraphs -O2 \
	-fomit-frame-pointer -fno-strict-aliasing -fno-common -ffast-math\
	-D__KERNEL__  -DMODULE  -include $(KERNEL_INCLUDES)/linux/modversions.h
AFLAGS := -D__ASSEMBLY__ $(CPPFLAGS)


all: radeonfb.o radeon_vid.o rage128_vid.o

radeonfb.o: radeonfb.c radeon.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(basename $@).c
radeon_vid.o: radeon_vid.c radeon.h radeon_vid.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(basename $@).c
rage128_vid.o: radeon_vid.c radeon.h radeon_vid.h
	$(CC) $(CFLAGS) -DRAGE128 $(INCLUDES) -c radeon_vid.c -o $@

dep:

clean: 
	rm -f *.o *~

install:
	mkdir -p $(MOD_PATH)
	cp radeonfb.o $(MOD_PATH)
	cp radeon_vid.o $(MOD_PATH)
	cp rage128_vid.o $(MOD_PATH)
	depmod -a
nodes:
	mknod /dev/radeon_vid c 178 0
	ln -s /dev/radeon_vid /dev/rage128_vid
# Stuff below is for me only ;)
vid:
	make install
	rmmod radeon_vid
	modprobe radeon_vid mtrr=1
