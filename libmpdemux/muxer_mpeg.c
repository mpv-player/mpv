
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "aviheader.h"
#include "ms_hdr.h"

#include "stream/stream.h"
#include "muxer.h"
#include "demuxer.h"
#include "stheader.h"
#include "m_option.h"
#include "mpeg_hdr.h"
#include "mp3_hdr.h"
#include "liba52/a52.h"

#define PACK_HEADER_START_CODE 0x01ba
#define SYSTEM_HEADER_START_CODE 0x01bb
#define PSM_START_CODE 0x01bc

#define PES_PRIVATE1 0x01bd
#define PES_PRIVATE2 0x01bf

#define MUX_MPEG1 1
#define MUX_MPEG2 2

#define VIDEO_MPEG1 0x10000001
#define VIDEO_MPEG2 0x10000002
#define AUDIO_MP2 0x50
#define AUDIO_MP3 0x55
#define AUDIO_A52 0x2000
#define AUDIO_LPCM 0x10001	/* only a placeholder at the moment */
#define AUDIO_AAC1 0x706D
//#define AUDIO_AAC2 (short) mmioFOURCC('m','p','4','a')
#define AUDIO_AAC2 0x504D

#define ASPECT_1_1 1
#define ASPECT_4_3 2
#define ASPECT_16_9 3
#define ASPECT_2_21_1 4

#define FRAMERATE_23976 1
#define FRAMERATE_24 2
#define FRAMERATE_25 3
#define FRAMERATE_2997 4
#define FRAMERATE_30 5
#define FRAMERATE_50 6
#define FRAMERATE_5994 7
#define FRAMERATE_60 8

static char ftypes[] = {'?', 'I', 'P', 'B'}; 
#define FTYPE(x) (ftypes[(x)])

static const char *framerates[] = {
	"unchanged", "23.976", "24", "25", "29.97", "30", "50", "59.94", "60"
};

static const char *aspect_ratios[] = {
	"unchanged", "1/1", "4/3", "16/9", "2.21/1"
};

static char *conf_mux = "mpeg2";
static uint16_t conf_packet_size = 0;		//dvd
static uint32_t conf_muxrate = 0;		//kb/s
static float conf_vaspect = 0; 
static float conf_vframerate = 0;
static uint32_t conf_vwidth = 0, conf_vheight = 0, conf_panscan_width = 0, conf_panscan_height = 0;
static uint32_t conf_vbitrate = 0;
static int conf_init_vpts = 200, conf_init_apts = 200;
static int conf_ts_allframes = 0;
static int conf_init_adelay = 0, conf_init_vdelay = 0;
static int conf_abuf_size = 0;
static int conf_vbuf_size = 0;
static int conf_drop = 0;
static int conf_telecine = 0;
static int conf_interleaving2 = 0;
static float conf_telecine_src = 0;
static float conf_telecine_dest = 0;

enum FRAME_TYPE {
	I_FRAME = 1,
	P_FRAME = 2,
	B_FRAME = 3
};

typedef struct {
	uint8_t *buffer;
	size_t size;
	size_t alloc_size;
	uint8_t type;
	uint64_t pts, dts, idur;
	uint32_t pos;		//start offset for the frame
} mpeg_frame_t;

typedef struct {
	uint8_t cnt;		// how many entries we use
	struct {
		uint8_t id, type;
		uint32_t bufsize;
		uint32_t format;
	} streams[50];		//16 video + 16 audio mpa + 16 audio private + bd/bf for dvd
} sys_info_t;

typedef struct {
	uint8_t cnt;		// how many entries we use
	struct {
		uint8_t id;
		uint8_t type;
		uint32_t format;
	} streams[50];		//16 video + 16 audio mpa + 16 audio private + bd/bf for dvd
} psm_info_t;

typedef struct {
	int size;
	uint64_t dts;
} buffer_track_t;

typedef struct {
	uint64_t dts, pts;
	uint64_t frame_dts, frame_pts;
	int len, stflen;
} pack_stats_t;

typedef struct {
	int mux;
	sys_info_t sys_info;
	psm_info_t psm_info;
	uint16_t packet_size;
	int is_dvd, is_xvcd, is_xsvcd, is_genmpeg1, is_genmpeg2, rawpes, ts_allframes, has_video, has_audio;
	int update_system_header, use_psm;
	off_t headers_size, data_size;
	uint64_t scr;
	uint64_t delta_scr;
	uint64_t last_psm_scr;
	uint32_t muxrate;
	uint8_t *buff;
	uint32_t headers_cnt;
	double init_adelay;
	int drop;
	
	//video patching parameters
	uint8_t vaspect, vframerate;
	uint16_t vwidth, vheight, panscan_width, panscan_height;
	uint32_t vbitrate;
	int patch_seq, patch_sde;
	int psm_streams_cnt;

//2 million frames are enough
#define MAX_PATTERN_LENGTH 2000000
	uint8_t bff_mask[MAX_PATTERN_LENGTH];
} muxer_priv_t;


typedef struct {
	int has_pts, has_dts, pes_is_aligned, type, min_pes_hlen;
	int delay_rff;
	uint64_t pts, last_pts, last_dts, dts, size, frame_duration, delta_pts, nom_delta_pts, last_saved_pts;
	uint32_t buffer_size;
	double delta_clock, timer;
	int drop_delayed_frames;
	mpeg_frame_t *framebuf;
	uint16_t framebuf_cnt;
	uint16_t framebuf_used;
	int32_t last_tr;
	int max_tr;
	uint8_t id, is_mpeg12, telecine;
	uint64_t vframes;
	int64_t display_frame;
	mp_mpeg_header_t picture;
	int max_buffer_size;
	buffer_track_t *buffer_track;
	int track_pos, track_len, track_bufsize;	//pos and len control the array, bufsize is the size of the buffer
	unsigned char *pack;
	int pack_offset, pes_offset, pes_set, payload_offset;
	int frames;
	int last_frame_rest; 	//the rest of the previous frame
	int is_ready;
	int mpa_layer;
} muxer_headers_t;

#define PULLDOWN32 1
#define TELECINE_FILM2PAL 2
#define TELECINE_DGPULLDOWN 3

