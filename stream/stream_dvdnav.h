#ifndef _MPLAYER_DVDNAV_STREAM_H
#define _MPLAYER_DVDNAV_STREAM_H

#include <dvdnav.h>

typedef struct {
  int event;             /* event number fromd dvdnav_events.h */
  void * details;        /* event details */
  int len;               /* bytes in details */
} dvdnav_event_t;

typedef struct {
  dvdnav_t *       dvdnav;              /* handle to libdvdnav stuff */
  char *           filename;            /* path */
  int              ignore_timers;       /* should timers be skipped? */
  int              sleeping;            /* are we sleeping? */
  unsigned int     sleep_until;         /* timer */
  int              started;             /* Has mplayer initialization finished? */
  unsigned char    prebuf[STREAM_BUFFER_SIZE]; /* prefill buffer */
  int              prelen;              /* length of prefill buffer */
  unsigned int     duration;            /* in milliseconds */
  int              mousex, mousey;
  int              title;
} dvdnav_priv_t;

extern int dvd_nav_still;
extern int dvd_nav_skip_opening;
extern char dvd_nav_text[50];
extern int osd_show_dvd_nav_delay;
extern int osd_show_dvd_nav_highlight;
extern int osd_show_dvd_nav_sx;
extern int osd_show_dvd_nav_ex;
extern int osd_show_dvd_nav_sy;
extern int osd_show_dvd_nav_ey;

int dvdnav_number_of_subs(stream_t *stream);
int dvdnav_sid_from_lang(stream_t *stream, unsigned char *language);
int mp_dvdnav_handle_input(stream_t *stream, int cmd, int *button);
void mp_dvdnav_update_mouse_pos(stream_t *stream, int32_t x, int32_t y, int* button);

#endif
