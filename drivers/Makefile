KERNEL_INCLUDES = /lib/modules/`uname -r`/build/include
VERSION = $(shell grep UTS_RELEASE $(KERNEL_INCLUDES)/linux/version.h | cut -d '"' -f2)
MDIR = /lib/modules/$(VERSION)/misc

CFLAGS = -O2 -D__KERNEL__ -DMODULE -I$(KERNEL_INCLUDES) -Wall -include $(KERNEL_INCLUDES)/linux/modversions.h

OBJS = mga_vid.o tdfx_vid.o radeon/radeon_vid.o radeon/rage128_vid.o

all: $(OBJS) mga_vid_test tdfx_vid_test

mga_vid.o: mga_vid.c mga_vid.h
tdfx_vid.o: tdfx_vid.c 3dfx.h
radeon/radeon_vid.o: radeon/radeon_vid.c radeon/radeon.h radeon/radeon_vid.h
radeon/rage128_vid.o: radeon/radeon_vid.c radeon/radeon.h radeon/radeon_vid.h
	$(CC) $(CFLAGS) -DRAGE128 -c $< -o $@
radeon/radeon_vid.o radeon/rage128_vid.o:
  CFLAGS += -fomit-frame-pointer -fno-strict-aliasing -fno-common -ffast-math

%_test: %_test.c
	$(CC) -O -o $@ $<

install:
	-mkdir -p $(MDIR)
	install -m 644 $(OBJS) $(MDIR)
	depmod -a
	-mknod /dev/mga_vid  c 178 0
	-mknod /dev/tdfx_vid c 178 0
	-mknod /dev/radeon_vid c 178 0
	-ln -s /dev/radeon_vid /dev/rage128_vid

dep depend:

clean:
	rm -f *.o *~ mga_vid_test tdfx_vid_test
	rm -f radeon/*.o radeon/*~

distclean: clean

.PHONY: all install dep depend clean distclean
