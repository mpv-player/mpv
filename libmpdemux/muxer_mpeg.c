
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "config.h"
#include "../version.h"
#include "../mp_msg.h"

#include "bswap.h"

#include "aviheader.h"
#include "ms_hdr.h"

#include "muxer.h"
#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "../m_option.h"
#include "mpeg_hdr.h"

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
static char *conf_vaspect = NULL, *conf_vframerate = NULL;
static uint32_t conf_vwidth = 0, conf_vheight = 0, conf_panscan_width = 0, conf_panscan_height = 0;
static uint32_t conf_vbitrate = 0;
static int conf_init_vpts = 200, conf_init_apts = 200;
static int conf_ts_allframes = 0;
static int conf_init_adelay = 0;
static int conf_drop = 0;
static int conf_skip_padding = 0;
static int conf_reorder = 0;

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
	uint32_t temp_ref;
	uint64_t pts, dts, idur;
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
	int mux;
	sys_info_t sys_info;
	psm_info_t psm_info;
	uint16_t packet_size;
	int is_dvd, is_xvcd, is_xsvcd, is_genmpeg1, is_genmpeg2, ts_allframes, has_video, has_audio;
	int skip_padding;
	int update_system_header, use_psm;
	off_t headers_size, data_size;
	uint64_t scr, vbytes, abytes, init_delay_pts;
	uint32_t muxrate;
	uint8_t *buff, *tmp, *residual;
	uint32_t residual_cnt, headers_cnt;
	double init_adelay;
	int drop;
	
	//video patching parameters
	uint8_t vaspect, vframerate;
	uint16_t vwidth, vheight, panscan_width, panscan_height;
	uint32_t vbitrate;
	int patch_seq, patch_sde;
	int psm_streams_cnt;
} muxer_priv_t;


typedef struct {
	int has_pts, has_dts, pes_is_aligned, type, is_late, min_pes_hlen, psm_fixed;
	uint64_t pts, last_pts, last_dts, dts, init_pts, init_dts, size, frame_duration, delta_pts, nom_delta_pts, frame_size, last_saved_pts;
	uint32_t buffer_size;
	uint8_t pes_priv_headers[4], has_pes_priv_headers;	//for A52 and LPCM
	uint32_t bitrate, rest;
	int32_t compensate;
	double delta_clock, timer, aframe_delta_pts, last_dpts;
	mpeg_frame_t *framebuf;
	uint16_t framebuf_cnt;
	uint16_t framebuf_used;
	uint16_t max_pl_size;
	int32_t last_tr;
	uint32_t max_tr;
	uint8_t id, reorder, is_mpeg12;
	mp_mpeg_header_t picture;
} muxer_headers_t;


m_option_t mpegopts_conf[] = {
	{"format", &(conf_mux), CONF_TYPE_STRING, 0, 0 ,0, NULL},
	{"size", &(conf_packet_size), CONF_TYPE_INT, CONF_RANGE, 0, 65535, NULL},
	{"muxrate", &(conf_muxrate), CONF_TYPE_INT, CONF_RANGE, 0, 12000000, NULL},	//12 Mb/s
	{"vaspect", &(conf_vaspect), CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"vframerate", &(conf_vframerate), CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"vwidth", &(conf_vwidth), CONF_TYPE_INT, CONF_RANGE, 1, 4095, NULL},
	{"vheight", &(conf_vheight), CONF_TYPE_INT, CONF_RANGE, 1, 4095, NULL},
	{"vpswidth", &(conf_panscan_width), CONF_TYPE_INT, CONF_RANGE, 1, 16383, NULL},
	{"vpsheight", &(conf_panscan_height), CONF_TYPE_INT, CONF_RANGE, 1, 16383, NULL},
	{"vbitrate", &(conf_vbitrate), CONF_TYPE_INT, CONF_RANGE, 1, 104857599, NULL},
	{"init_vpts", &(conf_init_vpts), CONF_TYPE_INT, CONF_RANGE, 100, 700, NULL},		//2*frametime at 60fps
	{"init_apts", &(conf_init_apts), CONF_TYPE_INT, CONF_RANGE, 100, 700, NULL},
	{"vdelay", &conf_init_adelay, CONF_TYPE_INT, CONF_RANGE, 1, 32760, NULL},
	{"drop", &conf_drop, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"tsaf", &conf_ts_allframes, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"skip_padding", &conf_skip_padding, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"reorder", &conf_reorder, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noreorder", &conf_reorder, CONF_TYPE_FLAG, 0, 0, 0, NULL},
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

static void fix_buffer_params(muxer_priv_t *priv, uint8_t id, uint32_t size)
{
	uint8_t i;
	
	for(i = 0; i < priv->sys_info.cnt; i++)
		if(priv->sys_info.streams[i].id == id)
			priv->sys_info.streams[i].bufsize = size;
}

static inline int is_mpeg1(uint32_t x)
{
	return (
		(x == 0x10000001) ||
		(x == mmioFOURCC('m','p','g','1')) ||
		(x == mmioFOURCC('M','P','G','1'))
	);
}

static inline int is_mpeg2(uint32_t x)
{
	return (
		(x == 0x10000002) ||
		(x == mmioFOURCC('m','p','g','2')) ||
		(x == mmioFOURCC('M','P','G','2')) ||
		(x == mmioFOURCC('m','p','e','g')) ||
		(x == mmioFOURCC('M','P','E','G'))
	);
}

static inline int is_mpeg4(uint32_t x)
{
	return (
		(x == 0x10000004) ||
		(x == mmioFOURCC('d','i','v','x')) ||
		(x == mmioFOURCC('D','I','V','X')) ||
		(x == mmioFOURCC('x','v','i','d')) ||
		(x == mmioFOURCC('X','V','I','D')) ||
		(x == mmioFOURCC('X','v','i','D')) ||
		(x == mmioFOURCC('x','v','i','x')) ||
		(x == mmioFOURCC('X','V','I','X')) ||
		(x == mmioFOURCC('m','p','4','v')) ||
		(x == mmioFOURCC('M','P','4','V'))
	);
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
		tmp[i].alloc_size = size;
		tmp[i].pts = 0;
	}
	
	return tmp;
}

static uint32_t calc_pack_hlen(muxer_priv_t *priv, muxer_headers_t *h);

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
  if (!(s->b_buffer = malloc(priv->packet_size))) {
    free (s);
    return NULL; // no mem?!
  }
  s->b_buffer_size = priv->packet_size;
  s->b_buffer_ptr = 0;
  s->b_buffer_len = 0;
  s->priv = (muxer_headers_t*) calloc(1, sizeof(muxer_headers_t));
  if(s->priv == NULL) {
    free(s);
    return NULL;
  }
  spriv = (muxer_headers_t *) s->priv;
  muxer->streams[muxer->avih.dwStreams]=s;
  s->type=type;
  s->id=muxer->avih.dwStreams;
  s->muxer=muxer;
  
  if (type == MUXER_TYPE_VIDEO) {
    spriv->type = 1;
    spriv->init_dts = 0;
    spriv->init_pts = conf_init_vpts * 90 * 1024;
    spriv->last_pts = spriv->init_pts;
    spriv->last_saved_pts = 0;
    spriv->init_dts = spriv->last_dts = spriv->init_pts;
    spriv->pts = spriv->dts = 0;
    spriv->last_dpts = spriv->timer;
    spriv->delta_pts = spriv->nom_delta_pts = 0;
    spriv->id = 0xe0 + muxer->num_videos;
    s->ckid = be2me_32 (0x100 + spriv->id);
    if(priv->is_genmpeg1 || priv->is_genmpeg2) {
      int n = priv->sys_info.cnt;
      
      priv->sys_info.streams[n].id = spriv->id;
      priv->sys_info.streams[n].type = 1;
      priv->sys_info.streams[n].bufsize = (s->h.dwSuggestedBufferSize > 0 ? s->h.dwSuggestedBufferSize : 46*1024);
      priv->sys_info.cnt++;
    }
    muxer->num_videos++;
    priv->has_video++;
    s->h.fccType=streamtypeVIDEO;
    if(!muxer->def_v) muxer->def_v=s;
    spriv->framebuf_cnt = 30;
    spriv->framebuf_used = 0;
    spriv->framebuf = init_frames(spriv->framebuf_cnt, (size_t) 5000);
    memset(&(spriv->picture), 0, sizeof(spriv->picture));
    if(spriv->framebuf == NULL) {
      mp_msg(MSGT_MUXER, MSGL_FATAL, "Couldn't allocate initial frames structure, abort!\n");
      return NULL;
    }
    if(priv->is_xvcd)
      spriv->min_pes_hlen = 18;
    else if(priv->is_xsvcd)
      spriv->min_pes_hlen = 22;
    spriv->reorder = conf_reorder;
    mp_msg (MSGT_MUXER, MSGL_DBG2, "Added video stream %d, ckid=%X\n", muxer->num_videos, s->ckid);
  } else { // MUXER_TYPE_AUDIO
    spriv->type = 0;
    spriv->pts = 1;
    spriv->dts = 0;
    spriv->max_pl_size = priv->packet_size - calc_pack_hlen(priv, spriv);
    spriv->init_pts = conf_init_apts * 90 * 1024;
    spriv->pts = spriv->init_pts;
    spriv->dts = 0;
    spriv->id = 0xc0 + muxer->num_audios;
    s->ckid = be2me_32 (0x100 + spriv->id);
    if(priv->is_genmpeg1 || priv->is_genmpeg2) {
      int n = priv->sys_info.cnt;
      
      priv->sys_info.streams[n].id = spriv->id;
      priv->sys_info.streams[n].type = 0;
      priv->sys_info.streams[n].bufsize = (s->h.dwSuggestedBufferSize > 0 ? s->h.dwSuggestedBufferSize : 4*1024);
      priv->sys_info.cnt++;
    }
    if(priv->is_xvcd)
      spriv->min_pes_hlen = 13;
    else if(priv->is_xsvcd)
      spriv->min_pes_hlen = 17;
    
    muxer->num_audios++;
    priv->has_audio++;
    s->h.fccType=streamtypeAUDIO;
    mp_msg (MSGT_MUXER, MSGL_DBG2, "Added audio stream %d, ckid=%X\n", s->id - muxer->num_videos + 1, s->ckid);
  }
  muxer->avih.dwStreams++;
  return s;
}

