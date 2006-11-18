
ifneq ($(wildcard ../config.mak),)
include ../config.mak
endif

SRCS = dvd_input.c \
       dvd_reader.c \
       dvd_udf.c \
       ifo_print.c \
       ifo_read.c \
       nav_print.c \
       nav_read.c \
       md5.c \

OBJS = $(SRCS:.c=.o)

LIB=libdvdread

.SUFFIXES: .c .o

CFLAGS= -I.. $(OPTFLAGS) -D__USE_UNIX98 -D_GNU_SOURCE -DSTDC_HEADERS \
	-DHAVE_LIMITS_H -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64

ifeq ($(DVDCSS_INTERNAL),yes)
CFLAGS += -I../libdvdcss -DHAVE_DVDCSS_DVDCSS_H
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
