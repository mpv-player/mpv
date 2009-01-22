/*
 * Demultiplexer for MPEG2 Transport Streams.
 *
 * Written by Nico <nsabbi@libero.it>
 * Kind feedback is appreciated; 'sucks' and alike is not.
 * Originally based on demux_pva.c written by Matteo Giani and FFmpeg (libavformat) sources
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "parse_es.h"
#include "stheader.h"

#include "ms_hdr.h"
#include "mpeg_hdr.h"

#define TS_PH_PACKET_SIZE 192
#define TS_FEC_PACKET_SIZE 204
#define TS_PACKET_SIZE 188
#define NB_PID_MAX 8192

#define MAX_HEADER_SIZE 6			/* enough for PES header + length */
#define MAX_CHECK_SIZE	65535
#define TS_MAX_PROBE_SIZE 2000000 /* do not forget to change this in cfg-common-opts.h, too */
#define NUM_CONSECUTIVE_TS_PACKETS 32
#define NUM_CONSECUTIVE_AUDIO_PACKETS 348
#define MAX_A52_FRAME_SIZE 3840

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

#define TYPE_AUDIO 1
#define TYPE_VIDEO 2

int ts_prog;
int ts_keep_broken=0;
off_t ts_probe = 0;
int audio_substream_id = -1;
extern char *dvdsub_lang, *audio_lang;	//for -alang

typedef enum
{
	UNKNOWN		= -1,
	VIDEO_MPEG1 	= 0x10000001,
	VIDEO_MPEG2 	= 0x10000002,
	VIDEO_MPEG4 	= 0x10000004,
	VIDEO_H264 	= 0x10000005,
	VIDEO_AVC	= mmioFOURCC('a', 'v', 'c', '1'),
	VIDEO_VC1	= mmioFOURCC('W', 'V', 'C', '1'),
	AUDIO_MP2   	= 0x50,
	AUDIO_A52   	= 0x2000,
	AUDIO_DTS	= 0x2001,
	AUDIO_LPCM_BE  	= 0x10001,
	AUDIO_AAC	= mmioFOURCC('M', 'P', '4', 'A'),
	SPU_DVD		= 0x3000000,
	SPU_DVB		= 0x3000001,
	PES_PRIVATE1	= 0xBD00000,
	SL_PES_STREAM	= 0xD000000,
	SL_SECTION	= 0xD100000,
	MP4_OD		= 0xD200000,
} es_stream_type_t;

typedef struct {
	uint8_t *buffer;
	uint16_t buffer_len;
} ts_section_t;

typedef struct {
	int size;
	unsigned char *start;
	uint16_t payload_size;
	es_stream_type_t type, subtype;
	float pts, last_pts;
	int pid;
	char lang[4];
	int last_cc;				// last cc code (-1 if first packet)
	int is_synced;
	ts_section_t section;
	uint8_t *extradata;
	int extradata_alloc, extradata_len;
	struct {
		uint8_t au_start, au_end, last_au_end;
	} sl;
} ES_stream_t;

typedef struct {
	void *sh;
	int id;
	int type;
} sh_av_t;

typedef struct MpegTSContext {
	int packet_size; 		// raw packet size, including FEC if present e.g. 188 bytes
	ES_stream_t *pids[NB_PID_MAX];
	sh_av_t streams[NB_PID_MAX];
} MpegTSContext;


typedef struct {
	demux_stream_t *ds;
	demux_packet_t *pack;
	int offset, buffer_size;
} av_fifo_t;

#define MAX_EXTRADATA_SIZE 64*1024
typedef struct {
	int32_t object_type;	//aka codec used
	int32_t stream_type;	//video, audio etc.
	uint8_t buf[MAX_EXTRADATA_SIZE];
	uint16_t buf_size;
	uint8_t szm1;
} mp4_decoder_config_t;

typedef struct {
	//flags
	uint8_t flags;
	uint8_t au_start;
	uint8_t au_end;
	uint8_t random_accesspoint;
	uint8_t random_accesspoint_only;
	uint8_t padding;
	uint8_t use_ts;
	uint8_t idle;
	uint8_t duration;
	
	uint32_t ts_resolution, ocr_resolution;
	uint8_t ts_len, ocr_len, au_len, instant_bitrate_len, degr_len, au_seqnum_len, packet_seqnum_len;
	uint32_t timescale;
	uint16_t au_duration, cts_duration;
	uint64_t ocr, dts, cts;
} mp4_sl_config_t;

typedef struct {
	uint16_t id;
	uint8_t flags;
	mp4_decoder_config_t decoder;
	mp4_sl_config_t sl;
} mp4_es_descr_t;

typedef struct {
	uint16_t id;
	uint8_t flags;
	mp4_es_descr_t *es;
	uint16_t es_cnt;
} mp4_od_t;

typedef struct {
	uint8_t skip;
	uint8_t table_id;
	uint8_t ssi;
	uint16_t section_length;
	uint16_t ts_id;
	uint8_t version_number;
	uint8_t curr_next;
	uint8_t section_number;
	uint8_t last_section_number;
	struct pat_progs_t {
		uint16_t id;
		uint16_t pmt_pid;
	} *progs;
	uint16_t progs_cnt;
	ts_section_t section;
} pat_t;

typedef struct {
	uint16_t progid;
	uint8_t skip;
	uint8_t table_id;
	uint8_t ssi;
	uint16_t section_length;
	uint8_t version_number;
	uint8_t curr_next;
	uint8_t section_number;
	uint8_t last_section_number;
	uint16_t PCR_PID;
	uint16_t prog_descr_length;
	ts_section_t section;
	uint16_t es_cnt;
	struct pmt_es_t {
		uint16_t pid;
		uint32_t type;	//it's 8 bit long, but cast to the right type as FOURCC
		uint16_t descr_length;
		uint8_t format_descriptor[5];
		uint8_t lang[4];
		uint16_t mp4_es_id;
	} *es;
	mp4_od_t iod, *od;
	mp4_es_descr_t *mp4es;
	int od_cnt, mp4es_cnt;
} pmt_t;

typedef struct {
	uint64_t size;
	float duration;
	float first_pts;
	float last_pts;
} TS_stream_info;

typedef struct {
	MpegTSContext ts;
	int last_pid;
	av_fifo_t fifo[3];	//0 for audio, 1 for video, 2 for subs
	pat_t pat;
	pmt_t *pmt;
	uint16_t pmt_cnt;
	uint32_t prog;
	uint32_t vbitrate;
	int keep_broken;
	int last_aid;
	int last_vid;
	char packet[TS_FEC_PACKET_SIZE];
	TS_stream_info vstr, astr;
} ts_priv_t;


typedef struct {
	es_stream_type_t type;
	ts_section_t section;
} TS_pids_t;


#define IS_AUDIO(x) (((x) == AUDIO_MP2) || ((x) == AUDIO_A52) || ((x) == AUDIO_LPCM_BE) || ((x) == AUDIO_AAC) || ((x) == AUDIO_DTS))
#define IS_VIDEO(x) (((x) == VIDEO_MPEG1) || ((x) == VIDEO_MPEG2) || ((x) == VIDEO_MPEG4) || ((x) == VIDEO_H264) || ((x) == VIDEO_AVC)  || ((x) == VIDEO_VC1))

static int ts_parse(demuxer_t *demuxer, ES_stream_t *es, unsigned char *packet, int probe);

static uint8_t get_packet_size(const unsigned char *buf, int size)
{
	int i;

	if (size < (TS_FEC_PACKET_SIZE * NUM_CONSECUTIVE_TS_PACKETS))
		return 0;

	for(i=0; i<NUM_CONSECUTIVE_TS_PACKETS; i++)
	{
		if (buf[i * TS_PACKET_SIZE] != 0x47)
		{
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "GET_PACKET_SIZE, pos %d, char: %2x\n", i, buf[i * TS_PACKET_SIZE]);
			goto try_fec;
		}
	}
	return TS_PACKET_SIZE;

try_fec:
	for(i=0; i<NUM_CONSECUTIVE_TS_PACKETS; i++)
	{
		if (buf[i * TS_FEC_PACKET_SIZE] != 0x47){
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "GET_PACKET_SIZE, pos %d, char: %2x\n", i, buf[i * TS_PACKET_SIZE]);
			goto try_philips;
		}
	}
	return TS_FEC_PACKET_SIZE;

 try_philips:
	for(i=0; i<NUM_CONSECUTIVE_TS_PACKETS; i++)
	{
		if (buf[i * TS_PH_PACKET_SIZE] != 0x47)
		return 0;
	}
	return TS_PH_PACKET_SIZE;
}

static int parse_avc_sps(uint8_t *buf, int len, int *w, int *h);

static void ts_add_stream(demuxer_t * demuxer, ES_stream_t *es)
{
	int i;
	ts_priv_t *priv = (ts_priv_t*) demuxer->priv;

	if(priv->ts.streams[es->pid].sh)
		return;

	if((IS_AUDIO(es->type) || IS_AUDIO(es->subtype)) && priv->last_aid+1 < MAX_A_STREAMS)
	{
		sh_audio_t *sh = new_sh_audio_aid(demuxer, priv->last_aid, es->pid);
		if(sh)
		{
			sh->format = IS_AUDIO(es->type) ? es->type : es->subtype;
			sh->ds = demuxer->audio;

			priv->ts.streams[es->pid].id = priv->last_aid;
			priv->ts.streams[es->pid].sh = sh;
			priv->ts.streams[es->pid].type = TYPE_AUDIO;
			mp_msg(MSGT_DEMUX, MSGL_V, "\r\nADDED AUDIO PID %d, type: %x stream n. %d\r\n", es->pid, sh->format, priv->last_aid);
			priv->last_aid++;
		}

		if(es->extradata && es->extradata_len)
		{
			sh->wf = (WAVEFORMATEX *) malloc(sizeof (WAVEFORMATEX) + es->extradata_len);
			sh->wf->cbSize = es->extradata_len;
			memcpy(sh->wf + 1, es->extradata, es->extradata_len);
		}
	}

	if((IS_VIDEO(es->type) || IS_VIDEO(es->subtype)) && priv->last_vid+1 < MAX_V_STREAMS)
	{
		sh_video_t *sh = new_sh_video_vid(demuxer, priv->last_vid, es->pid);
		if(sh)
		{
			sh->format = IS_VIDEO(es->type) ? es->type : es->subtype;;
			sh->ds = demuxer->video;

			priv->ts.streams[es->pid].id = priv->last_vid;
			priv->ts.streams[es->pid].sh = sh;
			priv->ts.streams[es->pid].type = TYPE_VIDEO;
			mp_msg(MSGT_DEMUX, MSGL_V, "\r\nADDED VIDEO PID %d, type: %x stream n. %d\r\n", es->pid, sh->format, priv->last_vid);
			priv->last_vid++;


			if(sh->format == VIDEO_AVC && es->extradata && es->extradata_len)
			{
				int w = 0, h = 0;
				sh->bih = (BITMAPINFOHEADER *) calloc(1, sizeof(BITMAPINFOHEADER) + es->extradata_len);
				sh->bih->biSize= sizeof(BITMAPINFOHEADER) + es->extradata_len;
				sh->bih->biCompression = sh->format;
				memcpy(sh->bih + 1, es->extradata, es->extradata_len);
				mp_msg(MSGT_DEMUXER,MSGL_DBG2, "EXTRADATA(%d BYTES): \n", es->extradata_len);
				for(i = 0;i < es->extradata_len; i++)
					mp_msg(MSGT_DEMUXER,MSGL_DBG2, "%02x ", (int) es->extradata[i]);
				mp_msg(MSGT_DEMUXER,MSGL_DBG2,"\n");
				if(parse_avc_sps(es->extradata, es->extradata_len, &w, &h))
				{
					sh->bih->biWidth = w;
					sh->bih->biHeight = h;
				}
			}
		}
	}
}

static int ts_check_file(demuxer_t * demuxer)
{
	const int buf_size = (TS_FEC_PACKET_SIZE * NUM_CONSECUTIVE_TS_PACKETS);
	unsigned char buf[TS_FEC_PACKET_SIZE * NUM_CONSECUTIVE_TS_PACKETS], done = 0, *ptr;
	uint32_t _read, i, count = 0, is_ts;
	int cc[NB_PID_MAX], last_cc[NB_PID_MAX], pid, cc_ok, c, good, bad;
	uint8_t size = 0;
	off_t pos = 0;
	off_t init_pos;

	mp_msg(MSGT_DEMUX, MSGL_V, "Checking for MPEG-TS...\n");

	init_pos = stream_tell(demuxer->stream);
	is_ts = 0;
	while(! done)
	{
		i = 1;
		c = 0;

		while(((c=stream_read_char(demuxer->stream)) != 0x47)
			&& (c >= 0)
			&& (i < MAX_CHECK_SIZE)
			&& ! demuxer->stream->eof
		) i++;


		if(c != 0x47)
		{
			mp_msg(MSGT_DEMUX, MSGL_V, "THIS DOESN'T LOOK LIKE AN MPEG-TS FILE!\n");
			is_ts = 0;
			done = 1;
			continue;
		}

		pos = stream_tell(demuxer->stream) - 1;
		buf[0] = c;
		_read = stream_read(demuxer->stream, &buf[1], buf_size-1);

		if(_read < buf_size-1)
		{
			mp_msg(MSGT_DEMUX, MSGL_V, "COULDN'T READ ENOUGH DATA, EXITING TS_CHECK\n");
			stream_reset(demuxer->stream);
			return 0;
		}

		size = get_packet_size(buf, buf_size);
		if(size)
		{
			done = 1;
			is_ts = 1;
		}

		if(pos - init_pos >= MAX_CHECK_SIZE)
		{
			done = 1;
			is_ts = 0;
		}
	}

	mp_msg(MSGT_DEMUX, MSGL_V, "TRIED UP TO POSITION %"PRIu64", FOUND %x, packet_size= %d, SEEMS A TS? %d\n", (uint64_t) pos, c, size, is_ts);
	stream_seek(demuxer->stream, pos);

	if(! is_ts)
	  return 0;

	//LET'S CHECK continuity counters
	good = bad = 0;
	for(count = 0; count < NB_PID_MAX; count++)
	{
		cc[count] = last_cc[count] = -1;
	}

	for(count = 0; count < NUM_CONSECUTIVE_TS_PACKETS; count++)
	{
		ptr = &(buf[size * count]);
		pid = ((ptr[1] & 0x1f) << 8) | ptr[2];
		mp_msg(MSGT_DEMUX, MSGL_DBG2, "BUF: %02x %02x %02x %02x, PID %d, SIZE: %d \n",
		ptr[0], ptr[1], ptr[2], ptr[3], pid, size);

		if((pid == 8191) || (pid < 16))
			continue;

		cc[pid] = (ptr[3] & 0xf);
		cc_ok = (last_cc[pid] < 0) || ((((last_cc[pid] + 1) & 0x0f) == cc[pid]));
		mp_msg(MSGT_DEMUX, MSGL_DBG2, "PID %d, COMPARE CC %d AND LAST_CC %d\n", pid, cc[pid], last_cc[pid]);
		if(! cc_ok)
			//return 0;
			bad++;
		else
			good++;

		last_cc[pid] = cc[pid];
	}

	mp_msg(MSGT_DEMUX, MSGL_V, "GOOD CC: %d, BAD CC: %d\n", good, bad);

	if(good >= bad)
			return size;
	else
			return 0;
}


