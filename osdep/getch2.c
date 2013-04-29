/*
 * GyS-TermIO v2.0 (for GySmail v3)
 * a very small replacement of ncurses library
 *
 * copyright (C) 1999 A'rpi/ESP-team
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

//#define HAVE_TERMCAP
#if !defined(__MORPHOS__)
#define CONFIG_IOCTL
#endif

#define MAX_KEYS 64
#define BUF_LEN 256

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef CONFIG_IOCTL
#include <sys/ioctl.h>
#endif

#ifdef HAVE_TERMIOS
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif
#endif

#include <unistd.h>
#include <fcntl.h>

#include "core/bstr.h"
#include "core/mp_fifo.h"
#include "core/input/input.h"
#include "core/input/keycodes.h"
#include "getch2.h"

#ifdef HAVE_TERMIOS
static volatile struct termios tio_orig;
static volatile int tio_orig_set;
#endif
static int getch2_len=0;
static unsigned char getch2_buf[BUF_LEN];

int screen_width=80;
int screen_height=24;
char * erase_to_end_of_line = NULL;

typedef struct {
  int len;
  int code;
  char chars[8];
} keycode_st;
static keycode_st getch2_keys[MAX_KEYS];
static int getch2_key_db=0;

#ifdef HAVE_TERMCAP

#if 0
#include <termcap.h>
#else
int tgetent(char *BUFFER, char *TERMTYPE);
int tgetnum(char *NAME);
int tgetflag(char *NAME);
char *tgetstr(char *NAME, char **AREA);
#endif

static char term_buffer[4096];
static char term_buffer2[4096];
static char *term_p=term_buffer2;

static void termcap_add(char *id,int code){
char *p=tgetstr(id,&term_p);
  if(!p) return;
  if(getch2_key_db>=MAX_KEYS) return;
  getch2_keys[getch2_key_db].len=strlen(p);
  strncpy(getch2_keys[getch2_key_db].chars,p,8);
  getch2_keys[getch2_key_db].code=code;
  ++getch2_key_db;
/*  printf("%s=%s\n",id,p); */
}

static int success=0;

int load_termcap(char *termtype){
  if(!termtype) termtype=getenv("TERM");
  if(!termtype) termtype="unknown";
  success=tgetent(term_buffer, termtype);
  if(success<0){ printf("Could not access the 'termcap' data base.\n"); return 0; }
  if(success==0){ printf("Terminal type `%s' is not defined.\n", termtype);return 0;}

  screen_width=tgetnum("co");
  screen_height=tgetnum("li");
  if(screen_width<1 || screen_width>255) screen_width=80;
  if(screen_height<1 || screen_height>255) screen_height=24;
  erase_to_end_of_line= tgetstr("ce",&term_p);

  termcap_add("kP",MP_KEY_PGUP);
  termcap_add("kN",MP_KEY_PGDWN);
  termcap_add("kh",MP_KEY_HOME);
  termcap_add("kH",MP_KEY_END);
  termcap_add("kI",MP_KEY_INS);
  termcap_add("kD",MP_KEY_DEL);
  termcap_add("kb",MP_KEY_BS);
  termcap_add("kl",MP_KEY_LEFT);
  termcap_add("kd",MP_KEY_DOWN);
  termcap_add("ku",MP_KEY_UP);
  termcap_add("kr",MP_KEY_RIGHT);
  termcap_add("k0",MP_KEY_F+0);
  termcap_add("k1",MP_KEY_F+1);
  termcap_add("k2",MP_KEY_F+2);
  termcap_add("k3",MP_KEY_F+3);
  termcap_add("k4",MP_KEY_F+4);
  termcap_add("k5",MP_KEY_F+5);
  termcap_add("k6",MP_KEY_F+6);
  termcap_add("k7",MP_KEY_F+7);
  termcap_add("k8",MP_KEY_F+8);
  termcap_add("k9",MP_KEY_F+9);
  termcap_add("k;",MP_KEY_F+10);
  return getch2_key_db;
}

#endif

void get_screen_size(void){
#ifdef CONFIG_IOCTL
  struct winsize ws;
  if (ioctl(0, TIOCGWINSZ, &ws) < 0 || !ws.ws_row || !ws.ws_col) return;
/*  printf("Using IOCTL\n"); */
  screen_width=ws.ws_col;
  screen_height=ws.ws_row;
#endif
}

