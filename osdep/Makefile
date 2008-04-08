include ../config.mak

LIBNAME_COMMON = libosdep.a

SRCS_COMMON                          += $(GETCH)
SRCS_COMMON                          += $(TIMER)

SRCS_COMMON-$(HAVE_SYS_MMAN_H)       += mmap_anon.c
SRCS_COMMON-$(MACOSX_FINDER_SUPPORT) += macosx_finder_args.c

SRCS_COMMON-$(NEED_GETTIMEOFDAY)     += gettimeofday.c
SRCS_COMMON-$(NEED_GLOB)             += glob-win.c
SRCS_COMMON-$(NEED_MMAP)             += mmap-os2.c
SRCS_COMMON-$(NEED_SETENV)           += setenv.c
SRCS_COMMON-$(NEED_SHMEM)            += shmem.c
SRCS_COMMON-$(NEED_STRSEP)           += strsep.c
SRCS_COMMON-$(NEED_SWAB)             += swab.c
SRCS_COMMON-$(NEED_VSSCANF)          += vsscanf.c

include ../mpcommon.mak

mplayer-rc.o: mplayer.rc
	$(WINDRES) -o $@ $<