static inline int32_t progid_idx_in_pmt(ts_priv_t *priv, uint16_t progid)
{
	int x;

	if(priv->pmt == NULL)
		return -1;

	for(x = 0; x < priv->pmt_cnt; x++)
	{
		if(priv->pmt[x].progid == progid)
			return x;
	}

	return -1;
}


static inline int32_t progid_for_pid(ts_priv_t *priv, int pid, int32_t req)		//finds the first program listing a pid
{
	int i, j;
	pmt_t *pmt;


	if(priv->pmt == NULL)
		return -1;


	for(i=0; i < priv->pmt_cnt; i++)
	{
		pmt = &(priv->pmt[i]);

		if(pmt->es == NULL)
			return -1;

		for(j = 0; j < pmt->es_cnt; j++)
		{
			if(pmt->es[j].pid == pid)
			{
				if((req == 0) || (req == pmt->progid))
					return pmt->progid;
			}
		}

	}
	return -1;
}

static inline int32_t prog_pcr_pid(ts_priv_t *priv, int progid)
{
	int i;

	if(priv->pmt == NULL)
		return -1;
	for(i=0; i < priv->pmt_cnt; i++)
	{
		if(priv->pmt[i].progid == progid)
			return priv->pmt[i].PCR_PID;
	}
	return -1;
}


static inline int pid_match_lang(ts_priv_t *priv, uint16_t pid, char *lang)
{
	uint16_t i, j;
	pmt_t *pmt;

	if(priv->pmt == NULL)
		return -1;

	for(i=0; i < priv->pmt_cnt; i++)
	{
		pmt = &(priv->pmt[i]);

		if(pmt->es == NULL)
			return -1;

		for(j = 0; j < pmt->es_cnt; j++)
		{
			if(pmt->es[j].pid != pid)
				continue;

			mp_msg(MSGT_DEMUXER, MSGL_V, "CMP LANG %s AND %s, pids: %d %d\n",pmt->es[j].lang, lang, pmt->es[j].pid, pid);
			if(strncmp(pmt->es[j].lang, lang, 3) == 0)
			{
				return 1;
			}
		}

	}

	return -1;
}

typedef struct {
	int32_t atype, vtype, stype;	//types
	int32_t apid, vpid, spid;	//stream ids
	char slang[4], alang[4];	//languages
	uint16_t prog;
	off_t probe;
} tsdemux_init_t;

//stripped down version of a52_syncinfo() from liba52
//copyright belongs to Michel Lespinasse <walken@zoy.org> and Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
int mp_a52_framesize(uint8_t * buf, int *srate)
{
	int rate[] = {	32,  40,  48,  56,  64,  80,  96, 112,
			128, 160, 192, 224, 256, 320, 384, 448,
			512, 576, 640
	};
	uint8_t halfrate[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3};
	int frmsizecod, bitrate, half;
	
	if((buf[0] != 0x0b) || (buf[1] != 0x77))	/* syncword */
		return 0;
	
	if(buf[5] >= 0x60)		/* bsid >= 12 */
		return 0;

	half = halfrate[buf[5] >> 3];
	
	frmsizecod = buf[4] & 63;
	if(frmsizecod >= 38)
		return 0;

	bitrate = rate[frmsizecod >> 1];
	
	switch(buf[4] & 0xc0) 
	{
		case 0:	/* 48 KHz */
			*srate = 48000 >> half;
			return 4 * bitrate;
		case 0x40:	/* 44.1 KHz */
			*srate = 44100 >> half;
			return 2 * (320 * bitrate / 147 + (frmsizecod & 1));
		case 0x80: /* 32 KHz */
			*srate = 32000 >> half;
			return 6 * bitrate;
	}
	
	return 0;
}

//second stage: returns the count of A52 syncwords found
static int a52_check(char *buf, int len)
{
	int cnt, frame_length, ok, srate;
	
	cnt = ok = 0;
	if(len < 8)
		return 0;
		
	while(cnt < len - 7)	
	{
		if(buf[cnt] == 0x0B && buf[cnt+1] == 0x77)
		{
			frame_length = mp_a52_framesize(&buf[cnt], &srate);
			if(frame_length>=7 && frame_length<=3840)
			{
				cnt += frame_length;
				ok++;
			}
			else
			    cnt++;
		}
		else
			cnt++;
	}

	mp_msg(MSGT_DEMUXER, MSGL_V, "A52_CHECK(%d input bytes), found %d frame syncwords of %d bytes length\n", len, ok, frame_length);	
	return ok;
}


static off_t ts_detect_streams(demuxer_t *demuxer, tsdemux_init_t *param)
{
	int video_found = 0, audio_found = 0, sub_found = 0, i, num_packets = 0, req_apid, req_vpid, req_spid;
	int is_audio, is_video, is_sub, has_tables;
	int32_t p, chosen_pid = 0;
	off_t pos=0, ret = 0, init_pos, end_pos;
	ES_stream_t es;
	unsigned char tmp[TS_FEC_PACKET_SIZE];
	ts_priv_t *priv = (ts_priv_t*) demuxer->priv;
	struct {
		char *buf;
		int pos;
	} pes_priv1[8192], *pptr;
	char *tmpbuf;

	priv->last_pid = 8192;		//invalid pid

	req_apid = param->apid;
	req_vpid = param->vpid;
	req_spid = param->spid;

	has_tables = 0;
	memset(pes_priv1, 0, sizeof(pes_priv1));
	init_pos = stream_tell(demuxer->stream);
	mp_msg(MSGT_DEMUXER, MSGL_V, "PROBING UP TO %"PRIu64", PROG: %d\n", (uint64_t) param->probe, param->prog);
	end_pos = init_pos + (param->probe ? param->probe : TS_MAX_PROBE_SIZE);
	while(1)
	{
		pos = stream_tell(demuxer->stream);
		if(pos > end_pos || demuxer->stream->eof)
			break;

		if(ts_parse(demuxer, &es, tmp, 1))
		{
			//Non PES-aligned A52 audio may escape detection if PMT is not present;
			//in this case we try to find at least 3 A52 syncwords
			if((es.type == PES_PRIVATE1) && (! audio_found) && req_apid > -2)
			{
				pptr = &pes_priv1[es.pid];
				if(pptr->pos < 64*1024)
				{
				tmpbuf = (char*) realloc(pptr->buf, pptr->pos + es.size);
				if(tmpbuf != NULL)
				{
					pptr->buf = tmpbuf;
					memcpy(&(pptr->buf[ pptr->pos ]), es.start, es.size);
					pptr->pos += es.size;
					if(a52_check(pptr->buf, pptr->pos) > 2)
					{
						param->atype = AUDIO_A52;
						param->apid = es.pid;
						es.type = AUDIO_A52;
					}
				}
				}
			}
			
			is_audio = IS_AUDIO(es.type) || ((es.type==SL_PES_STREAM) && IS_AUDIO(es.subtype));
			is_video = IS_VIDEO(es.type) || ((es.type==SL_PES_STREAM) && IS_VIDEO(es.subtype));
			is_sub   = ((es.type == SPU_DVD) || (es.type == SPU_DVB));


			if((! is_audio) && (! is_video) && (! is_sub))
				continue;
			if(is_audio && req_apid==-2)
				continue;

			if(is_video)
			{
				mp_msg(MSGT_IDENTIFY, MSGL_V, "ID_VIDEO_ID=%d\n", es.pid);
    				chosen_pid = (req_vpid == es.pid);
				if((! chosen_pid) && (req_vpid > 0))
					continue;
			}
			else if(is_audio)
			{
				mp_msg(MSGT_IDENTIFY, MSGL_V, "ID_AUDIO_ID=%d\n", es.pid);
				if (es.lang[0] > 0)
					mp_msg(MSGT_IDENTIFY, MSGL_V, "ID_AID_%d_LANG=%s\n", es.pid, es.lang);
				if(req_apid > 0)
				{
					chosen_pid = (req_apid == es.pid);
					if(! chosen_pid)
						continue;
				}
				else if(param->alang[0] > 0 && es.lang[0] > 0)
				{
					if(pid_match_lang(priv, es.pid, param->alang) == -1)
						continue;

					chosen_pid = 1;
					param->apid = req_apid = es.pid;
				}
			}
			else if(is_sub)
			{
				mp_msg(MSGT_IDENTIFY, MSGL_V, "ID_SUBTITLE_ID=%d\n", es.pid);
				if (es.lang[0] > 0)
					mp_msg(MSGT_IDENTIFY, MSGL_V, "ID_SID_%d_LANG=%s\n", es.pid, es.lang);
				chosen_pid = (req_spid == es.pid);
				if((! chosen_pid) && (req_spid > 0))
					continue;
			}

			if(req_apid < 0 && (param->alang[0] == 0) && req_vpid < 0 && req_spid < 0)
				chosen_pid = 1;

			if((ret == 0) && chosen_pid)
			{
				ret = stream_tell(demuxer->stream);
			}

			p = progid_for_pid(priv, es.pid, param->prog);
			if(p != -1)
			{
				has_tables++;
				if(!param->prog && chosen_pid)
					param->prog = p;
			}

			if((param->prog > 0) && (param->prog != p))
			{
				if(audio_found)
				{
					if(is_video && (req_vpid == es.pid))
					{
						param->vtype = IS_VIDEO(es.type) ? es.type : es.subtype;
						param->vpid = es.pid;
						video_found = 1;
						break;
					}
				}

				if(video_found)
				{
					if(is_audio && (req_apid == es.pid))
					{
						param->atype = IS_AUDIO(es.type) ? es.type : es.subtype;
						param->apid = es.pid;
						audio_found = 1;
						break;
					}
				}


				continue;
			}


			mp_msg(MSGT_DEMUXER, MSGL_DBG2, "TYPE: %x, PID: %d, PROG FOUND: %d\n", es.type, es.pid, param->prog);


			if(is_video)
			{
				if((req_vpid == -1) || (req_vpid == es.pid))
				{
					param->vtype = IS_VIDEO(es.type) ? es.type : es.subtype;
					param->vpid = es.pid;
					video_found = 1;
				}
			}


			if(((req_vpid == -2) || (num_packets >= NUM_CONSECUTIVE_AUDIO_PACKETS)) && audio_found && !param->probe)
			{
				//novideo or we have at least 348 audio packets (64 KB) without video (TS with audio only)
				param->vtype = 0;
				break;
			}

			if(is_sub)
			{
				if((req_spid == -1) || (req_spid == es.pid))
				{
					param->stype = es.type;
					param->spid = es.pid;
					sub_found = 1;
				}
			}

			if(is_audio)
			{
				if((req_apid == -1) || (req_apid == es.pid))
				{
					param->atype = IS_AUDIO(es.type) ? es.type : es.subtype;
					param->apid = es.pid;
					audio_found = 1;
				}
			}

			if(audio_found && (param->apid == es.pid) && (! video_found))
				num_packets++;

			if((has_tables==0) && (video_found && audio_found) && (pos >= 1000000))
				break;
		}
	}

	for(i=0; i<8192; i++)
	{
		if(pes_priv1[i].buf != NULL)
		{
			free(pes_priv1[i].buf);
			pes_priv1[i].buf = NULL;
			pes_priv1[i].pos = 0;
		}
	}
						
	if(video_found)
	{
		if(param->vtype == VIDEO_MPEG1)
			mp_msg(MSGT_DEMUXER, MSGL_INFO, "VIDEO MPEG1(pid=%d) ", param->vpid);
		else if(param->vtype == VIDEO_MPEG2)
			mp_msg(MSGT_DEMUXER, MSGL_INFO, "VIDEO MPEG2(pid=%d) ", param->vpid);
		else if(param->vtype == VIDEO_MPEG4)
			mp_msg(MSGT_DEMUXER, MSGL_INFO, "VIDEO MPEG4(pid=%d) ", param->vpid);
		else if(param->vtype == VIDEO_H264)
			mp_msg(MSGT_DEMUXER, MSGL_INFO, "VIDEO H264(pid=%d) ", param->vpid);
		else if(param->vtype == VIDEO_VC1)
			mp_msg(MSGT_DEMUXER, MSGL_INFO, "VIDEO VC1(pid=%d) ", param->vpid);
		else if(param->vtype == VIDEO_AVC)
			mp_msg(MSGT_DEMUXER, MSGL_INFO, "VIDEO AVC(NAL-H264, pid=%d) ", param->vpid);
	}
	else
	{
		param->vtype = UNKNOWN;
		//WE DIDN'T MATCH ANY VIDEO STREAM
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "NO VIDEO! ");
	}

	if(param->atype == AUDIO_MP2)
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "AUDIO MPA(pid=%d)", param->apid);
	else if(param->atype == AUDIO_A52)
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "AUDIO A52(pid=%d)", param->apid);
	else if(param->atype == AUDIO_DTS)
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "AUDIO DTS(pid=%d)", param->apid);
	else if(param->atype == AUDIO_LPCM_BE)
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "AUDIO LPCM(pid=%d)", param->apid);
	else if(param->atype == AUDIO_AAC)
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "AUDIO AAC(pid=%d)", param->apid);
	else
	{
		audio_found = 0;
		param->atype = UNKNOWN;
		//WE DIDN'T MATCH ANY AUDIO STREAM, SO WE FORCE THE DEMUXER TO IGNORE AUDIO
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "NO AUDIO! ");
	}

	if(param->stype == SPU_DVD || param->stype == SPU_DVB)
		mp_msg(MSGT_DEMUXER, MSGL_INFO, " SUB %s(pid=%d) ", (param->stype==SPU_DVD ? "DVD" : "DVB"), param->spid);
	else
	{
		param->stype = UNKNOWN;
		mp_msg(MSGT_DEMUXER, MSGL_INFO, " NO SUBS (yet)! ");
	}

	if(video_found || audio_found)
	{
		if(!param->prog)
		{
			p = progid_for_pid(priv, video_found ? param->vpid : param->apid, 0);
			if(p != -1)
				param->prog = p;
		}

		if(demuxer->stream->eof && (ret == 0))
			ret = init_pos;
		mp_msg(MSGT_DEMUXER, MSGL_INFO, " PROGRAM N. %d\n", param->prog);
	}
	else
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "\n");


	for(i=0; i<8192; i++)
	{
		if(priv->ts.pids[i] != NULL)
		{
			priv->ts.pids[i]->payload_size = 0;
			priv->ts.pids[i]->pts = priv->ts.pids[i]->last_pts = 0;
			priv->ts.pids[i]->last_cc = -1;
			priv->ts.pids[i]->is_synced = 0;
		}
	}

	return ret;
}

