#include "config.h"

#ifdef HAVE_CDDA

#include "stream.h"
#include "../cfgparser.h"

#include <cdda_interface.h>
#include <cdda_paranoia.h>

#include <stdio.h>
#include <stdlib.h>


static int speed = -1;
static int paranoia_mode = 1;
static char* generic_dev = NULL;
static int sector_size = 0;
static int search_overlap = -1;
static int toc_bias = 0;
static int toc_offset = 0;
static int no_skip = 0;

static config_t cdda_opts[] = {
  { "speed", &speed, CONF_TYPE_INT, CONF_RANGE,1,100, NULL },
  { "paranoia", &paranoia_mode, CONF_TYPE_INT,CONF_RANGE, 0, 2, NULL },
  { "generic-dev", &generic_dev, CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "sector-size", &sector_size, CONF_TYPE_INT, CONF_RANGE,1,100, NULL },
  { "overlap", &search_overlap, CONF_TYPE_INT, CONF_RANGE,0,75, NULL },
  { "toc-bias", &toc_bias, CONF_TYPE_INT, 0, 0, 0, NULL },
  { "toc-offset", &toc_offset, CONF_TYPE_INT, 0, 0, 0, NULL },
  { "noskip", &no_skip, CONF_TYPE_FLAG, 0 , 0, 1, NULL },
  { "skip", &no_skip, CONF_TYPE_FLAG, 0 , 1, 0, NULL },
  {NULL, NULL, 0, 0, 0, 0, NULL}
};

static config_t cdda_conf[] = {
  { "cdda", &cdda_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
  { NULL,NULL, 0, 0, 0, 0, NULL}
};

void cdda_register_options(m_config_t* cfg) {
  m_config_register_options(cfg,cdda_conf);
}

typedef struct {
  cdrom_drive* cd;
  cdrom_paranoia* cdp;
  int sector;
  int start_sector;
  int end_sector;
} cdda_priv;

stream_t* open_cdda(char* dev,char* track) {
  stream_t* st;
  int start_track = 0;
  int end_track = 0;
  int mode = paranoia_mode;
  int offset = toc_offset;
  cdrom_drive* cdd = NULL;
  cdda_priv* priv;
  char* end = strchr(track,'-');

  if(!end)
    start_track = end_track = atoi(track);
  else {
    int st_len = end - track;
    int ed_len = strlen(track) - 1 - st_len;

    if(st_len) {
      char st[st_len + 1];
      strncpy(st,track,st_len);
      st[st_len] = '\0';
      start_track = atoi(st);
    }
    if(ed_len) {
      char ed[ed_len + 1];
      strncpy(ed,end+1,ed_len);
      ed[ed_len] = '\0';
      end_track = atoi(ed);
    }
  }
    
  if(generic_dev)
    cdd = cdda_identify_scsi(generic_dev,dev,0,NULL);
  else
    cdd = cdda_identify(dev,0,NULL);

  if(!cdd) {
    mp_msg(MSGT_OPEN,MSGL_ERR,"Can't open cdda device\n");
    return NULL;
  }

  cdda_verbose_set(cdd, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);


  if(sector_size) {
    cdd->nsectors = sector_size;
    cdd->bigbuff = sector_size * CD_FRAMESIZE_RAW;
  }

  if(cdda_open(cdd) != 0) {
    mp_msg(MSGT_OPEN,MSGL_ERR,"Can't open disc\n");
    cdda_close(cdd);
    return NULL;
  }

  priv = (cdda_priv*)malloc(sizeof(cdda_priv));
  priv->cd = cdd;

  if(toc_bias)
    offset -= cdda_track_firstsector(cdd,1);

  if(offset) {
    int i;
    for(i = 0 ; i < cdd->tracks + 1 ; i++)
      cdd->disc_toc[i].dwStartSector += offset;
  }

  if(speed)
    cdda_speed_set(cdd,speed);

  if(start_track)
    priv->start_sector = cdda_track_firstsector(cdd,start_track);
  else
    priv->start_sector = cdda_disc_firstsector(cdd);

  if(end_track) {
    int last = cdda_tracks(cdd);
    if(end_track > last) end_track = last;
    priv->end_sector = cdda_track_lastsector(cdd,end_track);
  } else
    priv->end_sector = cdda_disc_lastsector(cdd);

  priv->cdp = paranoia_init(cdd);
  if(priv->cdp == NULL) {
    cdda_close(cdd);
    free(priv);
    return NULL;
  }

  if(mode == 0)
    mode = PARANOIA_MODE_DISABLE;
  else if(mode == 1)
    mode = PARANOIA_MODE_OVERLAP;
  else
    mode = PARANOIA_MODE_FULL;
  
  if(no_skip)
    mode |= PARANOIA_MODE_NEVERSKIP;

  if(search_overlap >= 0)
    paranoia_overlapset(cdd,search_overlap);

  paranoia_seek(priv->cdp,priv->start_sector,SEEK_SET);
  priv->sector = priv->start_sector;

  st = new_stream(-1,STREAMTYPE_CDDA);
  st->priv = priv;
  st->start_pos = priv->start_sector*CD_FRAMESIZE_RAW;
  st->end_pos = priv->end_sector*CD_FRAMESIZE_RAW;

  return st;
}

static void cdparanoia_callback(long inpos, int function) {
}

int read_cdda(stream_t* s) {
  cdda_priv* p = (cdda_priv*)s->priv;
  int16_t * buf;
  
  buf = paranoia_read(p->cdp,cdparanoia_callback);

  p->sector++;
  s->pos = p->sector*CD_FRAMESIZE_RAW;
  memcpy(s->buffer,buf,CD_FRAMESIZE_RAW);

  if(p->sector == p->end_sector)
    s->eof = 1;

  return CD_FRAMESIZE_RAW;
}

void seek_cdda(stream_t* s) {
  cdda_priv* p = (cdda_priv*)s->priv;
  int sec = s->pos/CD_FRAMESIZE_RAW;

  if(sec < p->start_sector)
    sec = p->start_sector;
  else if(sec > p->end_sector)
    sec = p->end_sector;

  p->sector = sec;
  s->pos = sec*CD_FRAMESIZE_RAW;

  paranoia_seek(p->cdp,sec,SEEK_SET);

}

void close_cdda(stream_t* s) {
  cdda_priv* p = (cdda_priv*)s->priv;
  paranoia_free(p->cdp);
  cdda_close(p->cd);
}

#endif
