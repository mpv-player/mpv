/*
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "mp_msg.h"
#include "osdep/timer.h"
#include "input/input.h"
#include "stream.h"
#include "libmpdemux/demuxer.h"
#include <dvdnav/dvdnav.h>
#include "stream_dvdnav.h"
#include "libvo/video_out.h"
#include "libavutil/common.h"
#include "spudec.h"
#include "m_option.h"
#include "m_struct.h"
#include "help_mp.h"
#include "stream_dvd_common.h"

/* state flags */
typedef enum {
  NAV_FLAG_EOF                  = 1 << 0,  /* end of stream has been reached */
  NAV_FLAG_WAIT                 = 1 << 1,  /* wait event */
  NAV_FLAG_WAIT_SKIP            = 1 << 2,  /* wait skip disable */
  NAV_FLAG_CELL_CHANGE          = 1 << 3,  /* cell change event */
  NAV_FLAG_WAIT_READ_AUTO       = 1 << 4,  /* wait read auto mode */
  NAV_FLAG_WAIT_READ            = 1 << 5,  /* suspend read from stream */
  NAV_FLAG_VTS_DOMAIN           = 1 << 6,  /* vts domain */
  NAV_FLAG_SPU_SET              = 1 << 7,  /* spu_clut is valid */
  NAV_FLAG_STREAM_CHANGE        = 1 << 8,  /* title, chapter, audio or SPU */
  NAV_FLAG_AUDIO_CHANGE         = 1 << 9,  /* audio stream change event */
  NAV_FLAG_SPU_CHANGE           = 1 << 10, /* spu stream change event */
} dvdnav_state_t;

typedef struct {
  dvdnav_t *       dvdnav;              /* handle to libdvdnav stuff */
  char *           filename;            /* path */
  unsigned int     duration;            /* in milliseconds */
  int              mousex, mousey;
  int              title;
  unsigned int     spu_clut[16];
  dvdnav_highlight_event_t hlev;
  int              still_length;        /* still frame duration */
  unsigned int     state;
} dvdnav_priv_t;

static struct stream_priv_s {
  int track;
  char* device;
} stream_priv_dflts = {
  0,
  NULL
};

