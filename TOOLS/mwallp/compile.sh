
gcc -g mwallp.c jpeg.c ../../libvo/aclib.o ../../libmpcodecs/img_format.o \
    ../../cpudetect.o ../../mp_msg.o ../../postproc/libpostproc.a \
    -I../../libmpcodecs -I../.. -I../../postproc \
    -L/usr/X11/lib -lX11 -ljpeg -o mwallp
