
include ../config.mak

LIBNAME = libosdep.a

SRCS= strl.c \
      fseeko.c \

SRCS-$(HAVE_SYS_MMAN_H)       += mmap_anon.c
SRCS-$(MACOSX_FINDER_SUPPORT) += macosx_finder_args.c
ifneq ($(TARGET_OS),MINGW32)
SRCS-$(STREAM_CACHE)          += shmem.c
endif

SRCS-$(NEED_GETTIMEOFDAY)     += gettimeofday.c
SRCS-$(NEED_SCANDIR)          += scandir.c
SRCS-$(NEED_SETENV)           += setenv.c
SRCS-$(NEED_STRSEP)           += strsep.c
SRCS-$(NEED_SWAB)             += swab.c
SRCS-$(NEED_VSSCANF)          += vsscanf.c

getch = getch2.c
timer = timer-lx.c
ifeq ($(TARGET_WIN32),yes)
timer = timer-win2.c
endif
ifeq ($(TARGET_OS),Darwin)
timer = timer-darwin.c
endif
ifeq ($(TARGET_OS),MINGW32)
getch = getch2-win.c
SRCS-$(NEED_GLOB)             += glob-win.c
endif
SRCS += $(timer)
SRCS += $(getch)

include ../mpcommon.mak

mplayer-rc.o: mplayer.rc
	windres -o $@ $<