#define ST_OFF(f) M_ST_OFF(struct stream_priv_s,f)
/// URL definition
static const m_option_t stream_opts_fields[] = {
  {"filename", 	ST_OFF(device), CONF_TYPE_STRING, 0, 0, 0, NULL },
  {"hostname", 	ST_OFF(track),  CONF_TYPE_INT, M_OPT_RANGE, 1, 99, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};
static const struct m_struct_st stream_opts = {
  "dvd",
  sizeof(struct stream_priv_s),
  &stream_priv_dflts,
  stream_opts_fields
};

static int seek(stream_t *s, off_t newpos);
static void show_audio_subs_languages(dvdnav_t *nav);

static dvdnav_priv_t * new_dvdnav_stream(char * filename) {
  const char * title_str;
  dvdnav_priv_t *priv;

  if (!filename)
    return NULL;

  if (!(priv=calloc(1,sizeof(dvdnav_priv_t))))
    return NULL;

  if (!(priv->filename=strdup(filename))) {
    free(priv);
    return NULL;
  }

  dvd_set_speed(priv->filename, dvd_speed);

  if(dvdnav_open(&(priv->dvdnav),priv->filename)!=DVDNAV_STATUS_OK)
  {
    free(priv->filename);
    free(priv);
    return NULL;
  }

  if (!priv->dvdnav) {
    free(priv);
    return NULL;
  }

  if(1)	//from vlc: if not used dvdnav from cvs will fail
  {
    int len, event;
    char buf[2048];

    dvdnav_get_next_block(priv->dvdnav,buf,&event,&len);
    dvdnav_sector_search(priv->dvdnav, 0, SEEK_SET);
  }

  /* turn off dvdnav caching */
  dvdnav_set_readahead_flag(priv->dvdnav, 0);
  if(dvdnav_set_PGC_positioning_flag(priv->dvdnav, 1) != DVDNAV_STATUS_OK)
    mp_msg(MSGT_OPEN,MSGL_ERR,"stream_dvdnav, failed to set PGC positioning\n");
  /* report the title?! */
  if (dvdnav_get_title_string(priv->dvdnav,&title_str)==DVDNAV_STATUS_OK) {
    mp_msg(MSGT_IDENTIFY, MSGL_INFO,"Title: '%s'\n",title_str);
  }

  //dvdnav_event_clear(priv);

  return priv;
}

static void dvdnav_get_highlight (dvdnav_priv_t *priv, int display_mode) {
  pci_t *pnavpci = NULL;
  dvdnav_highlight_event_t *hlev = &(priv->hlev);
  int btnum;

  if (!priv || !priv->dvdnav)
    return;

  pnavpci = dvdnav_get_current_nav_pci (priv->dvdnav);
  if (!pnavpci)
    return;

  dvdnav_get_current_highlight (priv->dvdnav, &(hlev->buttonN));
  hlev->display = display_mode; /* show */

  if (hlev->buttonN > 0 && pnavpci->hli.hl_gi.btn_ns > 0 && hlev->display) {
    for (btnum = 0; btnum < pnavpci->hli.hl_gi.btn_ns; btnum++) {
      btni_t *btni = &(pnavpci->hli.btnit[btnum]);

      if (hlev->buttonN == btnum + 1) {
        hlev->sx = FFMIN (btni->x_start, btni->x_end);
        hlev->ex = FFMAX (btni->x_start, btni->x_end);
        hlev->sy = FFMIN (btni->y_start, btni->y_end);
        hlev->ey = FFMAX (btni->y_start, btni->y_end);

        hlev->palette = (btni->btn_coln == 0) ? 0 :
          pnavpci->hli.btn_colit.btn_coli[btni->btn_coln - 1][0];
        break;
      }
    }
  } else { /* hide button or no button */
    hlev->sx = hlev->ex = 0;
    hlev->sy = hlev->ey = 0;
    hlev->palette = hlev->buttonN = 0;
  }
}

static inline int dvdnav_get_duration (int length) {
  return (length == 255) ? 0 : length * 1000;
}

static int dvdnav_stream_read(dvdnav_priv_t * priv, unsigned char *buf, int *len) {
  int event = DVDNAV_NOP;

  *len=-1;
  if (dvdnav_get_next_block(priv->dvdnav,buf,&event,len)!=DVDNAV_STATUS_OK) {
    mp_msg(MSGT_OPEN,MSGL_V, "Error getting next block from DVD %d (%s)\n",event, dvdnav_err_to_string(priv->dvdnav) );
    *len=-1;
  }
  else if (event!=DVDNAV_BLOCK_OK) {
    // need to handle certain events internally (like skipping stills)
    switch (event) {
      case DVDNAV_NAV_PACKET:
        return event;
      case DVDNAV_STILL_FRAME: {
        dvdnav_still_event_t *still_event = (dvdnav_still_event_t *) buf;
        priv->still_length = still_event->length;
        /* set still frame duration */
        priv->duration = dvdnav_get_duration (priv->still_length);
        if (priv->still_length <= 1) {
          pci_t *pnavpci = dvdnav_get_current_nav_pci (priv->dvdnav);
          priv->duration = mp_dvdtimetomsec (&pnavpci->pci_gi.e_eltm);
        }
        break;
      }
      case DVDNAV_HIGHLIGHT: {
        dvdnav_get_highlight (priv, 1);
        break;
      }
      case DVDNAV_CELL_CHANGE: {
        dvdnav_cell_change_event_t *ev =  (dvdnav_cell_change_event_t*)buf;
        uint32_t nextstill;

        priv->state &= ~NAV_FLAG_WAIT_SKIP;
        priv->state |= NAV_FLAG_STREAM_CHANGE;
        if(ev->pgc_length)
          priv->duration = ev->pgc_length/90;

        if (dvdnav_is_domain_vts(priv->dvdnav)) {
          mp_msg(MSGT_IDENTIFY, MSGL_INFO, "DVDNAV_TITLE_IS_MOVIE\n");
          priv->state &= ~NAV_FLAG_VTS_DOMAIN;
        } else {
          mp_msg(MSGT_IDENTIFY, MSGL_INFO, "DVDNAV_TITLE_IS_MENU\n");
          priv->state |= NAV_FLAG_VTS_DOMAIN;
        }

        nextstill = dvdnav_get_next_still_flag (priv->dvdnav);
        if (nextstill) {
          priv->duration = dvdnav_get_duration (nextstill);
          priv->still_length = nextstill;
          if (priv->still_length <= 1) {
            pci_t *pnavpci = dvdnav_get_current_nav_pci (priv->dvdnav);
            priv->duration = mp_dvdtimetomsec (&pnavpci->pci_gi.e_eltm);
          }
        }

        break;
      }
      case DVDNAV_SPU_CLUT_CHANGE: {
        memcpy(priv->spu_clut, buf, 16*sizeof(unsigned int));
        priv->state |= NAV_FLAG_SPU_SET;
        break;
      }
      case DVDNAV_WAIT: {
        if ((priv->state & NAV_FLAG_WAIT_SKIP) &&
            !(priv->state & NAV_FLAG_WAIT))
          dvdnav_wait_skip (priv->dvdnav);
        else
          priv->state |= NAV_FLAG_WAIT;
        break;
      }
      case DVDNAV_VTS_CHANGE: {
        priv->state &= ~NAV_FLAG_WAIT_SKIP;
        priv->state |= NAV_FLAG_STREAM_CHANGE;
        break;
      }
      case DVDNAV_SPU_STREAM_CHANGE: {
        priv->state |= NAV_FLAG_STREAM_CHANGE;
        break;
      }
    }

    *len=0;
  }
  return event;
}

static void update_title_len(stream_t *stream) {
  dvdnav_priv_t *priv = stream->priv;
  dvdnav_status_t status;
  uint32_t pos = 0, len = 0;

  status = dvdnav_get_position(priv->dvdnav, &pos, &len);
  if(status == DVDNAV_STATUS_OK && len) {
    stream->end_pos = (off_t) len * 2048;
    stream->seek = seek;
  } else {
    stream->seek = NULL;
    stream->end_pos = 0;
  }
}


static int seek(stream_t *s, off_t newpos) {
  uint32_t sector = 0;
  dvdnav_priv_t *priv = s->priv;

  if(s->end_pos && newpos > s->end_pos)
     newpos = s->end_pos;
  sector = newpos / 2048ULL;
  if(dvdnav_sector_search(priv->dvdnav, (uint64_t) sector, SEEK_SET) != DVDNAV_STATUS_OK)
    goto fail;

  s->pos = newpos;

  return 1;

fail:
  mp_msg(MSGT_STREAM,MSGL_INFO,"dvdnav_stream, seeking to %"PRIu64" failed: %s\n", newpos, dvdnav_err_to_string(priv->dvdnav));

  return 1;
}

static void stream_dvdnav_close(stream_t *s) {
  dvdnav_priv_t *priv = s->priv;
  dvdnav_close(priv->dvdnav);
  priv->dvdnav = NULL;
  dvd_set_speed(priv->filename, -1);
  free(priv);
}


static int fill_buffer(stream_t *s, char *but, int len)
{
    int event;

    dvdnav_priv_t* priv=s->priv;
    if (priv->state & NAV_FLAG_WAIT_READ) /* read is suspended */
      return -1;
    len=0;
    if(!s->end_pos)
      update_title_len(s);
    while(!len) /* grab all event until DVDNAV_BLOCK_OK (len=2048), DVDNAV_STOP or DVDNAV_STILL_FRAME */
    {
      event=dvdnav_stream_read(priv, s->buffer, &len);
      if(event==-1 || len==-1)
      {
        mp_msg(MSGT_CPLAYER,MSGL_ERR, "DVDNAV stream read error!\n");
        return 0;
      }
      if (event != DVDNAV_BLOCK_OK)
        dvdnav_get_highlight (priv, 1);
      switch (event) {
        case DVDNAV_STOP: {
          priv->state |= NAV_FLAG_EOF;
          return len;
        }
        case DVDNAV_BLOCK_OK:
        case DVDNAV_NAV_PACKET:
        case DVDNAV_STILL_FRAME:
          return len;
        case DVDNAV_WAIT: {
          if (priv->state & NAV_FLAG_WAIT)
            return len;
          break;
        }
        case DVDNAV_VTS_CHANGE: {
          int tit = 0, part = 0;
          dvdnav_vts_change_event_t *vts_event = (dvdnav_vts_change_event_t *)s->buffer;
          mp_msg(MSGT_CPLAYER,MSGL_INFO, "DVDNAV, switched to title: %d\r\n", vts_event->new_vtsN);
          priv->state |= NAV_FLAG_CELL_CHANGE;
          priv->state |= NAV_FLAG_AUDIO_CHANGE;
          priv->state |= NAV_FLAG_SPU_CHANGE;
          priv->state &= ~NAV_FLAG_WAIT_SKIP;
          priv->state &= ~NAV_FLAG_WAIT;
          s->end_pos = 0;
          update_title_len(s);
          show_audio_subs_languages(priv->dvdnav);
          if (priv->state & NAV_FLAG_WAIT_READ_AUTO)
            priv->state |= NAV_FLAG_WAIT_READ;
          if(dvdnav_current_title_info(priv->dvdnav, &tit, &part) == DVDNAV_STATUS_OK) {
            mp_msg(MSGT_CPLAYER,MSGL_V, "\r\nDVDNAV, NEW TITLE %d\r\n", tit);
            dvdnav_get_highlight (priv, 0);
            if(priv->title > 0 && tit != priv->title) {
              priv->state |= NAV_FLAG_EOF;
              return 0;
            }
          }
          break;
        }
        case DVDNAV_CELL_CHANGE: {
          priv->state |= NAV_FLAG_CELL_CHANGE;
          priv->state |= NAV_FLAG_AUDIO_CHANGE;
          priv->state |= NAV_FLAG_SPU_CHANGE;
          priv->state &= ~NAV_FLAG_WAIT_SKIP;
          priv->state &= ~NAV_FLAG_WAIT;
          if (priv->state & NAV_FLAG_WAIT_READ_AUTO)
            priv->state |= NAV_FLAG_WAIT_READ;
          if(priv->title > 0 && dvd_last_chapter > 0) {
            int tit=0, part=0;
            if(dvdnav_current_title_info(priv->dvdnav, &tit, &part) == DVDNAV_STATUS_OK && part > dvd_last_chapter) {
              priv->state |= NAV_FLAG_EOF;
              return 0;
            }
          }
          dvdnav_get_highlight (priv, 1);
        }
        break;
        case DVDNAV_AUDIO_STREAM_CHANGE:
          priv->state |= NAV_FLAG_AUDIO_CHANGE;
        break;
        case DVDNAV_SPU_STREAM_CHANGE:
          priv->state |= NAV_FLAG_SPU_CHANGE;
        break;
      }
  }
  mp_msg(MSGT_STREAM,MSGL_DBG2,"DVDNAV fill_buffer len: %d\n",len);
  return len;
}

static int control(stream_t *stream, int cmd, void* arg) {
  dvdnav_priv_t* priv=stream->priv;
  int tit, part;

  switch(cmd)
  {
    case STREAM_CTRL_SEEK_TO_CHAPTER:
    {
      int chap = *((unsigned int *)arg)+1;

      if(chap < 1 || dvdnav_current_title_info(priv->dvdnav, &tit, &part) != DVDNAV_STATUS_OK)
        break;
      if(dvdnav_part_play(priv->dvdnav, tit, chap) != DVDNAV_STATUS_OK)
        break;
      return 1;
    }
    case STREAM_CTRL_GET_NUM_CHAPTERS:
    {
      if(dvdnav_current_title_info(priv->dvdnav, &tit, &part) != DVDNAV_STATUS_OK)
        break;
      if(dvdnav_get_number_of_parts(priv->dvdnav, tit, &part) != DVDNAV_STATUS_OK)
        break;
      if(!part)
        break;
      *((unsigned int *)arg) = part;
      return 1;
    }
    case STREAM_CTRL_GET_CURRENT_CHAPTER:
    {
      if(dvdnav_current_title_info(priv->dvdnav, &tit, &part) != DVDNAV_STATUS_OK)
        break;
      *((unsigned int *)arg) = part - 1;
      return 1;
    }
    case STREAM_CTRL_GET_TIME_LENGTH:
    {
      if(priv->duration || priv->still_length)
      {
        *((double *)arg) = (double)priv->duration / 1000.0;
        return 1;
      }
      break;
    }
    case STREAM_CTRL_GET_ASPECT_RATIO:
    {
      uint8_t ar = dvdnav_get_video_aspect(priv->dvdnav);
      *((double *)arg) = !ar ? 4.0/3.0 : 16.0/9.0;
      return 1;
    }
    case STREAM_CTRL_GET_CURRENT_TIME:
    {
      double tm;
      tm = dvdnav_get_current_time(priv->dvdnav)/90000.0f;
      if(tm != -1)
      {
        *((double *)arg) = tm;
        return 1;
      }
      break;
    }
    case STREAM_CTRL_SEEK_TO_TIME:
    {
      uint64_t tm = (uint64_t) (*((double*)arg) * 90000);
      if(dvdnav_time_search(priv->dvdnav, tm) == DVDNAV_STATUS_OK)
        return 1;
      break;
    }
    case STREAM_CTRL_GET_NUM_ANGLES:
    {
        uint32_t curr, angles;
        if(dvdnav_get_angle_info(priv->dvdnav, &curr, &angles) != DVDNAV_STATUS_OK)
          break;
        *((int *)arg) = angles;
        return 1;
    }
    case STREAM_CTRL_GET_ANGLE:
    {
        uint32_t curr, angles;
        if(dvdnav_get_angle_info(priv->dvdnav, &curr, &angles) != DVDNAV_STATUS_OK)
          break;
        *((int *)arg) = curr;
        return 1;
    }
    case STREAM_CTRL_SET_ANGLE:
    {
        uint32_t curr, angles;
        int new_angle = *((int *)arg);
        if(dvdnav_get_angle_info(priv->dvdnav, &curr, &angles) != DVDNAV_STATUS_OK)
          break;
        if(new_angle>angles || new_angle<1)
            break;
        if(dvdnav_angle_change(priv->dvdnav, new_angle) != DVDNAV_STATUS_OK)
        return 1;
    }
  }

  return STREAM_UNSUPPORTED;
}

static void identify_chapters(dvdnav_t *nav, uint32_t title)
{
  uint64_t *parts=NULL, duration=0;
  uint32_t n, i, t;
  n = dvdnav_describe_title_chapters(nav, title, &parts, &duration);
  if(parts) {
    t = duration / 90;
    mp_msg(MSGT_IDENTIFY, MSGL_V, "ID_DVD_TITLE_%d_LENGTH=%d.%03d\n", title, t / 1000, t % 1000);
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "TITLE %u, CHAPTERS: ", title);
    for(i=0; i<n; i++) {
      t = parts[i] /  90000;
      mp_msg(MSGT_IDENTIFY, MSGL_INFO, "%02d:%02d:%02d,", t/3600, (t/60)%60, t%60);
    }
    free(parts);
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "\n");
  }
}

