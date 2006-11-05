
ifneq ($(wildcard ../config.mak),)
include ../config.mak
endif

SRCS = css.c \
       device.c \
       error.c \
       ioctl.c \
       libdvdcss.c \
       #bsdi_ioctl \

OBJS = $(SRCS:.c=.o)

LIB=libdvdcss

.SUFFIXES: .c .o

ifeq ($(OPTFLAGS),)
OPTFLAGS = -mcpu=pentium -march=pentium -O4
endif

# -funroll-loops  removed, triggered gcc 3.0.4 (3.x?) bug
CFLAGS= -I.. $(OPTFLAGS) -D__USE_UNIX98 -D_GNU_SOURCE \
	-DHAVE_LIMITS_H -DHAVE_ERRNO_H -DHAVE_INTTYPES_H -DHAVE_UNISTD_H \
	-D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DVERSION=\"1.2.9\"

ifeq ($(TARGET_OS),CYGWIN)
CFLAGS+=-DSYS_CYGWIN
endif

ifeq ($(TARGET_OS),Darwin)
CFLAGS+=-D__DARWIN__
endif

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

all: $(LIB).a

$(LIB).a: $(OBJS)
	$(AR) rc $(LIB).a $(OBJS)
	$(RANLIB) $(LIB).a

clean:
	rm -f *.o *.a *~ *.so

distclean: clean
	rm -f .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
