
KERNEL_INCLUDES = /usr/src/linux/include
INCLUDES = -I$(KERNEL_INCLUDES)
CFLAGS = -g -O2  -Wall -D__KERNEL__  -DMODULE  	 -include $(KERNEL_INCLUDES)/linux/modversions.h
VERSION = $(shell grep UTS_RELEASE $(KERNEL_INCLUDES)/linux/version.h | cut -d '"' -f2)
MDIR = /lib/modules/$(VERSION)/misc

all: mga_vid.o mga_vid_test
# sis_vid.o

mga_vid.o: mga_vid.c mga_vid.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(basename $@).c

sis_vid.o: sis_vid.c sis_vid.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(basename $@).c

mga_vid_test: mga_vid_test.c
	$(CC) -g -O -Wall $(INCLUDES) -o $@ $@.c

install: mga_vid.o
	if test ! -d $(MDIR) ; then mkdir -p $(MDIR) ; fi
	install -m 644 mga_vid.o $(MDIR)/mga_vid.o
	depmod -a

dep:

clean: 
	rm -f *.o *~

distclean: clean
	rm -f mga_vid_test
