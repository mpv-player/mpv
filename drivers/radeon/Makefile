KERNEL_INCLUDES = /usr/src/linux/include
INCLUDES = -I$(KERNEL_INCLUDES)

#
# standard CFLAGS
#

CPPFLAGS := -D__KERNEL__ -I$(KERNEL_INCLUDES)

CFLAGS := $(CPPFLAGS) -Wall -Wstrict-prototypes -Wno-trigraphs -O2 \
	-fomit-frame-pointer -fno-strict-aliasing -fno-common\
	-D__KERNEL__  -DMODULE  -include $(KERNEL_INCLUDES)/linux/modversions.h
AFLAGS := -D__ASSEMBLY__ $(CPPFLAGS)


all: radeonfb.o

radeonfb.o: radeonfb.c radeon.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(basename $@).c

dep:

clean: 
	rm -f *.o *~

distclean: clean
	rm -f mga_vid_test

install:
	@echo Please copy *.o files to '/lib/modules/LINUX_VER/kernel/drivers/video' folder

