include ../config.mak

# Generated automatically from Makefile.in by configure.
DEFINES=-rdynamic -fPIC $(WIN32_PATH) $(CDOPT) -D__WINE__ -Ddbg_printf=__vprintf \
	-DTRACE=__vprintf # -DDETAILED_OUT

LIB_OBJECTS= setup_FS.o pe_image.o module.o \
ext.o win32.o driver.o pe_resource.o \
resource.o registry.o elfdll.o afl.o vfl.o

CFLAGS=-g -I.

CDOPT=-g
all: libloader.a
    
clean:
	-rm -f *.o libloader.a

distclean: clean

.c.o: $@
	$(CC) $(CFLAGS) $(DEFINES) -c $<

libloader.a:  $(LIB_OBJECTS) stubs.s
	$(CC) -c ./stubs.s -o stubs.o
	$(AR) -r libloader.a $(LIB_OBJECTS) stubs.o

dep:
	echo "dependency not required/supported"
