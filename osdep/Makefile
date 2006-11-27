
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

getch = getch2.c
timer = timer-lx.c
ifeq ($(MACOSX_FINDER_SUPPORT),yes)
SRCS += macosx_finder_args.c
endif
ifeq ($(TARGET_WIN32),yes)
SRCS += mplayer-rc.c
timer = timer-win2.c
endif
ifeq ($(TARGET_OS),Darwin)
timer = timer-darwin.c
endif
ifeq ($(TARGET_OS),MINGW32)
getch = getch2-win.c
SRCS += glob-win.c
endif
SRCS += $(timer)
SRCS += $(getch)

include ../mpcommon.mak

mplayer-rc.o: mplayer.rc
	windres -o $@ $<
