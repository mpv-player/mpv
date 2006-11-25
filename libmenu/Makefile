
include ../config.mak

LIBNAME = libmenu.a

SRCS= menu.c \
      vf_menu.c \
      menu_cmdlist.c  \
      menu_pt.c \
      menu_list.c  \
      menu_filesel.c \
      menu_txt.c \
      menu_console.c \
      menu_param.c \

ifeq ($(DVBIN),yes)
SRCS += menu_dvbin.c
endif
OBJS=$(SRCS:.c=.o)

CFLAGS  = -I. -I.. $(OPTFLAGS)

.SUFFIXES: .c .o


.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

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

ifneq ($(wildcard .depend),)
include .depend
endif
