KERNEL_DIR=/lib/modules/`uname -r`/build
KERNEL_INCLUDES= $(KERNEL_DIR)/include
VERSION = $(shell grep UTS_RELEASE $(KERNEL_INCLUDES)/linux/version.h | cut -d '"' -f2)
MDIR = /lib/modules/$(VERSION)/misc

MGA_VID_OBJ = mga_vid.o
CFLAGS = -O2 -D__KERNEL__ -DMODULE -I$(KERNEL_INCLUDES) -Wall -include $(KERNEL_INCLUDES)/linux/modversions.h

all: $(MGA_VID_OBJ) mga_vid_test tdfx_vid.o tdfx_vid_test

mga_vid.o: mga_vid.c mga_vid.h
	$(CC) $(CFLAGS) -c $<

mga_vid_test: mga_vid_test.c
	$(CC) -O -o $@ $<

tdfx_vid.o: tdfx_vid.c 3dfx.h
	$(CC) $(CFLAGS) -c $<

tdfx_vid_test: tdfx_vid_test.c
	$(CC) -O -o $@ $<

install: $(MGA_VID_OBJ)
	-mkdir -p $(MDIR)
	install -m 644 $(MGA_VID_OBJ) $(MDIR)
	depmod -a

dep depend:

clean:
	rm -f *.o *~ mga_vid_test tdfx_vid_test

distclean: clean

.PHONY: all install dep depend clean distclean
