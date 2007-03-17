KERNEL_INCLUDES = /lib/modules/`uname -r`/build/include
VERSION = $(shell grep UTS_RELEASE $(KERNEL_INCLUDES)/linux/version.h | cut -d '"' -f2)
MDIR = /lib/modules/$(VERSION)/misc

CFLAGS = -O2 -D__KERNEL__ -DMODULE -I$(KERNEL_INCLUDES) -Wall -include $(KERNEL_INCLUDES)/linux/modversions.h

all: mga_vid.o mga_vid_test tdfx_vid.o tdfx_vid_test

mga_vid.o: mga_vid.c mga_vid.h
tdfx_vid.o: tdfx_vid.c 3dfx.h

%_test: %_test.c
	$(CC) -O -o $@ $<

install:
	-mkdir -p $(MDIR)
	install -m 644 mga_vid.o tdfx_vid.o $(MDIR)
	depmod -a

dep depend:

clean:
	rm -f *.o *~ mga_vid_test tdfx_vid_test

distclean: clean

.PHONY: all install dep depend clean distclean
