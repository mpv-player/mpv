# LINUX Makefile made by A'rpi / Astral
# Some cleanup by LGB: 	* 'make -C dir' instead of 'cd dir;make;cd..'
#			* for loops instead of linear sequence of make directories
#			* some minor problems with make clean and distclean were corrected

include config.mak

PRG = mplayer
PRG_AVIP = aviparse
PRG_TV = tvision
prefix = /usr/local
BINDIR = ${prefix}/bin
# BINDIR = /usr/local/bin
SRCS = linux/getch2.c linux/timer-lx.c linux/shmem.c xa/xa_gsm.c lirc_mp.c
OBJS = linux/getch2.o linux/timer-lx.o linux/shmem.o xa/xa_gsm.o lirc_mp.o
CFLAGS = $(OPTFLAGS) -Iloader -Ilibvo # -Wall
A_LIBS = -Lmp3lib -lMP3 -Llibac3 -lac3
VO_LIBS = -Llibvo -lvo $(X_LIBS)

.SUFFIXES: .c .o

# .PHONY: all clean

all:	$(PRG)
# $(PRG_AVIP)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

COMMONLIBS = libvo/libvo.a libac3/libac3.a mp3lib/libMP3.a

loader/libloader.a:
	$(MAKE) -C loader

libmpeg2/libmpeg2.a:
	$(MAKE) -C libmpeg2

libvo/libvo.a:
	$(MAKE) -C libvo

libac3/libac3.a:
	$(MAKE) -C libac3

mp3lib/libMP3.a:
	$(MAKE) -C mp3lib

opendivx/libdecore.a:
	$(MAKE) -C opendivx

encore/libencore.a:
	$(MAKE) -C encore

$(PRG):	mplayer.o $(OBJS) loader/libloader.a libmpeg2/libmpeg2.a opendivx/libdecore.a $(COMMONLIBS) encore/libencore.a
	$(CC) $(CFLAGS) -o $(PRG) mplayer.o $(OBJS) $(XMM_LIBS) $(LIRC_LIBS) $(A_LIBS) -lm $(TERMCAP_LIB) -Lloader -lloader -ldl -Llibmpeg2 -lmpeg2 -Lopendivx -ldecore $(VO_LIBS) -Lencore -lencore -lpthread

$(PRG_AVIP):	aviparse.o $(OBJS) loader/libloader.a $(COMMONLIBS)
	$(CC) $(CFLAGS) -o $(PRG_AVIP) aviparse.o $(OBJS) $(A_LIBS) -lm $(TERMCAP_LIB) -Lloader -lloader -ldl $(VO_LIBS) -lpthread

$(PRG_TV):	tvision.o $(OBJS) $(COMMONLIBS)
	$(CC) $(CFLAGS) -o $(PRG_TV) tvision.o $(OBJS) -lm $(TERMCAP_LIB) $(VO_LIBS)

install: $(PRG)
	strip $(PRG)
	cp $(PRG) $(BINDIR)
	install -D -m 644 DOCS/mplayer.1 $(prefix)/man/man1/mplayer.1

clean:
	rm -f *.o *~ $(OBJS)

distclean:
	@for a in mp3lib libac3 libmpeg2 opendivx encore libvo loader drivers drivers/syncfb ; do $(MAKE) -C $$a distclean ; done
	makedepend
	rm -f *~ $(PRG) $(PRG_AVIP) $(PRG_TV) $(OBJS) *.o *.a Makefile.bak

dep:	depend

depend:
	@for a in mp3lib libac3 libmpeg2 libvo opendivx encore ; do $(MAKE) -C $$a dep ; done
#	cd loader;make dep;cd ..
	makedepend -- $(CFLAGS) -- mplayer.c aviparse.c tvision.c $(SRCS) &>/dev/null

# DO NOT DELETE