static void identify(dvdnav_priv_t *priv, struct stream_priv_s *p)
{
  uint32_t titles=0, i;
  if(p->track <= 0) {
    dvdnav_get_number_of_titles(priv->dvdnav, &titles);
    for(i=0; i<titles; i++)
      identify_chapters(priv->dvdnav, i);
  }
  else
    identify_chapters(priv->dvdnav, p->track);
}

static void show_audio_subs_languages(dvdnav_t *nav)
{
  uint8_t lg;
  uint16_t i, lang, format, id, channels;
  int base[7] = {128, 0, 0, 0, 160, 136, 0};
  for(i=0; i<8; i++)
  {
    char tmp[] = "unknown";
    lg = dvdnav_get_audio_logical_stream(nav, i);
    if(lg == 0xff) continue;
    channels = dvdnav_audio_stream_channels(nav, lg);
    if(channels == 0xFFFF)
      channels = 2; //unknown
    else
      channels--;
    lang = dvdnav_audio_stream_to_lang(nav, lg);
    if(lang != 0xFFFF)
    {
      tmp[0] = lang >> 8;
      tmp[1] = lang & 0xFF;
      tmp[2] = 0;
    }
    format = dvdnav_audio_stream_format(nav, lg);
    if(format == 0xFFFF || format > 6)
      format = 1; //unknown
    id = i + base[format];
    mp_msg(MSGT_OPEN,MSGL_STATUS,MSGTR_DVDaudioStreamInfo, i,
           dvd_audio_stream_types[format], dvd_audio_stream_channels[channels], tmp, id);
    if (lang != 0xFFFF && lang && tmp[0])
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_AID_%d_LANG=%s\n", id, tmp);
  }

  for(i=0; i<32; i++)
  {
    char tmp[] = "unknown";
    lg = dvdnav_get_spu_logical_stream(nav, i);
    if(lg == 0xff) continue;
    lang = dvdnav_spu_stream_to_lang(nav, i);
    if(lang != 0xFFFF)
    {
      tmp[0] = lang >> 8;
      tmp[1] = lang & 0xFF;
      tmp[2] = 0;
    }
    mp_msg(MSGT_OPEN,MSGL_STATUS,MSGTR_DVDsubtitleLanguage, lg, tmp);
    if (lang != 0xFFFF && lang && tmp[0])
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_SID_%d_LANG=%s\n", lg, tmp);
  }
}