static int parse_avc_sps(uint8_t *buf, int len, int *w, int *h)
{
	int sps, sps_len;
	unsigned char *ptr; 
	mp_mpeg_header_t picture;
	if(len < 6)
		return 0;
	sps = buf[5] & 0x1f;
	if(!sps)
		return 0;
	sps_len = (buf[6] << 8) | buf[7];
	if(!sps_len || (sps_len > len - 8))
		return 0;
	ptr = &(buf[8]);
	picture.display_picture_width = picture.display_picture_height = 0;
	h264_parse_sps(&picture, ptr, len - 8);
	if(!picture.display_picture_width || !picture.display_picture_height)
		return 0;
	*w = picture.display_picture_width;
	*h = picture.display_picture_height;
	return 1;
}

static demuxer_t *demux_open_ts(demuxer_t * demuxer)
{
	int i;
	uint8_t packet_size;
	sh_video_t *sh_video;
	sh_audio_t *sh_audio;
	off_t start_pos;
	tsdemux_init_t params;
	ts_priv_t * priv = demuxer->priv;

	mp_msg(MSGT_DEMUX, MSGL_V, "DEMUX OPEN, AUDIO_ID: %d, VIDEO_ID: %d, SUBTITLE_ID: %d,\n",
		demuxer->audio->id, demuxer->video->id, demuxer->sub->id);


	demuxer->type= DEMUXER_TYPE_MPEG_TS;


	stream_reset(demuxer->stream);

	packet_size = ts_check_file(demuxer);
	if(!packet_size)
	    return NULL;

	priv = calloc(1, sizeof(ts_priv_t));
	if(priv == NULL)
	{
		mp_msg(MSGT_DEMUX, MSGL_FATAL, "DEMUX_OPEN_TS, couldn't allocate enough memory for ts->priv, exit\n");
		return NULL;
	}

	for(i=0; i < 8192; i++)
	{
	    priv->ts.pids[i] = NULL;
	    priv->ts.streams[i].id = -3;
	}
	priv->pat.progs = NULL;
	priv->pat.progs_cnt = 0;
	priv->pat.section.buffer = NULL;
	priv->pat.section.buffer_len = 0;

	priv->pmt = NULL;
	priv->pmt_cnt = 0;

	priv->keep_broken = ts_keep_broken;
	priv->ts.packet_size = packet_size;


	demuxer->priv = priv;
	if(demuxer->stream->type != STREAMTYPE_FILE)
		demuxer->seekable = 1;
	else
		demuxer->seekable = 1;


	params.atype = params.vtype = params.stype = UNKNOWN;
	params.apid = demuxer->audio->id;
	params.vpid = demuxer->video->id;
	params.spid = demuxer->sub->id;
	params.prog = ts_prog;
	params.probe = ts_probe;

	if(dvdsub_lang != NULL)
	{
		strncpy(params.slang, dvdsub_lang, 3);
		params.slang[3] = 0;
	}
	else
		memset(params.slang, 0, 4);

	if(audio_lang != NULL)
	{
		strncpy(params.alang, audio_lang, 3);
		params.alang[3] = 0;
	}
	else
		memset(params.alang, 0, 4);

	start_pos = ts_detect_streams(demuxer, &params);

	demuxer->sub->id = params.spid;
	priv->prog = params.prog;

	if(params.vtype != UNKNOWN)
	{
		ts_add_stream(demuxer, priv->ts.pids[params.vpid]);
		sh_video = priv->ts.streams[params.vpid].sh;
		demuxer->video->id = priv->ts.streams[params.vpid].id;
		sh_video->ds = demuxer->video;
		sh_video->format = params.vtype;
		demuxer->video->sh = sh_video;
	}

	if(params.atype != UNKNOWN)
	{
		ES_stream_t *es = priv->ts.pids[params.apid]; 
		
		if(!IS_AUDIO(es->type) && !IS_AUDIO(es->subtype) && IS_AUDIO(params.atype)) es->subtype = params.atype;
		ts_add_stream(demuxer, priv->ts.pids[params.apid]);
		sh_audio = priv->ts.streams[params.apid].sh;
		demuxer->audio->id = priv->ts.streams[params.apid].id;
		sh_audio->ds = demuxer->audio;
		sh_audio->format = params.atype;
		demuxer->audio->sh = sh_audio;
	}


	mp_msg(MSGT_DEMUXER,MSGL_V, "Opened TS demuxer, audio: %x(pid %d), video: %x(pid %d)...POS=%"PRIu64", PROBE=%"PRIu64"\n", params.atype, demuxer->audio->id, params.vtype, demuxer->video->id, (uint64_t) start_pos, ts_probe);


	start_pos = (start_pos <= priv->ts.packet_size ? 0 : start_pos - priv->ts.packet_size);
	demuxer->movi_start = start_pos;
	demuxer->reference_clock = MP_NOPTS_VALUE;
	stream_reset(demuxer->stream);
	stream_seek(demuxer->stream, start_pos);	//IF IT'S FROM A PIPE IT WILL FAIL, BUT WHO CARES?


	priv->last_pid = 8192;		//invalid pid

	for(i = 0; i < 3; i++)
	{
		priv->fifo[i].pack  = NULL;
		priv->fifo[i].offset = 0;
	}
	priv->fifo[0].ds = demuxer->audio;
	priv->fifo[1].ds = demuxer->video;
	priv->fifo[2].ds = demuxer->sub;

	priv->fifo[0].buffer_size = 1536;
	priv->fifo[1].buffer_size = 32767;
	priv->fifo[2].buffer_size = 32767;

	priv->pat.section.buffer_len = 0;
	for(i = 0; i < priv->pmt_cnt; i++)
		priv->pmt[i].section.buffer_len = 0;
	
	demuxer->filepos = stream_tell(demuxer->stream);
	return demuxer;
}

static void demux_close_ts(demuxer_t * demuxer)
{
	uint16_t i;
	ts_priv_t *priv = (ts_priv_t*) demuxer->priv;
	
	if(priv)
	{
		if(priv->pat.section.buffer)
			free(priv->pat.section.buffer);
		if(priv->pat.progs)
			free(priv->pat.progs);
	
		if(priv->pmt)
		{	
			for(i = 0; i < priv->pmt_cnt; i++)
			{
				if(priv->pmt[i].section.buffer)
					free(priv->pmt[i].section.buffer);
				if(priv->pmt[i].es)
					free(priv->pmt[i].es);
			}
			free(priv->pmt);
		}
		free(priv);
	}
	demuxer->priv=NULL;
}


unsigned char mp_getbits(unsigned char*, unsigned int, unsigned char);
#define getbits mp_getbits

static int mp4_parse_sl_packet(pmt_t *pmt, uint8_t *buf, uint16_t packet_len, int pid, ES_stream_t *pes_es)
{
	int i, n, m, mp4_es_id = -1;
	uint64_t v = 0;
	uint32_t pl_size = 0; 
	int deg_flag = 0;
	mp4_es_descr_t *es = NULL;
	mp4_sl_config_t *sl = NULL;
	uint8_t au_start = 0, au_end = 0, rap_flag = 0, ocr_flag = 0, padding = 0,  padding_bits = 0, idle = 0;
	
	pes_es->is_synced = 0;
	mp_msg(MSGT_DEMUXER,MSGL_V, "mp4_parse_sl_packet, pid: %d, pmt: %pm, packet_len: %d\n", pid, pmt, packet_len);	
	if(! pmt || !packet_len)
		return 0;
	
	for(i = 0; i < pmt->es_cnt; i++)
	{
		if(pmt->es[i].pid == pid)
			mp4_es_id = pmt->es[i].mp4_es_id;
	}
	if(mp4_es_id < 0)
		return -1;
	
	for(i = 0; i < pmt->mp4es_cnt; i++)
	{
		if(pmt->mp4es[i].id == mp4_es_id)
			es = &(pmt->mp4es[i]);
	}
	if(! es)
		return -1;
	
	pes_es->subtype = es->decoder.object_type;
	
	sl = &(es->sl);
	if(!sl)
		return -1;
		
	//now es is the complete es_descriptor of out mp4 ES stream
	mp_msg(MSGT_DEMUXER,MSGL_DBG2, "ID: %d, FLAGS: 0x%x, subtype: %x\n", es->id, sl->flags, pes_es->subtype);
	
	n = 0;
	if(sl->au_start)
		pes_es->sl.au_start = au_start = getbits(buf, n++, 1);
	else
		pes_es->sl.au_start = (pes_es->sl.last_au_end ? 1 : 0);
	if(sl->au_end)
		pes_es->sl.au_end = au_end = getbits(buf, n++, 1);
	
	if(!sl->au_start && !sl->au_end)
	{
		pes_es->sl.au_start = pes_es->sl.au_end = au_start = au_end = 1;
	}
	pes_es->sl.last_au_end = pes_es->sl.au_end;
	
	
	if(sl->ocr_len > 0)
		ocr_flag = getbits(buf, n++, 1);
	if(sl->idle)
		idle = getbits(buf, n++, 1);
	if(sl->padding)
		padding = getbits(buf, n++, 1);
	if(padding)
	{
		padding_bits = getbits(buf, n, 3);
		n += 3;
	}
	
	if(idle || (padding && !padding_bits))
	{
		pes_es->payload_size = 0;
		return -1;
	}
	
	//(! idle && (!padding || padding_bits != 0)) is true
	n += sl->packet_seqnum_len;
	if(sl->degr_len)
		deg_flag = getbits(buf, n++, 1);
	if(deg_flag)
		n += sl->degr_len;
	
	if(ocr_flag)
	{
		n += sl->ocr_len;
		mp_msg(MSGT_DEMUXER,MSGL_DBG2, "OCR: %d bits\n", sl->ocr_len);
	}
	
	if(packet_len * 8 <= n)
		return -1;
	
	mp_msg(MSGT_DEMUXER,MSGL_DBG2, "\nAU_START: %d, AU_END: %d\n", au_start, au_end);
	if(au_start)
	{
		int dts_flag = 0, cts_flag = 0, ib_flag = 0;
		
		if(sl->random_accesspoint)
			rap_flag = getbits(buf, n++, 1);

		//check commented because it seems it's rarely used, and we need this flag set in case of au_start
		//the decoder will eventually discard the payload if it can't decode it
		//if(rap_flag || sl->random_accesspoint_only)
			pes_es->is_synced = 1;
		
		n += sl->au_seqnum_len;
		if(packet_len * 8 <= n+8)
			return -1;
		if(sl->use_ts)
		{
			dts_flag = getbits(buf, n++, 1);
			cts_flag = getbits(buf, n++, 1);
		}
		if(sl->instant_bitrate_len)
			ib_flag = getbits(buf, n++, 1);
		if(packet_len * 8 <= n+8)
			return -1;
		if(dts_flag && (sl->ts_len > 0))
		{
			n += sl->ts_len;
			mp_msg(MSGT_DEMUXER,MSGL_DBG2, "DTS: %d bits\n", sl->ts_len);
		}
		if(packet_len * 8 <= n+8)
			return -1;
		if(cts_flag && (sl->ts_len > 0))
		{
			int i = 0, m;
			
			while(i < sl->ts_len)
			{
				m = FFMIN(8, sl->ts_len - i);
				v |= getbits(buf, n, m);
				if(sl->ts_len - i > 8)
					v <<= 8;
				i += m;
				n += m;
				if(packet_len * 8 <= n+8)
					return -1;
			}
			
			pes_es->pts = (float) v / (float) sl->ts_resolution;
			mp_msg(MSGT_DEMUXER,MSGL_DBG2, "CTS: %d bits, value: %"PRIu64"/%d = %.3f\n", sl->ts_len, v, sl->ts_resolution, pes_es->pts);
		}
		
		
		i = 0;
		pl_size = 0;
		while(i < sl->au_len)
		{
			m = FFMIN(8, sl->au_len - i);
			pl_size |= getbits(buf, n, m);
			if(sl->au_len - i > 8)
				pl_size <<= 8;
			i += m;
			n += m;
			if(packet_len * 8 <= n+8)
				return -1;
		}
		mp_msg(MSGT_DEMUXER,MSGL_DBG2, "AU_LEN: %u (%d bits)\n", pl_size, sl->au_len);
		if(ib_flag)
			n += sl->instant_bitrate_len;
	}
	
	m = (n+7)/8;
	if(0 < pl_size && pl_size < pes_es->payload_size)
		pes_es->payload_size = pl_size;
	
	mp_msg(MSGT_DEMUXER,MSGL_V, "mp4_parse_sl_packet, n=%d, m=%d, size from pes hdr: %u, sl hdr size: %u, RAP FLAGS: %d/%d\n", 
		n, m, pes_es->payload_size, pl_size, (int) rap_flag, (int) sl->random_accesspoint_only);
	
	return m;
}

