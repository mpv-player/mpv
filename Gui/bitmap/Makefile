
include ../config.mak

SRCS    = bitmap.c tga/tga.c jpeg/jpeg.c bmp/bmp.c
OBJS 	= bitmap.o tga/tga.o jpeg/jpeg.o bmp/bmp.o

.SUFFIXES: .c .o

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

### TARGETS ###

all:
	@( [ -e $(JPEGLIB) ] || \
	   make -C jpeg/jpeg-6b libjpeg.a )
#	 ( make -C jpeg/jpeg-6b libjpeg.a && mv jpeg/jpeg-6b/libjpeg.a "$(JPEGLIB)" ) )
	$(MAKE) $(OBJS)
	$(AR) rc libbitmap.a $(OBJS) jpeg/jpeg-6b/*.o
	ranlib libbitmap.a
	mv libbitmap.a ../libs

#comp: 
#	$(MAKE) $(OBJS)

clean:
	$(MAKE) -C jpeg/jpeg-6b clean
	-rm -rf $(OBJS) *.o *~ *.bak $(JPEGLIB)

dep:	depend

depend:
	makedepend -- $(CFLAGS) -- $(SRCS) &>/dev/null

# DO NOT DELETE

bitmap.o: /usr/include/stdlib.h /usr/include/features.h
bitmap.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
bitmap.o: /usr/lib/gcc-lib/i386-linux/2.95.2/include/stddef.h
bitmap.o: /usr/include/sys/types.h /usr/include/bits/types.h
bitmap.o: /usr/include/time.h /usr/include/endian.h
bitmap.o: /usr/include/bits/endian.h /usr/include/sys/select.h
bitmap.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
bitmap.o: /usr/include/sys/sysmacros.h /usr/include/alloca.h
bitmap.o: /usr/include/stdio.h
bitmap.o: /usr/lib/gcc-lib/i386-linux/2.95.2/include/stdarg.h
bitmap.o: /usr/include/libio.h /usr/include/_G_config.h
bitmap.o: /usr/include/bits/stdio_lim.h bitmap.h tga/tga.h bitmap.h
bitmap.o: jpeg/jpeg.h bmp/bmp.h
tga/tga.o: /usr/include/string.h /usr/include/features.h
tga/tga.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
tga/tga.o: /usr/lib/gcc-lib/i386-linux/2.95.2/include/stddef.h
tga/tga.o: /usr/include/stdio.h
tga/tga.o: /usr/lib/gcc-lib/i386-linux/2.95.2/include/stdarg.h
tga/tga.o: /usr/include/bits/types.h /usr/include/libio.h
tga/tga.o: /usr/include/_G_config.h /usr/include/bits/stdio_lim.h
tga/tga.o: /usr/include/stdlib.h /usr/include/sys/types.h /usr/include/time.h
tga/tga.o: /usr/include/endian.h /usr/include/bits/endian.h
tga/tga.o: /usr/include/sys/select.h /usr/include/bits/select.h
tga/tga.o: /usr/include/bits/sigset.h /usr/include/sys/sysmacros.h
tga/tga.o: /usr/include/alloca.h tga/tga.h bitmap.h
jpeg/jpeg.o: /usr/include/setjmp.h /usr/include/features.h
jpeg/jpeg.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
jpeg/jpeg.o: /usr/include/bits/setjmp.h /usr/include/bits/sigset.h
jpeg/jpeg.o: /usr/include/string.h
jpeg/jpeg.o: /usr/lib/gcc-lib/i386-linux/2.95.2/include/stddef.h
jpeg/jpeg.o: /usr/include/stdlib.h /usr/include/sys/types.h
jpeg/jpeg.o: /usr/include/bits/types.h /usr/include/time.h
jpeg/jpeg.o: /usr/include/endian.h /usr/include/bits/endian.h
jpeg/jpeg.o: /usr/include/sys/select.h /usr/include/bits/select.h
jpeg/jpeg.o: /usr/include/sys/sysmacros.h /usr/include/alloca.h
jpeg/jpeg.o: /usr/include/stdio.h
jpeg/jpeg.o: /usr/lib/gcc-lib/i386-linux/2.95.2/include/stdarg.h
jpeg/jpeg.o: /usr/include/libio.h /usr/include/_G_config.h
jpeg/jpeg.o: /usr/include/bits/stdio_lim.h jpeg/jpeg-6b/jpeglib.h
jpeg/jpeg.o: jpeg/jpeg-6b/jconfig.h jpeg/jpeg-6b/jmorecfg.h bitmap.h
jpeg/jpeg.o: jpeg/jpeg.h
bmp/bmp.o: /usr/include/stdlib.h /usr/include/features.h
bmp/bmp.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
bmp/bmp.o: /usr/lib/gcc-lib/i386-linux/2.95.2/include/stddef.h
bmp/bmp.o: /usr/include/sys/types.h /usr/include/bits/types.h
bmp/bmp.o: /usr/include/time.h /usr/include/endian.h
bmp/bmp.o: /usr/include/bits/endian.h /usr/include/sys/select.h
bmp/bmp.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
bmp/bmp.o: /usr/include/sys/sysmacros.h /usr/include/alloca.h
bmp/bmp.o: /usr/include/stdio.h
bmp/bmp.o: /usr/lib/gcc-lib/i386-linux/2.95.2/include/stdarg.h
bmp/bmp.o: /usr/include/libio.h /usr/include/_G_config.h
bmp/bmp.o: /usr/include/bits/stdio_lim.h bmp/bmp.h bitmap.h
