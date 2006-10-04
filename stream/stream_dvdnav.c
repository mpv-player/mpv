#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "mp_msg.h"
#include "osdep/timer.h"
#include "input/input.h"
#include "stream.h"
#include "libmpdemux/demuxer.h"
#include "stream_dvdnav.h"
#include "libvo/video_out.h"
#include "spudec.h"
#include "m_option.h"
#include "m_struct.h"
#include "help_mp.h"

extern char *dvd_device;
extern char *audio_lang, *dvdsub_lang;

static struct stream_priv_s {
  int track;
  char* device;
} stream_priv_dflts = {
  0,
  NULL
};

#define ST_OFF(f) M_ST_OFF(struct stream_priv_s,f)
/// URL definition
static m_option_t stream_opts_fields[] = {
  {"filename", 	ST_OFF(device), CONF_TYPE_STRING, 0, 0, 0, NULL },
  {"hostname", 	ST_OFF(track), CONF_TYPE_INT, 0, 0, 0, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};
static struct m_struct_st stream_opts = {
  "dvd",
  sizeof(struct stream_priv_s),
  &stream_priv_dflts,
  stream_opts_fields
};

int dvd_nav_skip_opening=0;     /* skip opening stalls? */
int osd_show_dvd_nav_delay=0;   /* count down for dvd nav text on OSD */
char dvd_nav_text[50];          /* for reporting stuff to OSD */
int osd_show_dvd_nav_highlight; /* show highlight area */
int osd_show_dvd_nav_sx;        /* start x .... */
int osd_show_dvd_nav_ex;
int osd_show_dvd_nav_sy;
int osd_show_dvd_nav_ey;
int dvd_nav_still=0;            /* are we on a still picture? */

static int seek(stream_t *s, off_t newpos);

static dvdnav_priv_t * new_dvdnav_stream(char * filename) {
  char * title_str;
  dvdnav_priv_t *dvdnav_priv;

  if (!filename)
    return NULL;

  if (!(dvdnav_priv=calloc(1,sizeof(*dvdnav_priv))))
    return NULL;

  if (!(dvdnav_priv->filename=strdup(filename))) {
    free(dvdnav_priv);
    return NULL;
  }

  if(dvdnav_open(&(dvdnav_priv->dvdnav),dvdnav_priv->filename)!=DVDNAV_STATUS_OK)
  {
    free(dvdnav_priv->filename);
    free(dvdnav_priv);
    return NULL;
  }

  if (!dvdnav_priv->dvdnav) {
    free(dvdnav_priv);
    return NULL;
  }

  if(1)	//from vlc: if not used dvdnav from cvs will fail
  {
    int len, event;
    char buf[2048];
    
    dvdnav_get_next_block(dvdnav_priv->dvdnav,buf,&event,&len);
    dvdnav_sector_search(dvdnav_priv->dvdnav, 0, SEEK_SET);
  }
  
  /* turn off dvdnav caching */
  dvdnav_set_readahead_flag(dvdnav_priv->dvdnav, 0);
  if(dvdnav_set_PGC_positioning_flag(dvdnav_priv->dvdnav, 1) != DVDNAV_STATUS_OK)
    mp_msg(MSGT_OPEN,MSGL_ERR,"stream_dvdnav, failed to set PGC positioning\n");
#if 1
  /* report the title?! */
  if (dvdnav_get_title_string(dvdnav_priv->dvdnav,&title_str)==DVDNAV_STATUS_OK) {
    mp_msg(MSGT_IDENTIFY, MSGL_INFO,"Title: '%s'\n",title_str);
  }
#endif

  //dvdnav_event_clear(dvdnav_priv);

  return dvdnav_priv;
}

static int dvdnav_stream_read(dvdnav_priv_t * dvdnav_priv, unsigned char *buf, int *len) {
  int event = DVDNAV_NOP;

  if (!len) return -1;
  *len=-1;
  if (!dvdnav_priv) return -1;
  if (!buf) return -1;

  if (dvd_nav_still) {
    mp_msg(MSGT_OPEN,MSGL_V, "%s: got a stream_read while I should be asleep!\n",__FUNCTION__);
    *len=0;
    return -1;
  }

  if (dvdnav_get_next_block(dvdnav_priv->dvdnav,buf,&event,len)!=DVDNAV_STATUS_OK) {
    mp_msg(MSGT_OPEN,MSGL_V, "Error getting next block from DVD %d (%s)\n",event, dvdnav_err_to_string(dvdnav_priv->dvdnav) );
    *len=-1;
  }
  else if (event!=DVDNAV_BLOCK_OK) {

    // need to handle certain events internally (like skipping stills)
    switch (event) {
    case DVDNAV_STILL_FRAME: {
      dvdnav_still_event_t *still_event = (dvdnav_still_event_t*)(buf);
      //if (dvdnav_priv->started) dvd_nav_still=1;
      //else
        dvdnav_still_skip(dvdnav_priv->dvdnav); // don't let dvdnav stall on this image

      break;
    }
    case DVDNAV_CELL_CHANGE: {
        dvdnav_cell_change_event_t *ev =  (dvdnav_cell_change_event_t*)buf;
        if(ev->pgc_length)
          dvdnav_priv->duration = ev->pgc_length/90;
        break;
    }
    case DVDNAV_WAIT:
        dvdnav_wait_skip(dvdnav_priv->dvdnav);
        break;
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
uint32_t pos = 0, len = 0, sector = 0;
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
  free(priv);
}


static int fill_buffer(stream_t *s, char *but, int len)
{
    int event;
    dvdnav_priv_t* dvdnav_priv=s->priv;
    len=0;
    if(!s->end_pos)
      update_title_len(s);
    while(!len) /* grab all event until DVDNAV_BLOCK_OK (len=2048), DVDNAV_STOP or DVDNAV_STILL_FRAME */
    {
        if(-1==(event=dvdnav_stream_read(dvdnav_priv, s->buffer, &len)) || len==-1)
        {
            mp_msg(MSGT_CPLAYER,MSGL_ERR, "DVDNAV stream read error!\n");
            return 0;
        }
        switch (event) {
            case DVDNAV_STOP: return len;
	    case DVDNAV_BLOCK_OK: return len;
            case DVDNAV_VTS_CHANGE: {
                s->end_pos = 0;
                update_title_len(s);
                break;
            }
        }
  }
  mp_msg(MSGT_STREAM,MSGL_DBG2,"DVDNAV fill_buffer len: %d\n",len);
  return len;
}

static int control(stream_t *stream, int cmd, void* arg) {
  dvdnav_priv_t* dvdnav_priv=stream->priv;
  int tit, part;

  switch(cmd) 
  {
    case STREAM_CTRL_SEEK_TO_CHAPTER:
    {
      int chap = *((unsigned int *)arg)+1;

      if(chap < 1 || dvdnav_current_title_info(dvdnav_priv->dvdnav, &tit, &part) != DVDNAV_STATUS_OK)
        break;
      if(dvdnav_part_play(dvdnav_priv->dvdnav, tit, chap) != DVDNAV_STATUS_OK)
        break;
      return 1;
    }
    case STREAM_CTRL_GET_NUM_CHAPTERS:
    {
      if(dvdnav_current_title_info(dvdnav_priv->dvdnav, &tit, &part) != DVDNAV_STATUS_OK)
        break;
      if(dvdnav_get_number_of_parts(dvdnav_priv->dvdnav, tit, &part) != DVDNAV_STATUS_OK)
        break;
      if(!part)
        break;
      *((unsigned int *)arg) = part;
      return 1;
    }
    case STREAM_CTRL_GET_CURRENT_CHAPTER:
    {
      if(dvdnav_current_title_info(dvdnav_priv->dvdnav, &tit, &part) != DVDNAV_STATUS_OK)
        break;
      *((unsigned int *)arg) = part - 1;
      return 1;
    }
    case STREAM_CTRL_GET_TIME_LENGTH:
    {
        if(dvdnav_priv->duration)
        {
          *((unsigned int *)arg) = dvdnav_priv->duration;
          return 1;
        }
    }
  }

  return STREAM_UNSUPORTED;
}

static int open_s(stream_t *stream,int mode, void* opts, int* file_format) {
  struct stream_priv_s* p = (struct stream_priv_s*)opts;
  char *filename;
  int event,len,tmplen=0;
  uint32_t pos, l2;
  dvdnav_priv_t *dvdnav_priv;
  dvdnav_status_t status;

  if(p->device) filename = p->device; 
  else if(dvd_device) filename= dvd_device; 
  else filename = DEFAULT_DVD_DEVICE;
  if(!(dvdnav_priv=new_dvdnav_stream(filename))) {
    mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_CantOpenDVD,filename);
    return STREAM_UNSUPORTED;
  }

  if(p->track > 0) {
  if(dvdnav_title_play(dvdnav_priv->dvdnav, p->track) != DVDNAV_STATUS_OK) {
    mp_msg(MSGT_OPEN,MSGL_FATAL,"dvdnav_stream, couldn't select title %d, error '%s'\n", p->track, dvdnav_err_to_string(dvdnav_priv->dvdnav));
    return STREAM_UNSUPORTED;
  }
  } else if(p->track == -1)
      dvdnav_menu_call(dvdnav_priv->dvdnav, DVD_MENU_Root);
    else {
    mp_msg(MSGT_OPEN,MSGL_INFO,"dvdnav_stream, you didn't specify a track number (as in dvdnav://1), playing whole disc\n");
    dvdnav_menu_call(dvdnav_priv->dvdnav, DVD_MENU_Title);
  }

  stream->sector_size = 2048;
  stream->flags = STREAM_READ | STREAM_SEEK;
  stream->fill_buffer = fill_buffer;
  stream->seek = seek;
  stream->control = control;
  stream->close = stream_dvdnav_close;
  stream->type = STREAMTYPE_DVDNAV;
  stream->priv=(void*)dvdnav_priv;
  *file_format = DEMUXER_TYPE_MPEG_PS;

  update_title_len(stream);
  if(!stream->pos)
    mp_msg(MSGT_OPEN,MSGL_ERR, "INIT ERROR: %d, couldn't get init pos %s\r\n", status, dvdnav_err_to_string(dvdnav_priv->dvdnav));
  
  mp_msg(MSGT_OPEN,MSGL_INFO, "Remember to disable MPlayer's cache when playing dvdnav:// streams (adding -nocache to your command like)\r\n");

  return STREAM_OK;
}


int mp_dvdnav_handle_input(stream_t *stream, int cmd, int *button) {
  dvdnav_priv_t * dvdnav_priv=(dvdnav_priv_t*)stream->priv;
  dvdnav_t *nav = dvdnav_priv->dvdnav;
  dvdnav_status_t status=DVDNAV_STATUS_ERR;
  pci_t *pci = dvdnav_get_current_nav_pci(nav);
  int reset = 0;

  if(cmd != MP_CMD_DVDNAV_SELECT && !pci)
    return 0;

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
      reset = 1;
      break;
    case MP_CMD_DVDNAV_PREVMENU: {
      int title=0, part=0;

      dvdnav_current_title_info(nav, &title, &part);
      if(title) {
        if(dvdnav_menu_call(nav, DVD_MENU_Part) == DVDNAV_STATUS_OK
           || dvdnav_menu_call(nav, DVD_MENU_Title) == DVDNAV_STATUS_OK) {
          reset = 1;
          break;
        }
      }
      if(dvdnav_menu_call(nav, DVD_MENU_Root) == DVDNAV_STATUS_OK)
        reset = 1;
      }
      break;
    case MP_CMD_DVDNAV_SELECT:
      status = dvdnav_button_activate(nav, pci);
      if(status == DVDNAV_STATUS_OK) reset = 1;
      break;
    case MP_CMD_DVDNAV_MOUSECLICK:
      /*
        this is a workaround: in theory the simple dvdnav_lower_button_select()+dvdnav_button_activate()
        should be enough (and generally it is), but there are cases when the calls to dvdnav_lower_button_select()
        and friends fail! Hence we have to call dvdnav_mouse_activate(priv->mousex, priv->mousey) with
        the coodinates saved by mp_dvdnav_update_mouse_pos().
        This last call always works well
      */
      status = dvdnav_mouse_activate(nav, pci, dvdnav_priv->mousex, dvdnav_priv->mousey);
      break;
    default:
      mp_msg(MSGT_CPLAYER, MSGL_V, "Unknown DVDNAV cmd %d\n", cmd);
      break;
  }

  if(status == DVDNAV_STATUS_OK)
      dvdnav_get_current_highlight(nav, button);

  return reset;
}

void mp_dvdnav_update_mouse_pos(stream_t *stream, int32_t x, int32_t y, int* button) {
  dvdnav_priv_t * dvdnav_priv=(dvdnav_priv_t*)stream->priv;
  dvdnav_t *nav = dvdnav_priv->dvdnav;
  dvdnav_status_t status;
  pci_t *pci = dvdnav_get_current_nav_pci(nav);

  if(!pci) return;

  status = dvdnav_mouse_select(nav, pci, x, y);
  if(status == DVDNAV_STATUS_OK) dvdnav_get_current_highlight(nav, button);
  else *button = -1;
  dvdnav_priv->mousex = x;
  dvdnav_priv->mousey = y;
}


stream_info_t stream_info_dvdnav = {
  "DVDNAV stream",
  "null",
  "",
  "",
  open_s,
  { "dvdnav", NULL },
  &stream_opts,
  1 // Urls are an option string
};