//this function parses the extension fields in the PES header and returns the substream_id, or -1 in case of errors
static int parse_pes_extension_fields(unsigned char *p, int pkt_len)
{
	int skip;
	unsigned char flags;
	
	if(!(p[7] & 0x1))	//no extension_field
		return -1;
	skip = 9;
	if(p[7] & 0x80)
	{
		skip += 5;
		if(p[7] & 0x40)
			skip += 5;
	}
	if(p[7] & 0x20)	//escr_flag
		skip += 6;
	if(p[7] & 0x10)	//es_rate_flag
		skip += 3;
	if(p[7] & 0x08)//dsm_trick_mode is unsupported, skip
	{
		skip = 0;//don't let's parse the extension fields
	}
	if(p[7] & 0x04)	//additional_copy_info
		skip += 1;
	if(p[7] & 0x02)	//pes_crc_flag
		skip += 2;
	if(skip >= pkt_len)	//too few bytes
		return -1;
	flags = p[skip];
	skip++;
	if(flags & 0x80)	//pes_private_data_flag
		skip += 16;
	if(skip >= pkt_len)
		return -1;
	if(flags & 0x40)	//pack_header_field_flag
	{
		unsigned char l = p[skip];
		skip += l;
	}
	if(flags & 0x20)	//program_packet_sequence_counter
		skip += 2;
	if(flags & 0x10)	//p_std
		skip += 2;
	if(skip >= pkt_len)
		return -1;
	if(flags & 0x01)	//finally the long desired pes_extension2
	{
		unsigned char l = p[skip];	//ext2 flag+len
		skip++;
		if((l == 0x81) && (skip < pkt_len))
		{
			int ssid = p[skip];
			mp_msg(MSGT_IDENTIFY, MSGL_V, "SUBSTREAM_ID=%d (0x%02X)\n", ssid, ssid);
			return ssid;
		}
	}
	
	return -1;
}

static int pes_parse2(unsigned char *buf, uint16_t packet_len, ES_stream_t *es, int32_t type_from_pmt, pmt_t *pmt, int pid)
{
	unsigned char  *p;
	uint32_t       header_len;
	int64_t        pts;
	uint32_t       stream_id;
	uint32_t       pkt_len, pes_is_aligned;

	//Here we are always at the start of a PES packet
	mp_msg(MSGT_DEMUX, MSGL_DBG2, "pes_parse2(%p, %d): \n", buf, (uint32_t) packet_len);

	if(packet_len == 0 || packet_len > 184)
	{
		mp_msg(MSGT_DEMUX, MSGL_DBG2, "pes_parse2, BUFFER LEN IS TOO SMALL OR TOO BIG: %d EXIT\n", packet_len);
		return 0;
	}

	p = buf;
	pkt_len = packet_len;


	mp_msg(MSGT_DEMUX, MSGL_DBG2, "pes_parse2: HEADER %02x %02x %02x %02x\n", p[0], p[1], p[2], p[3]);
	if (p[0] || p[1] || (p[2] != 1))
	{
		mp_msg(MSGT_DEMUX, MSGL_DBG2, "pes_parse2: error HEADER %02x %02x %02x (should be 0x000001) \n", p[0], p[1], p[2]);
		return 0 ;
	}

	packet_len -= 6;
	if(packet_len==0)
	{
		mp_msg(MSGT_DEMUX, MSGL_DBG2, "pes_parse2: packet too short: %d, exit\n", packet_len);
		return 0;
	}

	es->payload_size = (p[4] << 8 | p[5]);
	pes_is_aligned = (p[6] & 4);

	stream_id  = p[3];


	if (p[7] & 0x80)
	{ 	/* pts available */
		pts  = (int64_t)(p[9] & 0x0E) << 29 ;
		pts |=  p[10]         << 22 ;
		pts |= (p[11] & 0xFE) << 14 ;
		pts |=  p[12]         <<  7 ;
		pts |= (p[13] & 0xFE) >>  1 ;

		es->pts = pts / 90000.0f;
	}
	else
		es->pts = 0.0f;


	header_len = p[8];


	if (header_len + 9 > pkt_len) //9 are the bytes read up to the header_length field
	{
		mp_msg(MSGT_DEMUX, MSGL_DBG2, "demux_ts: illegal value for PES_header_data_length (0x%02x)\n", header_len);
		return 0;
	}
	
	if(stream_id==0xfd)
	{
		int ssid = parse_pes_extension_fields(p, pkt_len);
		if((audio_substream_id!=-1) && (ssid != audio_substream_id))
			return 0;
	}

	p += header_len + 9;
	packet_len -= header_len + 3;

	if(es->payload_size)
		es->payload_size -= header_len + 3;


	es->is_synced = 1;	//only for SL streams we have to make sure it's really true, see below
	if (stream_id == 0xbd)
	{
		mp_msg(MSGT_DEMUX, MSGL_DBG3, "pes_parse2: audio buf = %02X %02X %02X %02X %02X %02X %02X %02X, 80: %d\n",
			p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[0] & 0x80);


		/*
		* we check the descriptor tag first because some stations
		* do not include any of the A52 header info in their audio tracks
		* these "raw" streams may begin with a byte that looks like a stream type.
		*/


		if(
			(type_from_pmt == AUDIO_A52) ||		 /* A52 - raw */
			(p[0] == 0x0B && p[1] == 0x77)		/* A52 - syncword */
		)
		{
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "A52 RAW OR SYNCWORD\n");
			es->start = p;
			es->size  = packet_len;
			es->type  = AUDIO_A52;
			es->payload_size -= packet_len;

			return 1;
		}
		/* SPU SUBS */
		else if(type_from_pmt == SPU_DVB ||
		((p[0] == 0x20) && pes_is_aligned)) // && p[1] == 0x00))
		{
			es->start = p;
			es->size  = packet_len;
			es->type  = SPU_DVB;
			es->payload_size -= packet_len;

			return 1;
		}
		else if (pes_is_aligned && ((p[0] & 0xE0) == 0x20))	//SPU_DVD
		{
			//DVD SUBS
			es->start   = p+1;
			es->size    = packet_len-1;
			es->type    = SPU_DVD;
			es->payload_size -= packet_len;

			return 1;
		}
		else if (pes_is_aligned && (p[0] & 0xF8) == 0x80)
		{
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "A52 WITH HEADER\n");
			es->start   = p+4;
			es->size    = packet_len - 4;
			es->type    = AUDIO_A52;
			es->payload_size -= packet_len;

			return 1;
		}
		else if (pes_is_aligned && ((p[0]&0xf0) == 0xa0))
		{
			int pcm_offset;

			for (pcm_offset=0; ++pcm_offset < packet_len-1 ; )
			{
				if (p[pcm_offset] == 0x01 && p[pcm_offset+1] == 0x80)
				{ 	/* START */
					pcm_offset += 2;
					break;
				}
			}

			es->start   = p + pcm_offset;
			es->size    = packet_len - pcm_offset;
			es->type    = AUDIO_LPCM_BE;
			es->payload_size -= packet_len;

			return 1;
		}
		else
		{
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "PES_PRIVATE1\n");
			es->start   = p;
			es->size    = packet_len;
			es->type    = (type_from_pmt == UNKNOWN ? PES_PRIVATE1 : type_from_pmt);
			es->payload_size -= packet_len;

			return 1;
		}
	}
	else if(((stream_id >= 0xe0) && (stream_id <= 0xef)) || (stream_id == 0xfd && type_from_pmt != UNKNOWN))
	{
		es->start   = p;
		es->size    = packet_len;
		if(type_from_pmt != UNKNOWN)
		    es->type    = type_from_pmt;
		else
		    es->type    = VIDEO_MPEG2;
		if(es->payload_size)
			es->payload_size -= packet_len;

		mp_msg(MSGT_DEMUX, MSGL_DBG2, "pes_parse2: M2V size %d\n", es->size);
		return 1;
	}
	else if ((stream_id == 0xfa))
	{
		int l;
		
		es->is_synced = 0;
		if(type_from_pmt != UNKNOWN)	//MP4 A/V or SL
		{
			es->start   = p;
			es->size    = packet_len;
			es->type    = type_from_pmt;
				
			if(type_from_pmt == SL_PES_STREAM)
			{
				//if(pes_is_aligned)
				//{
					l = mp4_parse_sl_packet(pmt, p, packet_len, pid, es);
					mp_msg(MSGT_DEMUX, MSGL_DBG2, "L=%d, TYPE=%x\n", l, type_from_pmt);
					if(l < 0)
					{
						mp_msg(MSGT_DEMUX, MSGL_DBG2, "pes_parse2: couldn't parse SL header, passing along full PES payload\n");
						l = 0;
					}
				//}
			
				es->start   += l;
				es->size    -= l;
			}

			if(es->payload_size)
				es->payload_size -= packet_len;
			return 1;
		}
	}
	else if ((stream_id & 0xe0) == 0xc0)
	{
		es->start   = p;
		es->size    = packet_len;

		if(type_from_pmt != UNKNOWN)
			es->type = type_from_pmt;
		else
			es->type    = AUDIO_MP2;

		es->payload_size -= packet_len;

		return 1;
	}
	else if (type_from_pmt != -1)	//as a last resort here we trust the PMT, if present
	{
		es->start   = p;
		es->size    = packet_len;
		es->type    = type_from_pmt;
		es->payload_size -= packet_len;

		return 1;
	}
	else
	{
		mp_msg(MSGT_DEMUX, MSGL_DBG2, "pes_parse2: unknown packet, id: %x\n", stream_id);
	}

	es->is_synced = 0;
	return 0;
}




static int ts_sync(stream_t *stream)
{
	int c=0;

	mp_msg(MSGT_DEMUX, MSGL_DBG3, "TS_SYNC \n");

	while(((c=stream_read_char(stream)) != 0x47) && ! stream->eof);

	if(c == 0x47)
		return c;
	else
		return 0;
}


static void ts_dump_streams(ts_priv_t *priv)
{
	int i;

	for(i = 0; i < 3; i++)
	{
		if((priv->fifo[i].pack != NULL) && (priv->fifo[i].offset != 0))
		{
			resize_demux_packet(priv->fifo[i].pack, priv->fifo[i].offset);
			ds_add_packet(priv->fifo[i].ds, priv->fifo[i].pack);
			priv->fifo[i].offset = 0;
			priv->fifo[i].pack = NULL;
		}
	}
}


static inline int32_t prog_idx_in_pat(ts_priv_t *priv, uint16_t progid)
{
	int x;

	if(priv->pat.progs == NULL)
			return -1;

	for(x = 0; x < priv->pat.progs_cnt; x++)
	{
		if(priv->pat.progs[x].id == progid)
			return x;
	}

	return -1;
}


static inline int32_t prog_id_in_pat(ts_priv_t *priv, uint16_t pid)
{
	int x;

	if(priv->pat.progs == NULL)
		return -1;

	for(x = 0; x < priv->pat.progs_cnt; x++)
	{
		if(priv->pat.progs[x].pmt_pid == pid)
			return priv->pat.progs[x].id;
	}

	return -1;
}

static int collect_section(ts_section_t *section, int is_start, unsigned char *buff, int size)
{
	uint8_t *ptr;
	uint16_t tlen;
	int skip, tid;
	
	mp_msg(MSGT_DEMUX, MSGL_V, "COLLECT_SECTION, start: %d, size: %d, collected: %d\n", is_start, size, section->buffer_len);
	if(! is_start && !section->buffer_len)
		return 0;
	
	if(is_start)
	{
		if(! section->buffer)
		{
			section->buffer = (uint8_t*) malloc(4096+256);
			if(section->buffer == NULL)
				return 0;
		}
		section->buffer_len = 0;
	}
	
	if(size + section->buffer_len > 4096+256)
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "COLLECT_SECTION, excessive len: %d + %d\n", section->buffer_len, size);
		return 0;
	}

	memcpy(&(section->buffer[section->buffer_len]), buff, size);
	section->buffer_len += size;
	
	if(section->buffer_len < 3)
		return 0;
		
	skip = section->buffer[0];
	if(skip + 4 > section->buffer_len)
		return 0;
	
	ptr = &(section->buffer[skip + 1]);
	tid = ptr[0];
	tlen = ((ptr[1] & 0x0f) << 8) | ptr[2];
	mp_msg(MSGT_DEMUX, MSGL_V, "SKIP: %d+1, TID: %d, TLEN: %d, COLLECTED: %d\n", skip, tid, tlen, section->buffer_len);
	if(section->buffer_len < (skip+1+3+tlen))
	{
		mp_msg(MSGT_DEMUX, MSGL_DBG2, "DATA IS NOT ENOUGH, NEXT TIME\n");
		return 0;
	}
	
	return skip+1;
}

static int parse_pat(ts_priv_t * priv, int is_start, unsigned char *buff, int size)
{
	int skip;
	unsigned char *ptr;
	unsigned char *base;
	int entries, i;
	uint16_t progid;
	struct pat_progs_t *tmp;
	ts_section_t *section;

	section = &(priv->pat.section);
	skip = collect_section(section, is_start, buff, size);
	if(! skip)
		return 0;
	
	ptr = &(section->buffer[skip]);
	//PARSING
	priv->pat.table_id = ptr[0];
	if(priv->pat.table_id != 0)
		return 0;
	priv->pat.ssi = (ptr[1] >> 7) & 0x1;
	priv->pat.curr_next = ptr[5] & 0x01;
	priv->pat.ts_id = (ptr[3]  << 8 ) | ptr[4];
	priv->pat.version_number = (ptr[5] >> 1) & 0x1F;
	priv->pat.section_length = ((ptr[1] & 0x03) << 8 ) | ptr[2];
	priv->pat.section_number = ptr[6];
	priv->pat.last_section_number = ptr[7];

	//check_crc32(0xFFFFFFFFL, ptr, priv->pat.buffer_len - 4, &ptr[priv->pat.buffer_len - 4]);
	mp_msg(MSGT_DEMUX, MSGL_V, "PARSE_PAT: section_len: %d, section %d/%d\n", priv->pat.section_length, priv->pat.section_number, priv->pat.last_section_number);

	entries = (int) (priv->pat.section_length - 9) / 4;	//entries per section

	for(i=0; i < entries; i++)
	{
		int32_t idx;
		base = &ptr[8 + i*4];
		progid = (base[0] << 8) | base[1];

		if((idx = prog_idx_in_pat(priv, progid)) == -1)
		{
			int sz = sizeof(struct pat_progs_t) * (priv->pat.progs_cnt+1);
			tmp = realloc_struct(priv->pat.progs, priv->pat.progs_cnt+1, sizeof(struct pat_progs_t));
			if(tmp == NULL)
			{
				mp_msg(MSGT_DEMUX, MSGL_ERR, "PARSE_PAT: COULDN'T REALLOC %d bytes, NEXT\n", sz);
				break;
			}
			priv->pat.progs = tmp;
			idx = priv->pat.progs_cnt;
			priv->pat.progs_cnt++;
		}

		priv->pat.progs[idx].id = progid;
		priv->pat.progs[idx].pmt_pid = ((base[2]  & 0x1F) << 8) | base[3];
		mp_msg(MSGT_DEMUX, MSGL_V, "PROG: %d (%d-th of %d), PMT: %d\n", priv->pat.progs[idx].id, i+1, entries, priv->pat.progs[idx].pmt_pid);
		mp_msg(MSGT_IDENTIFY, MSGL_V, "PROGRAM_ID=%d (0x%02X), PMT_PID: %d(0x%02X)\n", progid, priv->pat.progs[idx].pmt_pid );
	}

	return 1;
}


