#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include "stream.h"
#include "m_option.h"
#include "m_struct.h"
#include "libavutil/common.h"
#include "mpbswap.h"
#include "libmpdemux/demuxer.h"

#include "cdd.h"

#include "mp_msg.h"
#include "help_mp.h"

#ifndef CD_FRAMESIZE_RAW
#define CD_FRAMESIZE_RAW CDIO_CD_FRAMESIZE_RAW
#endif


extern char *cdrom_device;

static struct cdda_params {
  int speed;
  int paranoia_mode;
  char* generic_dev;
  int sector_size;
  int search_overlap;
  int toc_bias;
  int toc_offset;
  int no_skip;
  char* device;
  m_span_t span;
} cdda_dflts = {
  -1,
  0,
  NULL,
  0,
  -1,
  0,
  0,
  0,
  NULL,
  { 0, 0 }
};

#define ST_OFF(f) M_ST_OFF(struct cdda_params,f)
static const m_option_t cdda_params_fields[] = {
  { "speed", ST_OFF(speed), CONF_TYPE_INT, M_OPT_RANGE,1,100, NULL },
  { "paranoia", ST_OFF(paranoia_mode), CONF_TYPE_INT,M_OPT_RANGE, 0, 2, NULL },
  { "generic-dev", ST_OFF(generic_dev), CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "sector-size", ST_OFF(sector_size), CONF_TYPE_INT, M_OPT_RANGE,1,100, NULL },
  { "overlap", ST_OFF(search_overlap), CONF_TYPE_INT, M_OPT_RANGE,0,75, NULL },
  { "toc-bias", ST_OFF(toc_bias), CONF_TYPE_INT, 0, 0, 0, NULL },
  { "toc-offset", ST_OFF(toc_offset), CONF_TYPE_INT, 0, 0, 0, NULL },
  { "noskip", ST_OFF(no_skip), CONF_TYPE_FLAG, 0 , 0, 1, NULL },
  { "skip", ST_OFF(no_skip), CONF_TYPE_FLAG, 0 , 1, 0, NULL },
  { "device", ST_OFF(device), CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "span",  ST_OFF(span), CONF_TYPE_OBJ_PARAMS, 0, 0, 0, &m_span_params_def },
  /// For url parsing
  { "hostname", ST_OFF(span), CONF_TYPE_OBJ_PARAMS, 0, 0, 0, &m_span_params_def },
  { "port", ST_OFF(speed), CONF_TYPE_INT, M_OPT_RANGE,1,100, NULL },
  { "filename", ST_OFF(device), CONF_TYPE_STRING, 0, 0, 0, NULL },
  {NULL, NULL, 0, 0, 0, 0, NULL}
};
static const struct m_struct_st stream_opts = {
  "cdda",
  sizeof(struct cdda_params),
  &cdda_dflts,
  cdda_params_fields
};

/// We keep these options but now they set the defaults
const m_option_t cdda_opts[] = {
  { "speed", &cdda_dflts.speed, CONF_TYPE_INT, M_OPT_RANGE,1,100, NULL },
  { "paranoia", &cdda_dflts.paranoia_mode, CONF_TYPE_INT,M_OPT_RANGE, 0, 2, NULL },
  { "generic-dev", &cdda_dflts.generic_dev, CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "sector-size", &cdda_dflts.sector_size, CONF_TYPE_INT, M_OPT_RANGE,1,100, NULL },
  { "overlap", &cdda_dflts.search_overlap, CONF_TYPE_INT, M_OPT_RANGE,0,75, NULL },
  { "toc-bias", &cdda_dflts.toc_bias, CONF_TYPE_INT, 0, 0, 0, NULL },
  { "toc-offset", &cdda_dflts.toc_offset, CONF_TYPE_INT, 0, 0, 0, NULL },
  { "noskip", &cdda_dflts.no_skip, CONF_TYPE_FLAG, 0 , 0, 1, NULL },
  { "skip", &cdda_dflts.no_skip, CONF_TYPE_FLAG, 0 , 1, 0, NULL },
  { "device", &cdda_dflts.device, CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "span", &cdda_dflts.span, CONF_TYPE_OBJ_PARAMS, 0, 0, 0, &m_span_params_def },
  {NULL, NULL, 0, 0, 0, 0, NULL}
};

