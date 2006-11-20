
LIBNAME = libmpeg2.a

include ../config.mak

SRCS	= alloc.c \
	  cpu_accel.c\
	  cpu_state.c \
	  decode.c \
	  header.c \
	  idct.c \
	  motion_comp.c \
	  slice.c \

OBJS	= $(SRCS:.c=.o)
INCLUDE = -I. -I../libvo -I..
CFLAGS  = $(INCLUDE) $(OPTFLAGS) -DMPG12PLAY

ifeq ($(TARGET_MMX),yes)
SRCS += idct_mmx.c motion_comp_mmx.c
endif

ifeq ($(TARGET_ALTIVEC),yes)
SRCS += motion_comp_altivec.c idct_altivec.c
endif

ifeq ($(TARGET_VIS),yes)
SRCS += motion_comp_vis.c
endif

ifeq ($(TARGET_ARCH_ALPHA),yes)
SRCS += idct_alpha.c motion_comp_alpha.c
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
