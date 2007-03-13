include ../config.mak

LIBNAME_COMMON = libfaad2.a

SRCS_COMMON = bits.c \
              cfft.c \
              common.c \
              decoder.c \
              drc.c \
              drm_dec.c \
              error.c \
              filtbank.c \
              hcr.c \
              huffman.c \
              ic_predict.c \
              is.c \
              lt_predict.c \
              mdct.c \
              mp4.c \
              ms.c \
              output.c \
              pns.c \
              ps_dec.c \
              ps_syntax.c  \
              pulse.c \
              rvlc.c \
              sbr_dct.c \
              sbr_dec.c \
              sbr_e_nf.c \
              sbr_fbt.c \
              sbr_hfadj.c \
              sbr_hfgen.c \
              sbr_huff.c \
              sbr_qmf.c \
              sbr_syntax.c \
              sbr_tf_grid.c \
              specrec.c \
              ssr.c \
              ssr_fb.c \
              ssr_ipqf.c \
              syntax.c \
              tns.c \

CFLAGS  = -D_GNU_SOURCE

# The FIXED_POINT implementation of FAAD2 improves performance
# on some platforms, especially for SBR files.
CFLAGS-$(FAAD_FIXED)  += -DFIXED_POINT

include ../mpcommon.mak
