SRCS_COMMON          += $(SRCS_COMMON-yes)
SRCS_COMMON          += $(SRCS_COMMON-yes-yes)
SRCS_COMMON          += $(SRCS_COMMON-yes-yes-yes)
SRCS_MPLAYER         += $(SRCS_MPLAYER-yes)
SRCS_MENCODER        += $(SRCS_MENCODER-yes)

OBJS_COMMON    += $(addsuffix .o, $(basename $(SRCS_COMMON)) )
OBJS_MPLAYER   += $(addsuffix .o, $(basename $(SRCS_MPLAYER)) )
OBJS_MENCODER  += $(addsuffix .o, $(basename $(SRCS_MENCODER)) )

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
	rm -f *.d .depend test test2

.depend: $(SRCS_COMMON) $(SRCS_MPLAYER) $(SRCS_MENCODER)
	$(MPDEPEND_CMD) > $@

%.d: %.c
	$(MPDEPEND_CMD) > $@

%.d: %.cpp
	$(MPDEPEND_CMD_CXX) > $@

%.d: %.m
	$(MPDEPEND_CMD) > $@

%.ho: %.h
	$(CC) $(CFLAGS) -Wno-unused -c -o $@ -x c $<

%.o: %.m
	$(CC) $(CFLAGS) -c -o $@ $<

ALLHEADERS = $(wildcard *.h)
checkheaders: $(ALLHEADERS:.h=.ho)

# Hack to keep .depend from being generated at the top level unnecessarily.
ifndef DEPS
DEPS = .depend
endif
-include $(DEPS)

.PHONY: libs *clean dep depend
