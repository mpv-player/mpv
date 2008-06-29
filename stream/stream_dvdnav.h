#ifndef MPLAYER_STREAM_DVDNAV_H
#define MPLAYER_STREAM_DVDNAV_H

#include <stdint.h>
#include "stream.h"

typedef struct {
  uint16_t sx, sy;
  uint16_t ex, ey;
  uint32_t palette;
} nav_highlight_t;

int mp_dvdnav_number_of_subs(stream_t *stream);
int mp_dvdnav_aid_from_audio_num(stream_t *stream, int audio_num);
int mp_dvdnav_aid_from_lang(stream_t *stream, unsigned char *language);
int mp_dvdnav_lang_from_aid(stream_t *stream, int id, unsigned char *buf);
int mp_dvdnav_sid_from_lang(stream_t *stream, unsigned char *language);
int mp_dvdnav_lang_from_sid(stream_t *stream, int sid, unsigned char *buf);
void mp_dvdnav_handle_input(stream_t *stream, int cmd, int *button);
void mp_dvdnav_update_mouse_pos(stream_t *stream, int32_t x, int32_t y, int* button);
void mp_dvdnav_get_highlight (stream_t *stream, nav_highlight_t *hl);
unsigned int *mp_dvdnav_get_spu_clut(stream_t *stream);
void mp_dvdnav_switch_title(stream_t *stream, int title);
int mp_dvdnav_is_eof (stream_t *stream);
int mp_dvdnav_skip_still (stream_t *stream);
int mp_dvdnav_skip_wait (stream_t *stream);
void mp_dvdnav_read_wait (stream_t *stream, int mode, int automode);
int mp_dvdnav_cell_has_changed (stream_t *stream, int clear);
int mp_dvdnav_audio_has_changed (stream_t *stream, int clear);
int mp_dvdnav_spu_has_changed (stream_t *stream, int clear);
int mp_dvdnav_stream_has_changed (stream_t *stream);

#endif /* MPLAYER_STREAM_DVDNAV_H */
