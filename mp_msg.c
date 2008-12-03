#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "config.h"

#ifdef CONFIG_ICONV
#include <iconv.h>
#include <errno.h>
char* get_term_charset(void);
#endif

#if defined(FOR_MENCODER)
#undef CONFIG_GUI
int use_gui;
#endif

#ifdef CONFIG_GUI
#include "gui/interface.h"
#endif
#include "mp_msg.h"

/* maximum message length of mp_msg */
#define MSGSIZE_MAX 3072

int mp_msg_levels[MSGT_MAX]; // verbose level of this module. initialized to -2
int mp_msg_level_all = MSGL_STATUS;
int verbose = 0;
int mp_msg_color = 0;
int mp_msg_module = 0;
#ifdef CONFIG_ICONV
char *mp_msg_charset = NULL;
static char *old_charset = NULL;
static iconv_t msgiconv;
#endif

const char* filename_recode(const char* filename)
{
#if !defined(CONFIG_ICONV) || !defined(MSG_CHARSET)
    return filename;
#else
    static iconv_t inv_msgiconv = (iconv_t)(-1);
    static char recoded_filename[MSGSIZE_MAX];
    size_t filename_len, max_path;
    char* precoded;
    if (!mp_msg_charset ||
        !strcasecmp(mp_msg_charset, MSG_CHARSET) ||
        !strcasecmp(mp_msg_charset, "noconv"))
        return filename;
    if (inv_msgiconv == (iconv_t)(-1)) {
        inv_msgiconv = iconv_open(MSG_CHARSET, mp_msg_charset);
        if (inv_msgiconv == (iconv_t)(-1))
            return filename;
    }
    filename_len = strlen(filename);
    max_path = MSGSIZE_MAX - 4;
    precoded = recoded_filename;
    if (iconv(inv_msgiconv, &filename, &filename_len,
              &precoded, &max_path) == (size_t)(-1) && errno == E2BIG) {
        precoded[0] = precoded[1] = precoded[2] = '.';
        precoded += 3;
    }
    *precoded = '\0';
    return recoded_filename;
#endif
}

void mp_msg_init(void){
    int i;
    char *env = getenv("MPLAYER_VERBOSE");
    if (env)
        verbose = atoi(env);
    for(i=0;i<MSGT_MAX;i++) mp_msg_levels[i] = -2;
    mp_msg_levels[MSGT_IDENTIFY] = -1; // no -identify output by default
#ifdef CONFIG_ICONV
    mp_msg_charset = getenv("MPLAYER_CHARSET");
    if (!mp_msg_charset)
      mp_msg_charset = get_term_charset();
#endif
}

int mp_msg_test(int mod, int lev)
{
    return lev <= (mp_msg_levels[mod] == -2 ? mp_msg_level_all + verbose : mp_msg_levels[mod]);
}

static void set_msg_color(FILE* stream, int lev)
{
    static const unsigned char v_colors[10] = {9, 1, 3, 15, 7, 2, 2, 8, 8, 8};
    int c = v_colors[lev];
#ifdef MP_ANNOY_ME
    /* that's only a silly color test */
    {
        int c;
        static int flag = 1;
        if (flag)
            for(c = 0; c < 24; c++)
                printf("\033[%d;3%dm***  COLOR TEST %d  ***\n", c>7, c&7, c);
        flag = 0;
    }
#endif
    if (mp_msg_color)
        fprintf(stream, "\033[%d;3%dm", c >> 3, c & 7);
}

static void print_msg_module(FILE* stream, int mod)
{
    static const char *module_text[MSGT_MAX] = {
        "GLOBAL",
        "CPLAYER",
        "GPLAYER",
        "VIDEOOUT",
        "AUDIOOUT",
        "DEMUXER",
        "DS",
        "DEMUX",
        "HEADER",
        "AVSYNC",
        "AUTOQ",
        "CFGPARSER",
        "DECAUDIO",
        "DECVIDEO",
        "SEEK",
        "WIN32",
        "OPEN",
        "DVD",
        "PARSEES",
        "LIRC",
        "STREAM",
        "CACHE",
        "MENCODER",
        "XACODEC",
        "TV",
        "OSDEP",
        "SPUDEC",
        "PLAYTREE",
        "INPUT",
        "VFILTER",
        "OSD",
        "NETWORK",
        "CPUDETECT",
        "CODECCFG",
        "SWS",
        "VOBSUB",
        "SUBREADER",
        "AFILTER",
        "NETST",
        "MUXER",
        "OSDMENU",
        "IDENTIFY",
        "RADIO",
        "ASS",
        "LOADER",
        "STATUSLINE",
    };
    int c2 = (mod + 1) % 15 + 1;

    if (!mp_msg_module)
        return;
    if (mp_msg_color)
        fprintf(stream, "\033[%d;3%dm", c2 >> 3, c2 & 7);
    fprintf(stream, "%9s", module_text[mod]);
    if (mp_msg_color)
        fprintf(stream, "\033[0;37m");
    fprintf(stream, ": ");
}

void mp_msg(int mod, int lev, const char *format, ... ){
    va_list va;
    char tmp[MSGSIZE_MAX];
    FILE *stream = lev <= MSGL_WARN ? stderr : stdout;
    static int header = 1;

    if (!mp_msg_test(mod, lev)) return; // do not display
    va_start(va, format);
    vsnprintf(tmp, MSGSIZE_MAX, format, va);
    va_end(va);
    tmp[MSGSIZE_MAX-2] = '\n';
    tmp[MSGSIZE_MAX-1] = 0;

#ifdef CONFIG_GUI
    if(use_gui)
        guiMessageBox(lev, tmp);
#endif

#if defined(CONFIG_ICONV) && defined(MSG_CHARSET)
    if (mp_msg_charset && strcasecmp(mp_msg_charset, "noconv")) {
      char tmp2[MSGSIZE_MAX];
      size_t inlen = strlen(tmp), outlen = MSGSIZE_MAX;
      char *in = tmp, *out = tmp2;
      if (!old_charset || strcmp(old_charset, mp_msg_charset)) {
        if (old_charset) {
          free(old_charset);
          iconv_close(msgiconv);
        }
        msgiconv = iconv_open(mp_msg_charset, MSG_CHARSET);
        old_charset = strdup(mp_msg_charset);
      }
      if (msgiconv == (iconv_t)(-1)) {
        fprintf(stderr,"iconv: conversion from %s to %s unsupported\n"
               ,MSG_CHARSET,mp_msg_charset);
      }else{
      memset(tmp2, 0, MSGSIZE_MAX);
      while (iconv(msgiconv, &in, &inlen, &out, &outlen) == -1) {
        if (!inlen || !outlen)
          break;
        *out++ = *in++;
        outlen--; inlen--;
      }
      strncpy(tmp, tmp2, MSGSIZE_MAX);
      tmp[MSGSIZE_MAX-1] = 0;
      tmp[MSGSIZE_MAX-2] = '\n';
      }
    }
#endif

    if (header)
        print_msg_module(stream, mod);
    set_msg_color(stream, lev);
    header = tmp[strlen(tmp)-1] == '\n' || tmp[strlen(tmp)-1] == '\r';
 
    fprintf(stream, "%s", tmp);
    fflush(stream);
}
