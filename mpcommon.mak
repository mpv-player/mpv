OBJS  = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)
OBJS := $(OBJS:.s=.o)
OBJS := $(OBJS:.cpp=.o)

CFLAGS += -I. -I.. $(OPTFLAGS)

.SUFFIXES: .c .o

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

all:    $(LIBNAME)

$(LIBNAME): $(OBJS)
	$(AR) r $@ $^
	$(RANLIB) $@

clean::
	rm -f *.o *.a *~

distclean:: clean
	rm -f .depend

dep depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

ifneq ($(wildcard .depend),)
include .depend
endif