static int open_s(stream_t *stream,int mode, void* opts, int* file_format) {
  struct stream_priv_s* p = (struct stream_priv_s*)opts;
  char *filename;
  dvdnav_priv_t *priv;

  if(p->device) filename = p->device;
  else if(dvd_device) filename= dvd_device;
  else filename = DEFAULT_DVD_DEVICE;
  if(!(priv=new_dvdnav_stream(filename))) {
    mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_CantOpenDVD,filename, strerror(errno));
    return STREAM_UNSUPPORTED;
  }

  if(p->track > 0) {
    priv->title = p->track;
    if(dvdnav_title_play(priv->dvdnav, p->track) != DVDNAV_STATUS_OK) {
      mp_msg(MSGT_OPEN,MSGL_FATAL,"dvdnav_stream, couldn't select title %d, error '%s'\n", p->track, dvdnav_err_to_string(priv->dvdnav));
      return STREAM_UNSUPPORTED;
    }
  } else if (p->track == 0) {
    if(dvdnav_menu_call(priv->dvdnav, DVD_MENU_Root) != DVDNAV_STATUS_OK)
      dvdnav_menu_call(priv->dvdnav, DVD_MENU_Title);
  }
  if(mp_msg_test(MSGT_IDENTIFY, MSGL_INFO))
    identify(priv, p);
  if(p->track > 0)
    show_audio_subs_languages(priv->dvdnav);
  if(dvd_angle > 1)
    dvdnav_angle_change(priv->dvdnav, dvd_angle);

  stream->sector_size = 2048;
  stream->flags = STREAM_READ | MP_STREAM_SEEK;
  stream->fill_buffer = fill_buffer;
  stream->seek = seek;
  stream->control = control;
  stream->close = stream_dvdnav_close;
  stream->type = STREAMTYPE_DVDNAV;
  stream->priv=(void*)priv;
  *file_format = DEMUXER_TYPE_MPEG_PS;

  update_title_len(stream);
  if(!stream->pos && p->track > 0)
    mp_msg(MSGT_OPEN,MSGL_ERR, "INIT ERROR: couldn't get init pos %s\r\n", dvdnav_err_to_string(priv->dvdnav));

  mp_msg(MSGT_OPEN,MSGL_INFO, "Remember to disable MPlayer's cache when playing dvdnav:// streams (adding -nocache to your command line)\r\n");

  return STREAM_OK;
}