int cdd_identify(const char *dev);
int cddb_resolve(const char *dev, char **xmcd_file);
cd_info_t* cddb_parse_xmcd(char *xmcd_file);

static int seek(stream_t* s,off_t pos);
static int fill_buffer(stream_t* s, char* buffer, int max_len);
static void close_cdda(stream_t* s);

static int get_track_by_sector(cdda_priv *p, unsigned int sector) {
  int i;
  for (i = p->cd->tracks; i >= 0 ; --i)
    if (p->cd->disc_toc[i].dwStartSector <= sector)
      break;
  return i;
}

static int control(stream_t *stream, int cmd, void *arg) {
  cdda_priv* p = stream->priv;
  switch(cmd) {
    case STREAM_CTRL_GET_NUM_CHAPTERS:
    {
      int start_track = get_track_by_sector(p, p->start_sector);
      int end_track = get_track_by_sector(p, p->end_sector);
      *(unsigned int *)arg = end_track + 1 - start_track;
      return STREAM_OK;
    }
    case STREAM_CTRL_SEEK_TO_CHAPTER:
    {
      int r;
      unsigned int track = *(unsigned int *)arg;
      int start_track = get_track_by_sector(p, p->start_sector);
      int seek_sector;
      track += start_track;
      if (track >= p->cd->tracks) {
        stream->eof = 1;
        return STREAM_ERROR;
      }
      seek_sector = track <= 0 ? p->start_sector
                               : p->cd->disc_toc[track].dwStartSector;
      r = seek(stream, seek_sector * CD_FRAMESIZE_RAW);
      if (r)
        return STREAM_OK;
      break;
    }
    case STREAM_CTRL_GET_CURRENT_CHAPTER:
    {
      int start_track = get_track_by_sector(p, p->start_sector);
      int cur_track = get_track_by_sector(p, p->sector);
      *(unsigned int *)arg = cur_track - start_track;
      return STREAM_OK;
    }
  }
  return STREAM_UNSUPPORTED;
}

