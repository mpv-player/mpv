OBJS  = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)
OBJS := $(OBJS:.cpp=.o)
OBJS := $(OBJS:.m=.o)

CFLAGS += -I. -I.. $(OPTFLAGS)

.SUFFIXES: .c .o

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

LIBS = $(LIBNAME) $(LIBNAME2)

all:    $(LIBS)

$(LIBNAME): $(OBJS)
	$(AR) r $@ $^
	$(RANLIB) $@

$(LIBNAME2): $(OBJS2)
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