void mp_dvdnav_handle_input(stream_t *stream, int cmd, int *button) {
  dvdnav_priv_t * priv = stream->priv;
  dvdnav_t *nav = priv->dvdnav;
  dvdnav_status_t status=DVDNAV_STATUS_ERR;
  pci_t *pci = dvdnav_get_current_nav_pci(nav);

  if(cmd != MP_CMD_DVDNAV_SELECT && !pci)
    return;

  switch(cmd) {
    case MP_CMD_DVDNAV_UP:
      status = dvdnav_upper_button_select(nav, pci);
      break;
    case MP_CMD_DVDNAV_DOWN:
      status = dvdnav_lower_button_select(nav, pci);
      break;
    case MP_CMD_DVDNAV_LEFT:
      status = dvdnav_left_button_select(nav, pci);
      break;
    case MP_CMD_DVDNAV_RIGHT:
      status = dvdnav_right_button_select(nav, pci);
      break;
    case MP_CMD_DVDNAV_MENU:
      status = dvdnav_menu_call(nav,DVD_MENU_Root);
      break;
    case MP_CMD_DVDNAV_PREVMENU: {
      int title=0, part=0;

      dvdnav_current_title_info(nav, &title, &part);
      if(title) {
        if((status=dvdnav_menu_call(nav, DVD_MENU_Part)) == DVDNAV_STATUS_OK)
          break;
      }
      if((status=dvdnav_menu_call(nav, DVD_MENU_Title)) == DVDNAV_STATUS_OK)
        break;
      status=dvdnav_menu_call(nav, DVD_MENU_Root);
      }
      break;
    case MP_CMD_DVDNAV_SELECT:
      status = dvdnav_button_activate(nav, pci);
      break;
    case MP_CMD_DVDNAV_MOUSECLICK:
      /*
        this is a workaround: in theory the simple dvdnav_lower_button_select()+dvdnav_button_activate()
        should be enough (and generally it is), but there are cases when the calls to dvdnav_lower_button_select()
        and friends fail! Hence we have to call dvdnav_mouse_activate(priv->mousex, priv->mousey) with
        the coodinates saved by mp_dvdnav_update_mouse_pos().
        This last call always works well
      */
      status = dvdnav_mouse_activate(nav, pci, priv->mousex, priv->mousey);
      break;
    default:
      mp_msg(MSGT_CPLAYER, MSGL_V, "Unknown DVDNAV cmd %d\n", cmd);
      break;
  }

  if(status == DVDNAV_STATUS_OK)
    dvdnav_get_current_highlight(nav, button);
}

