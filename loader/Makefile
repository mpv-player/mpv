include ../config.mak

# Generated automatically from Makefile.in by configure.
DEFINES=-DMPLAYER -D__WINE__ #-Ddbg_printf=__vprintf \
	#-DTRACE=__vprintf # -DDETAILED_OUT

SRCS= driver.c afl.c vfl.c
ifneq ($(TARGET_WIN32),yes)
SRCS+= ldt_keeper.c pe_image.c module.c ext.c win32.c \
       pe_resource.c resource.c registry.c elfdll.c wrapper.S
endif

OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)

# gcc-3.0 produces buggy code for acmStreamOpen() with
# "-O3 -fomit-frame-pointer" or "-O2 -fomit-frame-pointer
# -finline-functions -frename-registers" (code is OK with sole -O2),
# the bad code accesses parameters via %ebp without setting up a
# propper %ebp first!
# -fno-omit-frame-pointer works around this gcc-3.0 bug.  gcc-2.95.2 is OK.
# Note: -D_FILE_OFFSET_BITS=32 is required to disable using mmap64(),
# as it's broken in glibc 2.1.2 (bad header) and 2.1.3 (bad code)
CFLAGS=-I. -I.. $(OPTFLAGS) -U_FILE_OFFSET_BITS -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer
#CFLAGS=-I. -I.. -O -g #-fno-omit-frame-pointer

all: libloader.a

clean:
	rm -f *.o *.a *~

distclean: clean

.c.o:
	$(CC) $(CFLAGS) $(DEFINES) -c $<

libloader.a:  $(OBJS) stubs.s
	$(CC) -c ./stubs.s -o stubs.o
	$(AR) -r libloader.a $(OBJS) stubs.o
	$(RANLIB) libloader.a

dep depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