static inline int32_t es_pid_in_pmt(pmt_t * pmt, uint16_t pid)
{
	uint16_t i;

	if(pmt == NULL)
		return -1;

	if(pmt->es == NULL)
		return -1;

	for(i = 0; i < pmt->es_cnt; i++)
	{
		if(pmt->es[i].pid == pid)
			return (int32_t) i;
	}

	return -1;
}


static uint16_t get_mp4_desc_len(uint8_t *buf, int *len)
{
	//uint16_t i = 0, size = 0;
	int i = 0, j, size = 0;
	
	mp_msg(MSGT_DEMUX, MSGL_DBG2, "PARSE_MP4_DESC_LEN(%d), bytes: ", *len);
	j = FFMIN(*len, 4);
	while(i < j)
	{
		mp_msg(MSGT_DEMUX, MSGL_DBG2, " %x ", buf[i]);
		size |= (buf[i] & 0x7f);
		if(!(buf[i] & 0x80))
			break;
		size <<= 7;
		i++;
	}
	mp_msg(MSGT_DEMUX, MSGL_DBG2, ", SIZE=%d\n", size);
	
	*len = i+1;
	return size;
}


static uint16_t parse_mp4_slconfig_descriptor(uint8_t *buf, int len, void *elem)
{
	int i = 0;
	mp4_es_descr_t *es;
	mp4_sl_config_t *sl;
	
	mp_msg(MSGT_DEMUX, MSGL_V, "PARSE_MP4_SLCONFIG_DESCRIPTOR(%d)\n", len);
	es = (mp4_es_descr_t *) elem;
	if(!es)
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "argh! NULL elem passed, skip\n");
		return len;
	}
	sl = &(es->sl);

	sl->ts_len = sl->ocr_len = sl->au_len = sl->instant_bitrate_len = sl->degr_len = sl->au_seqnum_len = sl->packet_seqnum_len = 0;
	sl->ocr = sl->dts = sl->cts = 0;
	
	if(buf[0] == 0)
	{
		i++;
		sl->flags = buf[i];
		i++;
		sl->ts_resolution = (buf[i] << 24) | (buf[i+1] << 16) | (buf[i+2] << 8) | buf[i+3];
		i += 4;
		sl->ocr_resolution = (buf[i] << 24) | (buf[i+1] << 16) | (buf[i+2] << 8) | buf[i+3];
		i += 4;
		sl->ts_len = buf[i];
		i++;
		sl->ocr_len = buf[i];
		i++;
		sl->au_len = buf[i];
		i++;
		sl->instant_bitrate_len = buf[i];
		i++;
		sl->degr_len = (buf[i] >> 4) & 0x0f;
		sl->au_seqnum_len = ((buf[i] & 0x0f) << 1) | ((buf[i+1] >> 7) & 0x01);
		i++;
		sl->packet_seqnum_len = ((buf[i] >> 2) & 0x1f);
		i++;
		
	}
	else if(buf[0] == 1)
	{
		sl->flags = 0;
		sl->ts_resolution = 1000;
		sl->ts_len = 32;
		i++;
	}
	else if(buf[0] == 2)
	{
		sl->flags = 4;
		i++;
	}
	else 
	{
		sl->flags = 0;
		i++;
	}
	
	sl->au_start = (sl->flags >> 7) & 0x1;
	sl->au_end = (sl->flags >> 6) & 0x1;
	sl->random_accesspoint = (sl->flags >> 5) & 0x1;
	sl->random_accesspoint_only = (sl->flags >> 4) & 0x1;
	sl->padding = (sl->flags >> 3) & 0x1;
	sl->use_ts = (sl->flags >> 2) & 0x1;
	sl->idle = (sl->flags >> 1) & 0x1;
	sl->duration = sl->flags & 0x1;
	
	if(sl->duration)
	{
		sl->timescale = (buf[i] << 24) | (buf[i+1] << 16) | (buf[i+2] << 8) | buf[i+3];
		i += 4;
		sl->au_duration = (buf[i] << 8) | buf[i+1];
		i += 2;
		sl->cts_duration = (buf[i] << 8) | buf[i+1];
		i += 2; 
	}
	else	//no support for fixed durations atm
		sl->timescale = sl->au_duration = sl->cts_duration = 0;
	
	mp_msg(MSGT_DEMUX, MSGL_V, "MP4SLCONFIG(len=0x%x), predef: %d, flags: %x, use_ts: %d, tslen: %d, timescale: %d, dts: %"PRIu64", cts: %"PRIu64"\n", 
		len, buf[0], sl->flags, sl->use_ts, sl->ts_len, sl->timescale, (uint64_t) sl->dts, (uint64_t) sl->cts);
	
	return len;
}

static int parse_mp4_descriptors(pmt_t *pmt, uint8_t *buf, int len, void *elem);

static uint16_t parse_mp4_decoder_config_descriptor(pmt_t *pmt, uint8_t *buf, int len, void *elem)
{
	int i = 0, j;
	mp4_es_descr_t *es;
	mp4_decoder_config_t *dec;
	
	mp_msg(MSGT_DEMUX, MSGL_V, "PARSE_MP4_DECODER_CONFIG_DESCRIPTOR(%d)\n", len);
	es = (mp4_es_descr_t *) elem;
	if(!es)
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "argh! NULL elem passed, skip\n");
		return len;
	}
	dec = (mp4_decoder_config_t*) &(es->decoder);
	
	dec->object_type = buf[i];
	dec->stream_type =  (buf[i+1]>>2) & 0x3f;
	
	if(dec->object_type == 1 && dec->stream_type == 1)
	{
		 dec->object_type = MP4_OD;
		 dec->stream_type = MP4_OD;
	}
	else if(dec->stream_type == 4)
	{
		if(dec->object_type == 0x6a)
			dec->object_type = VIDEO_MPEG1;
		if(dec->object_type >= 0x60 && dec->object_type <= 0x65)
			dec->object_type = VIDEO_MPEG2;
		else if(dec->object_type == 0x20)
			dec->object_type = VIDEO_MPEG4;
		else if(dec->object_type == 0x21)
			dec->object_type = VIDEO_AVC;
		/*else if(dec->object_type == 0x22)
			fprintf(stderr, "TYPE 0x22\n");*/
		else dec->object_type = UNKNOWN;
	}
	else if(dec->stream_type == 5)
	{
		if(dec->object_type == 0x40)
			dec->object_type = AUDIO_AAC;
		else if(dec->object_type == 0x6b)
			dec->object_type = AUDIO_MP2;
		else if(dec->object_type >= 0x66 && dec->object_type <= 0x69)
			dec->object_type = AUDIO_MP2;
		else
			dec->object_type = UNKNOWN;
	}
	else
		dec->object_type = dec->stream_type = UNKNOWN;
	
	if(dec->object_type != UNKNOWN)
	{
		//update the type of the current stream
		for(j = 0; j < pmt->es_cnt; j++)
		{
			if(pmt->es[j].mp4_es_id == es->id)
			{
				pmt->es[j].type = SL_PES_STREAM;
			}
		}
	}
	
	if(len > 13)
		parse_mp4_descriptors(pmt, &buf[13], len-13, dec);
	
	mp_msg(MSGT_DEMUX, MSGL_V, "MP4DECODER(0x%x), object_type: 0x%x, stream_type: 0x%x\n", len, dec->object_type, dec->stream_type);
	
	return len;
}

static uint16_t parse_mp4_decoder_specific_descriptor(uint8_t *buf, int len, void *elem)
{
	int i;
	mp4_decoder_config_t *dec;
	
	mp_msg(MSGT_DEMUX, MSGL_V, "PARSE_MP4_DECODER_SPECIFIC_DESCRIPTOR(%d)\n", len);
	dec = (mp4_decoder_config_t *) elem;
	if(!dec)
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "argh! NULL elem passed, skip\n");
		return len;
	}
	
	mp_msg(MSGT_DEMUX, MSGL_DBG2, "MP4 SPECIFIC INFO BYTES: \n");
	for(i=0; i<len; i++)
		mp_msg(MSGT_DEMUX, MSGL_DBG2, "%02x ", buf[i]);
	mp_msg(MSGT_DEMUX, MSGL_DBG2, "\n");

	if(len > MAX_EXTRADATA_SIZE)
	{
		mp_msg(MSGT_DEMUX, MSGL_ERR, "DEMUX_TS, EXTRADATA SUSPICIOUSLY BIG: %d, REFUSED\r\n", len);
		return len;
	}
	memcpy(dec->buf, buf, len);
	dec->buf_size = len;
	
	return len;
}

static uint16_t parse_mp4_es_descriptor(pmt_t *pmt, uint8_t *buf, int len)
{
	int i = 0, j = 0, k, found;
	uint8_t flag;
	mp4_es_descr_t es, *target_es = NULL, *tmp;
	
	mp_msg(MSGT_DEMUX, MSGL_V, "PARSE_MP4ES: len=%d\n", len);
	memset(&es, 0, sizeof(mp4_es_descr_t));
	while(i < len)
	{
		es.id = (buf[i] << 8) | buf[i+1];
		mp_msg(MSGT_DEMUX, MSGL_V, "MP4ES_ID: %d\n", es.id);
		i += 2;
		flag = buf[i];
		i++;
		if(flag & 0x80)
			i += 2;
		if(flag & 0x40)
			i += buf[i]+1;
		if(flag & 0x20)		//OCR, maybe we need it
			i += 2;
		
		j = parse_mp4_descriptors(pmt, &buf[i], len-i, &es);
		mp_msg(MSGT_DEMUX, MSGL_V, "PARSE_MP4ES, types after parse_mp4_descriptors: 0x%x, 0x%x\n", es.decoder.object_type, es.decoder.stream_type);
		if(es.decoder.object_type != UNKNOWN && es.decoder.stream_type != UNKNOWN)
		{
			found = 0;
			//search this ES_ID if we already have it
			for(k=0; k < pmt->mp4es_cnt; k++)
			{
				if(pmt->mp4es[k].id == es.id)
				{
					target_es = &(pmt->mp4es[k]);
					found = 1;
				}
			}
			
			if(! found)
			{
				tmp = realloc_struct(pmt->mp4es, pmt->mp4es_cnt+1, sizeof(mp4_es_descr_t));
				if(tmp == NULL)
				{
					fprintf(stderr, "CAN'T REALLOC MP4_ES_DESCR\n");
					continue;
				}
				pmt->mp4es = tmp;
				target_es = &(pmt->mp4es[pmt->mp4es_cnt]);
				pmt->mp4es_cnt++;
			}
			memcpy(target_es, &es, sizeof(mp4_es_descr_t));
			mp_msg(MSGT_DEMUX, MSGL_V, "MP4ES_CNT: %d, ID=%d\n", pmt->mp4es_cnt, target_es->id);
		}

		i += j;
	}
	
	return len;
}

static void parse_mp4_object_descriptor(pmt_t *pmt, uint8_t *buf, int len, void *elem)
{
	int i, j = 0, id;
	
	i=0;
	id = (buf[0] << 2) | ((buf[1] & 0xc0) >> 6);
	mp_msg(MSGT_DEMUX, MSGL_V, "PARSE_MP4_OBJECT_DESCRIPTOR: len=%d, OD_ID=%d\n", len, id);
	if(buf[1] & 0x20)
	{
		i += buf[2] + 1;	//url
		mp_msg(MSGT_DEMUX, MSGL_V, "URL\n");
	}
	else
	{
		i = 2;
		
		while(i < len)
		{
			j = parse_mp4_descriptors(pmt, &(buf[i]), len-i, elem);
			mp_msg(MSGT_DEMUX, MSGL_V, "OBJD, NOW i = %d, j=%d, LEN=%d\n", i, j, len);
			i += j;
		}
	}
}


static void parse_mp4_iod(pmt_t *pmt, uint8_t *buf, int len, void *elem)
{
	int i, j = 0;
	mp4_od_t *iod = &(pmt->iod);
	
	iod->id = (buf[0] << 2) | ((buf[1] & 0xc0) >> 6);
	mp_msg(MSGT_DEMUX, MSGL_V, "PARSE_MP4_IOD: len=%d, IOD_ID=%d\n", len, iod->id);
	i = 2;
	if(buf[1] & 0x20)
	{
		i += buf[2] + 1;	//url
		mp_msg(MSGT_DEMUX, MSGL_V, "URL\n");
	}
	else
	{
		i = 7;
		while(i < len)
		{
			j = parse_mp4_descriptors(pmt, &(buf[i]), len-i, elem);
			mp_msg(MSGT_DEMUX, MSGL_V, "IOD, NOW i = %d, j=%d, LEN=%d\n", i, j, len);
			i += j;
		}
	}
}

