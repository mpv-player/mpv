include ../config.mak

# Generated automatically from Makefile.in by configure.
DEFINES=-DMPLAYER -D__WINE__ #-Ddbg_printf=__vprintf \
	#-DTRACE=__vprintf # -DDETAILED_OUT

ifeq ($(TARGET_WIN32),yes)
LIB_OBJECTS= driver.o afl.o vfl.o
else
LIB_OBJECTS= ldt_keeper.o pe_image.o module.o ext.o win32.o \
             driver.o pe_resource.o resource.o registry.o \
             elfdll.o afl.o vfl.o wrapper.o
endif

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

libloader.a:  $(LIB_OBJECTS) stubs.s
	$(CC) -c ./stubs.s -o stubs.o
ifeq ($(TARGET_OS),OpenBSD)
	./loader_objfix.sh
endif
	$(AR) -r libloader.a $(LIB_OBJECTS) stubs.o
	$(RANLIB) libloader.a

dep:
	echo "dependency not required/supported"
