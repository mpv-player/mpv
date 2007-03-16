include ../config.mak

LIBNAME_COMMON = libvorbisidec.a

SRCS_COMMON = bitwise.c \
              block.c \
              codebook.c \
              floor0.c \
              floor1.c \
              framing.c \
              info.c \
              mapping0.c \
              mdct.c \
              registry.c \
              res012.c \
              sharedbook.c \
              synthesis.c \
              window.c \

CFLAGS-$(TREMOR_LOW)  += -D_LOW_ACCURACY_

include ../mpcommon.mak
