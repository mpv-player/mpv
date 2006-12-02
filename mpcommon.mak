SRCS         += $(SRCS-yes)
SRCS2        += $(SRCS2-yes)
CFLAGS       += $(CFLAGS-yes)

OBJS  = $(addsuffix .o, $(basename $(SRCS)) )
OBJS2 = $(addsuffix .o, $(basename $(SRCS2)) )

CFLAGS += -I. -I.. $(OPTFLAGS)

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