void mp_dvdnav_update_mouse_pos(stream_t *stream, int32_t x, int32_t y, int* button) {
  dvdnav_priv_t * priv = stream->priv;
  dvdnav_t *nav = priv->dvdnav;
  dvdnav_status_t status;
  pci_t *pci = dvdnav_get_current_nav_pci(nav);

  if(!pci) return;

  status = dvdnav_mouse_select(nav, pci, x, y);
  if(status == DVDNAV_STATUS_OK) dvdnav_get_current_highlight(nav, button);
  else *button = -1;
  priv->mousex = x;
  priv->mousey = y;
}

static int mp_dvdnav_get_aid_from_format (stream_t *stream, int index, uint8_t lg) {
  dvdnav_priv_t * priv = stream->priv;
  uint8_t format;

  format = dvdnav_audio_stream_format(priv->dvdnav, lg);
  switch(format) {
  case DVDNAV_FORMAT_AC3:
    return index + 128;
  case DVDNAV_FORMAT_DTS:
    return index + 136;
  case DVDNAV_FORMAT_LPCM:
    return index + 160;
  case DVDNAV_FORMAT_MPEGAUDIO:
    return index;
  default:
    return -1;
  }

  return -1;
}

/**
 * \brief mp_dvdnav_aid_from_lang() returns the audio id corresponding to the language code 'lang'
 * \param stream: - stream pointer
 * \param lang: 2-characters language code[s], eventually separated by spaces of commas
 * \return -1 on error, current subtitle id if successful
 */
