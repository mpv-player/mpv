
MPROOT=../..

include $(MPROOT)/config.mak

INCLUDE = -I$(MPROOT) -I$(MPROOT)/loader $(EXTRA_INC)
CFLAGS  = $(OPTFLAGS) $(INCLUDE)

.SUFFIXES: .c .cpp .o

# .PHONY: all clean

all: netstream

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<


netstream: $(MPROOT)/libmpdemux/libmpdemux.a netstream.o
	$(CC) $(CFLAGS) -g netstream.o $(MPROOT)/mp_msg.c $(MPROOT)/osdep/shmem.c -o netstream $(MPROOT)/libmpdemux/libmpdemux.a $(MPROOT)/libmpdvdkit2/libmpdvdkit.a $(MPROOT)/libvo/aclib.o $(MPROOT)/libmpcodecs/img_format.o $(MPROOT)/libao2/afmt.o $(MPROOT)/sub_cc.o $(MPROOT)/m_option.o $(MPROOT)/m_struct.o $(MPROOT)/subreader.o $(ALSA_LIB) $(VORBIS_LIB) $(CDPARANOIA_LIB) -lz -lpthread

clean:
	rm -f *.o *.a *~

distclean:
	rm -f test Makefile.bak *.o *.a *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) test.c $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
