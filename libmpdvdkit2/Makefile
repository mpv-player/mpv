
ifneq ($(wildcard ../config.mak),)
include ../config.mak
endif

SRCS = css.c device.c dvd_input.c dvd_reader.c dvd_udf.c error.c ifo_print.c ifo_read.c ioctl.c libdvdcss.c nav_print.c nav_read.c

#bsdi_ioctl.c 

OBJS = $(SRCS:.c=.o)

LIB=libmpdvdkit

.SUFFIXES: .c .o

ifeq ($(OPTFLAGS),)
 OPTFLAGS = -mcpu=pentium -march=pentium -O4
else
 OPTFLAGS += -DHAVE_MPLAYER
endif

#	 \ -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64

# -funroll-loops  removed, triggered gcc 3.0.4 (3.x?) bug
CFLAGS= -I. $(OPTFLAGS) $(EXTRA_INC)\
	-DSYS_LINUX -D__USE_UNIX98 -D_REENTRANT -D_GNU_SOURCE \
	-ffast-math -fomit-frame-pointer 

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

all: $(LIB).a 
default: $(LIB).a

$(LIB).a: $(OBJS)
	$(AR) rc $(LIB).a $(OBJS)

$(LIB).so: $(OBJS)
	$(CC) -o $(LIB).so -shared -rdynamic $(OBJS)

clean:
	rm -f $(OBJS) $(LIB).a $(LIB).so .depend

distclean: clean

dep:    depend

depend: .depend

.depend: Makefile config.h
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)                                                                       
include .depend                                                                                    
endif                                                                                              
