SRCS_COMMON          += $(SRCS_COMMON-yes)
SRCS_MPLAYER         += $(SRCS_MPLAYER-yes)
SRCS_MENCODER        += $(SRCS_MENCODER-yes)

OBJS_COMMON    += $(addsuffix .o, $(basename $(SRCS_COMMON)) )
OBJS_MPLAYER   += $(addsuffix .o, $(basename $(SRCS_MPLAYER)) )
OBJS_MENCODER  += $(addsuffix .o, $(basename $(SRCS_MENCODER)) )

CFLAGS-$(LIBAVCODEC)     += -I../libavcodec
CFLAGS += $(CFLAGS-yes) $(OPTFLAGS)

LIBS-$(MPLAYER)  += $(LIBNAME_MPLAYER)
LIBS-$(MENCODER) += $(LIBNAME_MENCODER)
LIBS              = $(LIBNAME_COMMON) $(LIBS-yes)

libs: $(LIBS)

$(LIBNAME_COMMON):   $(OBJS_COMMON)
$(LIBNAME_MPLAYER):  $(OBJS_MPLAYER)
$(LIBNAME_MENCODER): $(OBJS_MENCODER)
$(LIBNAME_COMMON) $(LIBNAME_MPLAYER) $(LIBNAME_MENCODER):
	$(AR) r $@ $^
	$(RANLIB) $@

clean::
	rm -f *.o *.a *.ho *~

distclean:: clean
	rm -f .depend test test2

dep depend::
	$(CC) -MM $(CFLAGS) $(SRCS_COMMON) $(SRCS_MPLAYER) $(SRCS_MENCODER) 1>.depend

%.ho: %.h
	$(CC) $(CFLAGS) -Wno-unused -c -o $@ -x c $<

ALLHEADERS = $(wildcard *.h)
checkheaders: $(ALLHEADERS:.h=.ho)

-include .depend

.PHONY: libs clean distclean dep depend