static int open_cdda(stream_t *st,int m, void* opts, int* file_format) {
  struct cdda_params* p = (struct cdda_params*)opts;
  int mode = p->paranoia_mode;
  int offset = p->toc_offset;
#ifndef CONFIG_LIBCDIO
  cdrom_drive* cdd = NULL;
#else
  cdrom_drive_t* cdd = NULL;
#endif
  cdda_priv* priv;
  cd_info_t *cd_info,*cddb_info = NULL;
  unsigned int audiolen=0;
  int last_track;
  int i;
  char *xmcd_file = NULL;

  if(m != STREAM_READ) {
    m_struct_free(&stream_opts,opts);
    return STREAM_UNSUPPORTED;
  }

  if(!p->device) {
    if (cdrom_device)
      p->device = strdup(cdrom_device);
    else
      p->device = strdup(DEFAULT_CDROM_DEVICE);
  }

#ifdef CONFIG_CDDB
  // cdd_identify returns -1 if it cannot read the TOC,
  // in which case there is no point in calling cddb_resolve
  if(cdd_identify(p->device) >= 0 && strncmp(st->url,"cddb",4) == 0) {
    i = cddb_resolve(p->device, &xmcd_file);
    if(i == 0) {
      cddb_info = cddb_parse_xmcd(xmcd_file);
      free(xmcd_file);
    }
  }
#endif
  
#ifndef CONFIG_LIBCDIO
  if(p->generic_dev)
    cdd = cdda_identify_scsi(p->generic_dev,p->device,0,NULL);
  else
#endif
#if defined(__NetBSD__)
    cdd = cdda_identify_scsi(p->device,p->device,0,NULL);
#else
    cdd = cdda_identify(p->device,0,NULL);
#endif

  if(!cdd) {
    mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_MPDEMUX_CDDA_CantOpenCDDADevice);
    m_struct_free(&stream_opts,opts);
    free(cddb_info);
    return STREAM_ERROR;
  }

  cdda_verbose_set(cdd, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);

  if(p->sector_size) {
    cdd->nsectors = p->sector_size;
#ifndef CONFIG_LIBCDIO
    cdd->bigbuff = p->sector_size * CD_FRAMESIZE_RAW;
#endif
  }

  if(cdda_open(cdd) != 0) {
    mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_MPDEMUX_CDDA_CantOpenDisc);
    cdda_close(cdd);
    m_struct_free(&stream_opts,opts);
    free(cddb_info);
    return STREAM_ERROR;
  }

  cd_info = cd_info_new();
  mp_msg(MSGT_OPEN,MSGL_INFO,MSGTR_MPDEMUX_CDDA_AudioCDFoundWithNTracks,cdda_tracks(cdd));
  for(i=0;i<cdd->tracks;i++) {
	  char track_name[80];
	  long sec=cdda_track_firstsector(cdd,i+1);
	  long off=cdda_track_lastsector(cdd,i+1)-sec+1;

	  sprintf(track_name, "Track %d", i+1);
	  cd_info_add_track(cd_info, track_name, i+1, (unsigned int)(off/(60*75)), (unsigned int)((off/75)%60), (unsigned int)(off%75), sec, off );
	  audiolen += off;
  }
  cd_info->min  = (unsigned int)(audiolen/(60*75));
  cd_info->sec  = (unsigned int)((audiolen/75)%60);
  cd_info->msec = (unsigned int)(audiolen%75);

  priv = malloc(sizeof(cdda_priv));
  memset(priv, 0, sizeof(cdda_priv));
  priv->cd = cdd;
  priv->cd_info = cd_info;

  if(p->toc_bias)
    offset -= cdda_track_firstsector(cdd,1);

  if(offset) {
    int i;
    for(i = 0 ; i < cdd->tracks + 1 ; i++)
      cdd->disc_toc[i].dwStartSector += offset;
  }

  if(p->speed)
    cdda_speed_set(cdd,p->speed);

  last_track = cdda_tracks(cdd);
  if (p->span.start > last_track) p->span.start = last_track;
  if (p->span.end < p->span.start) p->span.end = p->span.start;
  if (p->span.end > last_track) p->span.end = last_track;
  if(p->span.start)
    priv->start_sector = cdda_track_firstsector(cdd,p->span.start);
  else
    priv->start_sector = cdda_disc_firstsector(cdd);

  if(p->span.end) {
    priv->end_sector = cdda_track_lastsector(cdd,p->span.end);
  } else
    priv->end_sector = cdda_disc_lastsector(cdd);

  priv->cdp = paranoia_init(cdd);
  if(priv->cdp == NULL) {
    cdda_close(cdd);
    free(priv);
    cd_info_free(cd_info);
    m_struct_free(&stream_opts,opts);
    free(cddb_info);
    return STREAM_ERROR;
  }

  if(mode == 0)
    mode = PARANOIA_MODE_DISABLE;
  else if(mode == 1)
    mode = PARANOIA_MODE_OVERLAP;
  else
    mode = PARANOIA_MODE_FULL;
  
  if(p->no_skip)
    mode |= PARANOIA_MODE_NEVERSKIP;
#ifndef CONFIG_LIBCDIO
  paranoia_modeset(cdd, mode);

  if(p->search_overlap >= 0)
    paranoia_overlapset(cdd,p->search_overlap);
#else
  paranoia_modeset(priv->cdp, mode);

  if(p->search_overlap >= 0)
    paranoia_overlapset(priv->cdp,p->search_overlap);
#endif

  paranoia_seek(priv->cdp,priv->start_sector,SEEK_SET);
  priv->sector = priv->start_sector;

#ifdef CONFIG_CDDB
  if(cddb_info) {
    cd_info_free(cd_info);
    priv->cd_info = cddb_info;
    cd_info_debug( cddb_info );
  }
#endif

  st->priv = priv;
  st->start_pos = priv->start_sector*CD_FRAMESIZE_RAW;
  st->end_pos = (priv->end_sector + 1) * CD_FRAMESIZE_RAW;
  st->type = STREAMTYPE_CDDA;
  st->sector_size = CD_FRAMESIZE_RAW;

  st->fill_buffer = fill_buffer;
  st->seek = seek;
  st->control = control;
  st->close = close_cdda;

  *file_format = DEMUXER_TYPE_RAWAUDIO;

  m_struct_free(&stream_opts,opts);

  return STREAM_OK;
}

