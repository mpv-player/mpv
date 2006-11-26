
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

include ../mpcommon.mak
