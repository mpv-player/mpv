#include "config.h"

#ifdef USE_DVDNAV
#include <dvdnav_internal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "mp_msg.h"
#include "../osdep/timer.h"
#include "../input/input.h"
#include "stream.h"
#include "dvdnav_stream.h"
#include "../libvo/video_out.h"
#include "../spudec.h"
#include "../m_option.h"
#include "../m_struct.h"

static struct stream_priv_s {
  int track;
  char* device;
} stream_priv_dflts = {
  1,
  NULL
};

#define ST_OFF(f) M_ST_OFF(struct stream_priv_s,f)
/// URL definition
static m_option_t stream_opts_fields[] = {
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

dvdnav_priv_t * new_dvdnav_stream(char * filename) {
  char * title_str;
  dvdnav_priv_t *dvdnav_priv;

  if (!filename)
    return NULL;

  if (!(dvdnav_priv=(dvdnav_priv_t*)calloc(1,sizeof(*dvdnav_priv))))
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

  dvdnav_stream_ignore_timers(dvdnav_priv,dvd_nav_skip_opening);

  /* turn on dvdnav caching */
  dvdnav_set_readahead_flag(dvdnav_priv->dvdnav,1);

  /* report the title?! */
  if (dvdnav_get_title_string(dvdnav_priv->dvdnav,&title_str)==DVDNAV_STATUS_OK) {
    mp_msg(MSGT_OPEN,MSGL_INFO,"Title: '%s'\n",title_str);
  }

  return dvdnav_priv;
}

int dvdnav_stream_reset(dvdnav_priv_t * dvdnav_priv) {
  if (!dvdnav_priv) return 0;

//  if (dvdnav_reset(dvdnav_priv->dvdnav)!=DVDNAV_STATUS_OK)
    return 0;

  dvdnav_priv->started=0;

  return 1;
}

void free_dvdnav_stream(dvdnav_priv_t * dvdnav_priv) {
  if (!dvdnav_priv) return;

  dvdnav_close(dvdnav_priv->dvdnav);
  dvdnav_priv->dvdnav=NULL;
  free(dvdnav_priv);
}

void dvdnav_stream_ignore_timers(dvdnav_priv_t * dvdnav_priv, int ignore) {
  if (!dvdnav_priv) return;

  dvdnav_priv->ignore_timers=ignore; 
}

int dvdnav_stream_sleeping(dvdnav_priv_t * dvdnav_priv) {
    unsigned int now;

    if (!dvdnav_priv) return 0;

    if(dvdnav_priv->sleeping)
    {
      now=GetTimer();
      while(dvdnav_priv->sleeping>1 || now<dvdnav_priv->sleep_until) {
//        printf("%s %u<%u\n",__FUNCTION__,now,dvdnav_priv->sleep_until);
//        usec_sleep(1000); /* 1ms granularity */
        return 1; 
      }
      dvdnav_still_skip(dvdnav_priv->dvdnav); // continue past...
      dvdnav_priv->sleeping=0;
      printf("%s: woke up!\n",__FUNCTION__);
    }
    dvd_nav_still=0;
    printf("%s: active\n",__FUNCTION__);
    return 0;
}

void dvdnav_stream_sleep(dvdnav_priv_t * dvdnav_priv, int seconds) {
    if (!dvdnav_priv) return;

    if (!dvdnav_priv->started) return;

    dvdnav_priv->sleeping=0;
    switch (seconds) {
    case 0:
            return;
    case 0xff:
            printf( "Sleeping indefinately\n" );
            dvdnav_priv->sleeping=2;
            break;
    default:
            printf( "Sleeping %d sec(s)\n", seconds );
            dvdnav_priv->sleep_until = GetTimer();// + seconds*1000000;
            dvdnav_priv->sleeping=1;
            break;
    }
    //if (dvdnav_priv->started) dvd_nav_still=1;
}

void dvdnav_stream_add_event(dvdnav_priv_t* dvdnav_priv, int event, unsigned char *buf, int len) {
  //printf("%s: %d\n",__FUNCTION__,event);

  dvdnav_event_t * dvdnav_event;
  mp_cmd_t * cmd;

  if (!dvdnav_priv->started) return;

  if (!(dvdnav_event=(dvdnav_event_t*)calloc(1,sizeof(*dvdnav_event)))) {
    printf("%s: dvdnav_event: out of memory!\n",__FUNCTION__);
    return;
  }
  dvdnav_event->event=event;
  dvdnav_event->details=calloc(1,len);
  memcpy(dvdnav_event->details,buf,len);
  dvdnav_event->len=len;

  if (!(cmd = (mp_cmd_t*)calloc(1,sizeof(*cmd)))) {
    printf("%s: mp_cmd_t: out of memory!\n",__FUNCTION__);
    free(dvdnav_event->details);
    free(dvdnav_event);
    return;
  }
  cmd->id=MP_CMD_DVDNAV_EVENT; // S+event;
  cmd->name=strdup("dvdnav_event"); // FIXME: do I really need a 'name'?
  cmd->nargs=1;
  cmd->args[0].v.v=dvdnav_event;

  mp_input_queue_cmd(cmd);
}

void dvdnav_stream_read(dvdnav_priv_t * dvdnav_priv, unsigned char *buf, int *len) {
  int event = DVDNAV_NOP;

  if (!len) return;
  *len=-1;
  if (!dvdnav_priv) return;
  if (!buf) return;

  if (dvd_nav_still) {
    printf("%s: got a stream_read while I should be asleep!\n",__FUNCTION__);
    *len=0;
    return;
  }

  if (dvdnav_get_next_block(dvdnav_priv->dvdnav,buf,&event,len)!=DVDNAV_STATUS_OK) {
    printf( "Error getting next block from DVD (%s)\n",dvdnav_err_to_string(dvdnav_priv->dvdnav) );
    *len=-1;
  }
  else if (event!=DVDNAV_BLOCK_OK) {

    // need to handle certain events internally (like skipping stills)
    switch (event) {
    case DVDNAV_STILL_FRAME: {
      dvdnav_still_event_t *still_event = (dvdnav_still_event_t*)(buf);

      printf("************************************************************ STILL \n");

      //if (dvdnav_priv->started) dvd_nav_still=1;
      //else
        dvdnav_still_skip(dvdnav_priv->dvdnav); // don't let dvdnav stall on this image

      break;
    }
    }

    // got an event, repeat the read
    dvdnav_stream_add_event(dvdnav_priv,event,buf,*len);
    *len=0;
  }
//  printf("%s: got %d\n",__FUNCTION__,*len);
}

void dvdnav_stream_fullstart(dvdnav_priv_t * dvdnav_priv) {
  if (dvdnav_priv && !dvdnav_priv->started) {
    dvdnav_stream_reset(dvdnav_priv);
    dvdnav_priv->started=1;
  }
}

unsigned int * dvdnav_stream_get_palette(dvdnav_priv_t * dvdnav_priv) {
  if (!dvdnav_priv) {
    printf("%s: NULL dvdnav_priv\n",__FUNCTION__);
    return NULL;
  }
  if (!dvdnav_priv->dvdnav) {
    printf("%s: NULL dvdnav_priv->dvdnav\n",__FUNCTION__);
    return NULL;
  }
  if (!dvdnav_priv->dvdnav->vm) {
    printf("%s: NULL dvdnav_priv->dvdnav->vm\n",__FUNCTION__);
    return NULL;
  }
  if (!dvdnav_priv->dvdnav->vm->state.pgc) {
    printf("%s: NULL dvdnav_priv->dvdnav->vm->state.pgc\n",__FUNCTION__);
    return NULL;
  }
  return dvdnav_priv->dvdnav->vm->state.pgc->palette;
}
  

static int seek(stream_t *s, off_t newpos) {
  if(newpos==0) {
    if(dvdnav_stream_reset((dvdnav_priv_t*)s->priv))
      s->pos=0;
  }
  if(newpos!=s->pos)
    mp_msg(MSGT_STREAM,MSGL_INFO,"Cannot seek in DVDNAV streams yet!\n");

  return 1;
}

static void stream_dvd_close(stream_t *s) {
  dvd_close(s->priv);
}

static int open_s(stream_t *stream,int mode, void* opts, int* file_format) {
  struct stream_priv_s* p = (struct stream_priv_s*)opts;
  char *filename, *name;
  int event,len,tmplen=0;
  dvdnav_priv_t *dvdnav_priv;

  mp_msg(MSGT_OPEN,MSGL_INFO,"URL: %s\n", filename);
  filename = strdup(stream->url);
  name = (filename[9] == '\0') ? NULL : filename + 9;
  if(!name) name=DEFAULT_DVD_DEVICE;
  if(!(dvdnav_priv=new_dvdnav_stream(name))) {
    mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_CantOpenDVD,name);
    return STREAM_UNSUPORTED;
  }

  stream->sector_size = 2048;
  stream->flags = STREAM_READ | STREAM_SEEK;
  stream->fill_buffer = fill_buffer;
  stream->seek = seek;
  stream->close = stream_dvd_close;
  stream->type = STREAMTYPE_DVDNAV;
  stream->priv=(void*)dvdnav_priv;

  return STREAM_OK;
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

#endif /* USE_DVDNAV */
