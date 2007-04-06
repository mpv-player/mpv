KERNEL_INC = /usr/src/linux/include
CFLAGS = -O2 -D__KERNEL__ -DMODULE -I$(KERNEL_INC) \
  -include $(KERNEL_INC)/linux/modversions.h
VERSION = $(shell grep RELEASE $(KERNEL_INC)/linux/version.h | cut -d'"' -f2)
MDIR = /lib/modules/$(VERSION)/misc

all: dhahelper.o test

dhahelper.o: dhahelper.c dhahelper.h

install: dhahelper.o
	-mkdir -p $(MDIR)
	install -m 644 dhahelper.o $(MDIR)/dhahelper.o
	depmod -a

dep depend:

clean:
	rm -f *.o *~ test

distclean: clean