static int parse_mp4_descriptors(pmt_t *pmt, uint8_t *buf, int len, void *elem)
{
	int tag, descr_len, i = 0, j = 0;
	
	mp_msg(MSGT_DEMUX, MSGL_V, "PARSE_MP4_DESCRIPTORS, len=%d\n", len);
	if(! len)
		return len;
	
	while(i < len)
	{
		tag = buf[i];
		j = len - i -1;
		descr_len = get_mp4_desc_len(&(buf[i+1]), &j);
		mp_msg(MSGT_DEMUX, MSGL_V, "TAG=%d (0x%x), DESCR_len=%d, len=%d, j=%d\n", tag, tag, descr_len, len, j);
		if(descr_len > len - j+1)
		{
			mp_msg(MSGT_DEMUX, MSGL_V, "descriptor is too long, exit\n");
			return len;
		}
		i += j+1;
		
		switch(tag)
		{
			case 0x1:
				parse_mp4_object_descriptor(pmt, &(buf[i]), descr_len, elem);
				break;
			case 0x2:
				parse_mp4_iod(pmt, &(buf[i]), descr_len, elem);
				break;
			case 0x3:
				parse_mp4_es_descriptor(pmt, &(buf[i]), descr_len);
				break;
			case 0x4:
				parse_mp4_decoder_config_descriptor(pmt, &buf[i], descr_len, elem);
				break;
			case 0x05:
				parse_mp4_decoder_specific_descriptor(&buf[i], descr_len, elem);
				break;
			case 0x6:
				parse_mp4_slconfig_descriptor(&buf[i], descr_len, elem);
				break;
			default:
				mp_msg(MSGT_DEMUX, MSGL_V, "Unsupported mp4 descriptor 0x%x\n", tag);
		}
		i += descr_len;
	}
	
	return len;
}

static ES_stream_t *new_pid(ts_priv_t *priv, int pid)
{
	ES_stream_t *tss;
	
	tss = malloc(sizeof(ES_stream_t));
	if(! tss)
		return NULL;
	memset(tss, 0, sizeof(ES_stream_t));
	tss->pid = pid;
	tss->last_cc = -1;
	tss->type = UNKNOWN;
	tss->subtype = UNKNOWN;
	tss->is_synced = 0;
	tss->extradata = NULL;
	tss->extradata_alloc = tss->extradata_len = 0;
	priv->ts.pids[pid] = tss;
	
	return tss;
}


static int parse_program_descriptors(pmt_t *pmt, uint8_t *buf, uint16_t len)
{
	uint16_t i = 0, k, olen = len;

	while(len > 0)
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "PROG DESCR, TAG=%x, LEN=%d(%x)\n", buf[i], buf[i+1], buf[i+1]);
		if(buf[i+1] > len-2)
		{
			mp_msg(MSGT_DEMUX, MSGL_V, "ERROR, descriptor len is too long, skipping\n");
			return olen;
		}

		if(buf[i] == 0x1d)
		{
			if(buf[i+3] == 2)	//buggy versions of vlc muxer make this non-standard mess (missing iod_scope)
				k = 3;
			else
				k = 4;		//this is standard compliant
			parse_mp4_descriptors(pmt, &buf[i+k], (int) buf[i+1]-(k-2), NULL);
		}

		len -= 2 + buf[i+1];
	}
	
	return olen;
}

static int parse_descriptors(struct pmt_es_t *es, uint8_t *ptr)
{
	int j, descr_len, len;

	j = 0;
	len = es->descr_length;
	while(len > 2)
	{
		descr_len = ptr[j+1];
		mp_msg(MSGT_DEMUX, MSGL_V, "...descr id: 0x%x, len=%d\n", ptr[j], descr_len);
		if(descr_len > len)
		{
			mp_msg(MSGT_DEMUX, MSGL_ERR, "INVALID DESCR LEN for tag %02x: %d vs %d max, EXIT LOOP\n", ptr[j], descr_len, len);
			return -1;
		}


		if(ptr[j] == 0x6a || ptr[j] == 0x7a)	//A52 Descriptor
		{
			if(es->type == 0x6)
			{
				es->type = AUDIO_A52;
				mp_msg(MSGT_DEMUX, MSGL_DBG2, "DVB A52 Descriptor\n");
			}
		}
		else if(ptr[j] == 0x7b)	//DVB DTS Descriptor
		{
			if(es->type == 0x6)
			{
				es->type = AUDIO_DTS;
				mp_msg(MSGT_DEMUX, MSGL_DBG2, "DVB DTS Descriptor\n");
			}
		}
		else if(ptr[j] == 0x59)	//Subtitling Descriptor
		{
			uint8_t subtype;

			mp_msg(MSGT_DEMUX, MSGL_DBG2, "Subtitling Descriptor\n");
			if(descr_len < 8)
			{
				mp_msg(MSGT_DEMUX, MSGL_DBG2, "Descriptor length too short for DVB Subtitle Descriptor: %d, SKIPPING\n", descr_len);
			}
			else
			{
				memcpy(es->lang, &ptr[j+2], 3);
				es->lang[3] = 0;
				subtype = ptr[j+5];
				if(
					(subtype >= 0x10 && subtype <= 0x13) ||
					(subtype >= 0x20 && subtype <= 0x23)
				)
				{
					es->type = SPU_DVB;
					//page parameters: compo page 2 bytes, ancillary page 2 bytes
				}
				else
					es->type = UNKNOWN;
			}
		}
		else if(ptr[j] == 0x50)	//Component Descriptor
		{
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "Component Descriptor\n");
			memcpy(es->lang, &ptr[j+5], 3);
			es->lang[3] = 0;
		}
		else if(ptr[j] == 0xa)	//Language Descriptor
		{
			memcpy(es->lang, &ptr[j+2], 3);
			es->lang[3] = 0;
			mp_msg(MSGT_DEMUX, MSGL_V, "Language Descriptor: %s\n", es->lang);
		}
		else if(ptr[j] == 0x5)	//Registration Descriptor (looks like e fourCC :) )
		{
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "Registration Descriptor\n");
			if(descr_len < 4)
			{
				mp_msg(MSGT_DEMUX, MSGL_DBG2, "Registration Descriptor length too short: %d, SKIPPING\n", descr_len);
			}
			else
			{
				char *d;
				memcpy(es->format_descriptor, &ptr[j+2], 4);
				es->format_descriptor[4] = 0;

				d = &ptr[j+2];
				if(d[0] == 'A' && d[1] == 'C' && d[2] == '-' && d[3] == '3')
				{
					es->type = AUDIO_A52;
				}
				else if(d[0] == 'D' && d[1] == 'T' && d[2] == 'S' && d[3] == '1')
				{
					es->type = AUDIO_DTS;
				}
				else if(d[0] == 'D' && d[1] == 'T' && d[2] == 'S' && d[3] == '2')
				{
					es->type = AUDIO_DTS;
				}
				else if(d[0] == 'V' && d[1] == 'C' && d[2] == '-' && d[3] == '1')
				{
					es->type = VIDEO_VC1;
				}
				else
					es->type = UNKNOWN;
				mp_msg(MSGT_DEMUX, MSGL_DBG2, "FORMAT %s\n", es->format_descriptor);
			}
		}
		else if(ptr[j] == 0x1e)
		{
			es->mp4_es_id = (ptr[j+2] << 8) | ptr[j+3];
			mp_msg(MSGT_DEMUX, MSGL_V, "SL Descriptor: ES_ID: %d(%x), pid: %d\n", es->mp4_es_id, es->mp4_es_id, es->pid);
		}
		else
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "Unknown descriptor 0x%x, SKIPPING\n", ptr[j]);

		len -= 2 + descr_len;
		j += 2 + descr_len;
	}

	return 1;
}

static int parse_sl_section(pmt_t *pmt, ts_section_t *section, int is_start, unsigned char *buff, int size)
{
	int tid, len, skip;
	uint8_t *ptr;
	skip = collect_section(section, is_start, buff, size);
	if(! skip)
		return 0;
		
	ptr = &(section->buffer[skip]);
	tid = ptr[0];
	len = ((ptr[1] & 0x0f) << 8) | ptr[2];
	mp_msg(MSGT_DEMUX, MSGL_V, "TABLEID: %d (av. %d), skip=%d, LEN: %d\n", tid, section->buffer_len, skip, len);
	if(len > 4093 || section->buffer_len < len || tid != 5)
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "SECTION TOO LARGE or wrong section type, EXIT\n");
		return 0;
	}
	
	if(! (ptr[5] & 1))
		return 0;
	
	//8 is the current position, len - 9 is the amount of data available
	parse_mp4_descriptors(pmt, &ptr[8], len - 9, NULL);
	
	return 1;
}

static int parse_pmt(ts_priv_t * priv, uint16_t progid, uint16_t pid, int is_start, unsigned char *buff, int size)
{
	unsigned char *base, *es_base;
	pmt_t *pmt;
	int32_t idx, es_count, section_bytes;
	uint8_t m=0;
	int skip;
	pmt_t *tmp;
	struct pmt_es_t *tmp_es;
	ts_section_t *section;
	ES_stream_t *tss;
	
	idx = progid_idx_in_pmt(priv, progid);

	if(idx == -1)
	{
		int sz = (priv->pmt_cnt + 1) * sizeof(pmt_t);
		tmp = realloc_struct(priv->pmt, priv->pmt_cnt + 1, sizeof(pmt_t));
		if(tmp == NULL)
		{
			mp_msg(MSGT_DEMUX, MSGL_ERR, "PARSE_PMT: COULDN'T REALLOC %d bytes, NEXT\n", sz);
			return 0;
		}
		priv->pmt = tmp;
		idx = priv->pmt_cnt;
		memset(&(priv->pmt[idx]), 0, sizeof(pmt_t));
		priv->pmt_cnt++;
		priv->pmt[idx].progid = progid;
	}

	pmt = &(priv->pmt[idx]);

	section = &(pmt->section);
	skip = collect_section(section, is_start, buff, size);
	if(! skip)
		return 0;
		
	base = &(section->buffer[skip]);

	mp_msg(MSGT_DEMUX, MSGL_V, "FILL_PMT(prog=%d), PMT_len: %d, IS_START: %d, TS_PID: %d, SIZE=%d, M=%d, ES_CNT=%d, IDX=%d, PMT_PTR=%p\n",
		progid, pmt->section.buffer_len, is_start, pid, size, m, pmt->es_cnt, idx, pmt);

	pmt->table_id = base[0];
	if(pmt->table_id != 2)
		return -1;
	pmt->ssi = base[1] & 0x80;
	pmt->section_length = (((base[1] & 0xf) << 8 ) | base[2]);
	pmt->version_number = (base[5] >> 1) & 0x1f;
	pmt->curr_next = (base[5] & 1);
	pmt->section_number = base[6];
	pmt->last_section_number = base[7];
	pmt->PCR_PID = ((base[8] & 0x1f) << 8 ) | base[9];
	pmt->prog_descr_length = ((base[10] & 0xf) << 8 ) | base[11];
	if(pmt->prog_descr_length > pmt->section_length - 9)
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "PARSE_PMT, INVALID PROG_DESCR LENGTH (%d vs %d)\n", pmt->prog_descr_length, pmt->section_length - 9);
		return -1;
	}

	if(pmt->prog_descr_length)
		parse_program_descriptors(pmt, &base[12], pmt->prog_descr_length);

	es_base = &base[12 + pmt->prog_descr_length];	//the beginning of th ES loop

	section_bytes= pmt->section_length - 13 - pmt->prog_descr_length;
	es_count  = 0;

	while(section_bytes >= 5)
	{
		int es_pid, es_type;

		es_type = es_base[0];
		es_pid = ((es_base[1] & 0x1f) << 8) | es_base[2];

		idx = es_pid_in_pmt(pmt, es_pid);
		if(idx == -1)
		{
			int sz = sizeof(struct pmt_es_t) * (pmt->es_cnt + 1);
			tmp_es = realloc_struct(pmt->es, pmt->es_cnt + 1, sizeof(struct pmt_es_t));
			if(tmp_es == NULL)
			{
				mp_msg(MSGT_DEMUX, MSGL_ERR, "PARSE_PMT, COULDN'T ALLOCATE %d bytes for PMT_ES\n", sz);
				continue;
			}
			pmt->es = tmp_es;
			idx = pmt->es_cnt;
			memset(&(pmt->es[idx]), 0, sizeof(struct pmt_es_t));
			pmt->es_cnt++;
		}

		pmt->es[idx].descr_length = ((es_base[3] & 0xf) << 8) | es_base[4];


		if(pmt->es[idx].descr_length > section_bytes - 5)
		{
			mp_msg(MSGT_DEMUX, MSGL_V, "PARSE_PMT, ES_DESCR_LENGTH TOO LARGE %d > %d, EXIT\n",
				pmt->es[idx].descr_length, section_bytes - 5);
			return -1;
		}


		pmt->es[idx].pid = es_pid;
		if(es_type != 0x6)
			pmt->es[idx].type = UNKNOWN;
		else
			pmt->es[idx].type = es_type;
		
		parse_descriptors(&pmt->es[idx], &es_base[5]);

		switch(es_type)
		{
			case 1:
				pmt->es[idx].type = VIDEO_MPEG1;
				break;
			case 2:
				pmt->es[idx].type = VIDEO_MPEG2;
				break;
			case 3:
			case 4:
				pmt->es[idx].type = AUDIO_MP2;
				break;
			case 6:
				if(pmt->es[idx].type == 0x6)	//this could have been ovrwritten by parse_descriptors
					pmt->es[idx].type = UNKNOWN;
				break;
			case 0x10:
				pmt->es[idx].type = VIDEO_MPEG4;
				break;
			case 0x0f:
			case 0x11:
				pmt->es[idx].type = AUDIO_AAC;
				break;
			case 0x1b:
				pmt->es[idx].type = VIDEO_H264;
				break;
			case 0x12:
				pmt->es[idx].type = SL_PES_STREAM;
				break;
			case 0x13:
				pmt->es[idx].type = SL_SECTION;
				break;
			case 0x81:
				pmt->es[idx].type = AUDIO_A52;
				break;
			case 0x8A:
			case 0x82:
			case 0x86:
				pmt->es[idx].type = AUDIO_DTS;
				break;
			case 0xEA:
				pmt->es[idx].type = VIDEO_VC1;
				break;
			default:
				mp_msg(MSGT_DEMUX, MSGL_DBG2, "UNKNOWN ES TYPE=0x%x\n", es_type);
				pmt->es[idx].type = UNKNOWN;
		}
		
		tss = priv->ts.pids[es_pid];			//an ES stream
		if(tss == NULL)
		{
			tss = new_pid(priv, es_pid);
			if(tss)
				tss->type = pmt->es[idx].type;
		}

		section_bytes -= 5 + pmt->es[idx].descr_length;
		mp_msg(MSGT_DEMUX, MSGL_V, "PARSE_PMT(%d INDEX %d), STREAM: %d, FOUND pid=0x%x (%d), type=0x%x, ES_DESCR_LENGTH: %d, bytes left: %d\n",
			progid, idx, es_count, pmt->es[idx].pid, pmt->es[idx].pid, pmt->es[idx].type, pmt->es[idx].descr_length, section_bytes);


		es_base += 5 + pmt->es[idx].descr_length;

		es_count++;
	}

	mp_msg(MSGT_DEMUX, MSGL_V, "----------------------------\n");
	return 1;
}