#ifndef CONFIG_LIBCDIO
static void cdparanoia_callback(long inpos, int function) {
#else
static void cdparanoia_callback(long int inpos, paranoia_cb_mode_t function) {
#endif
}

static int fill_buffer(stream_t* s, char* buffer, int max_len) {
  cdda_priv* p = (cdda_priv*)s->priv;
  cd_track_t *cd_track;
  int16_t * buf;
  int i;
  
  if((p->sector < p->start_sector) || (p->sector > p->end_sector)) {
    s->eof = 1;
    return 0;
  }

  buf = paranoia_read(p->cdp,cdparanoia_callback);
  if (!buf)
    return 0;

#ifdef WORDS_BIGENDIAN 
  for(i=0;i<CD_FRAMESIZE_RAW/2;i++)
          buf[i]=le2me_16(buf[i]);
#endif

  p->sector++;
  memcpy(buffer,buf,CD_FRAMESIZE_RAW);

  for(i=0;i<p->cd->tracks;i++){
	  if(p->cd->disc_toc[i].dwStartSector==p->sector-1) {
		  cd_track = cd_info_get_track(p->cd_info, i+1);
//printf("Track %d, sector=%d\n", i, p->sector-1);
		  if( cd_track!=NULL ) {
			mp_msg(MSGT_SEEK, MSGL_INFO, "\n%s\n", cd_track->name); 
			mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CDDA_TRACK=%d\n", cd_track->track_nb);
		  }
		  break;
	  }
  }

  
  return CD_FRAMESIZE_RAW;
}

static int seek(stream_t* s,off_t newpos) {
  cdda_priv* p = (cdda_priv*)s->priv;
  cd_track_t *cd_track;
  int sec;
  int current_track=0, seeked_track=0;
  int seek_to_track = 0;
  int i;
  
  s->pos = newpos;
  sec = s->pos/CD_FRAMESIZE_RAW;
  if (s->pos < 0 || sec > p->end_sector) {
    s->eof = 1;
    return 0;
  }

//printf("pos: %d, sec: %d ## %d\n", (int)s->pos, (int)sec, CD_FRAMESIZE_RAW);
//printf("sector: %d  new: %d\n", p->sector, sec );
 
  for(i=0;i<p->cd->tracks;i++){
//        printf("trk #%d: %d .. %d\n",i,p->cd->disc_toc[i].dwStartSector,p->cd->disc_toc[i+1].dwStartSector);
	if( p->sector>=p->cd->disc_toc[i].dwStartSector && p->sector<p->cd->disc_toc[i+1].dwStartSector ) {
		current_track = i;
	}
	if( sec>=p->cd->disc_toc[i].dwStartSector && sec<p->cd->disc_toc[i+1].dwStartSector ) {
		seeked_track = i;
		seek_to_track = sec == p->cd->disc_toc[i].dwStartSector;
	}
  }
//printf("current: %d, seeked: %d\n", current_track, seeked_track);
	if (current_track != seeked_track && !seek_to_track) {
//printf("Track %d, sector=%d\n", seeked_track, sec);
		  cd_track = cd_info_get_track(p->cd_info, seeked_track+1);
		  if( cd_track!=NULL ) {
			  mp_msg(MSGT_SEEK, MSGL_INFO, "\n%s\n", cd_track->name);
			  mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CDDA_TRACK=%d\n", cd_track->track_nb);
		  }

	}
#if 0
  if(sec < p->start_sector)
    sec = p->start_sector;
  else if(sec > p->end_sector)
    sec = p->end_sector;
#endif

  p->sector = sec;
//  s->pos = sec*CD_FRAMESIZE_RAW;

//printf("seek: %d, sec: %d\n", (int)s->pos, sec);
  paranoia_seek(p->cdp,sec,SEEK_SET);
  return 1;
}

static void close_cdda(stream_t* s) {
  cdda_priv* p = (cdda_priv*)s->priv;
  paranoia_free(p->cdp);
  cdda_close(p->cd);
  cd_info_free(p->cd_info);
  free(p);
}

const stream_info_t stream_info_cdda = {
  "CDDA",
  "cdda",
  "Albeu",
  "",
  open_cdda,
  { "cdda",
#ifdef CONFIG_CDDB
    "cddb",
#endif
    NULL },
  &stream_opts,
  1 // Urls are an option string
};
