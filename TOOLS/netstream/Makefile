
MPROOT=../..

include $(MPROOT)/config.mak

CFLAGS  = $(OPTFLAGS) -I$(MPROOT) -I$(MPROOT)/loader

all: netstream

# FIXME: linking is a mess that should be fixed properly some day
# it does not work with either GUI, LIVE555 or libavformat enabled
DEPS = $(MPROOT)/libmpdemux/libmpdemux.a \
	$(MPROOT)/stream/stream.a \
	$(MPROOT)/dvdread/libdvdread.a \
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
	-lm \

ifeq ($(TARGET_WIN32),yes)
DEPS += $(MPROOT)/osdep/glob-win.o -lwsock32
endif

netstream: $(MPROOT)/libmpdemux/libmpdemux.a $(MPROOT)/stream/stream.a netstream.o

	$(CC) $(CFLAGS) netstream.o -o netstream $(DEPS)

clean:
	rm -f *.o *.a *~

.PHONY: all clean
