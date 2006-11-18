
MPROOT=../..

include $(MPROOT)/config.mak

INCLUDE = -I$(MPROOT) -I$(MPROOT)/loader
CFLAGS  = $(OPTFLAGS) $(INCLUDE)

.SUFFIXES: .c .cpp .o

# .PHONY: all clean

all: netstream

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<


# FIXME: linking is a mess that should be fixed properly some day
# it does not work with either GUI, LIVE555 or libavformat enabled
DEPS = $(MPROOT)/libmpdemux/libmpdemux.a \
	$(MPROOT)/stream/stream.a \
	$(MPROOT)/dvdread/libdvdread.a \
	$(MPROOT)/libmpcodecs/native/minilzo.o \
	$(MPROOT)/libvo/aclib.o \
	$(MPROOT)/libmpcodecs/img_format.o \
	$(MPROOT)/liba52/liba52.a \
	$(MPROOT)/m_option.o \
	$(MPROOT)/m_struct.o \
	$(MPROOT)/subreader.o \
	$(MPROOT)/mp_msg.o \
	$(MPROOT)/osdep/shmem.o \
	$(MPROOT)/osdep/timer-*.o \
	$(MPROOT)/osdep/strl.o \
	$(COMMON_LIBS) \
	-lm \

ifeq ($(TARGET_WIN32),yes)
DEPS += $(MPROOT)/osdep/glob-win.o -lwsock32
endif

netstream: $(MPROOT)/libmpdemux/libmpdemux.a $(MPROOT)/stream/stream.a netstream.o

	$(CC) $(CFLAGS) netstream.o -o netstream $(DEPS)

clean:
	rm -f *.o *.a *~

distclean: clean
	rm -f .depend test

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) test.c $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