static pmt_t* pmt_of_pid(ts_priv_t *priv, int pid, mp4_decoder_config_t **mp4_dec)
{
	int32_t i, j, k;

	if(priv->pmt)
	{
		for(i = 0; i < priv->pmt_cnt; i++)
		{
			if(priv->pmt[i].es && priv->pmt[i].es_cnt)
			{
				for(j = 0; j < priv->pmt[i].es_cnt; j++)
				{
					if(priv->pmt[i].es[j].pid == pid)
					{
						//search mp4_es_id
						if(priv->pmt[i].es[j].mp4_es_id)
						{
							for(k = 0; k < priv->pmt[i].mp4es_cnt; k++)
							{
								if(priv->pmt[i].mp4es[k].id == priv->pmt[i].es[j].mp4_es_id)
								{
									*mp4_dec = &(priv->pmt[i].mp4es[k].decoder);
									break;
								}
							}
						}
						
						return &(priv->pmt[i]);
					}
				}
			}
		}
	}
	
	return NULL;
}


static inline int32_t pid_type_from_pmt(ts_priv_t *priv, int pid)
{
	int32_t pmt_idx, pid_idx, i, j;

	pmt_idx = progid_idx_in_pmt(priv, priv->prog);

	if(pmt_idx != -1)
	{
		pid_idx = es_pid_in_pmt(&(priv->pmt[pmt_idx]), pid);
		if(pid_idx != -1)
			return priv->pmt[pmt_idx].es[pid_idx].type;
	}
	//else
	//{
		for(i = 0; i < priv->pmt_cnt; i++)
		{
			pmt_t *pmt = &(priv->pmt[i]);
			for(j = 0; j < pmt->es_cnt; j++)
				if(pmt->es[j].pid == pid)
					return pmt->es[j].type;
		}
	//}

	return UNKNOWN;
}


static inline uint8_t *pid_lang_from_pmt(ts_priv_t *priv, int pid)
{
	int32_t pmt_idx, pid_idx, i, j;

	pmt_idx = progid_idx_in_pmt(priv, priv->prog);

	if(pmt_idx != -1)
	{
		pid_idx = es_pid_in_pmt(&(priv->pmt[pmt_idx]), pid);
		if(pid_idx != -1)
			return priv->pmt[pmt_idx].es[pid_idx].lang;
	}
	else
	{
		for(i = 0; i < priv->pmt_cnt; i++)
		{
			pmt_t *pmt = &(priv->pmt[i]);
			for(j = 0; j < pmt->es_cnt; j++)
				if(pmt->es[j].pid == pid)
					return pmt->es[j].lang;
		}
	}

	return NULL;
}


static int fill_packet(demuxer_t *demuxer, demux_stream_t *ds, demux_packet_t **dp, int *dp_offset, TS_stream_info *si)
{
	int ret = 0;

	if((*dp != NULL) && (*dp_offset > 0))
	{
		ret = *dp_offset;
		resize_demux_packet(*dp, ret);	//shrinked to the right size
		ds_add_packet(ds, *dp);
		mp_msg(MSGT_DEMUX, MSGL_DBG2, "ADDED %d  bytes to %s fifo, PTS=%.3f\n", ret, (ds == demuxer->audio ? "audio" : (ds == demuxer->video ? "video" : "sub")), (*dp)->pts);
		if(si)
		{
			float diff = (*dp)->pts - si->last_pts;
			float dur;

			if(abs(diff) > 1) //1 second, there's a discontinuity
			{
				si->duration += si->last_pts - si->first_pts;
				si->first_pts = si->last_pts = (*dp)->pts;
			}
			else
			{
				si->last_pts = (*dp)->pts;
			}
			si->size += ret;
			dur = si->duration + (si->last_pts - si->first_pts);

			if(dur > 0 && ds == demuxer->video)
			{
				ts_priv_t * priv = (ts_priv_t*) demuxer->priv;
				if(dur > 1)	//otherwise it may be unreliable
					priv->vbitrate = (uint32_t) ((float) si->size / dur);
			}
		}
	}

	*dp = NULL;
	*dp_offset = 0;

	return ret;
}

static int fill_extradata(mp4_decoder_config_t * mp4_dec, ES_stream_t *tss)
{
	uint8_t *tmp;
	
	mp_msg(MSGT_DEMUX, MSGL_DBG2, "MP4_dec: %p, pid: %d\n", mp4_dec, tss->pid);
		
	if(mp4_dec->buf_size > tss->extradata_alloc)
	{
		tmp = (uint8_t *) realloc(tss->extradata, mp4_dec->buf_size);
		if(!tmp)
			return 0;
		tss->extradata = tmp;
		tss->extradata_alloc = mp4_dec->buf_size;
	}
	memcpy(tss->extradata, mp4_dec->buf, mp4_dec->buf_size);
	tss->extradata_len = mp4_dec->buf_size;
	mp_msg(MSGT_DEMUX, MSGL_V, "EXTRADATA: %p, alloc=%d, len=%d\n", tss->extradata, tss->extradata_alloc, tss->extradata_len);
	
	return tss->extradata_len;
}

// 0 = EOF or no stream found
// else = [-] number of bytes written to the packet
static int ts_parse(demuxer_t *demuxer , ES_stream_t *es, unsigned char *packet, int probe)
{
	ES_stream_t *tss;
	uint8_t done = 0;
	int buf_size, is_start, pid, base;
	int len, cc, cc_ok, afc, retv = 0, is_video, is_audio, is_sub;
	ts_priv_t * priv = (ts_priv_t*) demuxer->priv;
	stream_t *stream = demuxer->stream;
	char *p;
	demux_stream_t *ds = NULL;
	demux_packet_t **dp = NULL;
	int *dp_offset = 0, *buffer_size = 0;
	int32_t progid, pid_type, bad, ts_error;
	int junk = 0, rap_flag = 0;
	pmt_t *pmt;
	mp4_decoder_config_t *mp4_dec;
	TS_stream_info *si;


	while(! done)
	{
		bad = ts_error = 0;
		ds = (demux_stream_t*) NULL;
		dp = (demux_packet_t **) NULL;
		dp_offset = buffer_size = NULL;
		rap_flag = 0;
		mp4_dec = NULL;
		es->is_synced = 0;
		si = NULL;

		junk = priv->ts.packet_size - TS_PACKET_SIZE;
		buf_size = priv->ts.packet_size - junk;

		if(stream_eof(stream))
		{
			if(! probe)
			{
				ts_dump_streams(priv);
				demuxer->filepos = stream_tell(demuxer->stream);
			}

			return 0;
		}


		if(! ts_sync(stream))
		{
			mp_msg(MSGT_DEMUX, MSGL_INFO, "TS_PARSE: COULDN'T SYNC\n");
			return 0;
		}

		len = stream_read(stream, &packet[1], 3);
		if (len != 3)
			return 0;
		buf_size -= 4;

		if((packet[1]  >> 7) & 0x01)	//transport error
			ts_error = 1;


		is_start = packet[1] & 0x40;
		pid = ((packet[1] & 0x1f) << 8) | packet[2];

		tss = priv->ts.pids[pid];			//an ES stream
		if(tss == NULL)
		{
			tss = new_pid(priv, pid);
			if(tss == NULL)
				continue;
		}

		cc = (packet[3] & 0xf);
		cc_ok = (tss->last_cc < 0) || ((((tss->last_cc + 1) & 0x0f) == cc));
		tss->last_cc = cc;
		    
		bad = ts_error; // || (! cc_ok);
		if(bad)
		{
			if(priv->keep_broken == 0)
			{
				stream_skip(stream, buf_size-1+junk);
				continue;
			}
			
			is_start = 0;	//queued to the packet data
		}

		if(is_start)
			tss->is_synced = 1;

		if((!is_start && !tss->is_synced) || ((pid > 1) && (pid < 16)) || (pid == 8191))		//invalid pid
		{
			stream_skip(stream, buf_size-1+junk);
			continue;
		}

    
		afc = (packet[3] >> 4) & 3;
		if(! (afc % 2))	//no payload in this TS packet
		{
			stream_skip(stream, buf_size-1+junk);
			continue;
		}
		
		if(afc > 1)
		{
			int c;
			c = stream_read_char(stream);
			buf_size--;
			if(c < 0 || c > 183)	//broken from the stream layer or invalid
			{
				stream_skip(stream, buf_size-1+junk);
				continue;
			}
			
			//c==0 is allowed!
			if(c > 0)
			{
				uint8_t pcrbuf[188];
				int flags = stream_read_char(stream);
				int has_pcr;
				rap_flag = (flags & 0x40) >> 6;
				has_pcr = flags & 0x10;
				
				buf_size--;
				c--;
				stream_read(stream, pcrbuf, c);

				if(has_pcr)
				{
					int pcr_pid = prog_pcr_pid(priv, priv->prog);
					if(pcr_pid == pid)
					{
						uint64_t pcr, pcr_ext;
	
						pcr  = (int64_t)(pcrbuf[0]) << 25;
						pcr |=  pcrbuf[1]         << 17 ;
						pcr |= (pcrbuf[2]) << 9;
						pcr |=  pcrbuf[3]  <<  1 ;
						pcr |= (pcrbuf[4] & 0x80) >>  7;
	
						pcr_ext = (pcrbuf[4] & 0x01) << 8;
						pcr_ext |= pcrbuf[5];
	
						pcr = pcr * 300 + pcr_ext;
						
						demuxer->reference_clock = (double)pcr/(double)27000000.0;
					}
				}
				
				buf_size -= c;
				if(buf_size == 0)
					continue;
			}
		}

		//find the program that the pid belongs to; if (it's the right one or -1) && pid_type==SL_SECTION
		//call parse_sl_section()
		pmt = pmt_of_pid(priv, pid, &mp4_dec);
		if(mp4_dec)
		{
			fill_extradata(mp4_dec, tss);
			if(IS_VIDEO(mp4_dec->object_type) || IS_AUDIO(mp4_dec->object_type))
			{
				tss->type = SL_PES_STREAM;
				tss->subtype = mp4_dec->object_type;
			}
		}
		
		
		//TABLE PARSING

		base = priv->ts.packet_size - buf_size;

		priv->last_pid = pid;

		is_video = IS_VIDEO(tss->type) || (tss->type==SL_PES_STREAM && IS_VIDEO(tss->subtype));
		is_audio = IS_AUDIO(tss->type) || (tss->type==SL_PES_STREAM && IS_AUDIO(tss->subtype)) || (tss->type == PES_PRIVATE1);
		is_sub	= ((tss->type == SPU_DVD) || (tss->type == SPU_DVB));
		pid_type = pid_type_from_pmt(priv, pid);

			// PES CONTENT STARTS HERE
		if(! probe)
		{
			if((is_video || is_audio) && is_start && !priv->ts.streams[pid].sh)
				ts_add_stream(demuxer, tss);

			if((pid == demuxer->sub->id))	//or the lang is right
			{
				pid_type = SPU_DVD;
			}

			if(is_video && (demuxer->video->id == priv->ts.streams[pid].id))
			{
				ds = demuxer->video;

				dp = &priv->fifo[1].pack;
				dp_offset = &priv->fifo[1].offset;
				buffer_size = &priv->fifo[1].buffer_size;
				si = &priv->vstr;
			}
			else if(is_audio && (demuxer->audio->id == priv->ts.streams[pid].id))
			{
				ds = demuxer->audio;

				dp = &priv->fifo[0].pack;
				dp_offset = &priv->fifo[0].offset;
				buffer_size = &priv->fifo[0].buffer_size;
				si = &priv->astr;
			}
			else if(is_sub
				|| (pid_type == SPU_DVD) || (pid_type == SPU_DVB))
			{
				//SUBS are infrequent, so the initial detection may fail
				// and we may need to add them at play-time
				if(demuxer->sub->id == -1)
				{
					uint16_t p;
					p = progid_for_pid(priv, tss->pid, priv->prog);

					if(p == priv->prog)
					{
						int asgn = 0;
						uint8_t *lang;

						if(dvdsub_lang)
						{
							if ((lang = pid_lang_from_pmt(priv, pid)))
								asgn = (strncmp(lang, dvdsub_lang, 3) == 0);
						}
						else		//no language specified with -slang
							asgn = 1;

						if(asgn)
						{
							demuxer->sub->id = tss->pid;
							mp_msg(MSGT_DEMUX, MSGL_INFO, "CHOSEN SUBs pid 0x%x (%d) FROM PROG %d\n", tss->pid, tss->pid, priv->prog);
						}
					}
				}

				if(demuxer->sub->id == tss->pid)
				{
					ds = demuxer->sub;

					dp = &priv->fifo[2].pack;
					dp_offset = &priv->fifo[2].offset;
					buffer_size = &priv->fifo[2].buffer_size;
				}
				else
				{
					stream_skip(stream, buf_size+junk);
					continue;
				}
			}

			//IS IT TIME TO QUEUE DATA to the dp_packet?
			if(is_start && (dp != NULL))
			{
				retv = fill_packet(demuxer, ds, dp, dp_offset, si);
			}


			if(dp && *dp == NULL)
			{
				if(*buffer_size > MAX_PACK_BYTES)
					*buffer_size = MAX_PACK_BYTES;
				*dp = new_demux_packet(*buffer_size);	//es->size
				*dp_offset = 0;
				if(! *dp)
				{
					fprintf(stderr, "fill_buffer, NEW_ADD_PACKET(%d)FAILED\n", *buffer_size);
					continue;
				}
				mp_msg(MSGT_DEMUX, MSGL_DBG2, "CREATED DP(%d)\n", *buffer_size);
			}
		}


		if(probe || !dp)	//dp is NULL for tables and sections
		{
			p = &packet[base];
		}
		else	//feeding
		{
			if(*dp_offset + buf_size > *buffer_size)
			{
				*buffer_size = *dp_offset + buf_size + TS_FEC_PACKET_SIZE;
				resize_demux_packet(*dp, *buffer_size);
			}
			p = &((*dp)->buffer[*dp_offset]);
		}

		len = stream_read(stream, p, buf_size);
		if(len < buf_size)
		{
			mp_msg(MSGT_DEMUX, MSGL_DBG2,  "\r\nts_parse() couldn't read enough data: %d < %d\r\n", len, buf_size);
			continue;
		}
		stream_skip(stream, junk);

		if(pid  == 0)
		{
			parse_pat(priv, is_start, p, buf_size);
			continue;
		}
		else if((tss->type == SL_SECTION) && pmt)
		{
			int k, mp4_es_id = -1;
			ts_section_t *section;
			for(k = 0; k < pmt->mp4es_cnt; k++)
			{
				if(pmt->mp4es[k].decoder.object_type == MP4_OD && pmt->mp4es[k].decoder.stream_type == MP4_OD)
					mp4_es_id = pmt->mp4es[k].id;
			}
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "MP4ESID: %d\n", mp4_es_id);
			for(k = 0; k < pmt->es_cnt; k++)
			{
				if(pmt->es[k].mp4_es_id == mp4_es_id)
				{
					section = &(tss->section);
					parse_sl_section(pmt, section, is_start, &packet[base], buf_size);
				}
			}
			continue;
		}
		else
		{
			progid = prog_id_in_pat(priv, pid);
			if(progid != -1)
			{
				if(pid != demuxer->video->id && pid != demuxer->audio->id && pid != demuxer->sub->id)
				{
					parse_pmt(priv, progid, pid, is_start, &packet[base], buf_size);
					continue;
				}
				else
					mp_msg(MSGT_DEMUX, MSGL_ERR, "Argh! Data pid %d used in the PMT, Skipping PMT parsing!\n", pid);
			}
		}

		if(!probe && !dp)
			continue;

		if(is_start)
		{
			uint8_t *lang = NULL;

			mp_msg(MSGT_DEMUX, MSGL_DBG2, "IS_START\n");

			len = pes_parse2(p, buf_size, es, pid_type, pmt, pid);
			if(! len)
			{
				tss->is_synced = 0;
				continue;
			}
			es->pid = tss->pid;
			tss->is_synced |= es->is_synced || rap_flag;
			tss->payload_size = es->payload_size;

			if(is_audio && (lang = pid_lang_from_pmt(priv, es->pid)))
			{
				memcpy(es->lang, lang, 3);
				es->lang[3] = 0;
			}
			else
				es->lang[0] = 0;
			
			if(probe)
			{
				if(es->type == UNKNOWN)
					return 0;
				
				tss->type = es->type;
				tss->subtype = es->subtype;
				
				return 1;
			}
			else
			{
				if(es->pts == 0.0f)
					es->pts = tss->pts = tss->last_pts;
				else
					tss->pts = tss->last_pts = es->pts;

				mp_msg(MSGT_DEMUX, MSGL_DBG2, "ts_parse, NEW pid=%d, PSIZE: %u, type=%X, start=%p, len=%d\n",
					es->pid, es->payload_size, es->type, es->start, es->size);

				demuxer->filepos = stream_tell(demuxer->stream) - es->size;

				memmove(p, es->start, es->size);
				*dp_offset += es->size;
				(*dp)->flags = 0;
				(*dp)->pos = stream_tell(demuxer->stream);
				(*dp)->pts = es->pts;

				if(retv > 0)
					return retv;
				else
					continue;
			}
		}
		else
		{
			uint16_t sz;

			es->pid = tss->pid;
			es->type = tss->type;
			es->subtype = tss->subtype;
			es->pts = tss->pts = tss->last_pts;
			es->start = &packet[base];


			if(tss->payload_size > 0)
			{
				sz = FFMIN(tss->payload_size, buf_size);
				tss->payload_size -= sz;
				es->size = sz;
			}
			else
			{
				if(is_video)
				{
					sz = es->size = buf_size;
				}
				else
				{
					continue;
				}
			}


			if(! probe)
			{
				*dp_offset += sz;

				if(*dp_offset >= MAX_PACK_BYTES)
				{
					(*dp)->pts = tss->last_pts;
					retv = fill_packet(demuxer, ds, dp, dp_offset, si);
					return 1;
				}

				continue;
			}
			else
			{
				memcpy(es->start, p, sz);

				if(es->size)
					return es->size;
				else
					continue;
			}
		}
	}

	return 0;
}


