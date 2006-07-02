
gcc -g -O4 gltest.c ../../osdep/timer-lx.o -o gltest -L/usr/X11/lib -L/usr/X11R6/lib -lglut -lGL -lGLU -lX11 -lXext -lXmu -lXi -lm
