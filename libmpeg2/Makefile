
LIBNAME = libmpeg2.a

include ../config.mak

SRCS	= alloc.c cpu_accel.c cpu_state.c decode.c header.c idct.c idct_alpha.c idct_mlib.c idct_mmx.c motion_comp.c motion_comp_alpha.c motion_comp_mlib.c motion_comp_mmx.c slice.c

OBJS	= $(SRCS:.c=.o)
INCLUDE = -I. -I../libvo -I.. $(EXTRA_INC) $(MLIB_INC)
CFLAGS  = $(OPTFLAGS) $(INCLUDE) -DMPG12PLAY

ifeq ($(TARGET_ALTIVEC),yes)
SRCS += motion_comp_altivec.c idct_altivec.c
endif

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME):	$(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)
	$(RANLIB) $(LIBNAME)

all:	$(LIBNAME)

clean:
	rm -f *.o *.a *~

distclean:
	rm -f Makefile.bak *.o *.a *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
