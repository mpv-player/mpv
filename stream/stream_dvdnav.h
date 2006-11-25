#ifndef _MPLAYER_DVDNAV_STREAM_H
#define _MPLAYER_DVDNAV_STREAM_H

#include <dvdnav.h>

typedef struct {
  int event;             /* event number fromd dvdnav_events.h */
  void * details;        /* event details */
  int len;               /* bytes in details */
} dvdnav_event_t;

typedef struct {
  uint16_t sx, sy;
  uint16_t ex, ey;
} nav_highlight_t;

typedef struct {
  dvdnav_t *       dvdnav;              /* handle to libdvdnav stuff */
  char *           filename;            /* path */
  int              ignore_timers;       /* should timers be skipped? */
  int              sleeping;            /* are we sleeping? */
  unsigned int     sleep_until;         /* timer */
  int              started;             /* Has mplayer initialization finished? */
  unsigned int     duration;            /* in milliseconds */
  int              mousex, mousey;
  int              title;
} dvdnav_priv_t;


int dvdnav_number_of_subs(stream_t *stream);
int dvdnav_sid_from_lang(stream_t *stream, unsigned char *language);
int mp_dvdnav_handle_input(stream_t *stream, int cmd, int *button);
void mp_dvdnav_update_mouse_pos(stream_t *stream, int32_t x, int32_t y, int* button);
void mp_dvdnav_get_highlight (nav_highlight_t *hl);

#endif