static void write_mpeg_ts(unsigned char *b, uint64_t ts, uint8_t mod) {
  ts >>= 10;
  b[0] = mod | ((ts >> 29) & 0xf) | 1;
  b[1] = (ts >> 22) & 0xff;
  b[2] = ((ts >> 14) & 0xff) | 1;
  b[3] = (ts >> 7) & 0xff;
  b[4] = ((ts << 1) & 0xff) | 1;
}


static void write_mpeg_rate(int type, unsigned char *b, unsigned int rate) 
{
	rate = (rate+49) / 50;
	
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
	uint16_t t1, t2, t3;
	ts >>= 10;
	ts &= 0x1FFFFFFFFULL;	//33 bits, no extension; input must be * 92160000
	t1 = (ts >> 30) & 0x7;;
	t2 = (ts >> 15) & 0x7fff;
	t3 = ts & 0x7fff;
	
	b[0] = (t1 << 3 ) | 0x44 | ((t2 >> 13) & 0x3);
	b[1] = (t2 >> 5);
	b[2] = (t2 & 0x1f) << 3 | 0x4 | ((t3 >> 13) & 0x3);
	b[3] = (t3 >> 5);
	b[4] = (t3 & 0x1f) << 3 | 0x4;
	b[5] = 1;
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
	buff[len++] = 0xe1;	//system_audio_lock, system_video_lock, marker, 1 video stream bound
	
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
			buff[len++] = 6;	//... lower
			
			//registration descriptor
			buff[len++] = 0x5;	//tag
			buff[len++] = 4;	//length: 4 bytes
			memcpy(&(buff[len]), (char*) &(priv->psm_info.streams[i].format), 4);
			len += 4;
			dlen += 10;
		}
	}
	*(uint16_t *)(&buff[10]) = be2me_16(dlen);	//length of the es descriptors
	
	*(uint16_t *)(&buff[4]) = be2me_16(len - 6 + 4);	// length field fixed, including size of CRC32
	
	*(uint32_t *)(&buff[len]) = CalcCRC32(buff, len);
	
	len += 4;	//for crc
	
	return len;
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
			buff[len] = 0x10;	//std flag
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

	if(h->has_pes_priv_headers > 0)
	{
		memcpy(&buff[len], h->pes_priv_headers, h->has_pes_priv_headers);
		len += h->has_pes_priv_headers;	
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

static void write_psm_block(muxer_t *muxer, FILE *f)
{
	uint16_t offset, stuffing_len;
	muxer_priv_t *priv = (muxer_priv_t *) muxer->priv;
	uint8_t *buff = priv->buff;
	
	offset = write_mpeg_pack_header(muxer, buff);
	offset += write_mpeg_psm(muxer, &buff[offset]);
	stuffing_len = priv->packet_size - offset;
	if(stuffing_len > 0)
	{
		//insert a PES padding packet
		write_pes_padding(&buff[offset], stuffing_len);
		offset += stuffing_len;
	}
	fwrite(buff, offset, 1, f);
	priv->headers_size += offset;
}


static int write_nav_pack(uint8_t *buff)
{
	// concatenation of pes_private2 + 03d4 x 0 and pes_private2 + 03fa x 0
        int len;

	mp_msg(MSGT_MUXER, MSGL_V, "NAV\n");
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

static uint32_t calc_pes_hlen(int format, muxer_headers_t *h, muxer_priv_t *priv)
{
	uint32_t len;
	
	if(format == MUX_MPEG1)
		len = 6;
	else
		len = 9;	//era 12
	
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

	len += h->has_pes_priv_headers;
	
	return len;
}

static uint32_t calc_pack_hlen(muxer_priv_t *priv, muxer_headers_t *h)
{
	uint32_t len;

	if(priv->mux == MUX_MPEG1)
		len = 12;
	else
		len = 14;
	
	/*if((priv->is_genmpeg1 || priv->is_genmpeg2) && priv->update_system_header)
		len += (6 + (3*priv->sys_info.cnt));*/

	if(h->min_pes_hlen > 0)
		len += h->min_pes_hlen;
	else
		len += calc_pes_hlen(priv->mux, h, priv);
	
	return len;
}


static int write_mpeg_pack(muxer_t *muxer, muxer_stream_t *s, FILE *f, char *bl, uint32_t len, int isoend)
{
	size_t tot, offset, pes_hlen, pack_hlen;
	muxer_priv_t *priv;
	uint8_t *buff;
	int stuffing_len = 0, stflen;
	muxer_headers_t *spriv;

	priv = (muxer_priv_t *) muxer->priv;
	buff = priv->buff;

	if(isoend)
	{
		buff[0] = buff[1] = 0;
		buff[2] = 1;
		buff[3] = 0xb9;
		fwrite(buff, 4, 1, f);
		return 1;
	}
		
	if((len == 0) || (bl == NULL))			//PACK headers only
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
			
		fwrite(buff, offset, 1, f);
		priv->headers_size += offset;
		tot = offset;
		muxer->movi_end += tot;
		
		return tot;
	}
	else
	{
		spriv = (muxer_headers_t *) s->priv;
		
		mp_msg(MSGT_MUXER, MSGL_V, "\nwrite_mpeg_pack(MUX=%d, len=%u, rate=%u, id=%X, pts: %llu, dts: %llu, scr: %llu, PACK_size:%u\n", 
		priv->mux, len, muxer->sysrate, s->ckid, spriv->pts, spriv->dts, priv->scr, priv->packet_size);
		
		//stflen is the count of stuffing bytes in the pes header itself, 
		//stuffing_len is the size of the stuffing pes stream (must be at least 7 bytes long)
		stflen = stuffing_len = 0;
		offset = 0;
		offset = pack_hlen = write_mpeg_pack_header(muxer, &buff[offset]);
		
		if(priv->update_system_header && (priv->is_genmpeg1 || priv->is_genmpeg2))
		{
			pack_hlen += write_mpeg_system_header(muxer, &buff[offset]);
			priv->update_system_header = 0;
		}

		offset = pack_hlen;
		
		pes_hlen = calc_pes_hlen(priv->mux, spriv, priv);
		if(spriv->min_pes_hlen > 0)
		{
			if(spriv->min_pes_hlen > pes_hlen)
				stflen = spriv->min_pes_hlen - pes_hlen;
		}
			
		if((len >= priv->packet_size - pack_hlen - max(pes_hlen, spriv->min_pes_hlen)))
			stuffing_len = 0;
		else
			stuffing_len = priv->packet_size - pack_hlen - max(pes_hlen, spriv->min_pes_hlen) - len;
		
		if(stuffing_len > 0)
		{
			if(stuffing_len < 7)
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
		
		if(priv->skip_padding)	//variable packet size, just for fun and to reduce file size
		{
			stuffing_len = 0;
			len = min(len, priv->packet_size - pack_hlen - pes_hlen - stflen);
		}
		else
			len = priv->packet_size - pack_hlen - pes_hlen - stflen - stuffing_len;
		
		mp_msg(MSGT_MUXER, MSGL_V, "LEN=%d, pack: %d, pes: %d, stf: %d, stf2: %d\n", len, pack_hlen, pes_hlen, stflen, stuffing_len);

		pes_hlen = write_mpeg_pes_header(spriv, (uint8_t *) &s->ckid, &buff[offset], len, stflen, priv->mux);

		offset += pes_hlen;
		
		fwrite(buff, offset, 1, f);
		mp_msg(MSGT_MUXER, MSGL_V, "pack_len = %u, pes_hlen = %u, stuffing_len: %d+%d, SCRIVO: %d bytes di payload\n", 
			pack_hlen, pes_hlen, stuffing_len, stflen, len);
		fwrite(bl, len, 1, f);
		
		offset += len;
		
		if(stuffing_len > 0)
		{
			//insert a PES padding packet
			mp_msg(MSGT_MUXER, MSGL_V, "STUFFING: %d\n", stuffing_len);
			write_pes_padding(buff, stuffing_len);
			fwrite(buff, stuffing_len, 1, f);
		}
		else
			stuffing_len = 0;
	
		offset += stuffing_len;
		
		mp_msg(MSGT_MUXER, MSGL_V, "\nwritten len=%d, spriv: pack(%d), pes(%d), stuffing(%d) tot(%d), offset: %d\n", 
			len, pack_hlen, pes_hlen, stuffing_len, pack_hlen + pes_hlen + stuffing_len, offset);
		
		priv->headers_size += pack_hlen + pes_hlen + stuffing_len + stflen;
		priv->data_size += len;
		muxer->movi_end += offset;
			
		return len;
	}
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


#define max(a, b) ((a) >= (b) ? (a) : (b))
#define min(a, b) ((a) <= (b) ? (a) : (b))


static uint32_t get_audio_frame_size(muxer_headers_t *spriv, uint8_t *buf, int format, int samples_ps)
{
	uint32_t sz, tmp;
	
#ifdef USE_LIBA52
#include "../liba52/a52.h"
	if(format == 0x2000)
	{
		int t1, t2, t3;
		sz = (uint32_t) a52_syncinfo(buf, &t1, &t2, &t3);
		if(sz)
			return sz;
	}
#endif
	tmp = spriv->bitrate * (format == 0x2000 ? 1536 : 1152);
	sz = tmp / samples_ps;
	if(sz % 2)
		sz++;
		
	return sz;
}


static int reorder_frame(muxer_headers_t *spriv, uint8_t *ptr, size_t len, uint8_t pt, uint32_t temp_ref, uint64_t idur)
{
	uint16_t idx = 0, move=0;
	
	/*	HOW TO REORDER FRAMES IN DECODING ORDER:
		current frame is n
		IF pt(n)==I or P and
			n>0 && temp_ref(n) > temp_ref(n-1) && pt(n-1 .. n-m)==B
				then insert frame n before frame n-m 
				and shift forward the others
	*/

	idx = spriv->framebuf_used;
	if(spriv->reorder)
	{
		//stores the frame in decoding order
		if((idx > 0) && ((pt == I_FRAME) || (pt == P_FRAME)))
		{
			if((spriv->framebuf[idx - 1].type == B_FRAME) && (spriv->framebuf[idx - 1].temp_ref == temp_ref-1))
			{
				while((spriv->framebuf[idx - 1].type == B_FRAME) && (spriv->framebuf[idx - 1].temp_ref < temp_ref) && (idx > 0))
					idx--;
				move = spriv->framebuf_used - idx;	//from idx there are 'move' frames to move forward
			}
		}
	}
	
	//now idx is the position where we should store the frame 
	if(idx+move >= spriv->framebuf_cnt)
	{	//realloc
		//fprintf(stderr, "\nREALLOC1: %d\n", (int) spriv->framebuf_cnt+1);
		spriv->framebuf = (mpeg_frame_t*) realloc(spriv->framebuf, (spriv->framebuf_cnt+1)*sizeof(mpeg_frame_t));
		if(spriv->framebuf == NULL)
		{
			mp_msg(MSGT_MUXER, MSGL_FATAL, "Couldn't realloc frame buffer(idx), abort\n");
			return 0;
		}
		
		spriv->framebuf[spriv->framebuf_cnt].size = 0;
		spriv->framebuf[spriv->framebuf_cnt].alloc_size = 0;
		
		spriv->framebuf[spriv->framebuf_cnt].buffer = (uint8_t*) malloc(len);
		if(spriv->framebuf[spriv->framebuf_cnt].buffer == NULL)
		{
			mp_msg(MSGT_MUXER, MSGL_FATAL, "Couldn't realloc frame buffer(frame), abort\n");
			return 0;
		}
		spriv->framebuf[spriv->framebuf_cnt].alloc_size = len;		
		spriv->framebuf_cnt++;
	}
	

	
	while(move > 0)
	{
		mpeg_frame_t f;
		
		f = spriv->framebuf[move + idx];
		spriv->framebuf[move + idx] = spriv->framebuf[move + idx - 1];
		spriv->framebuf[move + idx - 1] = f;
		move--;
	}
	
	if(spriv->framebuf[idx].alloc_size < len)
	{
		spriv->framebuf[idx].buffer = realloc(spriv->framebuf[idx].buffer, len);
		if(spriv->framebuf[idx].buffer == NULL)
		{
			mp_msg(MSGT_MUXER, MSGL_FATAL, "Couldn't realloc frame buffer(frame), abort\n");
			return 0;
		}
		spriv->framebuf[idx].alloc_size = len;
	}
	
	mp_msg(MSGT_MUXER, MSGL_DBG2, "\nIDX: %u, type: %c, temp_ref: %u, ptr: %p, len: %u, alloc: %u, buffer: %p\n", 
		(uint32_t) idx, FTYPE(pt), temp_ref, ptr, (uint32_t) len, (uint32_t) spriv->framebuf[idx].alloc_size, spriv->framebuf[idx].buffer);
		
	memcpy(spriv->framebuf[idx].buffer, ptr, len);
	spriv->framebuf[idx].size = len;
	spriv->framebuf[idx].temp_ref = temp_ref;
	spriv->framebuf[idx].type = pt;
	spriv->framebuf[idx].idur = idur;
	spriv->framebuf_used++;
	
	return 1;
}


static uint32_t dump_audio(muxer_t *muxer, muxer_stream_t *as, uint32_t abytes, int force)
{
	uint32_t len = 0, sz;
	uint64_t num_frames = 0, next_pts;
	uint16_t rest;
	int64_t tmp;
	double delta_pts;
	muxer_priv_t *priv = (muxer_priv_t *) muxer->priv;
	muxer_headers_t *apriv = (muxer_headers_t*) as->priv;
	
	if((abytes < apriv->frame_size) && (! force))	//pl_size
	//if((abytes < apriv->max_pl_size) && (! force))	//pl_size
	{
		apriv->is_late = 1;
		mp_msg(MSGT_MUXER, MSGL_V, "NOT SAVING: %u bytes\n", abytes);
		return 0;
	}
	
	abytes = min(abytes, as->b_buffer_len);			//available bytes
	if(! abytes)
		return 0;

	if(as->ckid == be2me_32(0x1bd))
		apriv->has_pes_priv_headers = 4;
	else
		apriv->has_pes_priv_headers = 0;

	rest = (apriv->size % apriv->frame_size);
	if(rest)
		rest = apriv->frame_size - rest;
	
	sz = priv->packet_size - calc_pack_hlen(priv, apriv);	//how many payload bytes we are about to write
	num_frames = (sz + apriv->frame_size - 1 - rest) / apriv->frame_size;
	
	mp_msg(MSGT_MUXER, MSGL_V, "\nAUDIO: tot=%llu, sz=%u bytes, FRAMES: %llu * %u, REST: %u, DELTA_PTS: %u\n", 
		apriv->size, sz, num_frames, (uint32_t) apriv->frame_size, (uint32_t) rest, (uint32_t) ((num_frames * apriv->delta_pts) >> 10));
	
	next_pts = ((uint64_t) (num_frames * apriv->delta_pts)) + apriv->pts;
	if(((priv->scr + (63000*1024)) < next_pts) && (priv->scr < apriv->pts) && (! force))
	{
		apriv->is_late = 1;
		return 0;
	}

	if(as->ckid == be2me_32(0x1bd))
	{
		apriv->pes_priv_headers[0] = 0x80;
		apriv->pes_priv_headers[1] = num_frames;
		apriv->pes_priv_headers[2] = ((rest+1) >> 8) & 0xff;	//256 * 0 ...
		apriv->pes_priv_headers[3] = (rest+1) & 0xff;		// + 1 byte(s) to skip
	}
	
	if((priv->is_xsvcd || priv->is_xvcd) && apriv->size == 0)
		apriv->buffer_size = 4*1024;

	if(apriv->pts < priv->scr)
		mp_msg(MSGT_MUXER, MSGL_ERR, "\nERROR: SCR: %llu, APTS: %llu, DELTA=-%.3lf secs\n", 
			priv->scr, apriv->pts, (double) ((priv->scr - apriv->pts)/92160000.0));
	
	len = write_mpeg_pack(muxer, as, muxer->file, &(as->b_buffer[as->b_buffer_ptr]), abytes, 0);
	if((priv->is_xsvcd || priv->is_xvcd) && apriv->size == 0)
		apriv->buffer_size = 0;
	
	apriv->size += len;
	apriv->pts = next_pts;
	
	mp_msg(MSGT_MUXER, MSGL_DBG2, "NUM_FRAMES: %llu\n", num_frames);
	
	tmp = apriv->pts - priv->scr;
	if((abs(tmp) > (63000*1024)) || (apriv->pts <= priv->scr))
	{
		double d;
		d = (double) apriv->frame_size / (double) apriv->bitrate;
		d *= (tmp - (63000*1024));
		apriv->compensate = (uint32_t) d;
		
		if(abs(tmp) > 92160000)	//usually up to 1 second it still acceptable
			mp_msg(MSGT_MUXER, MSGL_ERR, "\nWARNING: SCR: %llu, APTS: %llu, DELTA=%.3lf secs, BYTES=%d\n", priv->scr, apriv->pts, 
				(((double) tmp)/92160000.0), apriv->compensate);
	}

	mp_msg(MSGT_MUXER, MSGL_V, "\nWRITTEN AUDIO: %u bytes, TIMER: %.3lf, FRAMES: %llu * %u, DELTA_PTS: %.3lf\n", 
		len, (double) (apriv->pts/92160000), num_frames, (uint32_t) apriv->frame_size, delta_pts);
	
	as->b_buffer_ptr += len;
	as->b_buffer_len -= len;
	
	if(as->b_buffer_len > 0)
		memmove(as->b_buffer, &(as->b_buffer[as->b_buffer_ptr]), as->b_buffer_len);
	as->b_buffer_ptr = 0;
	
	return len;
}

static void drop_delayed_audio(muxer_t *muxer, muxer_stream_t *as, int64_t size)
{
	//muxer_priv_t *priv = (muxer_priv_t *) muxer->priv;
	muxer_headers_t *apriv = (muxer_headers_t *) as->priv;
	int64_t size1, div, rest;	//initial value
	double rest_pts;
	
	div = size / apriv->frame_size;
	rest = size % apriv->frame_size;
	if(rest >= apriv->frame_size / 2)
		size1 = (div+1) * apriv->frame_size;
	else
		size1 = (div) * apriv->frame_size;

	fprintf(stderr, "SIZE1: %llu, LEN: %llu\n", size1, (uint64_t)as->b_buffer_len);
	size1 = min(size1, as->b_buffer_len);
	memmove(as->b_buffer, &(as->b_buffer[size]), as->b_buffer_len - size1);
	as->b_buffer_len -= size1;
	
	rest = size1 - size;
	rest_pts = (double) rest / (double) apriv->bitrate;
	apriv->pts += (int64_t) (92160000.0 * rest_pts);
	mp_msg(MSGT_MUXER, MSGL_V, "DROPPED: %lld bytes, REST= %lld, REST_PTS: %.3lf, AUDIO_PTS%.3lf\n", size1, rest, rest_pts, (double) (apriv->pts/92160000.0));
}


static void save_delayed_audio(muxer_t *muxer, muxer_stream_t *as, uint64_t dur)
{
	muxer_priv_t *priv = (muxer_priv_t *) muxer->priv;
	muxer_headers_t *apriv = (muxer_headers_t *) as->priv;
	uint64_t init_pts, last_pts;	//initial value
	
	init_pts = apriv->pts;
	mp_msg(MSGT_MUXER, MSGL_V, "DUR: %llu, DIFF: %llu\n", dur, apriv->pts - init_pts);
	while(dur > apriv->pts - init_pts)
	{
		priv->scr = (92160000 * apriv->size) / apriv->bitrate;
		last_pts = apriv->pts;
		dump_audio(muxer, as, as->b_buffer_len, 0);
		mp_msg(MSGT_MUXER, MSGL_V, "DUR: %llu, DIFF: %llu, SCR: %llu\n", dur, apriv->pts - init_pts, priv->scr);
	}
	
	//priv->init_delay_pts = last_pts;
	priv->init_delay_pts = (90 * 1024 * abs(conf_init_adelay)) + apriv->init_pts - (90 * 1024 * abs(conf_init_vpts));
	if(priv->init_delay_pts <= priv->scr)
		priv->init_delay_pts = last_pts;
	mp_msg(MSGT_MUXER, MSGL_INFO, "INIT_VPTS: %llu (%.3lf)\n", priv->init_delay_pts, (double) (priv->init_delay_pts/92160000.0));
}


static inline void update_scr(muxer_priv_t *priv, uint32_t len, uint32_t totlen, double mult)
{
	uint64_t delta_scr;
	double perc;
	
	perc = (double) len / (double) totlen;
	
	delta_scr = (uint64_t) (mult * perc);
	priv->scr += delta_scr;
	
	mp_msg(MSGT_MUXER, MSGL_V, "UPDATE SCR TO %llu (%.3lf): mult is %.3lf,  perc: %.3lf, %u/%u, delta: %llu\n", 
		priv->scr, (double) (priv->scr/92160000.0), mult, perc, len, totlen, delta_scr);
}


static int calc_frames_to_flush(muxer_headers_t *vpriv)
{
	int n, found = 0;
	
	if(vpriv->framebuf_used > 0)
	{
		n = 0;
		//let's count how many frames we'll store in the next pack sequence
		mp_msg(MSGT_MUXER, MSGL_V, "\n");
		while(n < vpriv->framebuf_used)
		{
			mp_msg(MSGT_MUXER, MSGL_V, "n=%d, type=%c, temp_ref=%u\n", n, FTYPE(vpriv->framebuf[n].type), vpriv->framebuf[n].temp_ref);
			if(n+1 < vpriv->framebuf_used)
				mp_msg(MSGT_MUXER, MSGL_V, "n+1=%d, type=%c, temp_ref=%u\n", n+1, FTYPE(vpriv->framebuf[n+1].type), vpriv->framebuf[n+1].temp_ref);
				
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


static uint64_t fix_pts(muxer_priv_t *priv, muxer_headers_t *vpriv, int n)
{
	int i, j, fixed = 0;
	uint64_t last_dts, last_idur, ret;
	uint32_t mintr, maxtr;

	ret = 0;
	if((vpriv->size == 0) && (fixed == 0))	//first pts adjustment, only at the beginning of the stream to manage BBI structures
	{
		int delay = 0;
		for(i = 0; i < n; i++)
		{
			if(vpriv->framebuf[i].type == I_FRAME)
			{
				for(j = i + 1; j < n; j++)
				{
					if(vpriv->framebuf[i].temp_ref >= vpriv->framebuf[j].temp_ref)
					{
						ret += vpriv->framebuf[j].idur;
						delay++;
						fixed = 1;
					}
				}
				if(fixed)
					break;
			}
		}
		
		if(! fixed)
			ret = 0;
		else
			vpriv->last_pts += ret;
		
		mp_msg(MSGT_MUXER, MSGL_INFO, "INITIAL DELAY of %d frames\n", delay);
	}
	
	//KLUDGE BEGINS: Gop header (0x000001b8 in the video stream that signals a temp_ref wraparound) is _not_ mandatory, 
	//so if we don't detect the right wraparound we will have a totally wrong timestamps assignment; let's go on
	mintr = vpriv->framebuf[0].temp_ref;
	maxtr = vpriv->framebuf[0].temp_ref;
	for(i = 0; i < n; i++)
	{
		mintr = min(vpriv->framebuf[i].temp_ref, mintr);
		maxtr = max(vpriv->framebuf[i].temp_ref, maxtr);
	}
	if(maxtr - mintr > 600)	//there must be a temp_ref wraparound
	{
		mp_msg(MSGT_MUXER, MSGL_INFO, "\nDETECTED possible temp_ref wraparound in the videostreams: n=%d, mintr=%u, maxtr=%u\n", n, mintr, maxtr);
		for(i = 0; i < n; i++)
		{
			if(vpriv->framebuf[i].temp_ref < 1000)
				vpriv->framebuf[i].temp_ref += 1024;
		}	
	}
	//KLUDGE ENDS
	
	
	for(i = 0; i < n; i++)
	{
		vpriv->framebuf[i].pts = vpriv->last_pts;
		
		for(j = 0; j < n; j++)
		{
			if((vpriv->framebuf[i].temp_ref >= vpriv->framebuf[j].temp_ref) && (i != j))
			{
				vpriv->framebuf[i].pts += vpriv->framebuf[j].idur;
			}
		}
	}

	if(vpriv->size == 0)
		last_dts = vpriv->init_dts = vpriv->framebuf[0].pts - (ret + vpriv->framebuf[0].idur);
	else
		last_dts = vpriv->last_dts;
	
	last_idur = 0;
	
	mp_msg(MSGT_MUXER, MSGL_V, "\n");
	for(i = 0; i < n; i++)
	{
		vpriv->framebuf[i].dts = last_dts + last_idur;
		last_idur = vpriv->framebuf[i].idur;
		last_dts = vpriv->framebuf[i].dts;
		mp_msg(MSGT_MUXER, MSGL_V, "I=%d, type: %c, TR: %u, pts=%.3lf, dts=%.3lf, size=%u\n", 
			i, FTYPE(vpriv->framebuf[i].type), vpriv->framebuf[i].temp_ref, (double) (vpriv->framebuf[i].pts/92160000.0), 
			(double) (vpriv->framebuf[i].dts/92160000.0), vpriv->framebuf[i].size);
	}
	
	if((vpriv->size == 0) && (priv->init_delay_pts > 0))
	{
		uint64_t diff;
		
		for(i = 0; i < vpriv->framebuf_used; i++)
		{
			vpriv->framebuf[i].pts += priv->init_delay_pts;
			vpriv->framebuf[i].dts += priv->init_delay_pts;
		}
		
		diff = vpriv->framebuf[0].pts - vpriv->framebuf[0].dts;
		if(vpriv->init_pts >= diff)
			vpriv->init_dts = vpriv->init_pts - diff;
		else
			vpriv->init_dts = diff;
		
		vpriv->last_dts +=  priv->init_delay_pts;
		
		vpriv->init_pts = 0;
		vpriv->last_pts +=  priv->init_delay_pts;
		
		priv->init_delay_pts = 0;
		mp_msg(MSGT_MUXER, MSGL_INFO, "INIT delayed video timestamps: PTS=%.3lf, DTS=%.3lf, DUR=%.3lf\n", 
			(double) (vpriv->last_pts/92160000.0), (double) (vpriv->last_dts/92160000.0), (double) (vpriv->framebuf[0].idur/92160000.0));
	}
	
	return ret;
}


static void check_pts(muxer_priv_t *priv, muxer_headers_t *vpriv, int i)
{
	uint64_t dpts;
	
	dpts = max(vpriv->last_saved_pts, vpriv->pts) - min(vpriv->last_saved_pts, vpriv->pts);
	dpts += vpriv->framebuf[i].idur;

	if((!priv->ts_allframes) && (
		(priv->is_dvd && (vpriv->framebuf[i].type != I_FRAME)) ||
		((priv->is_genmpeg1 || priv->is_genmpeg2) && (vpriv->framebuf[i].type != I_FRAME) && (dpts < 64512000)))	//0.7 seconds
	)
		vpriv->pts = vpriv->dts = 0;
	
	if(vpriv->dts && ((vpriv->dts < priv->scr) || (vpriv->pts <= vpriv->dts)))
	{
		mp_msg(MSGT_MUXER, MSGL_V, "\nWARNING, SCR: %.3lf, DTS: %.3lf, PTS: %.3lf\n", 
		(double) priv->scr/92160000.0,(double) vpriv->dts/92160000.0, (double) vpriv->pts/92160000.0);
		vpriv->dts = 0;
	}
	
	if(vpriv->pts && (vpriv->pts <= priv->scr))
	{
		mp_msg(MSGT_MUXER, MSGL_ERR, "ERROR: SCR: %.3lf, VPTS: %.3lf, DELTA=-%.3lf secs\n", 
		(double) (priv->scr/92160000.0), (double)(vpriv->pts/92160000.0), (double) ((priv->scr - vpriv->pts)/92160000.0));
		vpriv->pts = vpriv->dts = 0;
	}
	
	if(vpriv->pts)
		vpriv->last_saved_pts = vpriv->pts;
}

static uint32_t calc_audio_chunk_size(muxer_stream_t *as, double duration, int finalize)
{
	muxer_headers_t *apriv;
	uint32_t x, div, rest, abytes;
	double adur;
	uint64_t iaduration;
	
	apriv = (muxer_headers_t*) as->priv;
	abytes = (uint32_t) (duration * apriv->bitrate); //size of audio data to write
	x = (abytes + apriv->max_pl_size - 1) / apriv->max_pl_size;
	x *= apriv->max_pl_size;
	adur = (double) x / (double) apriv->bitrate;
	iaduration = (uint64_t) ((double) 92160000 * adur);

	abytes -= apriv->compensate;
	div = abytes / apriv->max_pl_size;
	rest = abytes % apriv->max_pl_size;
	if(apriv->compensate > 0)
		abytes = apriv->max_pl_size * (div - 1);
	else if(apriv->compensate < 0)
		abytes = apriv->max_pl_size * (div + 1);
	else
		abytes = apriv->max_pl_size * (rest ? div + 1 : div);
	apriv->compensate = 0;

	if(finalize)
		abytes = as->b_buffer_len;

	return abytes;
}

static int flush_buffers(muxer_t *muxer, int finalize)
{
	int i, n, pl_size, found;
	size_t saved;
	uint32_t abytes, vbytes, bytes = 0, frame_bytes = 0, audio_rest = 0, tot = 0, muxrate;
	uint32_t offset;
	uint64_t idur, init_delay = 0;
	muxer_stream_t *s, *vs, *as;
	muxer_headers_t *vpriv = NULL, *apriv = NULL;
	muxer_priv_t *priv = (muxer_priv_t *) muxer->priv;
	uint8_t *tmp;
	double mult, duration;
	uint64_t iduration;
	mpeg_frame_t temp_frame;
	
	/* 
		analyzes all streams and decides what to flush
		trying to respect an interleaving distribution
		equal to the v_bitrate/a_bitrate proportion
	*/
init:
	n = 0;
	vs = as = NULL;
	abytes = vbytes = found = 0;
	for(i = 0; i < muxer->avih.dwStreams; i++)
	{
		s = muxer->streams[i];
		if(s->type == MUXER_TYPE_VIDEO)
		{
			vs = muxer->streams[i];
			vpriv = (muxer_headers_t*) vs->priv;
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
		mp_msg(MSGT_MUXER, MSGL_V, "\nVIDEO, FLUSH %d frames (of %d), 0 to %d\n", n, vpriv->framebuf_used, n-1);

		tmp = priv->tmp;
		vbytes = 0;
		vpriv = (muxer_headers_t*) vs->priv;
		
		if(priv->residual_cnt)
		{
			mpeg_frame_t *f = &(vpriv->framebuf[0]);
			size_t sz = f->size + priv->residual_cnt;
			
			if(f->alloc_size < sz)
			{
				f->buffer = (uint8_t *) realloc(f->buffer, sz);
				f->alloc_size = sz;
			}
			memmove(&(f->buffer[priv->residual_cnt]), f->buffer, f->size);
			memcpy(f->buffer, priv->residual, priv->residual_cnt);
			f->size += priv->residual_cnt;
			priv->residual_cnt = 0;
		}
		
		duration = 0;
		iduration = 0;
		for(i = 0; i < n; i++)
		{
			vbytes += vpriv->framebuf[i].size;
			iduration += vpriv->framebuf[i].idur;
		}
		duration = (double) (iduration / 92160000.0);
		
		if(vpriv->is_mpeg12)
			init_delay = fix_pts(priv, vpriv, n);
		else
			init_delay = 0;
			
		if(as != NULL)
		{
			apriv = (muxer_headers_t*) as->priv;
			abytes = calc_audio_chunk_size(as, duration, finalize);
			
			if((abytes / apriv->max_pl_size) > n)
				audio_rest = (abytes - (apriv->max_pl_size * n)) / n;
			else
				audio_rest = 0;
			
			if(as->b_buffer_len < abytes)
			{
				mp_msg(MSGT_MUXER, MSGL_V, "Not enough audio data (%u < %u), exit\n", as->b_buffer_len, abytes);
				return 0;
			}
		}
		
		if((as != NULL) && (init_delay > 0))
		{
			if(apriv->size == 0)
				apriv->pts += init_delay;
		}
		
		saved = 0;
		bytes = vbytes + abytes;
		muxrate = (uint32_t) ((double) bytes/duration);
		if(muxrate > muxer->sysrate && (priv->is_genmpeg1 || priv->is_genmpeg2))
		{
			mp_msg(MSGT_MUXER, MSGL_V, "NEW MUXRATE: %u -> %u\n", muxrate, muxer->sysrate);
			muxer->sysrate = muxrate;
		}
		
		idur = 0;
		offset = 0;
		priv->scr = vpriv->framebuf[0].dts - vpriv->init_dts;
		
		if((priv->is_xvcd || priv->is_xsvcd) && (vpriv->size == 0))
			vpriv->buffer_size = (priv->is_xvcd ? 46 : 230)*1024;
			
		i = 0;
		while(i < n)
		{
			int frame_begin = 1, update;
			uint32_t pl_size = 0, target_size;
			uint8_t *buf;
			
			mp_msg(MSGT_MUXER, MSGL_V, "FRAME: %d, type: %c, TEMP_REF: %u, SIZE: %u\n",
				i, FTYPE(vpriv->framebuf[i].type), vpriv->framebuf[i].temp_ref, vpriv->framebuf[i].size);
			
			vpriv->pts = vpriv->framebuf[i].pts;
			vpriv->dts = vpriv->framebuf[i].dts;
			check_pts(priv, vpriv, i);
			
			if(priv->is_dvd && (vpriv->framebuf[i].type == I_FRAME) && (offset == 0))
			{
				write_mpeg_pack(muxer, NULL, muxer->file, NULL, 0, 0);	//insert fake Nav Packet
				vpriv->pes_is_aligned = 1;
			}
			
			offset = 0;
			vbytes = vpriv->framebuf[i].size;
			while(vbytes > 0 && (i < n))
			{
				target_size = priv->packet_size - calc_pack_hlen(priv, vpriv);
				if((vbytes >= target_size) || ((vbytes < target_size) && (i == n-1)))
				{
					buf = &(vpriv->framebuf[i].buffer[offset]);
					pl_size = vbytes;
					update = 1;
				}
				else
				{
					uint32_t tmp_offset = 0;
					
					if(offset == 0)
					{
						vpriv->pts = vpriv->framebuf[i].pts;
						vpriv->dts = vpriv->framebuf[i].dts;
						check_pts(priv, vpriv, i);
					}
					else if(i < n-1)
					{
						vpriv->pts = vpriv->framebuf[i+1].pts;
						vpriv->dts = vpriv->framebuf[i+1].dts;
						check_pts(priv, vpriv, i+1);
					}
					else
						vpriv->pts = vpriv->dts = 0;
					
					target_size = priv->packet_size - calc_pack_hlen(priv, vpriv);	//it was only priv->packet_size
					update = 0;
					
					while((tmp_offset < target_size) && (i < n))
					{
						pl_size = min(target_size - tmp_offset, vbytes);
						memcpy(&(tmp[tmp_offset]), &(vpriv->framebuf[i].buffer[offset]), pl_size);
						tmp_offset += pl_size;
						offset += pl_size;
						vbytes -= pl_size;
					
						if(vbytes == 0)	//current frame is saved, pass to the next
						{
							i++;
							vbytes = vpriv->framebuf[i].size;
							offset = 0;
							frame_begin = 1;
						}
					}
					buf = tmp;
					pl_size = tmp_offset;
				}
			
				
				if((pl_size < priv->packet_size - calc_pack_hlen(priv, vpriv)) && !finalize && (i >= n - 1))
				{
					memcpy(priv->residual, buf, pl_size);
					priv->residual_cnt = pl_size;
					pl_size = update = vbytes = 0;	
				}
				if(pl_size)
					pl_size = write_mpeg_pack(muxer, vs, muxer->file, buf, pl_size, 0);
				vpriv->pes_is_aligned = 0;
				vpriv->pts = vpriv->dts = 0;
				vpriv->buffer_size = 0;
				vpriv->size += pl_size;
				if(update)
				{
					vbytes -= pl_size;
					offset += pl_size;
				}
				
				/* this is needed to calculate SCR */
				frame_bytes = max(vpriv->framebuf[i].size, priv->packet_size) + priv->packet_size;
				if(abytes > 0)
					//frame_bytes += min(apriv->max_pl_size, priv->packet_size) + audio_rest;
					frame_bytes += min(apriv->max_pl_size, abytes) + audio_rest;
				
				if(priv->ts_allframes)
				{
					tot = frame_bytes;
					mult = (double) vpriv->framebuf[min(i, n-1)].idur;
				}
				else
				{
					tot = bytes;
					//mult = (double) (max(iduration, iaduration));
					mult = (double) (iduration);
				}
				update_scr(priv, pl_size, tot, mult);

				
				if(abytes > 0 && frame_begin)	//it's time to save audio
				{
					pl_size = dump_audio(muxer, as, abytes, finalize);
					if(pl_size > 0)
					{
						abytes -= pl_size;
						update_scr(priv, pl_size, tot, mult);
					}
				}
				
				frame_begin = 0;
			}
			
			i++;
		}

		
		if(vpriv->is_mpeg12)
		{
			for(i = 0; i < n; i++)
			{
				vpriv->last_dts = vpriv->framebuf[i].dts;
				if(vpriv->framebuf[i].pts >= vpriv->last_pts)
				{
					vpriv->last_pts = vpriv->framebuf[i].pts;
					idur = vpriv->framebuf[i].idur;
				}
			}
			
			vpriv->last_dts += vpriv->framebuf[n-1].idur;
			vpriv->last_pts += idur;
		}

		for(i = n; i < vpriv->framebuf_used; i++)
		{
			temp_frame = vpriv->framebuf[i - n];
			vpriv->framebuf[i - n] = vpriv->framebuf[i];
			vpriv->framebuf[i] = temp_frame;
		}
		vpriv->framebuf_used -= n;


		if((as != NULL) && priv->has_audio)
		{
			while(abytes > 0)
			{
				mult = iduration;
				pl_size = dump_audio(muxer, as, abytes, finalize);
				if(pl_size > 0)
				{
					update_scr(priv, pl_size, bytes, mult);
					abytes -= pl_size;
				}
				else
					break;
			}
		}
		
		//goto init;
	}
	
	muxer->file_end = priv->scr;
	return found;
}


static uint64_t parse_fps(int fps)
{
	// 90000 * 1024 / fps
	switch(fps)
	{
		case 239760:
			return 3843844;
		case 240000:
			return 3840000;
		case 250000:
			return 3686400;
		case 299700:
			return 3075075;
		case 300000:
			return 3072000;
		case 500000:
			return 1843200;
		case 599400:
			return 1537538;
		case 600000:
			return 1536000;
		default:
			mp_msg(MSGT_MUXER, MSGL_ERR, "ERROR! unknown fps code: %d", fps);
			return 0;
	}
}

static size_t parse_mpeg12_video(muxer_stream_t *s, muxer_priv_t *priv, muxer_headers_t *spriv, float fps, size_t len)
{
	size_t ptr = 0, sz = 0, tmp = 0;
	
	mp_msg(MSGT_MUXER, MSGL_V,"parse_mpeg12_video, len=%u\n", (uint32_t) len);
	if(s->buffer[0] != 0 || s->buffer[1] != 0 || s->buffer[2] != 1 || len<6) 
	{
		mp_msg(MSGT_MUXER, MSGL_ERR,"Unknown video format, possibly non-MPEG1/2 stream, len=%d!\n", len);
		return 0;
	}
	
	if(s->buffer[3] == 0 || s->buffer[3] == 0xb3 || s->buffer[3] == 0xb8) 
	{	// Video (0) Sequence header (b3) or GOP (b8)
		uint32_t temp_ref;
		int pt;
		
		if(s->buffer[3] == 0xb3) //sequence
		{
			//uint8_t fps = s->buffer[7] & 0x0f;
			mp_header_process_sequence_header(&(spriv->picture), &(s->buffer[4]));
			spriv->delta_pts = spriv->nom_delta_pts = parse_fps(spriv->picture.fps);
			
			spriv->delta_clock = (double) 1/fps;
			mp_msg(MSGT_MUXER, MSGL_V, "\nFPS: %.3f, FRAMETIME: %.3lf\n", fps, (double)1/fps);
			if(priv->patch_seq)
				patch_seq(priv, s->buffer);
			
			tmp = 12;
			if(s->buffer[tmp-1] & 2)
				tmp += 64;
			
			if(s->buffer[tmp-1] & 1)
				tmp += 64;
			
			if(s->buffer[tmp] == 0 && s->buffer[tmp+1] == 0 && s->buffer[tmp+2] == 1 && s->buffer[tmp+3] == 0xb5) 
				mp_header_process_extension(&(spriv->picture), &(s->buffer[tmp+4]));
		}
		
		
		if(spriv->picture.mpeg1 == 0 && priv->patch_sde)
		{
			while((s->buffer[tmp] != 0 || s->buffer[tmp+1] != 0 || s->buffer[tmp+2] != 1 || s->buffer[tmp+3] != 0xb5 ||
				((s->buffer[tmp+4] & 0xf0) != 0x20)) &&
				(tmp < len-5))
				tmp++;
			
			if(tmp < len-5)		//found
				patch_panscan(priv, &(s->buffer[tmp+4]));
		}
		
		

		if(s->buffer[3])
		{	// Sequence or GOP -- scan for Picture
			s->gop_start = s->h.dwLength;
			while (ptr < len-5 && 
				(s->buffer[ptr] != 0 || s->buffer[ptr+1] != 0 || s->buffer[ptr+2] != 1 || s->buffer[ptr+3] != 0)) 
				ptr++;
		}

		if (ptr >= len-5) 
		{
			pt = 0; // Picture not found?!
			temp_ref = 0;
			mp_msg(MSGT_MUXER, MSGL_ERR,"Warning: picture not found in GOP!\n");
		} 
		else 
		{
			pt = (s->buffer[ptr+5] & 0x1c) >> 3;
			temp_ref = (s->buffer[ptr+4]<<2)+(s->buffer[ptr+5]>>6);
			mp_msg(MSGT_MUXER, MSGL_V, "Video frame type: %c, TR: %d\n", FTYPE(pt), temp_ref);
			if(spriv->picture.mpeg1 == 0) 
			{
				size_t tmp = ptr;
			
				while (ptr < len-5 && 
					(s->buffer[ptr] != 0 || s->buffer[ptr+1] != 0 || s->buffer[ptr+2] != 1 || s->buffer[ptr+3] != 0xb5)) 
						ptr++;
				if(ptr < len-5) 
				{
					mp_header_process_extension(&(spriv->picture), &(s->buffer[ptr+4]));
					if(spriv->picture.display_time >= 50 && spriv->picture.display_time <= 300) 
						spriv->delta_pts = (spriv->nom_delta_pts * spriv->picture.display_time) / 100;
				}
				else 
					spriv->delta_pts = spriv->nom_delta_pts;
			
				ptr = tmp;
			}
		}
		
		switch (pt) {
			case 2: // predictive
			  if (s->ipb[0]) {
			    sz = len + s->ipb[0];
			    s->ipb[0] = max(s->ipb[0], s->ipb[2]);
			    s->ipb[2] = 0;
			  } else if (s->ipb[2]) {
			    sz = len + s->ipb[2];
			    s->ipb[0] = s->ipb[2];
			    s->ipb[2] = 0;
			  } else
			    sz = 4 * len; // no bidirectional frames yet?
		
			  s->ipb[1] = len;
			  break;
			case 3: // bidirectional
			  s->ipb[2] += len;
			  sz = s->ipb[1] + s->ipb[2];
			  break;
			default: // intra-coded
			  sz = len; // no extra buffer for it...
		}

		reorder_frame(spriv, s->buffer, len, pt, temp_ref, spriv->delta_pts);
	}
	
	mp_msg(MSGT_MUXER, MSGL_V,"parse_mpeg12_video, return %u\n", (uint32_t) len);
	return sz;
}


static uint64_t fix_mp4_frame_duration(muxer_headers_t *vpriv)
{
	uint64_t mn, md, mx, diff;
	uint32_t i;

	mn = mx = vpriv->framebuf[0].pts;
	for(i = 0; i < 3; i++) 
	{
		mp_msg(MSGT_DECVIDEO,MSGL_V, "PTS: %llu\n", vpriv->framebuf[i].pts);
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
	mp_msg(MSGT_DECVIDEO,MSGL_V, "MIN: %llu, mid: %llu, max: %llu\n", mn, md, mx);
	
	if(mx - md > md - mn)
		diff = md - mn;
	else
		diff = mx - md;

	if(diff > 0)
	{
		for(i=0; i<3; i++) 
		{
			vpriv->framebuf[i].pts += diff;
			vpriv->framebuf[i].dts += i * diff;
			mp_msg(MSGT_MUXER, MSGL_V, "FIXED_PTS: %.3lf, FIXED_DTS: %.3lf\n", 
				(double) (vpriv->framebuf[i].pts/92160000.0), (double) (vpriv->framebuf[i].dts/92160000.0));
		}
		return diff;
	}
	else
		return 0;
}


static size_t parse_mpeg4_video(muxer_stream_t *s, muxer_priv_t *priv, muxer_headers_t *vpriv, float fps, size_t len)
{
	size_t ptr = 0;
	int64_t delta_pts;
	uint8_t pt;
	
	mp_msg(MSGT_MUXER, MSGL_V,"parse_mpeg4_video, len=%u\n", (uint32_t) len);
	if(len<6) 
	{
		mp_msg(MSGT_MUXER, MSGL_ERR,"Frame too short: %d, exit!\n", len);
		return 0;
	}
	
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
			else if((delta < 0) && (delta < (-vpriv->picture.timeinc_resolution/2)))
				delta += vpriv->picture.timeinc_resolution;
			
			delta_pts = (92160000 * (int64_t) delta) / vpriv->picture.timeinc_resolution;
			
			pt = vpriv->picture.picture_type + 1;
			mp_msg(MSGT_MUXER, MSGL_V, "\nTYPE: %c, RESOLUTION: %d, TEMP: %d, delta: %d, delta_pts: %lld = %.3lf, delta2: %.3lf\n", 
				FTYPE(pt), vpriv->picture.timeinc_resolution, vpriv->picture.timeinc_unit, delta, delta_pts, (double) (delta_pts/92160000.0),
				(double) delta / (double) vpriv->picture.timeinc_resolution);
			
			vpriv->last_tr = vpriv->picture.timeinc_unit;	
			
			break;
		}
		
		ptr++;
	}
	
	vpriv->last_dts += vpriv->frame_duration;
	vpriv->last_pts += delta_pts;
	
	reorder_frame(vpriv, s->buffer, len, pt, 0, delta_pts);
	vpriv->framebuf[vpriv->framebuf_used-1].pts = vpriv->last_pts;
	vpriv->framebuf[vpriv->framebuf_used-1].dts = vpriv->last_dts;
	vpriv->framebuf[vpriv->framebuf_used-1].idur = vpriv->frame_duration;
	
	/*mp_msg(MSGT_MUXER, MSGL_V, "\nMPEG4V, PT: %c, LEN=%u, DELTA_PTS: %.3lf, PTS: %.3lf, DTS: %.3lf\n", 
		FTYPE(pt), len, (delta_pts/92160000.0),
		(double) (vpriv->framebuf[vpriv->framebuf_used-1].pts/92160000.0), 
		(double) (vpriv->framebuf[vpriv->framebuf_used-1].dts/92160000.0), len);*/
	
	if(!vpriv->frame_duration && vpriv->framebuf_used == 3)
	{
		vpriv->frame_duration = fix_mp4_frame_duration(vpriv);
		if(vpriv->frame_duration)
		{
			vpriv->last_pts += vpriv->frame_duration;
			vpriv->last_dts = vpriv->framebuf[vpriv->framebuf_used-1].dts;
			vpriv->delta_clock = ((double) vpriv->frame_duration)/92160000.0;
			mp_msg(MSGT_MUXER, MSGL_INFO, "FRAME DURATION: %llu   %.3lf\n", 
				vpriv->frame_duration, (double) (vpriv->frame_duration/92160000.0));
		}
	}
	
	mp_msg(MSGT_MUXER, MSGL_V, "LAST_PTS: %.3lf, LAST_DTS: %.3lf\n", 
		(double) (vpriv->last_pts/92160000.0), (double) (vpriv->last_dts/92160000.0));
	
	return len;
}

static void mpegfile_write_chunk(muxer_stream_t *s,size_t len,unsigned int flags){
  size_t ptr=0, sz = 0;
  uint64_t pts, tmp;
  muxer_t *muxer = s->muxer;
  muxer_priv_t *priv = (muxer_priv_t *)muxer->priv;
  muxer_headers_t *spriv = (muxer_headers_t*) s->priv;
  FILE *f;
  float fps;
  uint32_t stream_format;
  
  f = muxer->file;
 
  if(s->buffer == NULL)
  	return;
  
  pts = 0;
  if (s->type == MUXER_TYPE_VIDEO) { // try to recognize frame type...
	fps = (float) s->h.dwRate/ (float) s->h.dwScale;
  	spriv->type = 1;
	spriv->has_pes_priv_headers = 0;
	stream_format = s->bih->biCompression;

    if(is_mpeg1(stream_format) || is_mpeg2(stream_format))
    {
      spriv->is_mpeg12 = 1;
      if(len)
        sz = parse_mpeg12_video(s, priv, spriv, fps, len);
      else {
        tmp = (uint64_t) (92160000 / fps);
        spriv->last_pts += tmp;
        spriv->last_dts += tmp;
      }
    }
    else if(is_mpeg4(stream_format)) 
    {
      spriv->is_mpeg12 = 0;
      spriv->reorder = 0;
      if(spriv->size == 0)
        priv->use_psm = 1;
      if(len)
        sz = parse_mpeg4_video(s, priv, spriv, fps, len);
      else {
        tmp = (uint64_t) (92160000 / fps);
        spriv->last_pts += tmp;
        spriv->last_dts += tmp;
      }
    }

    mp_msg(MSGT_MUXER, MSGL_V,"mpegfile_write_chunk, Video codec=%x, len=%u, mpeg12 returned %u\n", stream_format, (uint32_t) len, (uint32_t) sz);

    ptr = 0;
    priv->vbytes += len;
    
    sz <<= 1;
  } else { // MUXER_TYPE_AUDIO
  	spriv->type = 0;
	stream_format = s->wf->wFormatTag;
	
		mp_msg(MSGT_MUXER, MSGL_V,"\nmpegfile_write_chunk, Audio codec=%x, len=%u, frame size=%u\n", 
			stream_format, (uint32_t) len, (uint32_t) spriv->frame_size);
	if(spriv->bitrate == 0)
		spriv->bitrate = s->wf->nAvgBytesPerSec;
	// I need to know the audio frame size
	if(spriv->frame_size == 0)
	{
		spriv->frame_size = get_audio_frame_size(spriv, s->buffer, stream_format, s->wf->nSamplesPerSec);
		spriv->aframe_delta_pts = ((double) spriv->frame_size / (double) spriv->bitrate);
		//spriv->delta_pts = (uint64_t) (spriv->aframe_delta_pts * 92160000);
		spriv->delta_pts = (uint64_t) (92160000 * spriv->frame_size) / spriv->bitrate;
		mp_msg(MSGT_MUXER, MSGL_INFO, "AUDIO FRAME SIZE: %u, DELTA_PTS: %llu (%.3lf)\n", (uint32_t) spriv->frame_size, spriv->delta_pts, spriv->aframe_delta_pts);
	}
	
		
	if(s->b_buffer_size - s->b_buffer_len < len)
	{
		s->b_buffer = realloc(s->b_buffer, len  + s->b_buffer_len);
		if(s->b_buffer == NULL)
		{
			mp_msg(MSGT_MUXER, MSGL_FATAL, "\nFATAL! couldn't realloc %d bytes\n", len  + s->b_buffer_len);
			return;
		}
		
		s->b_buffer_size = len  + s->b_buffer_len;
		mp_msg(MSGT_MUXER, MSGL_DBG2, "REALLOC(%d) bytes to AUDIO backbuffer\n", s->b_buffer_size);
	}
	memcpy(&(s->b_buffer[s->b_buffer_ptr + s->b_buffer_len]), s->buffer, len);
	s->b_buffer_len += len;
	
	if(stream_format == AUDIO_A52)
	{
		s->type = 1;
		s->ckid = be2me_32 (0x1bd);
		if(s->size == 0) 
		{
			spriv->max_pl_size -= 4;
			if(priv->is_genmpeg1 || priv->is_genmpeg2)
				fix_audio_sys_header(priv, spriv->id, 0xbd, 58*1024);	//only one audio at the moment
			spriv->id = 0xbd;
		}
	}
	
	if(priv->init_adelay < 0)
	{
		uint64_t delay_len;
		priv->abytes += len;
		delay_len = (uint64_t) abs((priv->init_adelay * (double) spriv->bitrate));
		if(priv->abytes >= delay_len)
		{
			if(priv->drop)
			{
				mp_msg(MSGT_MUXER, MSGL_V, "\nDROPPING %llu AUDIO BYTES, DELAY: %.3lf, BR: %u\n", delay_len, priv->init_adelay, spriv->bitrate);
				drop_delayed_audio(muxer, s, (int64_t) delay_len);
			}
			else
			{
				mp_msg(MSGT_MUXER, MSGL_V, "\nWRITING %llu EARLY AUDIO BYTES, DELAY: %.3lf, BR: %u\n", delay_len, priv->init_adelay, spriv->bitrate);
				save_delayed_audio(muxer, s, (uint64_t) (92160000 * (-priv->init_adelay)));
			}
			priv->init_adelay = 0.0;
			conf_init_adelay = 0;
			priv->drop = 0;
		}
	}
	
	sz = max(len, 2 * priv->packet_size);
  }
  
  if (s->h.dwSampleSize) {
	// CBR
	s->h.dwLength += len/s->h.dwSampleSize;
	if (len%s->h.dwSampleSize) mp_msg(MSGT_MUXER, MSGL_ERR, "Warning! len isn't divisable by samplesize!\n");
  } else {
	// VBR
	s->h.dwLength++;
  }
  
  s->size += len;
  s->timer = (double)s->h.dwLength*s->h.dwScale/s->h.dwRate;

  //if genmpeg1/2 and sz > last buffer size in the system header we must write the new sysheader
  if(sz > s->h.dwSuggestedBufferSize) { // increase and set STD 
	s->h.dwSuggestedBufferSize = sz;
	if(priv->is_genmpeg1 || priv->is_genmpeg2) {
		fix_buffer_params(priv, spriv->id, s->h.dwSuggestedBufferSize);
		priv->update_system_header = 1;
	}
  }
  
  if(spriv->psm_fixed == 0) {
  	add_to_psm(priv, spriv->id, stream_format);
	spriv->psm_fixed = 1;
	priv->psm_streams_cnt++;
	if((priv->psm_streams_cnt == muxer->num_videos + muxer->num_audios) && priv->use_psm)
		write_psm_block(muxer, muxer->file);
  }
  

  if(priv->init_adelay != 0)
    return;
  
  flush_buffers(muxer, 0);
}


static void mpegfile_write_index(muxer_t *muxer)
{
	muxer_priv_t *priv = (muxer_priv_t *) muxer->priv;
	while(flush_buffers(muxer, 0) > 0);
	flush_buffers(muxer, 1);
	if(priv->is_genmpeg1 || priv->is_genmpeg2)
		write_mpeg_pack(muxer, NULL, muxer->file, NULL, 0, 1);	//insert fake Nav Packet
		
	mp_msg(MSGT_MUXER, MSGL_INFO, "\nOverhead: %.3lf%% (%llu / %llu)\n", 100.0 * (double)priv->headers_size / (double)priv->data_size, priv->headers_size, priv->data_size);
}

static void mpegfile_write_header(muxer_t *muxer)
{
	muxer_priv_t *priv = (muxer_priv_t*) muxer->priv;
	
	priv->headers_cnt++;
	
	if((priv->is_genmpeg1 || priv->is_genmpeg2) && (priv->headers_cnt == muxer->avih.dwStreams))
	{
		int i;
		for(i = 0; i < muxer->avih.dwStreams; i++)
		{
			priv->sys_info.streams[i].bufsize = muxer->streams[i]->h.dwSuggestedBufferSize;
			mp_msg(MSGT_MUXER, MSGL_V, "IDX: %d, BUFSIZE: %u\n", i, priv->sys_info.streams[i].bufsize);
		}
	}
	
	//write the first system header only for generic mpeg1/2 muxes, and only when we have collected all necessary infos
	if(priv->is_genmpeg1 || priv->is_genmpeg2 || ((priv->is_xvcd || priv->is_xsvcd) && (priv->headers_cnt == 1)))
	{
		write_mpeg_pack(muxer, NULL, muxer->file, NULL, 0, 0);
		priv->update_system_header = 0;
	}
	
	return;
}

static void setup_sys_params(muxer_priv_t *priv)
{
	if(priv->is_dvd)
	{
		priv->sys_info.cnt = 4;
		
		priv->sys_info.streams[0].id = 0xb9;
		priv->sys_info.streams[0].type = 1;
		priv->sys_info.streams[0].bufsize = 232*1024;
			
		priv->sys_info.streams[1].id = 0xb8;
		priv->sys_info.streams[1].type = 0;
		priv->sys_info.streams[1].bufsize = 4*1024;
		
		priv->sys_info.streams[2].id = 0xbd;
		priv->sys_info.streams[2].type = 1;
		priv->sys_info.streams[2].bufsize = 58*1024;
		
		priv->sys_info.streams[3].id = 0xbf;
		priv->sys_info.streams[3].type = 1;
		priv->sys_info.streams[3].bufsize = 2*1024;
	}
	else if(priv->is_xvcd || priv->is_xsvcd)
	{
		priv->sys_info.cnt = 2;
		
		priv->sys_info.streams[0].id = 0xe0;
		priv->sys_info.streams[0].type = 1;
		priv->sys_info.streams[0].bufsize = (priv->is_xvcd ? 46: 230)*1024;
			
		priv->sys_info.streams[1].id = 0xc0;
		priv->sys_info.streams[1].type = 0;
		priv->sys_info.streams[1].bufsize = 4*1024;
	}
	else
		priv->sys_info.cnt = 0;
}


int muxer_init_muxer_mpeg(muxer_t *muxer){
  muxer_priv_t *priv;
  priv = (muxer_priv_t *) calloc(1, sizeof(muxer_priv_t));
  if(priv == NULL)
  	return 0;
  priv->update_system_header = 1;
  
  //calloc() already zero-ed all flags, so we assign only the ones we need
  
  if(conf_mux != NULL) {
    if(! strcasecmp(conf_mux, "mpeg2"))
    {
    	priv->mux = MUX_MPEG2;
	priv->packet_size = 2048;
	priv->is_genmpeg2 = 1;
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
	//what's the right muxrate?
	priv->muxrate = 2788 * 125;
	priv->ts_allframes = 1;
    }
    else if(! strcasecmp(conf_mux, "xvcd"))
    {
	priv->mux = MUX_MPEG1;
	priv->is_xvcd = 1;
	priv->packet_size = 2324;
	//what's the right muxrate?
	priv->muxrate = 1394 * 125;
	priv->ts_allframes = 1;
    }
    else
    {
	priv->mux = MUX_MPEG1;
	priv->is_genmpeg1 = 1;
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
  mp_msg(MSGT_MUXER, MSGL_INFO, "PACKET SIZE: %u bytes\n", priv->packet_size);
  setup_sys_params(priv);

  priv->skip_padding = conf_skip_padding;
  if(conf_vaspect != NULL)
  {
	if(! strcmp(conf_vaspect, "1/1"))
		priv->vaspect = ASPECT_1_1;
	else if(! strcmp(conf_vaspect, "4/3"))
		priv->vaspect = ASPECT_4_3;
	else if(! strcmp(conf_vaspect, "16/9"))
		priv->vaspect = ASPECT_16_9;
	else if(! strcmp(conf_vaspect, "2.21/1"))
		priv->vaspect = ASPECT_2_21_1;
  }
  
  priv->vframerate = 0;		// no change
  if(conf_vframerate != NULL)
  {
	if(! strcmp(conf_vframerate, "23.976"))
		priv->vframerate = FRAMERATE_23976;
	else if(! strcmp(conf_vframerate, "24"))
		priv->vframerate = FRAMERATE_24;
	else if(! strcmp(conf_vframerate, "25"))
		priv->vframerate = FRAMERATE_25;
	else if(! strcmp(conf_vframerate, "29.97"))
		priv->vframerate = FRAMERATE_2997;
	else if(! strcmp(conf_vframerate, "30"))
		priv->vframerate = FRAMERATE_30;
	else if(! strcmp(conf_vframerate, "50"))
		priv->vframerate = FRAMERATE_50;
	else if(! strcmp(conf_vframerate, "59.94"))
		priv->vframerate = FRAMERATE_5994;
	else if(! strcmp(conf_vframerate, "60"))
		priv->vframerate = FRAMERATE_60;
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
  
  if(conf_init_adelay)
  	priv->init_adelay = - (double) conf_init_adelay / (double) 1000.0;
  
  priv->drop = conf_drop;
  
  priv->buff = (uint8_t *) malloc(priv->packet_size);
  priv->tmp = (uint8_t *) malloc(priv->packet_size);
  priv->residual = (uint8_t *) malloc(priv->packet_size);
  if((priv->buff == NULL) || (priv->tmp == NULL) || (priv->residual == NULL))
  {
	mp_msg(MSGT_MUXER, MSGL_ERR, "\nCouldn't allocate %d bytes, exit\n", priv->packet_size);
	return 0;
  }
  
  muxer->priv = (void *) priv;
  muxer->cont_new_stream = &mpegfile_new_stream;
  muxer->cont_write_chunk = &mpegfile_write_chunk;
  muxer->cont_write_header = &mpegfile_write_header;
  muxer->cont_write_index = &mpegfile_write_index;
  return 1;
}