m_option_t mpegopts_conf[] = {
	{"format", &(conf_mux), CONF_TYPE_STRING, M_OPT_GLOBAL, 0 ,0, NULL},
	{"size", &(conf_packet_size), CONF_TYPE_INT, M_OPT_GLOBAL|M_OPT_RANGE, 0, 65535, NULL},
	{"muxrate", &(conf_muxrate), CONF_TYPE_INT, M_OPT_GLOBAL|M_OPT_RANGE, 0, 12000000, NULL},	//12 Mb/s
	{"vaspect", &(conf_vaspect), CONF_TYPE_FLOAT, M_OPT_GLOBAL, 0, 0, NULL},
	{"vframerate", &(conf_vframerate), CONF_TYPE_FLOAT, M_OPT_GLOBAL, 0, 0, NULL},
	{"vwidth", &(conf_vwidth), CONF_TYPE_INT, M_OPT_GLOBAL|M_OPT_RANGE, 1, 4095, NULL},
	{"vheight", &(conf_vheight), CONF_TYPE_INT, M_OPT_GLOBAL|M_OPT_RANGE, 1, 4095, NULL},
	{"vpswidth", &(conf_panscan_width), CONF_TYPE_INT, M_OPT_GLOBAL|M_OPT_RANGE, 1, 16383, NULL},
	{"vpsheight", &(conf_panscan_height), CONF_TYPE_INT, M_OPT_GLOBAL|M_OPT_RANGE, 1, 16383, NULL},
	{"vbitrate", &(conf_vbitrate), CONF_TYPE_INT, M_OPT_GLOBAL|M_OPT_RANGE, 1, 104857599, NULL},
	{"vdelay", &conf_init_vdelay, CONF_TYPE_INT, M_OPT_GLOBAL|M_OPT_RANGE, 0, 32760, NULL},
	{"adelay", &conf_init_adelay, CONF_TYPE_INT, M_OPT_GLOBAL|M_OPT_RANGE, 0, 32760, NULL},
	{"vbuf_size", &conf_vbuf_size, CONF_TYPE_INT, M_OPT_GLOBAL|M_OPT_RANGE, 40, 1194, NULL},
	{"abuf_size", &conf_abuf_size, CONF_TYPE_INT, M_OPT_GLOBAL|M_OPT_RANGE, 4, 64, NULL},
	{"drop", &conf_drop, CONF_TYPE_FLAG, M_OPT_GLOBAL, 0, 1, NULL},
	{"tsaf", &conf_ts_allframes, CONF_TYPE_FLAG, M_OPT_GLOBAL, 0, 1, NULL},
	{"telecine", &conf_telecine, CONF_TYPE_FLAG, M_OPT_GLOBAL, 0, PULLDOWN32, NULL},
	{"interleaving2", &conf_interleaving2, CONF_TYPE_FLAG, M_OPT_GLOBAL, 0, 1, NULL},
	{"film2pal", &conf_telecine, CONF_TYPE_FLAG, M_OPT_GLOBAL, 0, TELECINE_FILM2PAL, NULL},
	{"tele_src", &(conf_telecine_src), CONF_TYPE_FLOAT, M_OPT_GLOBAL, 0, 0, NULL},
	{"tele_dest", &(conf_telecine_dest), CONF_TYPE_FLOAT, M_OPT_GLOBAL, 0, 0, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};

static void fix_audio_sys_header(muxer_priv_t *priv, uint8_t id, uint8_t newid, uint32_t size)
{
	uint8_t i;
	
	for(i = 0; i < priv->sys_info.cnt; i++)
	{
		if(priv->sys_info.streams[i].id == id)
		{
			priv->sys_info.streams[i].id = newid;
			priv->sys_info.streams[i].type = 1;
			priv->sys_info.streams[i].bufsize = size;
		}
	}
}

static inline int is_mpeg1(uint32_t x)
{
	return
		(x == 0x10000001) ||
		(x == mmioFOURCC('m','p','g','1')) ||
		(x == mmioFOURCC('M','P','G','1'));
}

static inline int is_mpeg2(uint32_t x)
{
	return
		(x == 0x10000002) ||
		(x == mmioFOURCC('m','p','g','2')) ||
		(x == mmioFOURCC('M','P','G','2')) ||
		(x == mmioFOURCC('m','p','e','g')) ||
		(x == mmioFOURCC('M','P','E','G'));
}

static inline int is_mpeg4(uint32_t x)
{
	return
		(x == 0x10000004) ||
		(x == mmioFOURCC('d','i','v','x')) ||
		(x == mmioFOURCC('D','I','V','X')) ||
		(x == mmioFOURCC('x','v','i','d')) ||
		(x == mmioFOURCC('X','V','I','D')) ||
		(x == mmioFOURCC('X','v','i','D')) ||
		(x == mmioFOURCC('x','v','i','x')) ||
		(x == mmioFOURCC('X','V','I','X')) ||
		(x == mmioFOURCC('m','p','4','v')) ||
		(x == mmioFOURCC('M','P','4','V')) ||
		(x == mmioFOURCC('F', 'M','P','4')) ||
		(x == mmioFOURCC('f', 'm','p','4')) ||
		(x == mmioFOURCC('D', 'X','5','0')) ||
		(x == mmioFOURCC('d', 'x','5','0'));
}

//from unrarlib.c
static uint32_t CalcCRC32(uint8_t *buff, uint32_t size)
{
	uint32_t i, j, CRCTab[256], crc;
	
	for(i = 0;i < 256; i++)
	{
		for(crc = i, j = 0; j < 8; j++)
			crc= (crc & 1) ? (crc >> 1)^0xEDB88320L : (crc >> 1);
		CRCTab[i] = crc;
	}

	
	crc = 0xffffffff;
	for(i = 0; i < size; i++)
		crc = (crc << 8) ^ CRCTab[((crc >> 24) ^ buff[i]) & 0xff];
	
	return crc;
}


static void add_to_psm(muxer_priv_t *priv, uint8_t id, uint32_t format)
{
	uint8_t i;
	
	i = priv->psm_info.cnt;
	priv->psm_info.streams[i].id = id;
	priv->psm_info.streams[i].format = format;
	
	if(is_mpeg1(format))
		priv->psm_info.streams[i].type = 0x01;
	else if(is_mpeg2(format))
		priv->psm_info.streams[i].type = 0x02;
	else if(is_mpeg4(format))
		priv->psm_info.streams[i].type = 0x10;
	else if(format == AUDIO_MP2 || format == AUDIO_MP3)
		priv->psm_info.streams[i].type = 0x03;
	else if(format == AUDIO_AAC1 || format == AUDIO_AAC2)
		priv->psm_info.streams[i].type = 0x0f;
	else
		priv->psm_info.streams[i].type = 0x81;
	
	if(format == AUDIO_A52)
		memcpy((char*) &(priv->psm_info.streams[i].format), "AC-3", 4);
	
	priv->psm_info.cnt++;
}


static mpeg_frame_t *init_frames(uint16_t num, size_t size)
{
	mpeg_frame_t *tmp;
	uint16_t i;
	
	tmp = (mpeg_frame_t *) calloc(num, sizeof(mpeg_frame_t));
	if(tmp == NULL)
		return NULL;
		
	for(i=0; i < num; i++)
	{
		tmp[i].buffer = (uint8_t *) calloc(1, size);
		if(tmp[i].buffer == NULL)
			return NULL;
		tmp[i].size = 0;
		tmp[i].pos = 0;
		tmp[i].alloc_size = size;
		tmp[i].pts = 0;
	}
	
	return tmp;
}

static int add_frame(muxer_headers_t *spriv, uint64_t idur, uint8_t *ptr, int len, uint8_t pt, uint64_t dts, uint64_t pts);

static muxer_stream_t* mpegfile_new_stream(muxer_t *muxer,int type){
  muxer_priv_t *priv = (muxer_priv_t*) muxer->priv;
  muxer_stream_t *s;
  muxer_headers_t *spriv;

  if (!muxer) return NULL;
  if(muxer->avih.dwStreams>=MUXER_MAX_STREAMS){
    mp_msg(MSGT_MUXER, MSGL_ERR, "Too many streams! increase MUXER_MAX_STREAMS !\n");
    return NULL;
  }
  switch (type) {
    case MUXER_TYPE_VIDEO:
      if (muxer->num_videos >= 16) {
	mp_msg(MSGT_MUXER, MSGL_ERR, "MPEG files can't contain more than 16 video streams!\n");
	return NULL;
      }
      break;
    case MUXER_TYPE_AUDIO:
      if (muxer->num_audios >= 16) {
	mp_msg(MSGT_MUXER, MSGL_ERR, "MPEG files can't contain more than 16 audio streams!\n");
	return NULL;
      }
      break;
    default:
      mp_msg(MSGT_MUXER, MSGL_ERR, "Unknown stream type!\n");
      return NULL;
  }
  s = (muxer_stream_t*) calloc(1, sizeof(muxer_stream_t));
  if(!s) return NULL; // no mem!?
  if (!(s->b_buffer = malloc(priv->packet_size)))
    goto init_fail;
  s->b_buffer_size = priv->packet_size;
  s->b_buffer_ptr = 0;
  s->b_buffer_len = 0;
  s->priv = (muxer_headers_t*) calloc(1, sizeof(muxer_headers_t));
  if(s->priv == NULL)
    goto init_fail;
  spriv = (muxer_headers_t *) s->priv;
  spriv->pack = malloc(priv->packet_size);
  if(! spriv->pack)
    goto init_fail;
  spriv->buffer_track = calloc(1, 4096*sizeof(buffer_track_t));
  if(!spriv->buffer_track)
    goto init_fail;
  spriv->track_pos = 0;
  spriv->track_len = 4096;
  muxer->streams[muxer->avih.dwStreams]=s;
  s->type=type;
  s->id=muxer->avih.dwStreams;
  s->muxer=muxer;

  if (type == MUXER_TYPE_VIDEO) {
    spriv->type = 1;
    spriv->last_pts = conf_init_vpts * 90 * 300;
    if(conf_init_vdelay) {
      spriv->last_dts += conf_init_vdelay * 90 * 300;
      spriv->last_pts += conf_init_vdelay * 90 * 300;
    }
    spriv->id = 0xe0 + muxer->num_videos;
    s->ckid = be2me_32 (0x100 + spriv->id);
    if(priv->is_genmpeg1 || priv->is_genmpeg2) {
      int v = (conf_vbuf_size ? conf_vbuf_size*1024 :
        (s->h.dwSuggestedBufferSize ? s->h.dwSuggestedBufferSize : 46*1024));
      int n = priv->sys_info.cnt;

      priv->sys_info.streams[n].id = spriv->id;
      priv->sys_info.streams[n].type = 1;
      priv->sys_info.streams[n].bufsize = v;
      priv->sys_info.cnt++;
    }
    muxer->num_videos++;
    priv->has_video++;
    s->h.fccType=streamtypeVIDEO;
    if(!muxer->def_v) muxer->def_v=s;
    spriv->framebuf_cnt = 30;
    spriv->framebuf_used = 0;
    spriv->framebuf = init_frames(spriv->framebuf_cnt, (size_t) 5000);
    if(spriv->framebuf == NULL) {
      mp_msg(MSGT_MUXER, MSGL_FATAL, "Couldn't allocate initial frames structure, abort!\n");
      goto init_fail;
    }
    memset(&(spriv->picture), 0, sizeof(spriv->picture));
    if(priv->is_xvcd)
      spriv->min_pes_hlen = 18;
    else if(priv->is_xsvcd)
      spriv->min_pes_hlen = 22;
    spriv->telecine = conf_telecine;
    mp_msg (MSGT_MUXER, MSGL_DBG2, "Added video stream %d, ckid=%X\n", muxer->num_videos, s->ckid);
  } else { // MUXER_TYPE_AUDIO
    spriv->type = 0;
    spriv->drop_delayed_frames = conf_drop;
    spriv->last_pts = conf_init_apts * 90 * 300;
    if(conf_init_adelay && ! spriv->drop_delayed_frames)
      spriv->last_pts += conf_init_adelay * 90 * 300;
    spriv->pts = spriv->last_pts;
    spriv->id = 0xc0 + muxer->num_audios;
    s->ckid = be2me_32 (0x100 + spriv->id);
    if(priv->is_genmpeg1 || priv->is_genmpeg2) {
      int a1 = (conf_abuf_size ? conf_abuf_size*1024 :
        (s->h.dwSuggestedBufferSize ? s->h.dwSuggestedBufferSize : 4*1024));
      int n = priv->sys_info.cnt;

      priv->sys_info.streams[n].id = spriv->id;
      priv->sys_info.streams[n].type = 0;
      priv->sys_info.streams[n].bufsize = a1;
      priv->sys_info.cnt++;
    }
    if(priv->is_xvcd)
      spriv->min_pes_hlen = 13;
    else if(priv->is_xsvcd)
      spriv->min_pes_hlen = 17;

    muxer->num_audios++;
    priv->has_audio++;
    s->h.fccType=streamtypeAUDIO;

    spriv->framebuf_cnt = 30;
    spriv->framebuf_used = 0;
    spriv->framebuf = init_frames(spriv->framebuf_cnt, (size_t) 2048);
    if(spriv->framebuf == NULL) {
      mp_msg(MSGT_MUXER, MSGL_FATAL, "Couldn't allocate initial frames structure, abort!\n");
      goto init_fail;
    }

    mp_msg (MSGT_MUXER, MSGL_DBG2, "Added audio stream %d, ckid=%X\n", s->id - muxer->num_videos + 1, s->ckid);
  }
  muxer->avih.dwStreams++;
  return s;

init_fail:
  if(s)
  {
    if(s->priv)
    {
      spriv = s->priv;
      if(spriv->pack)
        free(spriv->pack);
      if(spriv->buffer_track)
        free(spriv->buffer_track);
      free(s->priv);
    }
    if(s->b_buffer)
      free(s->b_buffer);
    free(s);
  }
  return NULL;
}

static void write_mpeg_ts(unsigned char *b, uint64_t ts, uint8_t mod) {
  ts /= 300;
  b[0] = mod | ((ts >> 29) & 0xf) | 1;
  b[1] = (ts >> 22) & 0xff;
  b[2] = ((ts >> 14) & 0xff) | 1;
  b[3] = (ts >> 7) & 0xff;
  b[4] = ((ts << 1) & 0xff) | 1;
}


static void write_mpeg_rate(int type, unsigned char *b, unsigned int rate) 
{
	rate = ((rate*8)+399) / 400;
	
	if(type == MUX_MPEG1)
	{
		b[0] = ((rate >> 15) & 0x7f) | 0x80;
		b[1] = (rate >> 7) & 0xff;
		b[2] = ((rate << 1) & 0xff) | 1;
	}
	else
	{
		b[0] = (rate >> 14);
		b[1] = (rate >> 6) & 0xff;
		b[2] = ((rate & 0x3f) << 2) | 0x03;	
	}
}


static void write_mpeg_std(unsigned char *b, unsigned int size, unsigned int type, uint8_t mod) 
{
	//type = 0:mpeg audio/128, 1:video and pes private streams (including ac3/dts/lpcm)/1024
	if(type == 0)	//audio
		size = (size + 127) / 128;
	else		//video or other
		size = ((size + 1023) / 1024);

	if(! size)
		size++;

	b[0] = ((size >> 8) & 0x3f) | (type==1 ? 0x60 : 0x40) | mod;
	b[1] = size & 0xff;
}

static void write_mpeg2_scr(unsigned char *b, uint64_t ts) 
{
	uint16_t t1, t2, t3, scr_ext;
	scr_ext = ts % 300ULL;
	ts /= 300ULL;
	ts &= 0x1FFFFFFFFULL;	//33 bits
	t1 = (ts >> 30) & 0x7;;
	t2 = (ts >> 15) & 0x7fff;
	t3 = ts & 0x7fff;
	
	b[0] = (t1 << 3 ) | 0x44 | ((t2 >> 13) & 0x3);
	b[1] = (t2 >> 5);
	b[2] = (t2 & 0x1f) << 3 | 0x4 | ((t3 >> 13) & 0x3);
	b[3] = (t3 >> 5);
	b[4] = (t3 & 0x1f) << 3 | ((scr_ext >> 7) & 0x03) | 0x4;
	b[5] = ((scr_ext << 1) & 0xFF) | 1;
}


static int write_mpeg_pack_header(muxer_t *muxer, char *buff)
{
	int len;
	muxer_priv_t *priv;
	
	priv = (muxer_priv_t *) muxer->priv;
	*(uint32_t *)buff = be2me_32(PACK_HEADER_START_CODE);
	if(priv->mux==MUX_MPEG1)
	{
		write_mpeg_ts(&buff[4], priv->scr, 0x20); // 0010 and SCR
		write_mpeg_rate(priv->mux, &buff[9], muxer->sysrate);
		len = 12;
	}
	else
	{
		write_mpeg2_scr(&buff[4], priv->scr); // 0010 and SCR
		write_mpeg_rate(priv->mux, &buff[10], muxer->sysrate);
		buff[13] = 0xf8; //5 bits reserved + 3 set to 0 to indicate 0 stuffing bytes 
		len = 14;
	}

	return len;
}


static int write_mpeg_system_header(muxer_t *muxer, char *buff)
{
	int len;
	uint8_t i;
	muxer_priv_t *priv;
	priv = (muxer_priv_t *) muxer->priv;
	
	len = 0;
	*(uint32_t *)(&buff[len]) = be2me_32(SYSTEM_HEADER_START_CODE);
	len += 4;
	*(uint16_t *)(&buff[len]) = 0; 	//fake length, we'll fix it later
	len += 2;
	write_mpeg_rate(MUX_MPEG1, &buff[len], muxer->sysrate);
	len += 3;
		
	buff[len++] = 0x4 | (priv->is_xvcd ? 1 : 0); 	//1 audio stream bound, no fixed, CSPS only for xvcd
	//stolen from libavformat
	if(priv->is_xvcd || priv->is_dvd)
		buff[len++] = 0xe1;	//system_audio_lock, system_video_lock, marker, 1 video stream bound
	else
		buff[len++] = 0x21;	//marker, 1 video stream bound
	
	buff[len++] = ((priv->mux == MUX_MPEG1) ? 0xff : 0x7f);	//in mpeg2 there's the packet rate restriction
	
	for(i = 0; i < priv->sys_info.cnt; i++)
	{
		buff[len++] = priv->sys_info.streams[i].id;
		write_mpeg_std(&buff[len], priv->sys_info.streams[i].bufsize, priv->sys_info.streams[i].type, 
			(priv->sys_info.streams[i].type == 1 ? 0xe0: 0xc0));
		len += 2;
	}
	
	*(uint16_t *)(&buff[4]) = be2me_16(len - 6);	// length field fixed
	
	return len;
}

static int write_mpeg_psm(muxer_t *muxer, char *buff)
{
	int len;
	uint8_t i;
	uint16_t dlen;
	muxer_priv_t *priv;
	priv = (muxer_priv_t *) muxer->priv;
	
	len = 0;
	*(uint32_t *)(&buff[len]) = be2me_32(PSM_START_CODE);
	len += 4;
	*(uint16_t *)(&buff[len]) = 0; 	//fake length, we'll fix it later
	len += 2;
	buff[len++] = 0xe0;		//1 current, 2 bits reserved, 5 version 0
	buff[len++] = 0xff;		//7 reserved, 1 marker
	buff[len] = buff[len+1] = 0;	//length  of the program descriptors (unused)
	len += 2;
	*(uint16_t *)(&buff[len]) = 0; //length of the es descriptors
	len += 2;
	
	dlen = 0;
	for(i = 0; i < priv->psm_info.cnt; i++)
	{
		if(
			(priv->psm_info.streams[i].id == 0xbd) || 
			(priv->psm_info.streams[i].id >= 0xe0 && priv->psm_info.streams[i].id <= 0xef) || 
			(priv->psm_info.streams[i].id >= 0xc0 && priv->psm_info.streams[i].id <= 0xcf)
		)
		{
			buff[len++] = priv->psm_info.streams[i].type;
			buff[len++] = priv->psm_info.streams[i].id;
			buff[len++] = 0;	//len of descriptor upper ...
			buff[len++] = 0;	//... lower
			
			dlen += 4;
		}
	}
	*(uint16_t *)(&buff[10]) = be2me_16(dlen);	//length of the es descriptors
	
	*(uint16_t *)(&buff[4]) = be2me_16(len - 6 + 4);	// length field fixed, including size of CRC32
	
	*(uint32_t *)(&buff[len]) = be2me_32(CalcCRC32(buff, len));
	
	len += 4;	//for crc
	
	return len;
}

static int psm_is_late(muxer_priv_t *priv)
{
	return !priv->data_size || (priv->scr >= priv->last_psm_scr + 27000000ULL);
}

static int write_mpeg_pes_header(muxer_headers_t *h, uint8_t *pes_id, uint8_t *buff, uint16_t plen, int stuffing_len, int mux_type)
{
	int len;
	
	len = 0;
	memcpy(&buff[len], pes_id, 4);
	len += 4;

	buff[len] = buff[len+1] = 0;	//fake len
	len += 2;

	if(mux_type == MUX_MPEG1)
	{
		if(stuffing_len > 0)
		{
			memset(&buff[len], 0xff, stuffing_len);
			len += stuffing_len;
		}
		
		if(h->buffer_size > 0)
		{
			write_mpeg_std(&buff[len], h->buffer_size, h->type, 0x40); // 01 is pes1 format	
			len += 2;
		}
	}
	else	//MPEG2
	{
		buff[len] = (h->pes_is_aligned ? 0x84 : 0x80);	//0x10... 
		len++;
		buff[len] = ((h->buffer_size > 0) ?  1 : 0) | (h->pts ? (h->dts ? 0xC0 : 0x80) : 0);	//pes extension + pts/dts flags
		len++;
		buff[len] = (h->pts ? (h->dts ? 10 : 5) : 0) + ((h->buffer_size > 0) ?  3 : 0) + stuffing_len;//pts + std + stuffing
		len++;
	}
	
	
	if(h->pts)
	{
		write_mpeg_ts(&buff[len], h->pts, (h->dts ? 0x30 : 0x20)); // 001x and both PTS/DTS
		len += 5;
		
		if(h->dts)
		{
			write_mpeg_ts(&buff[len], h->dts, 0x10); // 0001 before DTS
			len += 5;
		}
	}
	else
	{
		if(mux_type == MUX_MPEG1)
		{
			buff[len] = 0x0f;
			len += 1;
		}
	}

	
	if(mux_type == MUX_MPEG2)
	{
		if(h->buffer_size > 0)
		{
			buff[len] = 0x1e;	//std flag
			len++;
			
			write_mpeg_std(&buff[len], h->buffer_size, h->type, 0x40);
			len += 2;
		}
		
		if(stuffing_len > 0)
		{
			memset(&buff[len], 0xff, stuffing_len);
			len += stuffing_len;
		}
	}

	*((uint16_t*) &buff[4]) = be2me_16(len + plen - 6);	//fix pes packet size	
	return len;
}


static void write_pes_padding(uint8_t *buff, uint16_t len)
{
	//6 header bytes + len-6 0xff chars
	buff[0] = buff[1] = 0;
	buff[2] = 1;
	buff[3] = 0xbe;
	*((uint16_t*) &buff[4]) = be2me_16(len - 6);
	memset(&buff[6], 0xff, len - 6);
}


static int write_nav_pack(uint8_t *buff)
{
	// concatenation of pes_private2 + 03d4 x 0 and pes_private2 + 03fa x 0
        int len;

	mp_msg(MSGT_MUXER, MSGL_DBG3, "NAV\n");
	len = 0;
	*(uint32_t *)(&buff[len]) = be2me_32(PES_PRIVATE2);
	len += 4;
	buff[len++] = 0x3;
	buff[len++] = 0xd4;
	memset(&buff[len], 0, 0x03d4);
        len += 0x03d4;

	*(uint32_t *)(&buff[len]) = be2me_32(PES_PRIVATE2);
	len += 4;
	buff[len++] = 0x3;
	buff[len++] = 0xfa;
	memset(&buff[len], 0, 0x03fa);
        len += 0x03fa;

	return len;
}

static unsigned int calc_psm_len(muxer_priv_t *priv)
{
	return 16 + 4*(priv->psm_info.cnt);
}

static uint32_t calc_pes_hlen(int format, muxer_headers_t *h, muxer_priv_t *priv)
{
	uint32_t len;
	
	if(format == MUX_MPEG1)
		len = 6;
	else
		len = 9;
	
	if(h->pts)
	{
		len += 5;
		if(h->dts)
			len += 5;
	}
	else if(format == MUX_MPEG1)
		len += 1;
	
	if(h->buffer_size > 0)
	{
		if(format == MUX_MPEG2)
			len += 3;
		else
			len += 2;
	}

	//len = max(h->min_pes_hlen, len);
	
	return len;
}

	
static int write_mpeg_pack(muxer_t *muxer, muxer_stream_t *s, stream_t *stream, int isoend)
{
	size_t tot, offset;
	muxer_priv_t *priv;
	unsigned char *buff;
	int stuffing_len;

	priv = (muxer_priv_t *) muxer->priv;
	buff = priv->buff;

	if(isoend)
	{
		offset = priv->packet_size - 4;
		write_pes_padding(buff, offset);
		buff[offset + 0] = buff[offset + 1] = 0;
		buff[offset + 2] = 1;
		buff[offset + 3] = 0xb9;
		
		stream_write_buffer(stream, buff, priv->packet_size);
		return 1;
	}
	else	//FAKE DVD NAV PACK
	{
		offset = write_mpeg_pack_header(muxer, buff);
		offset += write_mpeg_system_header(muxer, &buff[offset]);
		
		//priv->update_system_header = 0;
		
		if(priv->is_dvd)
			offset += write_nav_pack(&buff[offset]);
			
		stuffing_len = priv->packet_size - offset;
		if(stuffing_len > 0)
		{
			//insert a PES padding packet
			write_pes_padding(&buff[offset], stuffing_len);
			offset += stuffing_len;
		}
			
		stream_write_buffer(stream, buff, offset);
		priv->headers_size += offset;
		tot = offset;
		muxer->movi_end += tot;
		
		return tot;
	}
}

static void update_demux_bufsize(muxer_headers_t *spriv, uint64_t dts, int framelen, int type)
{
	int dim = (spriv->track_len+16)*sizeof(buffer_track_t);

	if(spriv->track_pos+1 >= spriv->track_len)
	{
		buffer_track_t *tmp = realloc(spriv->buffer_track, dim);
		if(!tmp)
		{
			mp_msg(MSGT_MUXER, MSGL_ERR, "\r\nERROR, couldn't realloc %d bytes for tracking buffer\r\n", dim);
			return;
		}
		spriv->buffer_track = tmp;
		memset(&(spriv->buffer_track[spriv->track_pos+1]), 0, 16*sizeof(buffer_track_t));
		spriv->track_len += 16;
	}

	spriv->buffer_track[spriv->track_pos].size = framelen;
	spriv->buffer_track[spriv->track_pos].dts = dts;	//must be dts

	spriv->track_pos++;
}

static void fix_a52_headers(muxer_stream_t *s)
{
	muxer_headers_t *spriv = s->priv;
	int x = spriv->payload_offset;

	spriv->pack[x+0] = 0x80;
	spriv->pack[x+1] = spriv->frames;
	if(spriv->frames)
	{
		spriv->pack[x+2] = ((spriv->last_frame_rest+1) >> 8) & 0xff;	//256 * 0 ...
		spriv->pack[x+3] = (spriv->last_frame_rest+1) & 0xff;		// + 1 byte(s) to skip
	}
	else
		spriv->pack[x+2] = spriv->pack[x+3] = 0;
}

static inline void remove_frames(muxer_headers_t *spriv, int n)
{
	mpeg_frame_t tmp;
	int i;

	for(i = n; i < spriv->framebuf_used; i++)
	{
		tmp = spriv->framebuf[i - n];
		spriv->framebuf[i - n] = spriv->framebuf[i];
		spriv->framebuf[i] = tmp;
	}
	spriv->framebuf_used -= n;
}

static int calc_packet_len(muxer_stream_t *s, int psize, int finalize)
{
	muxer_headers_t *spriv = s->priv;
	int n, len, frpos, m;

	n = len = 0;
	frpos = spriv->framebuf[0].pos;
	while(len < psize && n < spriv->framebuf_used)
	{
		if(!frpos && len>0 && s->type == MUXER_TYPE_VIDEO && spriv->framebuf[n].type==I_FRAME)
			return len;
		m = FFMIN(spriv->framebuf[n].size - frpos, psize - len);
		len += m;
		frpos += m;
		if(frpos == spriv->framebuf[n].size)
		{
			frpos = 0;
			n++;
		}
	}

	if(len < psize && !finalize)
		return 0;
	return len;
}

static int find_packet_timestamps(muxer_priv_t *priv, muxer_stream_t *s, unsigned int start, uint64_t *dts, uint64_t *pts)
{
	muxer_headers_t *spriv = s->priv;
	int i, m, pes_hlen, ret, threshold;
	uint64_t spts, sdts, dpts;

	if(!spriv->framebuf_used)
		return 0;

	spts = spriv->pts;
	sdts = spriv->dts;
	spriv->dts = spriv->pts = 0;
	ret = 0;
	if(spriv->framebuf[0].pos == 0)	// start of frame
		i = 0;
	else
	{
		pes_hlen = calc_pes_hlen(priv->mux, spriv, priv);

		if(pes_hlen < spriv->min_pes_hlen) 
			pes_hlen = spriv->min_pes_hlen;

		m = spriv->framebuf[0].size - spriv->framebuf[0].pos;

		if(start + pes_hlen + m  >= priv->packet_size)	//spriv->pack_offset
			i = -1;	//this pack won't have a pts: no space available
		else
		{
			if(spriv->framebuf_used < 2)
				goto fail;
			
			if(spriv->framebuf[1].pts == spriv->framebuf[1].dts)
				threshold = 5;
			else
				threshold = 10;

			//headers+frame 0 < space available including timestamps	
			if(start + pes_hlen + m  < priv->packet_size - threshold)
				i = 1;
			else
				i = -1;
		}
	}

	if(i > -1)
	{
		dpts = FFMAX(spriv->last_saved_pts, spriv->framebuf[i].pts) - 
			FFMIN(spriv->last_saved_pts, spriv->framebuf[i].pts) +
			spriv->framebuf[0].idur;

		if(s->type != MUXER_TYPE_VIDEO)
			ret = 1;
		else if((spriv->framebuf[i].type == I_FRAME || priv->ts_allframes || dpts >= 36000*300))	//0.4 seconds
			ret = 1;

		if(ret)
		{
			*pts = spriv->framebuf[i].pts;
			*dts = spriv->framebuf[i].dts;
			if(*dts == *pts)
				*dts = 0;
		}
	}

fail:
	spriv->pts = spts;
	spriv->dts = sdts;
	return ret;
}

static int get_packet_stats(muxer_priv_t *priv, muxer_stream_t *s, pack_stats_t *p, int finalize)
{
	muxer_headers_t *spriv = s->priv;
	int len, len2, pack_hlen, pes_hlen, hlen, target, stflen, stuffing_len;
	uint64_t pts, dts;

	spriv->pts = spriv->dts = 0;
	p->dts = p->pts = p->frame_pts = p->frame_dts = 0;
	p->len = 0;

	if(priv->rawpes)
		pack_hlen = 0;
	else if(priv->mux == MUX_MPEG1)
		pack_hlen = 12;
	else
		pack_hlen = 14;
	if(priv->use_psm && psm_is_late(priv))
		pack_hlen += calc_psm_len(priv);

	if(find_packet_timestamps(priv, s, pack_hlen, &dts, &pts))
	{
		p->pts = p->frame_pts = pts;
		p->dts = p->frame_dts = dts;

		spriv->pts = pts;
		spriv->dts = dts;
	}
	pes_hlen = calc_pes_hlen(priv->mux, spriv, priv);

	p->stflen = stflen = (spriv->min_pes_hlen > pes_hlen ? spriv->min_pes_hlen - pes_hlen : 0);

	target = len = priv->packet_size - pack_hlen - pes_hlen - stflen;	//max space available
	if(s->type == MUXER_TYPE_AUDIO && s->wf->wFormatTag == AUDIO_A52)
		hlen = 4;
	else
		hlen = 0;

	len -= hlen;
	target -= hlen;
	
	len2 = calc_packet_len(s, target, finalize);
	if(!len2 || (len2 < target && s->type == MUXER_TYPE_AUDIO && !finalize))
	{
		//p->len = 0;
		//p->dts = p->pts = 0;
		spriv->pts = spriv->dts = 0;
		//fprintf(stderr, "\r\nLEN2: %d, target: %d, type: %d\r\n", len2, target, s->type);
		return 0;
	}

	len = len2;
	stuffing_len = 0;
	if(len < target)
	{
		if(s->type == MUXER_TYPE_VIDEO)
		{
			if(spriv->pts)
				target += 5;
			if(spriv->dts)
				target += 5;
			spriv->pts = spriv->dts = 0;
			p->pts = p->dts = 0;
		}

		stuffing_len = target - len;
		if(stuffing_len > 0 && stuffing_len < 7)
		{
			if(stflen + stuffing_len > 16)
			{
				int x = 7 - stuffing_len;
				stflen -= x;
				stuffing_len += x;
			}
			else
			{
				stflen += stuffing_len;
				stuffing_len = 0;
			}
		}
	}
	
	len += hlen;
	
	p->len = len;
	p->stflen = stflen;

	return p->len;
}

static int fill_packet(muxer_t *muxer, muxer_stream_t *s, int finalize)
{
	//try to fill a packet as much as possible
	//spriv->pack_offset is the start position initialized to 0
	//data is taken from spriv->framebuf
	//if audio and a52 insert the headers
	muxer_priv_t *priv = (muxer_priv_t *) muxer->priv;
	muxer_headers_t *spriv = (muxer_headers_t *) s->priv;
	int len, m, n, dvd_pack = 0;
	int write_psm = 0;
	mpeg_frame_t *frm;
	pack_stats_t p;

	spriv->dts = spriv->pts = 0;

	if(! spriv->framebuf_used)
	{
		spriv->pack_offset = 0;
		return 0;
	}

	if(!spriv->pack_offset)
	{
		if(priv->rawpes)
			spriv->pack_offset = 0;
		else
		spriv->pack_offset = write_mpeg_pack_header(muxer, spriv->pack);
		if(priv->update_system_header && (priv->is_genmpeg1 || priv->is_genmpeg2))
		{
			spriv->pack_offset += write_mpeg_system_header(muxer, &spriv->pack[spriv->pack_offset]);
			priv->update_system_header = 0;
		}

		if(priv->use_psm && psm_is_late(priv))
		{
			spriv->pack_offset += write_mpeg_psm(muxer, &spriv->pack[spriv->pack_offset]);
			write_psm = 1;
		}

		spriv->pes_set = 0;
		spriv->pes_offset = spriv->pack_offset;
		spriv->payload_offset = 0;
		spriv->frames = 0;
		spriv->last_frame_rest = 0;
	}

	if(!spriv->pes_set)
	{
		int bufsize = 0;
		//search the pts. yes if either it's video && (I-frame or priv->ts_allframes) && framebuf[i].pos == 0
		//or  it's audio && framebuf[i].pos == 0
		//NB pts and dts can only be relative to the first frame beginning in this pack
		if((priv->is_xsvcd || priv->is_xvcd || priv->rawpes) && spriv->size == 0)
		{
			if(s->type == MUXER_TYPE_VIDEO)
				bufsize = (conf_vbuf_size ? conf_vbuf_size : (priv->is_xvcd ? 46 : 232));
			else
				bufsize = (conf_abuf_size ? conf_abuf_size : 4);
			spriv->buffer_size = bufsize*1024;
		}

		if(priv->is_dvd && s->type == MUXER_TYPE_VIDEO 
			&& spriv->framebuf[0].type==I_FRAME && spriv->framebuf[0].pos==0)
			dvd_pack = 1;

		if(! get_packet_stats(priv, s, &p, finalize))
		{
			spriv->pack_offset = 0;
			return 0;
		}
		spriv->dts = p.dts;
		spriv->pts = p.pts;
		if(spriv->pts)
			spriv->last_saved_pts = p.pts;
		
		spriv->pack_offset += write_mpeg_pes_header(spriv, (uint8_t *) &s->ckid, &(spriv->pack[spriv->pack_offset]), 
			p.len, p.stflen, priv->mux);

		if(s->type == MUXER_TYPE_AUDIO && s->wf->wFormatTag == AUDIO_A52)
		{
			spriv->payload_offset = spriv->pack_offset;
			spriv->pack_offset += 4;	//for the 4 bytes of header
			if(!spriv->framebuf[0].pos)
				spriv->last_frame_rest = 0;
			else
				spriv->last_frame_rest = spriv->framebuf[0].size - spriv->framebuf[0].pos;
		}

		spriv->pes_set = 1;
	}


	if(spriv->dts || spriv->pts)
	{
		if((spriv->dts && priv->scr >= spriv->dts) || priv->scr >= spriv->pts)
			mp_msg(MSGT_MUXER, MSGL_ERR, "\r\nERROR: scr %.3lf, dts %.3lf, pts %.3lf\r\n", (double) priv->scr/27000000.0, (double) spriv->dts/27000000.0, (double) spriv->pts/27000000.0);
		else if(priv->scr + 63000*300 < spriv->dts)
			mp_msg(MSGT_MUXER, MSGL_INFO, "\r\nWARNING>: scr %.3lf, dts %.3lf, pts %.3lf, diff %.3lf, piff %.3lf\r\n", (double) priv->scr/27000000.0, (double) spriv->dts/27000000.0, (double) spriv->pts/27000000.0, (double)(spriv->dts - priv->scr)/27000000.0, (double)(spriv->pts - priv->scr)/27000000.0);
	}

	n = 0;
	len = 0;

	frm = spriv->framebuf;
	while(spriv->pack_offset < priv->packet_size && n < spriv->framebuf_used)
	{
		if(!frm->pos)
		{
			//since iframes must always be aligned at block boundaries exit when we find the 
			//beginning of one in the middle of the flush
			if(len > 0 && s->type == MUXER_TYPE_VIDEO && frm->type == I_FRAME)
			{
				break;
			}
			spriv->frames++;
			update_demux_bufsize(spriv, frm->dts, frm->size, s->type);
		}

		m = FFMIN(frm->size - frm->pos, priv->packet_size - spriv->pack_offset);
		memcpy(&(spriv->pack[spriv->pack_offset]), &(frm->buffer[frm->pos]), m);

		len += m;
		spriv->pack_offset += m;
		frm->pos += m;
		
		if(frm->pos == frm->size)	//end of frame
		{
			frm->pos = frm->size = 0;
			frm->pts = frm->dts = 0;
			n++;
			frm++;
		}
	}

	if((priv->is_xsvcd || priv->is_xvcd || priv->rawpes) && spriv->size == 0)
		spriv->buffer_size = 0;

	spriv->size += len;

	if(dvd_pack && (spriv->pack_offset == priv->packet_size))
		write_mpeg_pack(muxer, NULL, muxer->stream, 0);	//insert fake Nav Packet

	if(n > 0)
		remove_frames(spriv,  n);

	spriv->track_bufsize += len;
	if(spriv->track_bufsize > spriv->max_buffer_size)
		mp_msg(MSGT_MUXER, MSGL_ERR, "\r\nBUFFER OVERFLOW: %d > %d, pts: %"PRIu64"\r\n", spriv->track_bufsize, spriv->max_buffer_size, spriv->pts);

	if(s->type == MUXER_TYPE_AUDIO && s->wf->wFormatTag == AUDIO_A52)
		fix_a52_headers(s);
	
	if(spriv->pack_offset < priv->packet_size && !priv->rawpes)	//here finalize is set
	{
		int diff = priv->packet_size - spriv->pack_offset;
		write_pes_padding(&(spriv->pack[spriv->pack_offset]), diff);
		spriv->pack_offset += diff;
	}
	
	stream_write_buffer(muxer->stream, spriv->pack, spriv->pack_offset);

	priv->headers_size += spriv->pack_offset - len;
	priv->data_size += len;
	muxer->movi_end += spriv->pack_offset;

	spriv->pack_offset = 0;
	spriv->pes_set = 0;
	spriv->frames = 0;
	if(write_psm)
		priv->last_psm_scr = priv->scr;
	
	return len;
}

static inline int find_best_stream(muxer_t *muxer)
{
	int i, ndts;
	uint64_t dts = -1;
	muxer_priv_t *priv = muxer->priv;
	muxer_headers_t *spriv;
	pack_stats_t p;
	unsigned int perc, sperc;

	ndts = -1;
	perc = -1;
	
	//THIS RULE MUST ALWAYS apply: dts  <= SCR + 0.7 seconds
	for(i = 0; i < muxer->avih.dwStreams; i++)
	{
		spriv = muxer->streams[i]->priv;

		p.len = 0;
		get_packet_stats(priv, muxer->streams[i], &p, 0);
		
		if(spriv->track_bufsize + p.len > spriv->max_buffer_size)
			continue;
		if(p.frame_pts && p.frame_dts > priv->scr + 63000*300)
			continue;

		if(spriv->framebuf[0].dts <= dts)
		{
			dts = spriv->framebuf[0].dts;
			ndts = i;
		}

		if(conf_interleaving2)
		{
			sperc = (spriv->track_bufsize * 1024) / spriv->max_buffer_size;
			if(sperc < perc)
			{
				ndts = i;
				perc = sperc;
			}
		}
	}

	return ndts;
}

static void patch_seq(muxer_priv_t *priv, unsigned char *buf)
{
	if(priv->vwidth > 0)
	{
		buf[4] = (priv->vwidth >> 4) & 0xff;
		buf[5] &= 0x0f;
		buf[5] |= (priv->vwidth & 0x0f) << 4;
	}
	
	if(priv->vheight > 0)
	{
		buf[5] &= 0xf0;
		buf[5] |= (priv->vheight >> 8) & 0x0f;
		buf[6] = priv->vheight & 0xff;
	}
	
	if(priv->vaspect > 0)
		buf[7] = (buf[7] & 0x0f) | (priv->vaspect << 4);
	
	if(priv->vframerate > 0)
		buf[7] = (buf[7] & 0xf0) | priv->vframerate;
		
	if(priv->vbitrate > 0)
	{
		buf[8] = (priv->vbitrate >> 10);
		buf[9] = (priv->vbitrate >> 2);
		buf[10] = (buf[10] & 0x3f) | (unsigned char) ((priv->vbitrate & 0x4) << 2);
	}
}

static void patch_panscan(muxer_priv_t *priv, unsigned char *buf)
{	//patches sequence display extension (display_horizontal_size and display_vertical_size)
	//1: 
	int offset = 1;
	
	if(buf[0] & 0x01)
		offset += 3;
	
	if(priv->panscan_width > 0)
	{
		buf[offset] = (priv->panscan_width >> 6);
		buf[offset+1] = ((priv->panscan_width & 0x3F) << 2) | (buf[offset + 1] & 0x03);
	}
	
	offset++;
	
	if(priv->panscan_height > 0)
	{
		buf[offset] = (priv->panscan_height >> 13) << 7;
		buf[offset+1] = (priv->panscan_height >> 5) & 0xFF;
		buf[offset+2] = ((priv->panscan_height & 0x1F) << 3) | (buf[offset+2] & 0x07);
	}
}


static void update_scr(muxer_t *muxer)
{
	muxer_priv_t *priv = muxer->priv;
	muxer_stream_t *stream;
	muxer_headers_t *spriv;
	int i, j;
	uint64_t mindts = (uint64_t) -1;

	priv->scr += priv->delta_scr;

	for(i = 0; i < muxer->avih.dwStreams; i++)
	{
		stream = muxer->streams[i];
		spriv = stream->priv;
		if(spriv->framebuf_used && spriv->framebuf[0].dts < mindts)
			mindts = spriv->framebuf[0].dts;
	}

	mp_msg(MSGT_MUXER, MSGL_DBG2, "UPDATE SCR TO %"PRIu64" (%.3lf)\n", priv->scr, (double) (priv->scr/27000000.0));
	
	for(i = 0; i < muxer->avih.dwStreams; i++)
	{
		stream = muxer->streams[i];
		spriv = stream->priv;

		j = 0;
		while(j < spriv->track_pos && priv->scr >= spriv->buffer_track[j].dts)
		{
			spriv->track_bufsize -= spriv->buffer_track[j].size;
			j++;
		}
		if(spriv->track_bufsize < 0)
		{
			double d;
			muxer->sysrate = (muxer->sysrate * 11) / 10;	//raise by 10%
			d = (double) priv->packet_size / (double)muxer->sysrate;
			priv->delta_scr = (uint64_t) (d * 27000000.0f);
			mp_msg(MSGT_MUXER, MSGL_INFO, "\r\nBUFFER UNDEFLOW at stream %d, raising muxrate to %d kb/s, delta_scr: %"PRIu64"\r\n", i, muxer->sysrate/125, priv->delta_scr);
			spriv->track_bufsize = 0;
		}

		if(j > 0)
		{
			memmove(spriv->buffer_track, &(spriv->buffer_track[j]), (spriv->track_len - j) * sizeof(buffer_track_t));
			spriv->track_pos -= j;
			for(j = spriv->track_pos; j < spriv->track_len; j++)
				spriv->buffer_track[j].size = 0;
		}

		if(spriv->framebuf_used && spriv->framebuf[0].dts < mindts)
			mindts = spriv->framebuf[0].dts;
	}
}


static int calc_frames_to_flush(muxer_headers_t *vpriv)
{
	int n, found = 0;
	
	if(vpriv->framebuf_used > 0)
	{
		n = 0;
		//let's count how many frames we'll store in the next pack sequence
		mp_msg(MSGT_MUXER, MSGL_DBG2, "\n");
		while(n < vpriv->framebuf_used)
		{
			mp_msg(MSGT_MUXER, MSGL_DBG2, "CALC_FRAMES, n=%d, type=%c, pts=%.3lf\n", n, FTYPE(vpriv->framebuf[n].type), (double)vpriv->framebuf[n].pts/27000000.0f);
			if(n+1 < vpriv->framebuf_used)
				mp_msg(MSGT_MUXER, MSGL_DBG2, "n+1=%d, type=%c, pts=%.3lf\n", n+1, FTYPE(vpriv->framebuf[n+1].type), (double)vpriv->framebuf[n+1].pts/27000000.0f);
				
			if(vpriv->framebuf[n].type == I_FRAME)
			{
				if(n > 0)
				{
					found = 1;
					break;
				}
			}

			n++;
		}
	}
	
	if(found && (n < vpriv->framebuf_used+1))
		return n;
	else
		return 0;
}

static int flush_buffers(muxer_t *muxer, int finalize)
{
	int i, n, found;
	int skip_cnt;
	uint64_t init_delay = 0;
	muxer_stream_t *s, *vs, *as;
	muxer_headers_t *vpriv = NULL, *apriv = NULL;
	muxer_priv_t *priv = (muxer_priv_t *) muxer->priv;
	double duration;
	uint64_t iduration, iaduration;
	
	/* 
		analyzes all streams and decides what to flush
		trying to respect an interleaving distribution
		equal to the v_bitrate/a_bitrate proportion
	*/
	n = 0;
	vs = as = NULL;
	found = 0;
	for(i = 0; i < muxer->avih.dwStreams; i++)
	{
		s = muxer->streams[i];
		if(s->type == MUXER_TYPE_VIDEO)
		{
			vs = muxer->streams[i];
			vpriv = (muxer_headers_t*) vs->priv;
			if(!vpriv->is_ready)
				return 0;
			n = found = calc_frames_to_flush(vpriv);
		}
		else if(s->type == MUXER_TYPE_AUDIO)
			as = s;
	}
	
	if((! found) && finalize)
	{
		if(vpriv != NULL)
			found = n = vpriv->framebuf_used;
	}

	if(found)
	{
		mp_msg(MSGT_MUXER, MSGL_DBG2, "\nVIDEO, FLUSH %d frames (of %d), 0 to %d\n", n, vpriv->framebuf_used, n-1);

		vpriv = (muxer_headers_t*) vs->priv;
		
		duration = 0;
		iduration = 0;
		for(i = 0; i < n; i++)
			iduration += vpriv->framebuf[i].idur;
		duration = (double) (iduration / 27000000.0);
		
		if(as != NULL)
		{
			apriv = (muxer_headers_t*) as->priv;
			iaduration = 0;
			for(i = 0; i < apriv->framebuf_used; i++)
			{
				iaduration += apriv->framebuf[i].idur;
			}
			if(iaduration < iduration)
			{
				mp_msg(MSGT_MUXER, MSGL_DBG2, "Not enough audio data exit\n");
				return 0;
			}
		}
		
		if(as != NULL && (apriv->size == 0))
		{
			init_delay = vpriv->framebuf[0].pts - vpriv->framebuf[0].dts;
		
			for(i = 0; i < apriv->framebuf_cnt; i++)
			{
				apriv->framebuf[i].pts += init_delay;
				apriv->framebuf[i].dts += init_delay;
			}
			apriv->last_pts += init_delay;
			mp_msg(MSGT_MUXER, MSGL_DBG2, "\r\nINITIAL VIDEO DELAY: %.3lf, currAPTS: %.3lf\r\n", (double) init_delay/27000000.0f, (double) apriv->last_pts/27000000.0f);
		}
		
		if((priv->is_xvcd || priv->is_xsvcd) && (vpriv->size == 0))
			vpriv->buffer_size = (conf_vbuf_size ? conf_vbuf_size : (priv->is_xvcd ? 46 : 230))*1024;
			
		i = 0;
		skip_cnt = 0;
			
		while(1)
		{
			update_scr(muxer);
			i = find_best_stream(muxer);
			if(i < 0)
				continue;
			if(!fill_packet(muxer, muxer->streams[i], finalize))
				skip_cnt++;

			if(skip_cnt == muxer->avih.dwStreams)
			{
				found = 0;
				break;
			}
		}
	}
		
	muxer->file_end = priv->scr;
	return found;
}


static inline uint64_t parse_fps(float fps)
{
	// 90000 * 300 * 1001 / d , there's no rounding error with any of the admitted framerates
	int d = (int)(fps*1001+0.5);
	
	return 27027000000ULL / d;
}


static int soft_telecine(muxer_priv_t *priv, muxer_headers_t *vpriv, uint8_t *fps_ptr, uint8_t *se_ptr, uint8_t *pce_ptr, int n)
{
	if(! pce_ptr)
		return 0;
	if(fps_ptr != NULL)
	{
		*fps_ptr = (*fps_ptr & 0xf0) | priv->vframerate;
		vpriv->nom_delta_pts = parse_fps(conf_vframerate);
	}
	
	//in pce_ptr starting from bit 0 bit 24 is tff, bit 30 is rff, 
	if(pce_ptr[3] & 0x2)
	{
		mp_msg(MSGT_MUXER, MSGL_ERR, "\nERROR! RFF bit is already set, disabling telecining\n");
		vpriv->telecine = 0;
		return 0;
	}

	vpriv->picture.progressive_sequence = 0;
	vpriv->picture.progressive_frame = 1;
	if(se_ptr)
		se_ptr[1] &= 0xf7;
	
	//disable tff and rff and overwrite them with the value in bff_mask
	pce_ptr[3] = (pce_ptr[3] & 0x7d) | priv->bff_mask[vpriv->display_frame % MAX_PATTERN_LENGTH];
	pce_ptr[4] |= 0x80;	//sets progressive frame
	
	vpriv->display_frame += n;
	if(! vpriv->vframes)
		mp_msg(MSGT_MUXER, MSGL_INFO, "\nENABLED SOFT TELECINING, FPS=%.3f\n",conf_vframerate);
	
	return 1;
}

static size_t parse_mpeg12_video(muxer_stream_t *s, muxer_priv_t *priv, muxer_headers_t *spriv, float fps, size_t len)
{
	uint8_t *fps_ptr = NULL;	//pointer to the fps byte in the sequence header
	uint8_t *se_ptr = NULL;		//pointer to sequence extension
	uint8_t *pce_ptr = NULL;	//pointer to picture coding extension
	int frames_diff, d1, gop_reset = 0;	//how any frames we advanced respect to the last one
	int ret;
	int i, err;
	uint32_t temp_ref;
	int pt;
	
	mp_msg(MSGT_MUXER, MSGL_DBG2,"parse_mpeg12_video, len=%u\n", (uint32_t) len);
	if(s->buffer[0] != 0 || s->buffer[1] != 0 || s->buffer[2] != 1 || len<6) 
	{
		mp_msg(MSGT_MUXER, MSGL_ERR,"Unknown video format, possibly non-MPEG1/2 stream, len=%d!\n", len);
		return 0;
	}

	temp_ref = 0;
	pt = 0;
	err = 0;
	i = 0;
	while(i + 4 < len)
	{	// Video (0) Sequence header (b3) or GOP (b8)
		if((s->buffer[i] == 0) && (s->buffer[i+1] == 0) && (s->buffer[i+2] == 1))
		{
			switch(s->buffer[i+3])
			{
				case 0xb3: //sequence
				{
					if(i + 11 > len)
					{
						err=1;
						break;
					}
					fps_ptr = &(s->buffer[i+7]);
					mp_header_process_sequence_header(&(spriv->picture), &(s->buffer[i+4]));
					spriv->delta_pts = spriv->nom_delta_pts = parse_fps(spriv->picture.fps);
					
					spriv->delta_clock = (double) 1/fps;
					//the 2 lines below are needed to handle non-standard frame rates (such as 18)
					if(! spriv->delta_pts)
						spriv->delta_pts = spriv->nom_delta_pts = (uint64_t) ((double)27000000.0 * spriv->delta_clock );
					mp_msg(MSGT_MUXER, MSGL_DBG2, "\nFPS: %.3f, FRAMETIME: %.3lf\n", fps, (double)1/fps);
					if(priv->patch_seq)
						patch_seq(priv, &(s->buffer[i]));
				}
				break;

				case 0xb5:
					if(i + 9 > len)
					{
						err = 1;
						break;
					}
					mp_header_process_extension(&(spriv->picture), &(s->buffer[i+4]));
					if(((s->buffer[i+4] & 0xf0) == 0x10))
						se_ptr = &(s->buffer[i+4]);
					if(((s->buffer[i+4] & 0xf0) == 0x20))
					{
						if(priv->patch_sde)
							patch_panscan(priv, &(s->buffer[i+4]));
					}
					if((s->buffer[i+4] & 0xf0) == 0x80)
					{
						pce_ptr = &(s->buffer[i+4]);
					}
					break;
		
				case 0xb8:
					gop_reset = 1;
					break;
		
				case 0x00:
					if(i + 5 > len)
					{
						err = 1;
						break;
					}
					pt = (s->buffer[i+5] & 0x1c) >> 3;
					temp_ref = (s->buffer[i+4]<<2)+(s->buffer[i+5]>>6);
					break;
				}
			if(err) break;	//something went wrong
			if(s->buffer[i+3] >= 0x01 && s->buffer[i+3] <= 0xAF) break;	//slice, we have already analized what we need
		}
		i++;
	}
	if(err)
		mp_msg(MSGT_MUXER, MSGL_ERR,"Warning: picture too short or broken!\n");
	
	//following 2 lines are workaround: lavf doesn't sync to sequence headers before passing demux_packets
	if(!spriv->nom_delta_pts)	
		spriv->delta_pts = spriv->nom_delta_pts = parse_fps(fps);
	if(!spriv->vframes)
		spriv->last_tr = spriv->max_tr = temp_ref;
	d1 = temp_ref - spriv->last_tr;
	if(gop_reset)
		frames_diff = spriv->max_tr + 1 + temp_ref - spriv->last_tr;
	else
	{
		if(d1 < -6)	//there's a wraparound
			frames_diff = spriv->max_tr + 1 + temp_ref - spriv->last_tr;
		else if(d1 > 6)	//there's a wraparound
			frames_diff = spriv->max_tr + 1 + spriv->last_tr - temp_ref;
		else if(!d1)	//pre-emptive fix against broken sequences
			frames_diff = 1;
		else	
			frames_diff = d1;
	}
	mp_msg(MSGT_MUXER, MSGL_DBG2, "\nLAST: %d, TR: %d, GOP: %d, DIFF: %d, MAX: %d, d1: %d\n", 
	spriv->last_tr, temp_ref, gop_reset, frames_diff, spriv->max_tr, d1);

	if(temp_ref > spriv->max_tr || gop_reset)
		spriv->max_tr = temp_ref;
	
	spriv->last_tr = temp_ref;
	if(spriv->picture.mpeg1 == 0) 
	{
		if(spriv->telecine && pce_ptr)
		{
			soft_telecine(priv, spriv, fps_ptr, se_ptr, pce_ptr, frames_diff);
			spriv->picture.display_time = 100;
			mp_header_process_extension(&(spriv->picture), pce_ptr);
			if(spriv->picture.display_time >= 50 && spriv->picture.display_time <= 300) 
				spriv->delta_pts = (spriv->nom_delta_pts * spriv->picture.display_time) / 100;
		}
	}
	
	if(! spriv->vframes)
		frames_diff = 1;

	spriv->last_dts += spriv->delta_pts;
	spriv->last_pts += spriv->nom_delta_pts*(frames_diff-1) + spriv->delta_pts;

	ret = add_frame(spriv, spriv->delta_pts, s->buffer, len, pt, spriv->last_dts, spriv->last_pts);
	if(ret < 0)
	{
		mp_msg(MSGT_MUXER, MSGL_FATAL, "\r\nPARSE_MPEG12: add_frames(%d) failed, exit\r\n", len);
		return 0;
	}
	mp_msg(MSGT_MUXER, MSGL_DBG2, "\r\nVIDEO FRAME, PT: %C, tr: %d, diff: %d, dts: %.3lf, pts: %.3lf, pdt: %u, gop_reset: %d\r\n",
		ftypes[pt], temp_ref, frames_diff, ((double) spriv->last_dts/27000000.0f), 
		((double) spriv->last_pts/27000000.0f), spriv->picture.display_time, gop_reset);

	if(pt == B_FRAME)
	{
		int j, n, adj = 0;
		int64_t diff = spriv->last_dts - spriv->last_pts;
		
		if(diff != 0)
		{
			n = spriv->framebuf_used - 1;
			
			for(j = n; j >= 0; j--)
			{
				if(spriv->framebuf[j].pts >= spriv->last_pts)
				{
					spriv->framebuf[j].pts += diff;
					adj++;
				}
			}
			mp_msg(MSGT_MUXER, MSGL_V, "\r\nResynced B-frame by %d units, DIFF: %"PRId64" (%.3lf),[pd]ts=%.3lf\r\n",
				n, diff, (double) diff/27000000.0f, (double) spriv->last_pts/27000000.0f);
			spriv->last_pts = spriv->last_dts;
		}
	}
	spriv->vframes++;
	
	mp_msg(MSGT_MUXER, MSGL_DBG2,"parse_mpeg12_video, return %u\n", (uint32_t) len);
	return len;
}


static uint64_t fix_mp4_frame_duration(muxer_headers_t *vpriv)
{
	uint64_t mn, md, mx, diff;
	uint32_t i;

	mn = mx = vpriv->framebuf[0].pts;
	for(i = 0; i < 3; i++) 
	{
		mp_msg(MSGT_DECVIDEO,MSGL_DBG2, "PTS: %"PRIu64"\n", vpriv->framebuf[i].pts);
		if(vpriv->framebuf[i].pts < mn)
			mn = vpriv->framebuf[i].pts;
		if(vpriv->framebuf[i].pts > mx)
			mx = vpriv->framebuf[i].pts;
	}
	md = mn;
	for(i=0; i<3; i++) 
	{
		if((vpriv->framebuf[i].pts > mn) && (vpriv->framebuf[i].pts < mx))
		md = vpriv->framebuf[i].pts;
	}
	
	if(mx - md > md - mn)
		diff = md - mn;
	else
		diff = mx - md;

	mp_msg(MSGT_DECVIDEO,MSGL_DBG2, "MIN: %"PRIu64", mid: %"PRIu64", max: %"PRIu64", diff: %"PRIu64"\n", mn, md, mx, diff);
	if(diff > 0)
	{
		for(i=0; i<3; i++) 
		{
			vpriv->framebuf[i].pts += diff;
			vpriv->framebuf[i].dts += i * diff;
			mp_msg(MSGT_MUXER, MSGL_DBG2, "FIXED_PTS: %.3lf, FIXED_DTS: %.3lf\n", 
				(double) (vpriv->framebuf[i].pts/27000000.0), (double) (vpriv->framebuf[i].dts/27000000.0));
		}
		return diff;
	}
	else
		return 0;
}


static size_t parse_mpeg4_video(muxer_stream_t *s, muxer_priv_t *priv, muxer_headers_t *vpriv, float fps, size_t len)
{
	size_t ptr = 0;
	int64_t delta_pts=0;
	uint8_t pt;
	int ret;
	
	mp_msg(MSGT_MUXER, MSGL_DBG2,"parse_mpeg4_video, len=%u\n", (uint32_t) len);
	if(len<6) 
	{
		mp_msg(MSGT_MUXER, MSGL_ERR,"Frame too short: %d, exit!\n", len);
		return 0;
	}
	
	pt = 0;
	while(ptr < len - 5)
	{
		if(s->buffer[ptr] != 0 || s->buffer[ptr+1] != 0 || s->buffer[ptr+2] != 1)
		{
			ptr++;
			continue;
		}
		
		if(s->buffer[ptr+3] >= 0x20 && s->buffer[ptr+3] <= 0x2f) //VOL
		{
			mp4_header_process_vol(&(vpriv->picture), &(s->buffer[ptr+4]));
		}
		else if(s->buffer[ptr+3] == 0xb3)	//gov
		{
			//fprintf(stderr, "\nGOV\n");
		}
		else if(s->buffer[ptr+3] == 0xb6)	//vop
		{
			int32_t delta;
			mp4_header_process_vop(&(vpriv->picture), &(s->buffer[ptr+4]));
			
			delta = vpriv->picture.timeinc_unit - vpriv->last_tr;
			if((delta > 0) && (delta > (vpriv->picture.timeinc_resolution/2)))
				delta -= vpriv->picture.timeinc_resolution;
			else if((delta < 0) && (delta < (-(vpriv->picture.timeinc_resolution/2))))
				delta += vpriv->picture.timeinc_resolution;
			
			delta_pts = (27000000 * (int64_t) delta) / vpriv->picture.timeinc_resolution;
			//warning, it seems that packed bops can lead to delta == 0
			
			pt = vpriv->picture.picture_type + 1;
			mp_msg(MSGT_MUXER, MSGL_DBG2, "\nTYPE: %c, RESOLUTION: %d, TEMP: %d, delta: %d, delta_pts: %"PRId64" = %.3lf, delta2: %.3lf\n", 
				FTYPE(pt), vpriv->picture.timeinc_resolution, vpriv->picture.timeinc_unit, delta, delta_pts, (double) (delta_pts/27000000.0),
				(double) delta / (double) vpriv->picture.timeinc_resolution);
			
			vpriv->last_tr = vpriv->picture.timeinc_unit;	
			
			break;
		}
		
		ptr++;
	}
	
	if(vpriv->vframes)
	{
		vpriv->last_dts += vpriv->frame_duration;
		vpriv->last_pts += delta_pts;
	}
	
	ret = add_frame(vpriv, delta_pts, s->buffer, len, pt, vpriv->last_dts, vpriv->last_pts);
	if(ret < 0)
	{
		mp_msg(MSGT_MUXER, MSGL_FATAL, "\r\nPARSE_MPEG4: add_frames(%d) failed, exit\r\n", len);
		return 0;
	}
	
	if(!vpriv->frame_duration && vpriv->framebuf_used == 3)
	{
		vpriv->frame_duration = fix_mp4_frame_duration(vpriv);
		if(vpriv->frame_duration)
		{
			vpriv->last_pts += vpriv->frame_duration;
			vpriv->last_dts = vpriv->framebuf[vpriv->framebuf_used-1].dts;
			vpriv->delta_clock = ((double) vpriv->frame_duration)/27000000.0;
			mp_msg(MSGT_MUXER, MSGL_INFO, "FRAME DURATION: %"PRIu64"   %.3lf\n", 
				vpriv->frame_duration, (double) (vpriv->frame_duration/27000000.0));
			vpriv->is_ready = 1;
		}
	}
	
	mp_msg(MSGT_MUXER, MSGL_DBG2, "LAST_PTS: %.3lf, LAST_DTS: %.3lf\n", 
		(double) (vpriv->last_pts/27000000.0), (double) (vpriv->last_dts/27000000.0));

	vpriv->vframes++;
	
	return len;
}


static int fill_last_frame(muxer_headers_t *spriv,  uint8_t *ptr, int len)
{
	int idx;

	if(!len)
		return 0;
	
	if(spriv->framebuf_used == 0)
		idx = spriv->framebuf_used;
	else
		idx = spriv->framebuf_used - 1;

	if(spriv->framebuf[idx].alloc_size < spriv->framebuf[idx].size + len)
	{
		if(spriv->framebuf[idx].size > SIZE_MAX - (size_t)len)
			return 0;
		spriv->framebuf[idx].buffer = (uint8_t*) realloc(spriv->framebuf[idx].buffer, spriv->framebuf[idx].size + len);
		if(! spriv->framebuf[idx].buffer)
			return 0;
		spriv->framebuf[idx].alloc_size = spriv->framebuf[idx].size + len;
	}

	memcpy(&(spriv->framebuf[idx].buffer[spriv->framebuf[idx].size]), ptr, len);
	spriv->framebuf[idx].size += len;

	return len;
}

static int add_frame(muxer_headers_t *spriv, uint64_t idur, uint8_t *ptr, int len, uint8_t pt, uint64_t dts, uint64_t pts)
{
	int idx;

	idx = spriv->framebuf_used;
	if(idx >= spriv->framebuf_cnt)
	{
		spriv->framebuf = (mpeg_frame_t*) realloc_struct(spriv->framebuf, (spriv->framebuf_cnt+1), sizeof(mpeg_frame_t));
		if(spriv->framebuf == NULL)
		{
			mp_msg(MSGT_MUXER, MSGL_FATAL, "Couldn't realloc frame buffer(idx), abort\n");
			return -1;
		}
		
		spriv->framebuf[spriv->framebuf_cnt].size = 0;
		spriv->framebuf[spriv->framebuf_cnt].alloc_size = 0;
		spriv->framebuf[spriv->framebuf_cnt].pos = 0;
		
		spriv->framebuf[spriv->framebuf_cnt].buffer = (uint8_t*) malloc(len);
		if(spriv->framebuf[spriv->framebuf_cnt].buffer == NULL)
		{
			mp_msg(MSGT_MUXER, MSGL_FATAL, "Couldn't realloc frame buffer(frame), abort\n");
			return -1;
		}
		spriv->framebuf[spriv->framebuf_cnt].alloc_size = len;		
		spriv->framebuf_cnt++;
	}
	
	if(spriv->framebuf[idx].alloc_size < spriv->framebuf[idx].size + len)
	{
		if(spriv->framebuf[idx].size > SIZE_MAX - (size_t)len)
		{
			mp_msg(MSGT_MUXER, MSGL_FATAL, "Size overflow, couldn't realloc frame buffer(frame), abort\n");
			return -1;
		}
		spriv->framebuf[idx].buffer = realloc(spriv->framebuf[idx].buffer, spriv->framebuf[idx].size + len);
		if(spriv->framebuf[idx].buffer == NULL)
		{
			mp_msg(MSGT_MUXER, MSGL_FATAL, "Couldn't realloc frame buffer(frame), abort\n");
			return -1;
		}
		spriv->framebuf[idx].alloc_size = spriv->framebuf[idx].size + len;
	}
	
	memcpy(&(spriv->framebuf[idx].buffer[spriv->framebuf[idx].size]), ptr, len);
	spriv->framebuf[idx].size += len;
	spriv->framebuf[idx].pos = 0;
	spriv->framebuf[idx].type = pt;

	spriv->framebuf[idx].idur = idur;
	spriv->framebuf[idx].dts = dts;
	spriv->framebuf[idx].pts = pts;
	spriv->framebuf_used++;
	mp_msg(MSGT_MUXER, MSGL_DBG2, "\r\nAdded frame, size: %u, idur: %"PRIu64", dts: %"PRIu64", pts: %"PRIu64", used: %u\r\n", len, idur, dts, pts, spriv->framebuf_used);

	return idx;
}

static int analyze_mpa(muxer_stream_t *s)
{
	int i = 0, len, max, chans, srate, spf, layer;
	int score[4] = {0, 0, 0, 0};
	
	while(i < s->b_buffer_len + 3)
	{
		if(s->b_buffer[i] == 0xFF && ((s->b_buffer[i+1] & 0xE0) == 0xE0))
		{
			len = mp_get_mp3_header(&(s->b_buffer[i]), &chans, &srate, &spf, &layer, NULL);
			if(len > 0 && (srate == s->wf->nSamplesPerSec) && (i + len <= s->b_buffer_len))
			{
				score[layer]++;
				i += len;
			}
		}
		i++;
	}

	max = 0;
	layer = 2;
	for(i = 1; i <= 3; i++)
	{
		if(score[i] >= max)
		{
			max = score[i];
			layer = i;
		}
	}

	return layer;	//actual layer with the highest score
}

int aac_parse_frame(uint8_t *buf, int *srate, int *num);

static int parse_audio(muxer_stream_t *s, int finalize, unsigned int *nf, double *timer, double delay, int drop)
{
	int i, j, len, chans, srate, spf, layer, dummy, tot, num, frm_idx;
	int finished;
	unsigned int frames;
	uint64_t idur;
	double dur;
	muxer_headers_t *spriv = (muxer_headers_t *) s->priv;

	i = tot = frames = 0;
	finished = 0;
	while(1)
	{
		len = 0;
		switch(s->wf->wFormatTag)
		{
		case AUDIO_MP2:
		case AUDIO_MP3:
			{
				if(i + 3 >= s->b_buffer_len)
				{
					finished = 1;
					break;
				}
			
				if(s->b_buffer[i] == 0xFF && ((s->b_buffer[i+1] & 0xE0) == 0xE0))
				{
					len = mp_get_mp3_header(&(s->b_buffer[i]), &chans, &srate, &spf, &layer, NULL);
					if(len > 0 && (srate == s->wf->nSamplesPerSec) && (i + len <= s->b_buffer_len) 
						&& layer == spriv->mpa_layer)
					{
						dur = (double) spf / (double) srate;
						idur = (27000000ULL * spf) / srate;
					}
					else
						len = 0;
				}
			}
		break;

		case AUDIO_A52:
			{
				if(i + 6 >= s->b_buffer_len)
				{
					finished = 1;
					break;
				}
	
				if(s->b_buffer[i] == 0x0B && s->b_buffer[i+1] == 0x77)
				{
					srate = 0;
				#ifdef CONFIG_LIBA52
					len = a52_syncinfo(&(s->b_buffer[i]), &dummy, &srate, &dummy);
				#else
					len = mp_a52_framesize(&(s->b_buffer[i]), &srate);
				#endif
					if((len > 0) && (srate == s->wf->nSamplesPerSec) && (i + len <= s->b_buffer_len))
					{
						dur = (double) 1536 / (double) srate;
						idur = (27000000ULL * 1536) / srate;
					}
					else
						len = 0;
				}
			}
		break;

		case AUDIO_AAC1:
		case AUDIO_AAC2:
			{
				if(i + 7 >= s->b_buffer_len)
				{
					finished = 1;
					break;
				}
	
				if(s->b_buffer[i] == 0xFF && ((s->b_buffer[i+1] & 0xF6) == 0xF0))
				{
					len = aac_parse_frame(&(s->b_buffer[i]), &srate, &num);
					if((len > 0) && (srate == s->wf->nSamplesPerSec) && (i + len <= s->b_buffer_len))
					{
						dur = (double) 1024 / (double) srate;
						idur = (27000000ULL * 1024 * num) / srate;
					}
					else
						len = 0;
				}
			}
		}

		if(finished)
			break;

		if(!len)
		{
			i++;
			continue;
		}
		
		spriv->timer += dur;
		if(spriv->drop_delayed_frames && delay < 0 && spriv->timer <= -delay)
		{
			i += len;
			tot = i;
			continue;
		}

		frames++;
		fill_last_frame(spriv, &(s->b_buffer[tot]), i - tot);
		frm_idx = add_frame(spriv, idur, &(s->b_buffer[i]), len, 0, spriv->last_pts, spriv->last_pts);
		if(frm_idx < 0)
		{
			mp_msg(MSGT_MUXER, MSGL_FATAL, "Couldn't add audio frame buffer(frame), abort\n");
			goto audio_exit;
		}
		for(j = frm_idx; j < spriv->framebuf_cnt; j++)
			spriv->framebuf[j].pts = spriv->last_pts;
		spriv->last_pts += idur;

		i += len;
		tot = i;
	}

audio_exit:
	if(tot)
	{
		memmove(s->b_buffer, &(s->b_buffer[tot]), s->b_buffer_len - tot);
		s->b_buffer_len -= tot;
		s->b_buffer_ptr += tot;
		if(s->b_buffer_len > 0)
			memmove(s->b_buffer, &(s->b_buffer[s->b_buffer_ptr]), s->b_buffer_len);
		s->b_buffer_ptr = 0;
	}

	if(finalize)
	{
		frm_idx = add_frame(spriv, 0, s->b_buffer, s->b_buffer_len, 0, spriv->last_pts, spriv->last_pts);
		if(frm_idx >= 0)
		{
			for(j = frm_idx; j < spriv->framebuf_cnt; j++)
				spriv->framebuf[j].pts = spriv->last_pts;
		}
	}

	*nf = frames;
	*timer = spriv->timer;

	return tot;
}

static void fix_parameters(muxer_stream_t *stream)
{
	muxer_headers_t *spriv = stream->priv;
	muxer_t *muxer = stream->muxer;
	muxer_priv_t *priv = muxer->priv;
	uint32_t stream_format;
	int needs_psm = 0;

	if(stream->type == MUXER_TYPE_AUDIO)
	{
		stream_format = stream->wf->wFormatTag;
		spriv->is_ready = 1;
		if(conf_abuf_size)
			spriv->max_buffer_size = conf_abuf_size*1024;
		else
		spriv->max_buffer_size = 4*1024;
		if(stream->wf->wFormatTag == AUDIO_A52)
		{
			stream->ckid = be2me_32 (0x1bd);
			if(priv->is_genmpeg1 || priv->is_genmpeg2)
				fix_audio_sys_header(priv, spriv->id, 0xbd, FFMAX(conf_abuf_size, 58)*1024);	//only one audio at the moment
			spriv->id = 0xbd;
			if(!conf_abuf_size)
			spriv->max_buffer_size = 16*1024;
		}
		else if(stream->wf->wFormatTag == AUDIO_AAC1 || stream->wf->wFormatTag == AUDIO_AAC2)
			needs_psm = 1;
		else if(stream->wf->wFormatTag == AUDIO_MP2 || stream->wf->wFormatTag == AUDIO_MP3)
			spriv->is_ready = 0;
	}
	else	//video
	{
		stream_format = stream->bih->biCompression;
		if(conf_vbuf_size)
			spriv->max_buffer_size = conf_vbuf_size*1024;
		else
		{
			if(priv->is_dvd)
				spriv->max_buffer_size = 232*1024;
			else if(priv->is_xsvcd)
				spriv->max_buffer_size = 230*1024;
			else if(priv->is_xvcd)
				spriv->max_buffer_size = 46*1024;
			else
				spriv->max_buffer_size = 232*1024;	//no profile => unconstrained :) FIXME!!!
		}
		
		if(is_mpeg4(stream->bih->biCompression))
			spriv->is_ready = 0;
		else
			spriv->is_ready = 1;

		if(!is_mpeg1(stream_format) && !is_mpeg2(stream_format))
			needs_psm = 1;
	}
	
	if(priv->is_genmpeg2 && needs_psm)
	{
		priv->use_psm = 1;
		add_to_psm(priv, spriv->id, stream_format);
		priv->psm_streams_cnt++;
	}
}


static void mpegfile_write_chunk(muxer_stream_t *s,size_t len,unsigned int flags, double dts_arg, double pts_arg)
{
	size_t sz = 0;
	uint64_t tmp;
	muxer_t *muxer = s->muxer;
	muxer_priv_t *priv = (muxer_priv_t *)muxer->priv;
	muxer_headers_t *spriv = (muxer_headers_t*) s->priv;
	float fps;
	uint32_t stream_format, nf;

	if(s->buffer == NULL || len == -1)
		return;

	if (s->type == MUXER_TYPE_VIDEO)
	{ // try to recognize frame type...
		fps = (float) s->h.dwRate/ (float) s->h.dwScale;
		spriv->type = 1;
		stream_format = s->bih->biCompression;
		if(! spriv->vframes)
		{
			spriv->last_dts = spriv->last_pts - (uint64_t)(27000000.0f/fps);
			mp_msg(MSGT_MUXER, MSGL_INFO,"INITV: %.3lf, %.3lf, fps: %.3f\r\n", (double) spriv->last_pts/27000000.0f, (double) spriv->last_dts/27000000.0f, fps);
		}

		if(is_mpeg1(stream_format) || is_mpeg2(stream_format))
		{
			spriv->is_mpeg12 = 1;
			spriv->is_ready = 1;
			if(len)
				sz = parse_mpeg12_video(s, priv, spriv, fps, len);
			else
			{
				tmp = (uint64_t) (27000000.0f / fps);
				spriv->last_pts += tmp;
				spriv->last_dts += tmp;
			}
		}
		else if(is_mpeg4(stream_format)) 
		{
			spriv->is_mpeg12 = 0;
			spriv->telecine = 0;
			if(len)
				sz = parse_mpeg4_video(s, priv, spriv, fps, len);
			else
			{
				tmp = (uint64_t) (27000000.0f / fps);
				spriv->last_pts += tmp;
				spriv->last_dts += tmp;
			}
		}
		
		mp_msg(MSGT_MUXER, MSGL_DBG2,"mpegfile_write_chunk, Video codec=%x, len=%u, mpeg12 returned %u\n", stream_format, (uint32_t) len, (uint32_t) sz);
	}
	else
	{ // MUXER_TYPE_AUDIO
		double fake_timer;
		spriv->type = 0;
		stream_format = s->wf->wFormatTag;
		
		if(s->b_buffer_size - s->b_buffer_len < len)
		{
			void *tmp;

			if(s->b_buffer_len > SIZE_MAX - len)
			{
				mp_msg(MSGT_MUXER, MSGL_FATAL, "\nFATAL! couldn't realloc, integer overflow\n");
				return;
			}
			tmp = realloc(s->b_buffer, len  + s->b_buffer_len);
			if(!tmp)
			{
				mp_msg(MSGT_MUXER, MSGL_FATAL, "\nFATAL! couldn't realloc %d bytes\n", len  + s->b_buffer_len);
				return;
			}
			s->b_buffer = tmp;
			
			s->b_buffer_size = len  + s->b_buffer_len;
			mp_msg(MSGT_MUXER, MSGL_DBG2, "REALLOC(%d) bytes to AUDIO backbuffer\n", s->b_buffer_size);
		}
		memcpy(&(s->b_buffer[s->b_buffer_ptr + s->b_buffer_len]), s->buffer, len);
		s->b_buffer_len += len;
		
		if(!spriv->is_ready)
		{
			if(s->b_buffer_len >= 32*1024)
			{
				spriv->mpa_layer = analyze_mpa(s);
				spriv->is_ready = 1;
			}
		}
		else
		{
			parse_audio(s, 0, &nf, &fake_timer, priv->init_adelay, priv->drop);
			spriv->vframes += nf;
			if(! spriv->vframes)
				mp_msg(MSGT_MUXER, MSGL_INFO, "AINIT: %.3lf\r\n", (double) spriv->last_pts/27000000.0f);	
		}
	}

	flush_buffers(muxer, 0);
}


static void mpegfile_write_index(muxer_t *muxer)
{
	int i, nf;
	double fake_timer;
	muxer_priv_t *priv = (muxer_priv_t *) muxer->priv;

	mp_msg(MSGT_MUXER, MSGL_INFO, MSGTR_WritingTrailer);

	for(i = 0; i < muxer->avih.dwStreams; i++)
	{
		if(muxer->streams[i]->type == MUXER_TYPE_AUDIO)
			parse_audio(muxer->streams[i], 1, &nf, &fake_timer, priv->init_adelay, priv->drop);
	}
	while(flush_buffers(muxer, 0) > 0);
	flush_buffers(muxer, 1);
	if(priv->is_genmpeg1 || priv->is_genmpeg2)
	{
		priv->scr = 0;
		write_mpeg_pack(muxer, NULL, muxer->stream, 1);	//insert fake Nav Packet
	}
		
	mp_msg(MSGT_MUXER, MSGL_INFO, "\nOverhead: %.3lf%% (%"PRIu64" / %"PRIu64")\n", 100.0 * (double)priv->headers_size / (double)priv->data_size, priv->headers_size, priv->data_size);
}

static void mpegfile_write_header(muxer_t *muxer)
{
	muxer_priv_t *priv = (muxer_priv_t*) muxer->priv;
	
	mp_msg(MSGT_MUXER, MSGL_INFO, MSGTR_WritingHeader);
	
	priv->headers_cnt++;
	
	//write the first system header only for generic mpeg1/2 muxes, and only when we have collected all necessary infos
	if(priv->is_genmpeg1 || priv->is_genmpeg2 || ((priv->is_xvcd || priv->is_xsvcd) && (priv->headers_cnt == 1)))
	{
		write_mpeg_pack(muxer, NULL, muxer->stream, 0);
		priv->update_system_header = 0;
	}
	
	return;
}

static void setup_sys_params(muxer_priv_t *priv)
{
	if(priv->is_dvd)
	{
		int v = (conf_vbuf_size ? conf_vbuf_size : 232);
		int a1 = (conf_abuf_size ? conf_abuf_size : 4);
		int a2 = (conf_abuf_size>58 ? conf_abuf_size : 58);

		priv->sys_info.cnt = 4;
		
		priv->sys_info.streams[0].id = 0xb9;
		priv->sys_info.streams[0].type = 1;
		priv->sys_info.streams[0].bufsize = v*1024;
			
		priv->sys_info.streams[1].id = 0xb8;
		priv->sys_info.streams[1].type = 0;
		priv->sys_info.streams[1].bufsize = a1*1024;
		
		priv->sys_info.streams[2].id = 0xbd;
		priv->sys_info.streams[2].type = 1;
		priv->sys_info.streams[2].bufsize = a2*1024;
		
		priv->sys_info.streams[3].id = 0xbf;
		priv->sys_info.streams[3].type = 1;
		priv->sys_info.streams[3].bufsize = 2*1024;
	}
	else if(priv->is_xvcd || priv->is_xsvcd)
	{
		int v = (conf_vbuf_size ? conf_vbuf_size : (priv->is_xvcd ? 46: 230));
		int a1 = (conf_abuf_size ? conf_abuf_size : 4);

		priv->sys_info.cnt = 2;
		
		priv->sys_info.streams[0].id = 0xe0;
		priv->sys_info.streams[0].type = 1;
		priv->sys_info.streams[0].bufsize = v*1024;
			
		priv->sys_info.streams[1].id = 0xc0;
		priv->sys_info.streams[1].type = 0;
		priv->sys_info.streams[1].bufsize = a1*1024;
	}
	else
		priv->sys_info.cnt = 0;
}

/* excerpt from DGPulldown Copyright (C) 2005-2006, Donald Graft */
static void generate_flags(uint8_t *bff_mask, int source, int target)
{
	unsigned int i, trfp;
	uint64_t dfl,tfl;
	unsigned char ormask[4] = {0x0, 0x2, 0x80, 0x82};
	
	dfl = (target - source) << 1;
	tfl = source >> 1;
	
	trfp = 0;
	for(i = 0; i < MAX_PATTERN_LENGTH; i++)
	{
		tfl += dfl;
		if(tfl >= source)
		{
			tfl -= source;
			bff_mask[i] = ormask[trfp + 1];
			trfp ^= 2;
		}
		else
			bff_mask[i] = ormask[trfp];
	}
}

int muxer_init_muxer_mpeg(muxer_t *muxer)
{
	muxer_priv_t *priv;
	priv = (muxer_priv_t *) calloc(1, sizeof(muxer_priv_t));
	if(priv == NULL)
	return 0;
	priv->update_system_header = 1;
	
	//calloc() already zero-ed all flags, so we assign only the ones we need
	
	if(conf_mux != NULL)
	{
		if(! strcasecmp(conf_mux, "mpeg1"))
		{
			priv->mux = MUX_MPEG1;
			priv->packet_size = 2048;
			priv->is_genmpeg1 = 1;
			priv->muxrate = 1800 * 125;	//Constrained parameters
		}
		else if(! strcasecmp(conf_mux, "dvd"))
		{
			priv->mux = MUX_MPEG2;
			priv->is_dvd = 1;
			priv->packet_size = 2048;
			priv->muxrate = 10080 * 125;
		}
		else if(! strcasecmp(conf_mux, "xsvcd"))
		{
			priv->mux = MUX_MPEG2;
			priv->is_xsvcd = 1;
			priv->packet_size = 2324;
			priv->muxrate = 150*2324;
			priv->ts_allframes = 1;
		}
		else if(! strcasecmp(conf_mux, "xvcd"))
		{
			priv->mux = MUX_MPEG1;
			priv->is_xvcd = 1;
			priv->packet_size = 2324;
			priv->muxrate = 75*2352;
			priv->ts_allframes = 1;
		}
		else if(! strcasecmp(conf_mux, "pes1"))
		{
			priv->mux = MUX_MPEG1;
			priv->rawpes = 1;
			priv->packet_size = 2048;
			priv->muxrate = 10080 * 125;
			priv->ts_allframes = 1;
		}
		else if(! strcasecmp(conf_mux, "pes2"))
		{
			priv->mux = MUX_MPEG2;
			priv->rawpes = 1;
			priv->packet_size = 2048;
			priv->muxrate = 10080 * 125;
			priv->ts_allframes = 1;
		}
		else
		{
			if(strcasecmp(conf_mux, "mpeg2"))
				mp_msg(MSGT_MUXER, MSGL_ERR, "Unknown format %s, default to mpeg2\n", conf_mux);
			priv->mux = MUX_MPEG2;
			priv->is_genmpeg2 = 1;
			priv->packet_size = 2048;
			priv->muxrate = 1800 * 125;	//Constrained parameters
		}
	}
	
	if(conf_ts_allframes)
		priv->ts_allframes = 1;
	if(conf_muxrate > 0)
		priv->muxrate = conf_muxrate * 125;		// * 1000 / 8
	if(conf_packet_size)
		priv->packet_size = conf_packet_size;
	priv->delta_scr = (uint64_t) (90000.0f*300.0f*(double)priv->packet_size/(double)priv->muxrate);
	mp_msg(MSGT_MUXER, MSGL_INFO, "PACKET SIZE: %u bytes, deltascr: %"PRIu64"\n", priv->packet_size, priv->delta_scr);
	setup_sys_params(priv);
	
	if(conf_vaspect > 0)
	{
		int asp = (int) (conf_vaspect * 1000.0f);
		if(asp >= 1332 && asp <= 1334)
			priv->vaspect = ASPECT_4_3;
		else if(asp >= 1776 && asp <= 1778)
			priv->vaspect = ASPECT_16_9;
		else if(asp >= 2209 && asp <= 2211)
			priv->vaspect = ASPECT_2_21_1;
		else if(asp == 1000)
			priv->vaspect = ASPECT_1_1;
		else
			mp_msg(MSGT_MUXER, MSGL_ERR, "ERROR: unrecognized aspect %.3f\n", conf_vaspect);
	}
	
	priv->vframerate = 0;		// no change
	if(conf_telecine && conf_vframerate > 0)
	{
		mp_msg(MSGT_MUXER, MSGL_ERR, "ERROR: options 'telecine' and 'vframerate' are mutually exclusive, vframerate disabled\n");
		conf_vframerate = 0;
	}
	
	if(conf_telecine == TELECINE_FILM2PAL)
	{
		if(conf_telecine_src==0.0f) conf_telecine_src = 24000.0/1001.0;
		conf_telecine_dest = 25;
		conf_telecine = TELECINE_DGPULLDOWN;
	}
	else if(conf_telecine == PULLDOWN32)
	{
		if(conf_telecine_src==0.0f) conf_telecine_src = 24000.0/1001.0;
		conf_telecine_dest = 30000.0/1001.0;
		conf_telecine = TELECINE_DGPULLDOWN;
	}
	
	if(conf_telecine_src>0 && conf_telecine_dest>0 && conf_telecine_src < conf_telecine_dest)
	{
		int sfps, tfps;
		
		sfps = (int) (conf_telecine_src * 1001 + 0.5);
		tfps = (int) (conf_telecine_dest * 1001 + 0.5);
		if(sfps % 2 || tfps % 2)
		{
			sfps *= 2;
			tfps *= 2;
		}
		
		if(((tfps - sfps)>>1) > sfps)
		{
			mp_msg(MSGT_MUXER, MSGL_ERR, "ERROR! Framerate increment must be <= 1.5, telecining disabled\n");
			conf_telecine = 0;
		}
		else
		{
			generate_flags(priv->bff_mask, sfps, tfps);
			conf_telecine = TELECINE_DGPULLDOWN;
			conf_vframerate = conf_telecine_dest;
		}
	}
	
	if(conf_vframerate)
	{
		int fps;
		
		fps = (int) (conf_vframerate * 1001 + 0.5);
		switch(fps)
		{
			case 24000:
				priv->vframerate = FRAMERATE_23976;
				break;
			case 24024:
				priv->vframerate = FRAMERATE_24;
				break;
			case 25025:
				priv->vframerate = FRAMERATE_25;
				break;
			case 30000:
				priv->vframerate = FRAMERATE_2997;
				break;
			case 30030:
				priv->vframerate = FRAMERATE_30;
				break;
			case 50050:
				priv->vframerate = FRAMERATE_50;
				break;
			case 60000:
				priv->vframerate = FRAMERATE_5994;
				break;
			case 60060:
				priv->vframerate = FRAMERATE_60;
				break;
			default:
			{
				mp_msg(MSGT_MUXER, MSGL_ERR, "WRONG FPS: %d/1000, ignoring\n", fps);
				if(conf_telecine)
					mp_msg(MSGT_MUXER, MSGL_ERR, "DISABLED TELECINING\n");
				conf_telecine = 0;
			}
		}
	}
	
	priv->vwidth = (uint16_t) conf_vwidth;
	priv->vheight = (uint16_t) conf_vheight;
	priv->panscan_width = (uint16_t) conf_panscan_width;
	priv->panscan_height = (uint16_t) conf_panscan_height;
	priv->vbitrate = ((conf_vbitrate) * 10) >> 2;	//*1000 / 400
	
	if(priv->vaspect || priv->vframerate || priv->vwidth || priv->vheight || priv->vbitrate || priv->panscan_width || priv->panscan_height)
	{
		priv->patch_seq = priv->vaspect || priv->vframerate || priv->vwidth || priv->vheight || priv->vbitrate;
		priv->patch_sde = priv->panscan_width || priv->panscan_height;
		mp_msg(MSGT_MUXER, MSGL_INFO, "MPEG MUXER, patching");
		if(priv->vwidth || priv->vheight)
			mp_msg(MSGT_MUXER, MSGL_INFO, " resolution to %dx%d", priv->vwidth, priv->vheight);
		if(priv->panscan_width || priv->panscan_height)
			mp_msg(MSGT_MUXER, MSGL_INFO, " panscan to to %dx%d", priv->panscan_width, priv->panscan_height);
		if(priv->vframerate)
			mp_msg(MSGT_MUXER, MSGL_INFO, " framerate to %s fps", framerates[priv->vframerate]);
		if(priv->vaspect)
			mp_msg(MSGT_MUXER, MSGL_INFO, " aspect ratio to %s", aspect_ratios[priv->vaspect]);
		if(priv->vbitrate)
			mp_msg(MSGT_MUXER, MSGL_INFO, " bitrate to %u", conf_vbitrate);
		mp_msg(MSGT_MUXER, MSGL_INFO, "\n");
	}
	
	priv->has_video = priv->has_audio = 0;
	
	muxer->sysrate = priv->muxrate; 		// initial muxrate = constrained stream parameter
	priv->scr = muxer->file_end = 0;
	
	if(conf_init_vdelay && conf_drop)
	{
		mp_msg(MSGT_MUXER, MSGL_ERR, "\nmuxer_mpg, :drop and :vdelay used together are not supported, exiting\n");
		return 0;
	}
	if(conf_init_adelay)
		priv->init_adelay = - (double) conf_init_adelay / (double) 1000.0;
	
	priv->drop = conf_drop;
	
	priv->buff = (uint8_t *) malloc(priv->packet_size);
	if((priv->buff == NULL))
	{
		mp_msg(MSGT_MUXER, MSGL_ERR, "\nCouldn't allocate %d bytes, exit\n", priv->packet_size);
		return 0;
	}
	
	muxer->priv = (void *) priv;
	muxer->cont_new_stream = &mpegfile_new_stream;
	muxer->cont_write_chunk = &mpegfile_write_chunk;
	muxer->cont_write_header = &mpegfile_write_header;
	muxer->cont_write_index = &mpegfile_write_index;
	muxer->fix_stream_parameters = &fix_parameters;
	return 1;
}