void skip_audio_frame(sh_audio_t *sh_audio);

static void reset_fifos(demuxer_t *demuxer, int a, int v, int s)
{
	ts_priv_t* priv = demuxer->priv;
	if(a)
	{
		if(priv->fifo[0].pack != NULL)
		{
			free_demux_packet(priv->fifo[0].pack);
			priv->fifo[0].pack = NULL;
		}
		priv->fifo[0].offset = 0;
	}

	if(v)
	{
		if(priv->fifo[1].pack != NULL)
		{
			free_demux_packet(priv->fifo[1].pack);
			priv->fifo[1].pack = NULL;
		}
		priv->fifo[1].offset = 0;
	}

	if(s)
	{
		if(priv->fifo[2].pack != NULL)
		{
			free_demux_packet(priv->fifo[2].pack);
			priv->fifo[2].pack = NULL;
		}
		priv->fifo[2].offset = 0;
	}
	demuxer->reference_clock = MP_NOPTS_VALUE;
}


static void demux_seek_ts(demuxer_t *demuxer, float rel_seek_secs, float audio_delay, int flags)
{
	demux_stream_t *d_audio=demuxer->audio;
	demux_stream_t *d_video=demuxer->video;
	sh_audio_t *sh_audio=d_audio->sh;
	sh_video_t *sh_video=d_video->sh;
	ts_priv_t * priv = (ts_priv_t*) demuxer->priv;
	int i, video_stats;
	off_t newpos;

	//================= seek in MPEG-TS ==========================

	ts_dump_streams(demuxer->priv);
	reset_fifos(demuxer, sh_audio != NULL, sh_video != NULL, demuxer->sub->id > 0);

	demux_flush(demuxer);



	video_stats = (sh_video != NULL);
	if(video_stats)
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "IBPS: %d, vb: %d\r\n", sh_video->i_bps, priv->vbitrate);
		if(priv->vbitrate)
			video_stats = priv->vbitrate;
		else
			video_stats = sh_video->i_bps;
	}

	newpos = (flags & SEEK_ABSOLUTE) ? demuxer->movi_start : demuxer->filepos;
	if(flags & SEEK_FACTOR) // float seek 0..1
		newpos+=(demuxer->movi_end-demuxer->movi_start)*rel_seek_secs;
	else
	{
		// time seek (secs)
		if(! video_stats) // unspecified or VBR
			newpos += 2324*75*rel_seek_secs; // 174.3 kbyte/sec
		else
			newpos += video_stats*rel_seek_secs;
	}


	if(newpos < demuxer->movi_start)
  		newpos = demuxer->movi_start;	//begininng of stream

	stream_seek(demuxer->stream, newpos);
	for(i = 0; i < 8192; i++)
		if(priv->ts.pids[i] != NULL)
			priv->ts.pids[i]->is_synced = 0;

	videobuf_code_len = 0;

	if(sh_video != NULL)
		ds_fill_buffer(d_video);

	if(sh_audio != NULL)
	{
		ds_fill_buffer(d_audio);
	}

	while(sh_video != NULL)
	{
		if(sh_audio && !d_audio->eof && d_video->pts && d_audio->pts)
		{
			float a_pts=d_audio->pts;
			a_pts+=(ds_tell_pts(d_audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
			if(d_video->pts > a_pts)
			{
				skip_audio_frame(sh_audio);  // sync audio
				continue;
			}
		}


		i = sync_video_packet(d_video);
		if((sh_video->format == VIDEO_MPEG1) || (sh_video->format == VIDEO_MPEG2))
		{
			if(i==0x1B3 || i==0x1B8) break; // found it!
		}
		else if((sh_video->format == VIDEO_MPEG4) && (i==0x1B6))
			break;
		else if(sh_video->format == VIDEO_VC1 && (i==0x10E || i==0x10F))
			break;
		else	//H264
		{
			if((i & ~0x60) == 0x105 || (i & ~0x60) == 0x107) break;
		}

		if(!i || !skip_video_packet(d_video)) break; // EOF?
	}
}


static int demux_ts_fill_buffer(demuxer_t * demuxer, demux_stream_t *ds)
{
	ES_stream_t es;
	ts_priv_t *priv = (ts_priv_t *)demuxer->priv;
	
	return -ts_parse(demuxer, &es, priv->packet, 0);
}


static int ts_check_file_dmx(demuxer_t *demuxer)
{
    return ts_check_file(demuxer) ? DEMUXER_TYPE_MPEG_TS : 0;
}

static int is_usable_program(ts_priv_t *priv, pmt_t *pmt)
{
	int j;

	for(j = 0; j < pmt->es_cnt; j++)
	{
		if(priv->ts.pids[pmt->es[j].pid] == NULL || priv->ts.streams[pmt->es[j].pid].sh == NULL)
			continue;
		if(
			priv->ts.streams[pmt->es[j].pid].type == TYPE_VIDEO ||
			priv->ts.streams[pmt->es[j].pid].type == TYPE_AUDIO
		)
			return 1;
	}

	return 0;
}

static int demux_ts_control(demuxer_t *demuxer, int cmd, void *arg)
{
	ts_priv_t* priv = (ts_priv_t *)demuxer->priv;

	switch(cmd)
	{
		case DEMUXER_CTRL_SWITCH_AUDIO:
		case DEMUXER_CTRL_SWITCH_VIDEO:
		{
			void *sh = NULL;
			int i, n;
			int reftype, areset = 0, vreset = 0;
			demux_stream_t *ds;
			
			if(cmd == DEMUXER_CTRL_SWITCH_VIDEO)
			{
				reftype = TYPE_VIDEO;
				ds = demuxer->video;
				vreset  = 1;
			}
			else
			{
				reftype = TYPE_AUDIO;
				ds = demuxer->audio;
				areset = 1;
			}
			n = *((int*)arg);
			if(n == -2)
			{
				reset_fifos(demuxer, areset, vreset, 0);
				ds->id = -2;
				ds->sh = NULL;
				ds_free_packs(ds);
				*((int*)arg) = ds->id;
				return DEMUXER_CTRL_OK;
			}

			if(n < 0)
			{
				for(i = 0; i < 8192; i++)
				{
					if(priv->ts.streams[i].id == ds->id && priv->ts.streams[i].type == reftype)
						break;
				}

				while(!sh)
				{
					i = (i+1) % 8192;
					if(priv->ts.streams[i].type == reftype)
					{
						if(priv->ts.streams[i].id == ds->id)	//we made a complete loop
							break;
						sh = priv->ts.streams[i].sh;
					}
				}
			}
			else	//audio track <n>
			{
				for(i = 0; i < 8192; i++)
				{
					if(priv->ts.streams[i].id == n && priv->ts.streams[i].type == reftype)
					{
						sh = priv->ts.streams[i].sh;
						break;
					}
				}
			}

			if(sh)
			{
				if(ds->id != priv->ts.streams[i].id)
					reset_fifos(demuxer, areset, vreset, 0);
				ds->id = priv->ts.streams[i].id;
				ds->sh = sh;
				ds_free_packs(ds);
				mp_msg(MSGT_DEMUX, MSGL_V, "\r\ndemux_ts, switched to audio pid %d, id: %d, sh: %p\r\n", i, ds->id, sh);
			}

			*((int*)arg) = ds->id;
			return DEMUXER_CTRL_OK;
		}

		case DEMUXER_CTRL_IDENTIFY_PROGRAM:		//returns in prog->{aid,vid} the new ids that comprise a program
		{
			int i, j, cnt=0;
			int vid_done=0, aid_done=0;
			pmt_t *pmt = NULL;
			demux_program_t *prog = arg;

			if(priv->pmt_cnt < 2)
				return DEMUXER_CTRL_NOTIMPL;

			if(prog->progid == -1)
			{
				int cur_pmt_idx = 0;

				for(i = 0; i < priv->pmt_cnt; i++)
					if(priv->pmt[i].progid == priv->prog)
					{
						cur_pmt_idx = i;
						break;
					}

				i = (cur_pmt_idx + 1) % priv->pmt_cnt;
				while(i != cur_pmt_idx)
				{
					pmt = &priv->pmt[i];
					cnt = is_usable_program(priv, pmt);
					if(cnt)
						break;
					i = (i + 1) % priv->pmt_cnt;
				}
			}
			else
			{
				for(i = 0; i < priv->pmt_cnt; i++)
					if(priv->pmt[i].progid == prog->progid)
					{
						pmt = &priv->pmt[i]; //required program
						cnt = is_usable_program(priv, pmt);
					}
			}

			if(!cnt)
				return DEMUXER_CTRL_NOTIMPL;

			//finally some food
			prog->aid = prog->vid = -2;	//no audio and no video by default
			for(j = 0; j < pmt->es_cnt; j++)
			{
				if(priv->ts.pids[pmt->es[j].pid] == NULL || priv->ts.streams[pmt->es[j].pid].sh == NULL)
					continue;

				if(!vid_done && priv->ts.streams[pmt->es[j].pid].type == TYPE_VIDEO)
				{
					vid_done = 1;
					prog->vid = priv->ts.streams[pmt->es[j].pid].id;
				}
				else if(!aid_done && priv->ts.streams[pmt->es[j].pid].type == TYPE_AUDIO)
				{
					aid_done = 1;
					prog->aid = priv->ts.streams[pmt->es[j].pid].id;
				}
			}

			priv->prog = prog->progid = pmt->progid;
			return DEMUXER_CTRL_OK;
		}

		default:
			return DEMUXER_CTRL_NOTIMPL;
	}
}


const demuxer_desc_t demuxer_desc_mpeg_ts = {
  "MPEG-TS demuxer",
  "mpegts",
  "TS",
  "Nico Sabbi",
  "",
  DEMUXER_TYPE_MPEG_TS,
  0, // unsafe autodetect
  ts_check_file_dmx,
  demux_ts_fill_buffer,
  demux_open_ts,
  demux_close_ts,
  demux_seek_ts,
  demux_ts_control
};