bool getch2(struct mp_fifo *fifo)
{
    int retval = read(0, &getch2_buf[getch2_len], BUF_LEN-getch2_len);
    /* Return false on EOF to stop running select() on the FD, as it'd
     * trigger all the time. Note that it's possible to get temporary
     * EOF on terminal if the user presses ctrl-d, but that shouldn't
     * happen if the terminal state change done in getch2_enable()
     * works.
     */
    if (retval < 1)
        return retval;
    getch2_len += retval;

    while (getch2_len > 0 && (getch2_len > 1 || getch2_buf[0] != 27)) {
        int i, len, code;

        /* First find in the TERMCAP database: */
        for (i = 0; i < getch2_key_db; i++) {
            if ((len = getch2_keys[i].len) <= getch2_len)
                if(memcmp(getch2_keys[i].chars, getch2_buf, len) == 0) {
                    code = getch2_keys[i].code;
                    goto found;
                }
        }
        /* We always match some keypress here, with length 1 if nothing else.
         * Since some of the cases explicitly test remaining buffer length
         * having a keycode only partially read in the buffer could incorrectly
         * use the first byte as an independent character.
         * However the buffer is big enough that this shouldn't happen too
         * easily, and it's been this way for years without many complaints.
         * I see no simple fix as there's no easy test which would tell
         * whether a string must be part of a longer keycode. */
        len = 1;
        code = getch2_buf[0];
        /* Check the well-known codes... */
        if (code != 27) {
            if (code == 'A'-64) code = MP_KEY_HOME;
            else if (code == 'E'-64) code = MP_KEY_END;
            else if (code == 'D'-64) code = MP_KEY_DEL;
            else if (code == 'H'-64) code = MP_KEY_BS;
            else if (code == 'U'-64) code = MP_KEY_PGUP;
            else if (code == 'V'-64) code = MP_KEY_PGDWN;
            else if (code == 8 || code==127) code = MP_KEY_BS;
            else if (code == 10 || code==13) {
                if (getch2_len > 1) {
                    int c = getch2_buf[1];
                    if ((c == 10 || c == 13) && (c != code))
                        len = 2;
                }
                code = MP_KEY_ENTER;
            } else {
                int utf8len = bstr_parse_utf8_code_length(code);
                if (utf8len > 0 && utf8len <= getch2_len) {
                    struct bstr s = { getch2_buf, utf8len };
                    int unicode = bstr_decode_utf8(s, NULL);
                    if (unicode > 0) {
                        len = utf8len;
                        code = unicode;
                    }
                }
            }
        }
        else if (getch2_len > 1) {
            int c = getch2_buf[1];
            if (c == 27) {
                code = MP_KEY_ESC;
                len = 2;
                goto found;
            }
            if (c >= '0' && c <= '9') {
                code = c-'0'+MP_KEY_F;
                len = 2;
                goto found;
            }
            if (getch2_len >= 4 && c == '[' && getch2_buf[2] == '[') {
                int c = getch2_buf[3];
                if (c >= 'A' && c < 'A'+12) {
                    code = MP_KEY_F+1 + c-'A';
                    len = 4;
                    goto found;
                }
            }
            if ((c == '[' || c == 'O') && getch2_len >= 3) {
                int c = getch2_buf[2];
                const int ctable[] = {
                    MP_KEY_UP, MP_KEY_DOWN, MP_KEY_RIGHT, MP_KEY_LEFT, 0,
                    MP_KEY_END, MP_KEY_PGDWN, MP_KEY_HOME, MP_KEY_PGUP, 0, 0, MP_KEY_INS, 0, 0, 0,
                    MP_KEY_F+1, MP_KEY_F+2, MP_KEY_F+3, MP_KEY_F+4};
                if (c >= 'A' && c <= 'S')
                    if (ctable[c - 'A']) {
                        code = ctable[c - 'A'];
                        len = 3;
                        goto found;
                    }
            }
            if (getch2_len >= 4 && c == '[' && getch2_buf[3] == '~') {
                int c = getch2_buf[2];
                const int ctable[8] = {MP_KEY_HOME, MP_KEY_INS, MP_KEY_DEL, MP_KEY_END, MP_KEY_PGUP, MP_KEY_PGDWN, MP_KEY_HOME, MP_KEY_END};
                if (c >= '1' && c <= '8') {
                    code = ctable[c - '1'];
                    len = 4;
                    goto found;
                }
            }
            if (getch2_len >= 5 && c == '[' && getch2_buf[4] == '~') {
                int i = getch2_buf[2] - '0';
                int j = getch2_buf[3] - '0';
                if (i >= 0 && i <= 9 && j >= 0 && j <= 9) {
                    const short ftable[20] = {
                        11,12,13,14,15, 17,18,19,20,21,
                        23,24,25,26,28, 29,31,32,33,34 };
                    int a = i*10 + j;
                    for (i = 0; i < 20; i++)
                        if (ftable[i] == a) {
                            code = MP_KEY_F+1 + i;
                            len = 5;
                            goto found;
                        }
                }
            }
        }
    found:
        getch2_len -= len;
        for (i = 0; i < getch2_len; i++)
            getch2_buf[i] = getch2_buf[len+i];
        mplayer_put_key(fifo, code);
    }
    return true;
}

