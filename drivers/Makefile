KERNEL_DIR=/lib/modules/`uname -r`/build
KERNEL_INCLUDES= $(KERNEL_DIR)/include
INCLUDES = -I$(KERNEL_INCLUDES) -I$(KERNEL_INCLUDES)/asm
VERSION = $(shell grep UTS_RELEASE $(KERNEL_INCLUDES)/linux/version.h | cut -d '"' -f2)
MDIR = /lib/modules/$(VERSION)/misc

ifneq (,$(findstring 2.6, $(VERSION)))
obj-m += mga_vid.o
CFLAGS = -O2  -D__KERNEL__  -DMODULE  	 -include $(KERNEL_INCLUDES)/config/modversions.h -Wall
else
CFLAGS = -O2  -D__KERNEL__  -DMODULE  	 -include $(KERNEL_INCLUDES)/linux/modversions.h -Wall
endif

all:
ifneq (,$(findstring 2.6, $(VERSION)))
	$(MAKE) mga_vid.ko mga_vid_test
else
	$(MAKE) mga_vid.o mga_vid_test
endif
# sis_vid.o

mga_vid.o: mga_vid.c mga_vid.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(basename $@).c

mga_vid.ko: mga_vid.c mga_vid.h
	$(MAKE) -C $(KERNEL_DIR) SUBDIRS=$(PWD) modules

sis_vid.o: sis_vid.c sis_vid.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(basename $@).c

mga_vid_test: mga_vid_test.c
	$(CC) -O $(INCLUDES) -o $@ $@.c

tdfx_vid.o: tdfx_vid.c 3dfx.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(basename $@).c

tdfx_vid_tst: tdfx_vid_tst.c
	$(CC) -O $(INCLUDES) -o $@ $@.c

install: mga_vid.o
	-mkdir -p $(MDIR)
ifneq (,$(findstring 2.6, $(VERSION)))
	install -m 644 mga_vid.ko $(MDIR)/mga_vid.ko
else
	install -m 644 mga_vid.o $(MDIR)/mga_vid.o
endif
	depmod -a

dep depend:

clean: 
	rm -f *.o *.a *~
	rm -f *.ko .mga* mga_vid.mod.c

distclean: clean
	rm -f mga_vid_test tdfx_vid_tst

