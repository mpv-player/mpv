
KERNEL_INCLUDES = /usr/src/linux/include
INCLUDES = -I$(KERNEL_INCLUDES)
CFLAGS = -g -O2  -Wall -D__KERNEL__  -DMODULE  	 -include $(KERNEL_INCLUDES)/linux/modversions.h

all: mga_vid.o mga_vid_test
# sis_vid.o

mga_vid.o: mga_vid.c mga_vid.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(basename $@).c

sis_vid.o: sis_vid.c sis_vid.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(basename $@).c

mga_vid_test: mga_vid_test.c
	$(CC) -g -O -Wall $(INCLUDES) -o $@ $@.c

dep:

clean: 
	rm -f *.o *~

distclean: clean
	rm -f mga_vid_test
