KERNEL_INCLUDES = /usr/src/linux/include
INCLUDES = -I$(KERNEL_INCLUDES)
CFLAGS = -O2 -D__KERNEL__ -DMODULE -include $(KERNEL_INCLUDES)/linux/modversions.h
VERSION = $(shell grep UTS_RELEASE $(KERNEL_INCLUDES)/linux/version.h | cut -d '"' -f2)
MDIR = /lib/modules/$(VERSION)/misc

all: dhahelper.o test

dhahelper.o: dhahelper.c dhahelper.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(basename $@).c

test: test.c
	$(CC) -O $(INCLUDES) -o $@ $@.c

install: dhahelper.o
	if test ! -d $(MDIR) ; then mkdir -p $(MDIR) ; fi
	install -m 644 dhahelper.o $(MDIR)/dhahelper.o
	depmod -a

dep:

clean: 
	rm -f *.o *~

distclean: clean
	rm -f test
