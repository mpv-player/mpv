
MPROOT=../..

include $(MPROOT)/config.mak

CFLAGS  = $(OPTFLAGS) -I$(MPROOT)

all: netstream

# FIXME: linking is a mess that should be fixed properly some day
# it does not work with either GUI, LIVE555, libavformat, cdparanoia enabled
DEPS = $(MPROOT)/libmpdemux/libmpdemux.a \
	$(MPROOT)/stream/stream.a \
	$(MPROOT)/dvdread/libdvdread.a \
	$(MPROOT)/libdvdcss/libdvdcss.a \
	$(MPROOT)/libavutil/libavutil.a \
	$(MPROOT)/m_option.o \
	$(MPROOT)/m_struct.o \
	$(MPROOT)/mp_msg.o \
	$(MPROOT)/osdep/libosdep.a \
	-ltermcap -lm \

ifeq ($(TARGET_WIN32),yes)
DEPS += -lwsock32
endif

netstream: $(DEPS)
	$(CC) -o $@ netstream.o $^

clean:
	rm -f *.o *.a *~

.PHONY: all clean