int mp_dvdnav_aid_from_lang(stream_t *stream, unsigned char *language) {
  dvdnav_priv_t * priv = stream->priv;
  int k;
  uint8_t lg;
  uint16_t lang, lcode;

  while(language && strlen(language)>=2) {
    lcode = (language[0] << 8) | (language[1]);
    for(k=0; k<32; k++) {
      lg = dvdnav_get_audio_logical_stream(priv->dvdnav, k);
      if(lg == 0xff) continue;
      lang = dvdnav_audio_stream_to_lang(priv->dvdnav, lg);
      if(lang != 0xFFFF && lang == lcode)
        return mp_dvdnav_get_aid_from_format (stream, k, lg);
    }
    language += 2;
    while(language[0]==',' || language[0]==' ') ++language;
  }
  return -1;
}

/**
 * \brief mp_dvdnav_lang_from_aid() assigns to buf the language corresponding to audio id 'aid'
 * \param stream: - stream pointer
 * \param sid: physical subtitle id
 * \param buf: buffer to contain the 2-chars language string
 * \return 0 on error, 1 if successful
 */
int mp_dvdnav_lang_from_aid(stream_t *stream, int aid, unsigned char *buf) {
  uint8_t lg;
  uint16_t lang;
  dvdnav_priv_t * priv = stream->priv;

  if(aid < 0)
    return 0;
  lg = dvdnav_get_audio_logical_stream(priv->dvdnav, aid & 0x7);
  if(lg == 0xff) return 0;
  lang = dvdnav_audio_stream_to_lang(priv->dvdnav, lg);
  if(lang == 0xffff) return 0;
  buf[0] = lang >> 8;
  buf[1] = lang & 0xFF;
  buf[2] = 0;
  return 1;
}


/**
 * \brief mp_dvdnav_sid_from_lang() returns the subtitle id corresponding to the language code 'lang'
 * \param stream: - stream pointer
 * \param lang: 2-characters language code[s], eventually separated by spaces of commas
 * \return -1 on error, current subtitle id if successful
 */
int mp_dvdnav_sid_from_lang(stream_t *stream, unsigned char *language) {
  dvdnav_priv_t * priv = stream->priv;
  uint8_t lg, k;
  uint16_t lang, lcode;

  while(language && strlen(language)>=2) {
    lcode = (language[0] << 8) | (language[1]);
    for(k=0; k<32; k++) {
      lg = dvdnav_get_spu_logical_stream(priv->dvdnav, k);
      if(lg == 0xff) continue;
      lang = dvdnav_spu_stream_to_lang(priv->dvdnav, k);
      if(lang != 0xFFFF && lang == lcode) {
        return lg;
      }
    }
    language += 2;
    while(language[0]==',' || language[0]==' ') ++language;
  }
  return -1;
}

/**
 * \brief mp_dvdnav_lang_from_sid() assigns to buf the language corresponding to subtitle id 'sid'
 * \param stream: - stream pointer
 * \param sid: physical subtitle id
 * \param buf: buffer to contain the 2-chars language string
 * \return 0 on error, 1 if successful
 */
int mp_dvdnav_lang_from_sid(stream_t *stream, int sid, unsigned char *buf) {
    uint8_t k;
    uint16_t lang;
    dvdnav_priv_t *priv = stream->priv;
    if(sid < 0) return 0;
    for (k=0; k<32; k++)
        if (dvdnav_get_spu_logical_stream(priv->dvdnav, k) == sid)
            break;
    if (k == 32)
        return 0;
    lang = dvdnav_spu_stream_to_lang(priv->dvdnav, k);
    if(lang == 0xffff) return 0;
    buf[0] = lang >> 8;
    buf[1] = lang & 0xFF;
    buf[2] = 0;
    return 1;
}

/**
 * \brief mp_dvdnav_number_of_subs() returns the count of available subtitles
 * \param stream: - stream pointer
 * \return 0 on error, something meaningful otherwise
 */
int mp_dvdnav_number_of_subs(stream_t *stream) {
  dvdnav_priv_t * priv = stream->priv;
  uint8_t lg, k, n=0;

  if (priv->state & NAV_FLAG_VTS_DOMAIN) return 0;
  for(k=0; k<32; k++) {
    lg = dvdnav_get_spu_logical_stream(priv->dvdnav, k);
    if(lg == 0xff) continue;
    if(lg >= n) n = lg + 1;
  }
  return n;
}

/**
 * \brief mp_dvdnav_get_spu_clut() returns the spu clut
 * \param stream: - stream pointer
 * \return spu clut pointer
 */
unsigned int *mp_dvdnav_get_spu_clut(stream_t *stream) {
    dvdnav_priv_t *priv = stream->priv;
    return (priv->state & NAV_FLAG_SPU_SET) ? priv->spu_clut : NULL;
}

/**
 * \brief mp_dvdnav_get_highlight() get dvdnav highlight struct
 * \param stream: - stream pointer
 * \param hl    : - highlight struct pointer
 */
