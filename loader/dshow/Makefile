
LIBNAME = libDS_Filter.a

include ../../config.mak

SRCS	= DS_AudioDec.cpp DS_VideoDec.cpp DS_Filter.cpp allocator.cpp cmediasample.cpp guids.cpp inputpin.cpp outputpin.cpp
OBJS = $(SRCS:.cpp=.o)

# OBJS	= DS_AudioDec.o DS_VideoDec.o DS_Filter.o allocator.o cmediasample.o guids.o inputpin.o outputpin.o

INCLUDE = -I. -I.. -I../wine
CFLAGS  = $(OPTFLAGS) $(INCLUDE)

.SUFFIXES: .cpp .o

# .PHONY: all clean

.cpp.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):	.depend $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

test:   test.c $(LIBNAME)
	$(CC) test.c -Wall $(CFLAGS) -o test -L. -lDS_Filter -L.. -lloader $(ARCH_LIBS) -lstdc++

all:	$(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f Makefile.bak *.o *.a *~ .depend test test.raw

dep:    depend
depend: .depend

.depend: Makefile ../../config.mak
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