mplayer.o: /usr/include/stdio.h /usr/include/features.h
mplayer.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
mplayer.o: /usr/include/bits/types.h /usr/include/libio.h
mplayer.o: /usr/include/_G_config.h /usr/include/bits/stdio_lim.h
mplayer.o: /usr/include/stdlib.h /usr/include/sys/types.h /usr/include/time.h
mplayer.o: /usr/include/endian.h /usr/include/bits/endian.h
mplayer.o: /usr/include/sys/select.h /usr/include/bits/select.h
mplayer.o: /usr/include/bits/sigset.h /usr/include/sys/sysmacros.h
mplayer.o: /usr/include/alloca.h /usr/include/signal.h
mplayer.o: /usr/include/bits/signum.h /usr/include/bits/siginfo.h
mplayer.o: /usr/include/bits/sigaction.h /usr/include/bits/sigcontext.h
mplayer.o: /usr/include/asm/sigcontext.h /usr/include/bits/sigstack.h
mplayer.o: /usr/include/sys/ioctl.h /usr/include/bits/ioctls.h
mplayer.o: /usr/include/asm/ioctls.h /usr/include/asm/ioctl.h
mplayer.o: /usr/include/bits/ioctl-types.h /usr/include/sys/ttydefaults.h
mplayer.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
mplayer.o: /usr/include/bits/confname.h /usr/include/getopt.h
mplayer.o: /usr/include/sys/mman.h /usr/include/bits/mman.h
mplayer.o: /usr/include/sys/wait.h /usr/include/bits/waitflags.h
mplayer.o: /usr/include/bits/waitstatus.h /usr/include/sys/time.h
mplayer.o: /usr/include/bits/time.h /usr/include/sys/stat.h
mplayer.o: /usr/include/bits/stat.h /usr/include/fcntl.h
mplayer.o: /usr/include/bits/fcntl.h /usr/include/sys/soundcard.h
mplayer.o: /usr/include/linux/soundcard.h /usr/include/linux/ioctl.h
mplayer.o: /usr/include/linux/cdrom.h /usr/include/asm/byteorder.h
mplayer.o: /usr/include/asm/types.h
mplayer.o: /usr/include/linux/byteorder/little_endian.h
mplayer.o: /usr/include/linux/byteorder/swab.h
mplayer.o: /usr/include/linux/byteorder/generic.h version.h config.h
mplayer.o: mp3lib/mp3.h libac3/ac3.h /usr/include/inttypes.h
mplayer.o: /usr/include/stdint.h /usr/include/bits/wordsize.h
mplayer.o: libmpeg2/mpeg2.h libvo/video_out.h libvo/wskeys.h
mplayer.o: libmpeg2/mm_accel.h libmpeg2/mpeg2_internal.h loader/loader.h
mplayer.o: loader/wine/windef.h loader/wine/windef.h loader/wine/driver.h
mplayer.o: loader/wine/mmreg.h loader/wine/vfw.h loader/wine/msacm.h
mplayer.o: loader/wine/driver.h loader/wine/mmreg.h loader/wine/avifmt.h
mplayer.o: opendivx/decore.h linux/getch2.h linux/keycodes.h linux/timer.h
mplayer.o: linux/shmem.h help_mp.h aviprint.c codecs.c stream.c vcd_read.c
mplayer.o: demuxer.c demux_avi.c demux_mpg.c parse_es.c alaw.c xa/xa_gsm.h
mplayer.o: aviheader.c aviwrite.c asfheader.c demux_asf.c dll_init.c
mplayer.o: codecctrl.c
aviparse.o: /usr/include/stdio.h /usr/include/features.h
aviparse.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
aviparse.o: /usr/include/bits/types.h /usr/include/libio.h
aviparse.o: /usr/include/_G_config.h /usr/include/bits/stdio_lim.h
aviparse.o: /usr/include/stdlib.h /usr/include/sys/types.h
aviparse.o: /usr/include/time.h /usr/include/endian.h
aviparse.o: /usr/include/bits/endian.h /usr/include/sys/select.h
aviparse.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
aviparse.o: /usr/include/sys/sysmacros.h /usr/include/alloca.h
aviparse.o: /usr/include/signal.h /usr/include/bits/signum.h
aviparse.o: /usr/include/bits/siginfo.h /usr/include/bits/sigaction.h
aviparse.o: /usr/include/bits/sigcontext.h /usr/include/asm/sigcontext.h
aviparse.o: /usr/include/bits/sigstack.h /usr/include/sys/ioctl.h
aviparse.o: /usr/include/bits/ioctls.h /usr/include/asm/ioctls.h
aviparse.o: /usr/include/asm/ioctl.h /usr/include/bits/ioctl-types.h
aviparse.o: /usr/include/sys/ttydefaults.h /usr/include/unistd.h
aviparse.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
aviparse.o: /usr/include/getopt.h /usr/include/sys/mman.h
aviparse.o: /usr/include/bits/mman.h /usr/include/sys/wait.h
aviparse.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
aviparse.o: /usr/include/sys/time.h /usr/include/bits/time.h
aviparse.o: /usr/include/sys/stat.h /usr/include/bits/stat.h
aviparse.o: /usr/include/fcntl.h /usr/include/bits/fcntl.h
aviparse.o: /usr/include/linux/cdrom.h /usr/include/asm/byteorder.h
aviparse.o: /usr/include/asm/types.h
aviparse.o: /usr/include/linux/byteorder/little_endian.h
aviparse.o: /usr/include/linux/byteorder/swab.h
aviparse.o: /usr/include/linux/byteorder/generic.h config.h loader/loader.h
aviparse.o: loader/wine/windef.h loader/wine/windef.h loader/wine/driver.h
aviparse.o: loader/wine/mmreg.h loader/wine/vfw.h loader/wine/msacm.h
aviparse.o: loader/wine/driver.h loader/wine/mmreg.h loader/wine/avifmt.h
aviparse.o: linux/timer.h linux/shmem.h help_avp.h aviprint.c stream.c
aviparse.o: vcd_read.c
tvision.o: /usr/include/stdio.h /usr/include/features.h
tvision.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
tvision.o: /usr/include/bits/types.h /usr/include/libio.h
tvision.o: /usr/include/_G_config.h /usr/include/bits/stdio_lim.h
tvision.o: /usr/include/stdlib.h /usr/include/sys/types.h /usr/include/time.h
tvision.o: /usr/include/endian.h /usr/include/bits/endian.h
tvision.o: /usr/include/sys/select.h /usr/include/bits/select.h
tvision.o: /usr/include/bits/sigset.h /usr/include/sys/sysmacros.h
tvision.o: /usr/include/alloca.h /usr/include/unistd.h
tvision.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
tvision.o: /usr/include/getopt.h /usr/include/math.h
tvision.o: /usr/include/bits/huge_val.h /usr/include/bits/mathdef.h
tvision.o: /usr/include/bits/mathcalls.h /usr/include/errno.h
tvision.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
tvision.o: /usr/include/asm/errno.h /usr/include/fcntl.h
tvision.o: /usr/include/bits/fcntl.h /usr/include/string.h
tvision.o: /usr/include/ctype.h /usr/include/signal.h
tvision.o: /usr/include/bits/signum.h /usr/include/bits/siginfo.h
tvision.o: /usr/include/bits/sigaction.h /usr/include/bits/sigcontext.h
tvision.o: /usr/include/asm/sigcontext.h /usr/include/bits/sigstack.h
tvision.o: /usr/include/sys/socket.h /usr/include/bits/socket.h
tvision.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
tvision.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
tvision.o: /usr/include/bits/posix2_lim.h /usr/include/bits/sockaddr.h
tvision.o: /usr/include/asm/socket.h /usr/include/asm/sockios.h
tvision.o: /usr/include/sys/time.h /usr/include/bits/time.h
tvision.o: /usr/include/sys/ioctl.h /usr/include/bits/ioctls.h
tvision.o: /usr/include/asm/ioctls.h /usr/include/asm/ioctl.h
tvision.o: /usr/include/bits/ioctl-types.h /usr/include/sys/ttydefaults.h
tvision.o: /usr/include/sys/stat.h /usr/include/bits/stat.h
tvision.o: /usr/include/sys/mman.h /usr/include/bits/mman.h
tvision.o: /usr/include/sys/shm.h /usr/include/sys/ipc.h
tvision.o: /usr/include/bits/ipc.h /usr/include/bits/shm.h
tvision.o: /usr/include/sys/wait.h /usr/include/bits/waitflags.h
tvision.o: /usr/include/bits/waitstatus.h /usr/include/asm/types.h videodev.h
tvision.o: /usr/include/linux/types.h /usr/include/linux/posix_types.h
tvision.o: /usr/include/linux/stddef.h /usr/include/asm/posix_types.h
tvision.o: libvo/video_out.h /usr/include/inttypes.h /usr/include/stdint.h
tvision.o: /usr/include/bits/wordsize.h
linux/getch2.o: config.h /usr/include/stdio.h /usr/include/features.h
linux/getch2.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
linux/getch2.o: /usr/include/bits/types.h /usr/include/libio.h
linux/getch2.o: /usr/include/_G_config.h /usr/include/bits/stdio_lim.h
linux/getch2.o: /usr/include/stdlib.h /usr/include/sys/types.h
linux/getch2.o: /usr/include/time.h /usr/include/endian.h
linux/getch2.o: /usr/include/bits/endian.h /usr/include/sys/select.h
linux/getch2.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
linux/getch2.o: /usr/include/sys/sysmacros.h /usr/include/alloca.h
linux/getch2.o: /usr/include/string.h /usr/include/sys/time.h
linux/getch2.o: /usr/include/bits/time.h /usr/include/sys/ioctl.h
linux/getch2.o: /usr/include/bits/ioctls.h /usr/include/asm/ioctls.h
linux/getch2.o: /usr/include/asm/ioctl.h /usr/include/bits/ioctl-types.h
linux/getch2.o: /usr/include/sys/ttydefaults.h /usr/include/sys/termios.h
linux/getch2.o: /usr/include/termios.h /usr/include/bits/termios.h
linux/getch2.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
linux/getch2.o: /usr/include/bits/confname.h /usr/include/getopt.h
linux/getch2.o: linux/keycodes.h
linux/timer-lx.o: /usr/include/unistd.h /usr/include/features.h
linux/timer-lx.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
linux/timer-lx.o: /usr/include/bits/posix_opt.h /usr/include/bits/types.h
linux/timer-lx.o: /usr/include/bits/confname.h /usr/include/getopt.h
linux/timer-lx.o: /usr/include/sys/time.h /usr/include/time.h
linux/timer-lx.o: /usr/include/sys/select.h /usr/include/bits/select.h
linux/timer-lx.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
linux/shmem.o: /usr/include/stdio.h /usr/include/features.h
linux/shmem.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
linux/shmem.o: /usr/include/bits/types.h /usr/include/libio.h
linux/shmem.o: /usr/include/_G_config.h /usr/include/bits/stdio_lim.h
linux/shmem.o: /usr/include/stdlib.h /usr/include/sys/types.h
linux/shmem.o: /usr/include/time.h /usr/include/endian.h
linux/shmem.o: /usr/include/bits/endian.h /usr/include/sys/select.h
linux/shmem.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
linux/shmem.o: /usr/include/sys/sysmacros.h /usr/include/alloca.h
linux/shmem.o: /usr/include/string.h /usr/include/unistd.h
linux/shmem.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
linux/shmem.o: /usr/include/getopt.h /usr/include/errno.h
linux/shmem.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
linux/shmem.o: /usr/include/asm/errno.h /usr/include/sys/time.h
linux/shmem.o: /usr/include/bits/time.h /usr/include/sys/uio.h
linux/shmem.o: /usr/include/bits/uio.h /usr/include/sys/mman.h
linux/shmem.o: /usr/include/bits/mman.h /usr/include/sys/socket.h
linux/shmem.o: /usr/include/bits/socket.h /usr/include/limits.h
linux/shmem.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
linux/shmem.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
linux/shmem.o: /usr/include/bits/sockaddr.h /usr/include/asm/socket.h
linux/shmem.o: /usr/include/asm/sockios.h /usr/include/fcntl.h
linux/shmem.o: /usr/include/bits/fcntl.h /usr/include/sys/ipc.h
linux/shmem.o: /usr/include/bits/ipc.h /usr/include/sys/shm.h
linux/shmem.o: /usr/include/bits/shm.h
xa/xa_gsm.o: /usr/include/stdio.h /usr/include/features.h
xa/xa_gsm.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
xa/xa_gsm.o: /usr/include/bits/types.h /usr/include/libio.h
xa/xa_gsm.o: /usr/include/_G_config.h /usr/include/bits/stdio_lim.h
xa/xa_gsm.o: /usr/include/assert.h xa/xa_gsm_int.h xa/xa_gsm.h
lirc_mp.o: config.h
