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

dvdnav_priv_t * new_dvdnav_stream(char * filename);
int dvdnav_stream_reset(dvdnav_priv_t * dvdnav_priv);
void free_dvdnav_stream(dvdnav_priv_t * dvdnav_priv);

void dvdnav_stream_ignore_timers(dvdnav_priv_t * dvdnav_priv, int ignore);
void dvdnav_stream_read(dvdnav_priv_t * dvdnav_priv, unsigned char *buf, int *len);

void dvdnav_stream_sleep(dvdnav_priv_t *dvdnav_priv, int seconds);
int dvdnav_stream_sleeping(dvdnav_priv_t * dvdnav_priv);

void dvdnav_stream_fullstart(dvdnav_priv_t *dvdnav_priv);
unsigned int * dvdnav_stream_get_palette(dvdnav_priv_t * dvdnav_priv);

#endif
