
include ../config.mak

LIBNAME = libosdep.a

SRCS= shmem.c \
      strsep.c \
      strl.c \
      vsscanf.c \
      scandir.c \
      gettimeofday.c \
      fseeko.c \
      swab.c \
      setenv.c \
      mmap_anon.c \
      # timer.c \

getch = getch2.c
timer = timer-lx.c
ifeq ($(MACOSX_FINDER_SUPPORT),yes)
SRCS += macosx_finder_args.c
endif
ifeq ($(TARGET_WIN32),yes)
SRCS += osdep/mplayer-rc.c
endif
ifeq ($(TARGET_OS),Darwin)
timer = timer-darwin.c
endif
ifeq ($(TARGET_OS),CYGWIN)
timer = timer-win2.c
endif
ifeq ($(TARGET_OS),MINGW32)
timer = timer-win2.c
getch = getch2-win.c
SRCS += glob-win.c
endif
SRCS += $(timer)
SRCS += $(getch)

OBJS=$(SRCS:.c=.o)

CFLAGS  = -I. -I.. $(OPTFLAGS)
# -I/usr/X11R6/include/

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

mplayer-rc.o: mplayer.rc
	windres -o $@ $<

$(LIBNAME):     $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)
	$(RANLIB) $(LIBNAME)

all:    $(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean: clean
	rm -f .depend

dep depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