void mp_dvdnav_get_highlight (stream_t *stream, nav_highlight_t *hl) {
  dvdnav_priv_t *priv = stream->priv;
  dvdnav_highlight_event_t hlev = priv->hlev;

  hl->sx = hlev.sx;
  hl->sy = hlev.sy;
  hl->ex = hlev.ex;
  hl->ey = hlev.ey;
  hl->palette = hlev.palette;
}

void mp_dvdnav_switch_title (stream_t *stream, int title) {
  dvdnav_priv_t *priv = stream->priv;
  uint32_t titles;

  dvdnav_get_number_of_titles (priv->dvdnav, &titles);
  if (title > 0 && title <= titles)
    dvdnav_title_play (priv->dvdnav, title);
}

/**
 * \brief Check if end of stream has been reached
 * \param stream: - stream pointer
 * \return 1 on really eof
 */
int mp_dvdnav_is_eof (stream_t *stream) {
  return ((dvdnav_priv_t *) stream->priv)->state & NAV_FLAG_EOF;
}

/**
 * \brief Skip still frame
 * \param stream: - stream pointer
 * \return 0 on success
 */
int mp_dvdnav_skip_still (stream_t *stream) {
  dvdnav_priv_t *priv = stream->priv;
  if (priv->still_length == 0xff)
    return 1;
  dvdnav_still_skip(priv->dvdnav);
  return 0;
}

/**
 * \brief Skip wait event
 * \param stream: - stream pointer
 * \return 0 on success
 */
int mp_dvdnav_skip_wait (stream_t *stream) {
  dvdnav_priv_t *priv = stream->priv;
  if (!(priv->state & NAV_FLAG_WAIT))
    return 1;
  priv->state &= ~NAV_FLAG_WAIT;
  dvdnav_wait_skip(priv->dvdnav);
  return 0;
}

/**
 * \brief Set wait mode
 * \param stream  : - stream pointer
 * \param mode    : - if true, then suspend block read
 * \param automode: - if true, then VTS or cell change set wait mode
 */
void mp_dvdnav_read_wait (stream_t *stream, int mode, int automode) {
  dvdnav_priv_t *priv = stream->priv;
  if (mode == 0)
    priv->state &= ~NAV_FLAG_WAIT_READ;
  if (mode > 0)
    priv->state |= NAV_FLAG_WAIT_READ;
  if (automode == 0)
    priv->state &= ~NAV_FLAG_WAIT_READ_AUTO;
  if (automode > 0)
    priv->state |= NAV_FLAG_WAIT_READ_AUTO;
}

/**
 * \brief Check if cell has changed
 * \param stream: - stream pointer
 * \param clear : - if true, then clear cell change flag
 * \return 1 if cell has changed
 */
int mp_dvdnav_cell_has_changed (stream_t *stream, int clear) {
  dvdnav_priv_t *priv = stream->priv;
  if (!(priv->state & NAV_FLAG_CELL_CHANGE))
    return 0;
  if (clear) {
    priv->state &= ~NAV_FLAG_CELL_CHANGE;
    priv->state |= NAV_FLAG_STREAM_CHANGE;
  }
  return 1;
}

/**
 * \brief Check if audio has changed
 * \param stream: - stream pointer
 * \param clear : - if true, then clear audio change flag
 * \return 1 if audio has changed
 */
int mp_dvdnav_audio_has_changed (stream_t *stream, int clear) {
  dvdnav_priv_t *priv = stream->priv;

  if (!(priv->state & NAV_FLAG_AUDIO_CHANGE))
    return 0;

  if (clear)
    priv->state &= ~NAV_FLAG_AUDIO_CHANGE;

  return 1;
}

/**
 * \brief Check if SPU has changed
 * \param stream: - stream pointer
 * \param clear : - if true, then clear spu change flag
 * \return 1 if spu has changed
 */
int mp_dvdnav_spu_has_changed (stream_t *stream, int clear) {
  dvdnav_priv_t *priv =  stream->priv;

  if (!(priv->state & NAV_FLAG_SPU_CHANGE))
    return 0;

  if (clear)
    priv->state &= ~NAV_FLAG_SPU_CHANGE;

  return 1;
}

/* Notify if something has changed in stream
 * Can be related to title, chapter, audio or SPU
 */
int mp_dvdnav_stream_has_changed (stream_t *stream) {
  dvdnav_priv_t *priv = stream->priv;

  if (!(priv->state & NAV_FLAG_STREAM_CHANGE))
    return 0;

  priv->state &= ~NAV_FLAG_STREAM_CHANGE;
  return 1;
}

const stream_info_t stream_info_dvdnav = {
  "DVDNAV stream",
  "null",
  "",
  "",
  open_s,
  { "dvdnav", NULL },
  &stream_opts,
  1 // Urls are an option string
};
