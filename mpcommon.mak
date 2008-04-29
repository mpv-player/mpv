SRCS_COMMON          += $(SRCS_COMMON-yes) $(SRCS_COMMON-yes-yes) $(SRCS_COMMON-yes-yes-yes)
SRCS_MPLAYER         += $(SRCS_MPLAYER-yes)
SRCS_MENCODER        += $(SRCS_MENCODER-yes)

OBJS_COMMON    += $(addsuffix .o, $(basename $(SRCS_COMMON)) )
OBJS_MPLAYER   += $(addsuffix .o, $(basename $(SRCS_MPLAYER)) )
OBJS_MENCODER  += $(addsuffix .o, $(basename $(SRCS_MENCODER)) )

CFLAGS += $(CFLAGS-yes) $(OPTFLAGS)

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

.PHONY: checkheaders *clean dep depend