static volatile int getch2_active=0;
static volatile int getch2_enabled=0;

static void do_activate_getch2(void)
{
    if (getch2_active)
        return;
#ifdef HAVE_TERMIOS
    struct termios tio_new;
    tcgetattr(0,&tio_new);
    if (!tio_orig_set) {
        tio_orig = tio_new;
        tio_orig_set = 1;
    }
    tio_new.c_lflag &= ~(ICANON|ECHO); /* Clear ICANON and ECHO. */
    tio_new.c_cc[VMIN] = 1;
    tio_new.c_cc[VTIME] = 0;
    tcsetattr(0,TCSANOW,&tio_new);
#endif
    getch2_active=1;
}

static void do_deactivate_getch2(void)
{
    if (!getch2_active)
        return;
#ifdef HAVE_TERMIOS
    if (tio_orig_set) {
        // once set, it will never be set again
        // so we can cast away volatile here
        tcsetattr(0, TCSANOW, (const struct termios *) &tio_orig);
    }
#endif
    getch2_active=0;
}

// sigaction wrapper
static int setsigaction(int signo, void (*handler) (int),
                        int flags, bool do_mask)
{
    struct sigaction sa;
    sa.sa_handler = handler;
    if(do_mask)
        sigfillset(&sa.sa_mask);
    else
        sigemptyset(&sa.sa_mask);
    sa.sa_flags = flags;
    return sigaction(signo, &sa, NULL);
}

void getch2_poll(void){
    if (!getch2_enabled)
        return;

    // check if stdin is in the foreground process group
    int newstatus = (tcgetpgrp(0) == getpgrp());

    // and activate getch2 if it is, deactivate otherwise
    if (newstatus)
        do_activate_getch2();
    else
        do_deactivate_getch2();
}

static void stop_sighandler(int signum)
{
    do_deactivate_getch2();

    // note: for this signal, we use SA_RESETHAND but do NOT mask signals
    // so this will invoke the default handler
    raise(SIGTSTP);
}

static void continue_sighandler(int signum)
{
    // SA_RESETHAND has reset SIGTSTP, so we need to restore it here
    setsigaction(SIGTSTP, stop_sighandler, SA_RESETHAND, false);

    getch2_poll();
}

static void quit_request_sighandler(int signum)
{
    async_quit_request = 1;
}

void getch2_enable(void){
    if (getch2_enabled)
        return;

    // handlers to fix terminal settings
    setsigaction(SIGCONT, continue_sighandler, 0, true);
    setsigaction(SIGTSTP, stop_sighandler, SA_RESETHAND, false);
    setsigaction(SIGINT, quit_request_sighandler, SA_RESETHAND, false);
    setsigaction(SIGTTIN, SIG_IGN, 0, true);

    do_activate_getch2();

    getch2_enabled = 1;
}

void getch2_disable(void){
    if (!getch2_enabled)
        return;

    // restore signals
    setsigaction(SIGCONT, SIG_DFL, 0, false);
    setsigaction(SIGTSTP, SIG_DFL, 0, false);
    setsigaction(SIGINT, SIG_DFL, 0, false);
    setsigaction(SIGTTIN, SIG_DFL, 0, false);

    do_deactivate_getch2();

    getch2_enabled = 0;
}
