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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "demuxer.h"
#include "parse_es.h"
#include "stheader.h"

#include "bswap.h"
#include "../unrarlib.h"


#define TS_FEC_PACKET_SIZE 204
#define TS_PACKET_SIZE 188
#define NB_PID_MAX 8192

#define MAX_HEADER_SIZE 6			/* enough for PES header + length */
#define MAX_CHECK_SIZE	65535
#define TS_MAX_PROBE_SIZE 2000000 /* dont forget to change this in cfg-common.h too */
#define NUM_CONSECUTIVE_TS_PACKETS 32
#define NUM_CONSECUTIVE_AUDIO_PACKETS 348


int ts_prog;
int ts_keep_broken=0;
off_t ts_probe = TS_MAX_PROBE_SIZE;
extern char *dvdsub_lang, *audio_lang;	//for -alang

typedef enum
{
	UNKNOWN		= -1,
	VIDEO_MPEG1 	= 0x10000001,
	VIDEO_MPEG2 	= 0x10000002,
	VIDEO_MPEG4 	= 0x10000004,
	AUDIO_MP2   	= 0x50,
	AUDIO_A52   	= 0x2000,
	AUDIO_LPCM_BE  	= 0x10001,
	AUDIO_AAC	= mmioFOURCC('M', 'P', '4', 'A'),
	SPU_DVD		= 0x3000000,
	SPU_DVB		= 0x3000001
} es_stream_type_t;


typedef struct {
	int size;
	unsigned char *start;
	uint16_t payload_size;
	es_stream_type_t type;
	float pts, last_pts;
	int pid;
	char lang[4];
	int last_cc;				// last cc code (-1 if first packet)
	uint64_t seen;
} ES_stream_t;


typedef struct MpegTSContext {
	int packet_size; 		// raw packet size, including FEC if present e.g. 188 bytes
	ES_stream_t *pids[NB_PID_MAX];
} MpegTSContext;


typedef struct {
	demux_stream_t *ds;
	demux_packet_t *pack;
	int offset, buffer_size;
	int broken; //set if it's the final part of a chunk (doesn't have a corresponding is_start)
} av_fifo_t;

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
	char buffer[65535];
	uint16_t buffer_len;
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
	char buffer[2048];
	uint16_t buffer_len;
	uint16_t es_cnt;
	struct pmt_es_t {
		uint16_t pid;
		uint32_t type;	//it's 8 bit long, but cast to the right type as FOURCC
		uint16_t descr_length;
		uint8_t format_descriptor[5];
		uint8_t lang[4];
	} *es;
} pmt_t;

typedef struct {
	MpegTSContext ts;
	int last_pid;
	av_fifo_t fifo[3];	//0 for audio, 1 for video, 2 for subs
	pat_t pat;
	pmt_t *pmt;
	uint16_t pmt_cnt;
	uint32_t prog;
	int keep_broken;
} ts_priv_t;


static int ts_parse(demuxer_t *demuxer, ES_stream_t *es, unsigned char *packet, int probe);
extern void resync_audio_stream( sh_audio_t *sh_audio );

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
		if (buf[i * TS_FEC_PACKET_SIZE] != 0x47)
		return 0;
	}
	return TS_FEC_PACKET_SIZE;
}



int ts_check_file(demuxer_t * demuxer)
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

	mp_msg(MSGT_DEMUX, MSGL_V, "TRIED UP TO POSITION %llu, FOUND %x, packet_size= %d, SEEMS A TS? %d\n", (uint64_t) pos, c, size, is_ts);
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
	int16_t prog;
	off_t probe;
} tsdemux_init_t;

static off_t ts_detect_streams(demuxer_t *demuxer, tsdemux_init_t *param)
{
	int video_found = 0, audio_found = 0, sub_found = 0, i, num_packets = 0, req_apid, req_vpid, req_spid;
	int is_audio, is_video, is_sub, has_tables;
	int32_t p, chosen_pid = 0;
	off_t pos=0, ret = 0, init_pos;
	ES_stream_t es;
	unsigned char tmp[TS_FEC_PACKET_SIZE];
	ts_priv_t *priv = (ts_priv_t*) demuxer->priv;

	priv->last_pid = 8192;		//invalid pid

	req_apid = param->apid;
	req_vpid = param->vpid;
	req_spid = param->spid;

	has_tables = 0;
	init_pos = stream_tell(demuxer->stream);
	mp_msg(MSGT_DEMUXER, MSGL_INFO, "PROBING UP TO %llu, PROG: %d\n", (uint64_t) param->probe, param->prog);
	while((pos <= init_pos + param->probe) && (! demuxer->stream->eof))
	{
		pos = stream_tell(demuxer->stream);
		if(ts_parse(demuxer, &es, tmp, 1))
		{
			is_audio = ((es.type == AUDIO_MP2) || (es.type == AUDIO_A52) || (es.type == AUDIO_LPCM_BE) || (es.type == AUDIO_AAC));
			is_video = ((es.type == VIDEO_MPEG1) || (es.type == VIDEO_MPEG2) || (es.type == VIDEO_MPEG4));
			is_sub   = ((es.type == SPU_DVD) || (es.type == SPU_DVB));


			if((! is_audio) && (! is_video) && (! is_sub))
				continue;

			if(is_video)
			{
    				chosen_pid = (req_vpid == es.pid);
				if((! chosen_pid) && (req_vpid > 0))
					continue;
			}
			else if(is_audio)
			{
				if(req_apid > 0)
				{
					chosen_pid = (req_apid == es.pid);
					if(! chosen_pid)
						continue;
				}
				else if(param->alang[0] > 0)
				{
					if(pid_match_lang(priv, es.pid, param->alang) == -1)
						continue;

					chosen_pid = 1;
					param->apid = req_apid = es.pid;
				}
			}
			else if(is_sub)
			{
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
				has_tables++;

			if((param->prog == 0) && (p != -1))
			{
				if(chosen_pid)
					param->prog = p;
			}

			if((param->prog > 0) && (param->prog != p))
			{
				if(audio_found)
				{
					if(is_video && (req_vpid == es.pid))
					{
						param->vtype = es.type;
						param->vpid = es.pid;
						video_found = 1;
						break;
					}
				}

				if(video_found)
				{
					if(is_audio && (req_apid == es.pid))
					{
						param->atype = es.type;
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
					param->vtype = es.type;
					param->vpid = es.pid;
					video_found = 1;
				}
			}


			if(((req_vpid == -2) || (num_packets >= NUM_CONSECUTIVE_AUDIO_PACKETS)) && audio_found)
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
					param->atype = es.type;
					param->apid = es.pid;
					audio_found = 1;
				}
			}

			if(audio_found && (param->apid == es.pid) && (! video_found))
				num_packets++;

			if((req_apid == -2) && video_found)
			{
				param->atype = 0;
				break;
			}

			if((has_tables==0) && (video_found && audio_found) && (pos >= 1000000))
				break;
		}
	}

	if(video_found)
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "VIDEO MPEG%d(pid=%d)...", (param->vtype == VIDEO_MPEG1 ? 1 : (param->vtype == VIDEO_MPEG2 ? 2 : 4)), param->vpid);
	else
	{
		video_found = 0;
		param->vtype = UNKNOWN;
		//WE DIDN'T MATCH ANY VIDEO STREAM
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "NO VIDEO! ");
	}

	if(param->atype == AUDIO_MP2)
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "AUDIO MP2(pid=%d)", param->apid);
	else if(param->atype == AUDIO_A52)
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "AUDIO A52(pid=%d)", param->apid);
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
		mp_msg(MSGT_DEMUXER, MSGL_INFO, " SUB DVx(pid=%d) ", param->spid);
	else
	{
		param->stype = UNKNOWN;
		mp_msg(MSGT_DEMUXER, MSGL_INFO, " NO SUBS (yet)! ");
	}

	if(video_found || audio_found)
	{
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
			priv->ts.pids[i]->seen = 0;
			priv->ts.pids[i]->last_cc = -1;
		}
	}

	return ret;
}


demuxer_t *demux_open_ts(demuxer_t * demuxer)
{
	int i;
	uint8_t packet_size;
	sh_video_t *sh_video;
	sh_audio_t *sh_audio;
	off_t start_pos;
	tsdemux_init_t params;
	ts_priv_t * priv = (ts_priv_t*) demuxer->priv;

	mp_msg(MSGT_DEMUX, MSGL_INFO, "DEMUX OPEN, AUDIO_ID: %d, VIDEO_ID: %d, SUBTITLE_ID: %d,\n",
		demuxer->audio->id, demuxer->video->id, demuxer->sub->id);


	demuxer->type= DEMUXER_TYPE_MPEG_TS;


	stream_reset(demuxer->stream);

	packet_size = ts_check_file(demuxer);
	if(!packet_size)
	    return NULL;

	priv = malloc(sizeof(ts_priv_t));
	if(priv == NULL)
	{
		mp_msg(MSGT_DEMUX, MSGL_FATAL, "DEMUX_OPEN_TS, couldn't allocate enough memory for ts->priv, exit\n");
		return NULL;
	}

	for(i=0; i < 8192; i++)
	    priv->ts.pids[i] = NULL;
	priv->pat.progs = NULL;
	priv->pat.progs_cnt = 0;

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

	demuxer->audio->id = params.apid;
	demuxer->video->id = params.vpid;
	demuxer->sub->id = params.spid;
	priv->prog = params.prog;


	if(params.vtype != UNKNOWN)
	{
		if(params.vtype == VIDEO_MPEG4)
			demuxer->file_format= DEMUXER_TYPE_MPEG4_IN_TS;

		sh_video = new_sh_video(demuxer, 0);
		sh_video->ds = demuxer->video;
		sh_video->format = params.vtype;
		demuxer->video->sh = sh_video;
	}

	if(params.atype != UNKNOWN)
	{
		sh_audio = new_sh_audio(demuxer, 0);
		sh_audio->ds = demuxer->audio;
		sh_audio->format = params.atype;
		demuxer->audio->sh = sh_audio;
	}


	mp_msg(MSGT_DEMUXER,MSGL_INFO, "Opened TS demuxer, audio: %x(pid %d), video: %x(pid %d)...POS=%llu\n", params.atype, demuxer->audio->id, params.vtype, demuxer->video->id, (uint64_t) start_pos);


	start_pos = (start_pos <= priv->ts.packet_size ? 0 : start_pos - priv->ts.packet_size);
	demuxer->movi_start = start_pos;
	stream_reset(demuxer->stream);
	stream_seek(demuxer->stream, start_pos);	//IF IT'S FROM A PIPE IT WILL FAIL, BUT WHO CARES?


	priv->last_pid = 8192;		//invalid pid

	for(i = 0; i < 3; i++)
	{
		priv->fifo[i].pack  = NULL;
		priv->fifo[i].offset = 0;
		priv->fifo[i].broken = 1;
	}
	priv->fifo[0].ds = demuxer->audio;
	priv->fifo[1].ds = demuxer->video;
	priv->fifo[2].ds = demuxer->sub;

	priv->fifo[0].buffer_size = 1536;
	priv->fifo[1].buffer_size = 32767;
	priv->fifo[2].buffer_size = 32767;

	priv->pat.buffer_len = 0;

	demuxer->filepos = stream_tell(demuxer->stream);
	return demuxer;
}

void demux_close_ts(demuxer_t * demuxer)
{
	if(demuxer->priv)
	{
		free(demuxer->priv);
		demuxer->priv=NULL;
	}
}

static int pes_parse2(unsigned char *buf, uint16_t packet_len, ES_stream_t *es, int32_t type_from_pmt)
{
	unsigned char  *p;
	uint32_t       header_len;
	int64_t        pts;
	uint32_t       stream_id;
	uint32_t       pkt_len;

	//THE FOLLOWING CODE might be needed in the future:
	//uint8_t        es_rate_flag, escr_flag, pts_flag;
	//int64_t      escr, dts;
	//uint32_t es_rate;

	//Here we are always at the start of a PES packet
	mp_msg(MSGT_DEMUX, MSGL_DBG2, "pes_parse2(%p, %d): \n", buf, (uint32_t) packet_len);

	if(packet_len == 0)
	{
		mp_msg(MSGT_DEMUX, MSGL_DBG2, "pes_parse2(,PACKET_LEN = 0, EXIT\n");
		return 0;
	}

	if(packet_len > 184)
	{
		mp_msg(MSGT_DEMUX, MSGL_DBG2, "pes_parse2, BUFFER LEN IS TOO BIG: %d, EXIT\n", packet_len);
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

	/*
	CODE TO CALCULATE ES_RATE AND ESCR, ACTUALLY UNUSED BUT POSSIBLY NEEDED IN THE FUTURE

	pts_flag = ((p[7] & 0xc0) >> 6) & 3;
	escr_flag = p[7] & 0x20;
	es_rate_flag = p[7] & 0x10;
	mp_msg(MSGT_DEMUX, MSGL_V, "pes_parse: ES_RATE_FLAG=%d, ESCR_FLAG=%d, PTS_FLAG=%d, byte=%02X\n", es_rate_flag, escr_flag, pts_flag, p[7]);


	if(es_rate_flag)
	{
		char *base;
		int bytes = 0;

		if(pts_flag == 2)
			bytes += 5;
		else if(pts_flag == 3)
			bytes += 10;

		if(escr_flag)
			bytes += 6;

		base = p[8+bytes];
		es_rate = ((base[0] & 0x7f) << 8) | (base[1] << 8) | (base[0] & 0xfe);
		mp_msg(MSGT_DEMUX, MSGL_V, "demux_ts: ES_RATE=%d)\n", es_rate*50);
	}
	*/

	header_len = p[8];


	if (header_len + 9 > pkt_len) //9 are the bytes read up to the header_length field
	{
		mp_msg(MSGT_DEMUX, MSGL_DBG2, "demux_ts: illegal value for PES_header_data_length (0x%02x)\n", header_len);
		return 0;
	}

	p += header_len + 9;
	packet_len -= header_len + 3;

	/*
	if(packet_len <= 0)
		mp_msg(MSGT_DEMUX, MSGL_INFO, "\n\nNow: %d, prima: %d, ORIG: %d\n\n\n", packet_len, packet_len+3+header_len, pkt_len);
	*/

	if(es->payload_size)
		es->payload_size -= header_len + 3;


	if (stream_id == 0xbd)
	{
		int track; //, spu_id;

		mp_msg(MSGT_DEMUX, MSGL_DBG3, "pes_parse2: audio buf = %02X %02X %02X %02X %02X %02X %02X %02X, 80: %d\n",
			p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[0] & 0x80);

		track = p[0] & 0x0F;
		mp_msg(MSGT_DEMUX, MSGL_DBG2, "A52 TRACK: %d\n", track);


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
		(p[0] == 0x20)) // && p[1] == 0x00))
		{
			es->start = p;
			es->size  = packet_len;
			es->type  = SPU_DVB;
			es->payload_size -= packet_len;

			return 1;
		}
		else if ((p[0] & 0xE0) == 0x20)	//SPU_DVD
		{
			//DVD SUBS
			es->start   = p+1;
			es->size    = packet_len-1;
			es->type    = SPU_DVD;
			es->payload_size -= packet_len;

			return 1;
		}
		else if ((p[0] & 0xF0) == 0x80)
		{
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "A52 WITH HEADER\n");
			es->start   = p+4;
			es->size    = packet_len - 4;
			es->type    = AUDIO_A52;
			es->payload_size -= packet_len;

			return 1;
		}
		else if ((p[0]&0xf0) == 0xa0)
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
	}
	else if ((stream_id >= 0xbc) && ((stream_id & 0xf0) == 0xe0))
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
		if(type_from_pmt != -1)	//MP4 A/V
		{
			es->start   = p;
			es->size    = packet_len;
			es->type    = type_from_pmt;
			if(es->payload_size)
				es->payload_size -= packet_len;

			return 1;
		}
	}
	else if ((stream_id & 0xe0) == 0xc0)
	{
		int profile = 0, srate = 0, channels = 0;
		uint32_t hdr, l = 0;

		hdr = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
		if((hdr & 0xfff00000) == 0xfff00000)
		{
			// ADTS AAC shows with MPA layer 4 (00 in the layer bitstream)
			l = 4 - ((hdr & 0x00060000) >> 17);
			profile = ((hdr & 0x0000C000) >> 14);
			srate = ((hdr & 0x00003c00) >> 10);
			channels = ((hdr & 0x000001E0) >> 5);
		}
		mp_msg(MSGT_DEMUX, MSGL_DBG2, "\n\naudio header: %2X %2X %2X %2X\nLAYER: %d, PROFILE: %d, SRATE=%d, CHANNELS=%d\n\n", p[0], p[1], p[3], p[4], l, profile, srate, channels);

		es->start   = p;
		es->size    = packet_len;


		//if((type_from_pmt == 0x0f) || (l == 4)) //see in parse_pmt()
		if(l==4)
			es->type    = AUDIO_AAC;
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

	return 0;
}




static int ts_sync(stream_t *stream)
{
	int c=0;

	mp_msg(MSGT_DEMUX, MSGL_DBG2, "TS_SYNC \n");

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
		priv->fifo[i].broken = 1;
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

/*
DON'T REMOVE, I'LL NEED IT IN THE (NEAR) FUTURE)
static int check_crc32(uint32_t val, uint8_t *ptr, uint16_t len, uint8_t *vbase)
{
	uint32_t tab_crc32, calc_crc32;

	calc_crc32 = CalcCRC32(val, ptr, len);

	tab_crc32 = (vbase[0] << 24) | (vbase[1] << 16) | (vbase[2] << 8) | vbase[3];

	printf("CRC32, TAB: %x, CALC: %x, eq: %x\n", tab_crc32, calc_crc32, tab_crc32 == calc_crc32);
	return (tab_crc32 == calc_crc32);

}
*/

static int parse_pat(ts_priv_t * priv, int is_start, unsigned char *buff, int size)
{
	uint8_t skip, m = 0;
	unsigned char *ptr;
	unsigned char *base;
	int entries, i, sections;
	uint16_t progid;

	//PRE-FILLING
	if(! is_start)
	{
		if(priv->pat.buffer_len == 0) //a broken packet
		{
			return 0;
		}

		if(priv->pat.skip)
		    m = min(priv->pat.skip, size);

		priv->pat.skip -= m;
		if(m == size)
			return -1;	//keep on buffering
	}
	else	//IS_START, replace the old content
	{
		priv->pat.buffer_len = 0;
		skip = buff[0]+1;
		m = min(skip, size);

		priv->pat.skip = skip - m;

		if(m == size)
			return -1;
	}

	//FILLING
	memcpy(&(priv->pat.buffer[priv->pat.buffer_len]), &buff[m], size - m);

	priv->pat.buffer_len += size - m;

	//PARSING
	ptr = priv->pat.buffer;

	priv->pat.table_id = ptr[0];
	priv->pat.ssi = (ptr[1] >> 7) & 0x1;
	priv->pat.curr_next = ptr[5] & 0x01;
	priv->pat.ts_id = (ptr[3]  << 8 ) | ptr[4];
	priv->pat.version_number = (ptr[5] >> 1) & 0x1F;
	priv->pat.section_length = ((ptr[1] & 0x03) << 8 ) | ptr[2];
	priv->pat.section_number = ptr[6];
	priv->pat.last_section_number = ptr[7];


	if((! priv->pat.curr_next) || (priv->pat.table_id != 0)) // || (! priv->pat.ssi))
		return 0;


	//beginning of sections loop
	sections = priv->pat.last_section_number - priv->pat.section_number + 1;
	mp_msg(MSGT_DEMUX, MSGL_V, "PARSE_PAT, section %d of %d, TOTAL: %d\n", priv->pat.section_number, priv->pat.last_section_number, sections);

	if(priv->pat.section_length + 3 > priv->pat.buffer_len)
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "PARSE_PAT, section larger than buffer size: %d > %d, EXIT\n",
			priv->pat.section_length, priv->pat.buffer_len - 3);

		return -1;	//KEEP ON FILLING THE TABLE
	}

	//check_crc32(0xFFFFFFFFL, ptr, priv->pat.buffer_len - 4, &ptr[priv->pat.buffer_len - 4]);

	entries = (int) (priv->pat.section_length - 9) / 4;	//entries per section


	for(i=0; i < entries; i++)
	{
		int32_t idx;
		base = &ptr[8 + i*4];
		progid = (base[0] << 8) | base[1];

		if((idx = prog_idx_in_pat(priv, progid)) == -1)
		{
			int sz = sizeof(struct pat_progs_t) * (priv->pat.progs_cnt+1);
			priv->pat.progs = (struct pat_progs_t*) realloc(priv->pat.progs, sz);
			if(priv->pat.progs == NULL)
			{
				mp_msg(MSGT_DEMUX, MSGL_ERR, "PARSE_PAT: COULDN'T REALLOC %d bytes, NEXT\n", sz);
				break;
			}

			idx = priv->pat.progs_cnt;
			priv->pat.progs_cnt++;
		}

		priv->pat.progs[idx].id = progid;
		priv->pat.progs[idx].pmt_pid = ((base[2]  & 0x1F) << 8) | base[3];
		mp_msg(MSGT_DEMUX, MSGL_V, "PROG: %d (%d-th of %d), PMT: %d\n", priv->pat.progs[idx].id, i+1, entries, priv->pat.progs[idx].pmt_pid);
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


static int parse_descriptors(struct pmt_es_t *es, uint8_t *ptr)
{
	int j, descr_len, len;

	j = 0;
	len = es->descr_length;
	while(len > 2)
	{
		descr_len = ptr[j+1];
		mp_msg(MSGT_DEMUX, MSGL_DBG2, "...descr id: 0x%x, len=%d\n", ptr[j], descr_len);
		if(descr_len > len)
		{
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "INVALID DESCR LEN: %d vs %d max, EXIT LOOP\n", descr_len, len);
			return -1;
		}


		if(ptr[j] == 0x6a)	//A52 Descriptor
		{
			es->type = AUDIO_A52;
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "DVB A52 Descriptor\n");
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
				else
					es->type = UNKNOWN;
				mp_msg(MSGT_DEMUX, MSGL_DBG2, "FORMAT %s\n", es->format_descriptor);
			}
		}
		else
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "Unknown descriptor 0x%x, SKIPPING\n", ptr[j]);

		len -= 2 + descr_len;
		j += 2 + descr_len;
	}

	return 1;
}



static int parse_pmt(ts_priv_t * priv, uint16_t progid, uint16_t pid, int is_start, unsigned char *buff, int size)
{
	unsigned char *base, *es_base;
	pmt_t *pmt;
	int32_t idx, es_count, section_bytes;
	uint8_t skip, m=0;

	idx = progid_idx_in_pmt(priv, progid);

	if(idx == -1)
	{
		int sz = (priv->pmt_cnt + 1) * sizeof(pmt_t);
		priv->pmt = (pmt_t *) realloc(priv->pmt, sz);
		if(priv->pmt == NULL)
		{
			mp_msg(MSGT_DEMUX, MSGL_ERR, "PARSE_PMT: COULDN'T REALLOC %d bytes, NEXT\n", sz);
			return NULL;
		}

		idx = priv->pmt_cnt;
		memset(&(priv->pmt[idx]), 0, sizeof(pmt_t));
		priv->pmt_cnt++;
	}

	pmt = &(priv->pmt[idx]);


	if(! is_start)
	{
		if(pmt->buffer_len == 0)
		{
			//BROKEN PMT PACKET, DISCARD
			return -1;
		}

		if(pmt->skip)
		    m = min(pmt->skip, size);

		pmt->skip -= m;
		if(m == size)
			return 0;
	}
	else
	{
		pmt->buffer_len = 0;
		skip = buff[0] + 1;
		m = min(skip, size);

		pmt->skip = skip - m;

		if(m == size)
			return 0;
	}


	if(size-m + pmt->buffer_len > 2048)
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "FILL_PMT(prog=%d, PID=%d), ERROR! PMT TOO LONG, IGNORING\n", progid, pid);
		pmt->buffer_len = 0;
		return NULL;
	}

	memcpy(&(pmt->buffer[pmt->buffer_len]), &buff[m], size - m);
	pmt->progid = progid;
	pmt->buffer_len += size - m;

	mp_msg(MSGT_DEMUX, MSGL_V, "FILL_PMT(prog=%d), PMT_len: %d, IS_START: %d, TS_PID: %d, SIZE=%d, M=%d, ES_CNT=%d, IDX=%d, PMT_PTR=%p\n",
		progid, pmt->buffer_len, is_start, pid, size, m, pmt->es_cnt, idx, pmt);

	base = pmt->buffer;


	pmt->table_id = base[0];
	pmt->ssi = base[1] & 0x80;
	pmt->section_length = (((base[1] & 0xf) << 8 ) | base[2]);
	pmt->version_number = (base[5] >> 1) & 0x1f;
	pmt->curr_next = (base[5] & 1);
	pmt->section_number = base[6];
	pmt->last_section_number = base[7];
	pmt->PCR_PID = ((base[8] & 0x1f) << 8 ) | base[9];
	pmt->prog_descr_length = ((base[10] & 0xf) << 8 ) | base[11];



	if((pmt->curr_next == 0) || (pmt->table_id != 2))
		return -1;


	if(pmt->section_length + 3 > pmt->buffer_len)
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "PARSE_PMT, SECTION LENGTH TOO LARGE FOR CURRENT BUFFER (%d vs %d), NEXT TIME\n", pmt->section_length, pmt->buffer_len);

		return -1;
	}

	if(pmt->prog_descr_length > pmt->section_length - 9)
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "PARSE_PMT, INVALID PROG_DESCR LENGTH (%d vs %d)\n", pmt->prog_descr_length, pmt->section_length - 9);
		return -1;
	}


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
			pmt->es = (struct pmt_es_t *) realloc(pmt->es, sz);
			if(pmt->es == NULL)
			{
				mp_msg(MSGT_DEMUX, MSGL_ERR, "PARSE_PMT, COULDN'T ALLOCATE %d bytes for PMT_ES\n", sz);
				continue;
			}
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
		pmt->es[idx].type = es_type;

		pmt->es[idx].type = UNKNOWN;

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
				parse_descriptors(&pmt->es[idx], &es_base[5]);
				pmt->es[idx].type = AUDIO_MP2;
				break;
			case 6:
				parse_descriptors(&pmt->es[idx], &es_base[5]);
				if(pmt->es[idx].type == 0x6)
					pmt->es[idx].type = UNKNOWN;
				break;
			case 0x10:
				pmt->es[idx].type = VIDEO_MPEG4;
				break;
			case 0x11:
				parse_descriptors(&pmt->es[idx], &es_base[5]);
				pmt->es[idx].type = AUDIO_AAC;
				break;

			/*	seems to indicate an AAC in a certain broadcaster's tables, but
				it's deceiving, so it's commented out
			case 0x0f:
				parse_descriptors(&pmt->es[idx], &es_base[5]);
				pmt->es[idx].type = 0x0f;
				break;
			*/

			case 0x81:
				pmt->es[idx].type = AUDIO_A52;
				break;

			default:
			{
				if(es_type > 0x80)
				{
					parse_descriptors(&pmt->es[idx], &es_base[5]);
				}
				else
				{
					mp_msg(MSGT_DEMUX, MSGL_DBG2, "UNKNOWN ES TYPE=0x%x\n", es_type);
					pmt->es[idx].type = UNKNOWN;
				}
			}
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


static int fill_packet(demuxer_t *demuxer, demux_stream_t *ds, demux_packet_t **dp, int *dp_offset, int *broken)
{
	int ret = 0;

	if((*dp != NULL) && (*dp_offset > 0))
	{
		if(*broken == 0)
		{
			ret = *dp_offset;
			resize_demux_packet(*dp, ret);	//shrinked to the right size
			ds_add_packet(ds, *dp);
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "ADDED %d  bytes to %s fifo, PTS=%f\n", ret, (ds == demuxer->audio ? "audio" : (ds == demuxer->video ? "video" : "sub")), (*dp)->pts);
		}
		else
		{
			ret = 0;
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "BROKEN PES, DISCARDING\n");
			free_demux_packet(*dp);
		}
	}

	*broken = 1;
	*dp = NULL;
	*dp_offset = 0;

	return ret;
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
	char *p, tmp[TS_FEC_PACKET_SIZE];
	demux_stream_t *ds = NULL;
	demux_packet_t **dp = NULL;
	int *dp_offset = 0, *buffer_size = 0, *broken = NULL;
	int32_t progid, pid_type, bad, ts_error;


	while(! done)
	{
		bad = ts_error = 0;
		ds = (demux_stream_t*) NULL;
		dp = (demux_packet_t **) NULL;
		broken = dp_offset = buffer_size = NULL;

		buf_size = priv->ts.packet_size;

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
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "TS_PARSE: COULDN'T SYNC\n");
			return 0;
		}

		len = stream_read(stream, &packet[1], 3);
		if (len != 3)
			return 0;

		if((packet[1]  >> 7) & 0x01)	//transport error
			ts_error = 1;

		buf_size -= 4;

		is_start = packet[1] & 0x40;
		pid = ((packet[1] & 0x1f) << 8) | packet[2];

		tss = priv->ts.pids[pid];			//an ES stream
		if(tss == NULL)
		{
			tss = malloc(sizeof(ES_stream_t));
			if(! tss)
				continue;
			memset(tss, 0, sizeof(ES_stream_t));
			tss->pid = pid;
			tss->last_cc = -1;
			tss->type = UNKNOWN;
			tss->payload_size = 0;
			tss->seen = 0;
			priv->ts.pids[pid] = tss;
		}



		if(((pid > 1) && (pid < 16)) || (pid == 8191))		//invalid pid
		{
			stream_skip(stream, buf_size-1);
			continue;
		}

		cc = (packet[3] & 0xf);
		cc_ok = (tss->last_cc < 0) || ((((tss->last_cc + 1) & 0x0f) == cc));
		tss->last_cc = cc;
		    
		bad = ts_error || (! cc_ok);
    
		if(! bad)
		{
			// skip adaptation field, but only if cc_ok is not corrupt,
			//otherwise we may throw away good data  
			afc = (packet[3] >> 4) & 3;
			if(! (afc % 2))	//no payload in this TS packet
			{
			    stream_skip(stream, buf_size-1);
			    continue;
			}
			
			if(afc == 3)
			{
				int c;
				c = stream_read_char(stream);
				buf_size--;

				c = min(c, buf_size);
				stream_skip(stream, c);
				buf_size -= c;
				if(buf_size == 0)
					continue;

				afc = c + 1;
			}
			else
				afc = 0;	//payload only
		}
		else
		{
			// logically this packet should be dropped, but if I do it
			// certain streams play corrupted. Maybe the decoders know
			// how to deal with it, but at least I consider the packet
			// as "not initial"
			mp_msg(MSGT_DEMUX, MSGL_V, "ts_parse: PID=%d, Transport error: %d, CC_OK: %s\n\n", tss->pid, ts_error, (cc_ok ? "yes" : "no"));

			if(priv->keep_broken == 0)
			{
				stream_skip(stream, buf_size-1);
				continue;
			}
			
			is_start = 0;	//queued to the packet data
		}

		tss->seen++;
		if(tss->seen == 16 && cc_ok)	//at least a complete round
			mp_msg(MSGT_DEMUX, MSGL_V, "\nNew TS pid=%u\n", pid);


		//TABLE PARSING

		base = priv->ts.packet_size - buf_size;
		if(pid  == 0)
		{
			stream_read(stream,&packet[base], buf_size);
			parse_pat(priv, is_start, &packet[base], buf_size);
			continue;
		}
		else
		{
			progid = prog_id_in_pat(priv, pid);
			if(((progid != -1) || (pid == 16)))
			{
				if(pid != demuxer->video->id && pid != demuxer->audio->id && pid != demuxer->sub->id)
				{
					stream_read(stream,&packet[base], buf_size);
					parse_pmt(priv, progid, pid, is_start, &packet[base], buf_size);
					continue;
				}
				else
					mp_msg(MSGT_DEMUX, MSGL_ERR, "Argh! Data pid used in the PMT, Skipping PMT parsing!\n");
			}
		}


		priv->last_pid = pid;

		is_video = ((tss->type == VIDEO_MPEG1) || (tss->type == VIDEO_MPEG2) || (tss->type == VIDEO_MPEG4));
		is_audio = ((tss->type == AUDIO_MP2) || (tss->type == AUDIO_A52) || (tss->type == AUDIO_LPCM_BE) ||  (tss->type == AUDIO_AAC));
		is_sub	= ((tss->type == SPU_DVD) || (tss->type == SPU_DVB));
		pid_type = pid_type_from_pmt(priv, pid);

			// PES CONTENT STARTS HERE
		if(! probe)
		{
			if((pid == demuxer->sub->id))	//or the lang is right
			{
				pid_type = SPU_DVD;
			}

			if(is_video && (demuxer->video->id == tss->pid))
			{
				ds = demuxer->video;

				dp = &priv->fifo[1].pack;
				dp_offset = &priv->fifo[1].offset;
				buffer_size = &priv->fifo[1].buffer_size;
				broken = &priv->fifo[1].broken;
			}
			else if(is_audio && (demuxer->audio->id == tss->pid))
			{
				ds = demuxer->audio;

				dp = &priv->fifo[0].pack;
				dp_offset = &priv->fifo[0].offset;
				buffer_size = &priv->fifo[0].buffer_size;
				broken = &priv->fifo[0].broken;
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

						if(!strcmp(dvdsub_lang, ""))
							asgn = 1;
						else
						{
							lang = pid_lang_from_pmt(priv, pid);
							if(lang != NULL)
								asgn = (strncmp(lang, dvdsub_lang, 3) == 0);
							else
								asgn = 0;
						}

						if(asgn)
						{
							demuxer->sub->id = tss->pid;
							mp_msg(MSGT_DEMUX, MSGL_INFO, "CHOSEN SUBs pid 0x%x (%d) FROM PROG %d\n", tss->pid, tss->pid, priv->prog);
						}
					}
					else
					{
						mp_msg(MSGT_DEMUX, MSGL_V, "DISCARDED SUBs pid 0x%x (%d) NOT CHOSEN OR NOT IN PROG %d\n", tss->pid, tss->pid, priv->prog);
					}
				}

				if(demuxer->sub->id == tss->pid)
				{
					ds = demuxer->sub;

					dp = &priv->fifo[2].pack;
					dp_offset = &priv->fifo[2].offset;
					buffer_size = &priv->fifo[2].buffer_size;
					broken = &priv->fifo[2].broken;
				}
				else
				{
					stream_skip(stream, buf_size);
					continue;
				}
			}
			else
			{
				stream_skip(stream, buf_size);
				continue;
			}

			//IS IT TIME TO QUEUE DATA to the dp_packet?
			if(is_start && (dp != NULL))
			{
				retv = fill_packet(demuxer, ds, dp, dp_offset, broken);
			}


			if(*dp == NULL)
			{
				*dp = new_demux_packet(*buffer_size);	//es->size
				*dp_offset = 0;
				if(! *dp)
				{
					fprintf(stderr, "fill_buffer, NEW_ADD_PACKET(%d)FAILED\n", *buffer_size);
					continue;
				}
				mp_msg(MSGT_DEMUX, MSGL_DBG2, "CREATED DP(%d)\n", *buffer_size);
			}

			mp_msg(MSGT_DEMUX, MSGL_DBG2, "NOW PACKET_SIZE = %d, DP_OFFSET = %d\n", *buffer_size, *dp_offset);
		}



		if(is_start)
		{
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "IS_START\n");

			p = &packet[base];
			stream_read(stream, p, buf_size);

			len = pes_parse2(p, buf_size, es, pid_type);

			if(len > 0)
			{
				if(es->type == UNKNOWN)
					continue;

				es->pid = tss->pid;
				tss->type = es->type;

				if((es->pts < tss->last_pts) && es->pts)
					mp_msg(MSGT_DEMUX, MSGL_DBG2, "BACKWARDS PTS! : NEW: %f -> LAST: %f, PID %d\n", es->pts, tss->last_pts, tss->pid);

				if(es->pts == 0.0f)
					es->pts = tss->pts = tss->last_pts;
				else
					tss->pts = tss->last_pts = es->pts;

				mp_msg(MSGT_DEMUX, MSGL_DBG2, "ts_parse, NEW pid=%d, PSIZE: %u, type=%X, start=%p, len=%d\n",
					es->pid, es->payload_size, es->type, es->start, es->size);

				tss->payload_size = es->payload_size;

				demuxer->filepos = stream_tell(demuxer->stream) - es->size;

				if(probe)
					return 1; //es->size;
				else
				{
					*broken = 0;
					if(es->size > 0)
					{
						if(*dp_offset + es->size > *buffer_size)
						{
							*buffer_size = *dp_offset + es->size + TS_FEC_PACKET_SIZE;
							resize_demux_packet(*dp, *buffer_size);
							//we'll skip at least one RESIZE() in the next iteration of ts_parse()
							mp_msg(MSGT_DEMUX, MSGL_DBG2, "RESIZE DP TO %d\n", *buffer_size);
						}
						memcpy(&((*dp)->buffer[*dp_offset]), es->start, es->size);
						*dp_offset += es->size;
						(*dp)->flags = 0;
						(*dp)->pos = stream_tell(demuxer->stream);
						(*dp)->pts = es->pts;
					}

					if(retv > 0)
						return retv;
					else
						continue;
				}
			}
			else
				return 0;
		}
		else
		{
			uint16_t sz;

			if(tss->type == UNKNOWN)
			{
				stream_skip(stream, buf_size);
				continue;
			}


			es->pid = tss->pid;
			es->type = tss->type;
			es->pts = tss->pts = tss->last_pts;
			es->start = &packet[base];


			if(tss->payload_size > 0)
			{
				sz = min(tss->payload_size, buf_size);
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
					stream_skip(stream, buf_size);
					continue;
				}
			}


			if(! probe)
			{
				if(*dp_offset + sz > *buffer_size)
				{
					*buffer_size = *dp_offset + sz + TS_FEC_PACKET_SIZE;
					resize_demux_packet(*dp, *buffer_size);
					//we'll skip at least one RESIZE() in the next iteration of ts_parse()
					mp_msg(MSGT_DEMUX, MSGL_DBG2, "RESIZE DP TO %d\n", *buffer_size);
					//mp_msg(MSGT_DEMUX, MSGL_INFO, "SECOND TYPE=%x, bytes=%d, afc=%d, psize=%d, tell=%llu\n", es->type, sz, afc, tss->payload_size, (uint64_t) stream_tell(demuxer->stream));
				}

				stream_read(stream, &((*dp)->buffer[*dp_offset]), sz);
				*dp_offset += sz;

				if(buf_size - sz > 0)
				{
					stream_skip(stream, buf_size - sz);
				}

				continue;
			}
			else
			{
				stream_read(stream, es->start, sz);
				if(buf_size - sz) stream_read(stream, tmp, buf_size-sz);

				if(es->size)
					return es->size;
				else
					continue;
			}
		}
	}

	return 0;
}


extern void resync_audio_stream(sh_audio_t *sh_audio);
extern void skip_audio_frame(sh_audio_t *sh_audio);

static void reset_fifos(ts_priv_t* priv, int a, int v, int s)
{
	if(a)
	{
		if(priv->fifo[0].pack != NULL)
		{
			free_demux_packet(priv->fifo[0].pack);
			priv->fifo[0].pack = NULL;
		}
		priv->fifo[0].offset = 0;
		priv->fifo[0].broken = 1;
	}

	if(v)
	{
		if(priv->fifo[1].pack != NULL)
		{
			free_demux_packet(priv->fifo[1].pack);
			priv->fifo[1].pack = NULL;
		}
		priv->fifo[1].offset = 0;
		priv->fifo[1].broken = 1;
	}

	if(s)
	{
		if(priv->fifo[2].pack != NULL)
		{
			free_demux_packet(priv->fifo[2].pack);
			priv->fifo[2].pack = NULL;
		}
		priv->fifo[2].offset = 0;
		priv->fifo[2].broken = 1;
	}
}

extern int videobuf_code_len;
extern int sync_video_packet(demux_stream_t *);
extern int skip_video_packet(demux_stream_t *);

void demux_seek_ts(demuxer_t *demuxer, float rel_seek_secs, int flags)
{
	demux_stream_t *d_audio=demuxer->audio;
	demux_stream_t *d_video=demuxer->video;
	demux_stream_t *d_sub=demuxer->sub;
	sh_audio_t *sh_audio=d_audio->sh;
	sh_video_t *sh_video=d_video->sh;
	ts_priv_t * priv = (ts_priv_t*) demuxer->priv;
	int i, video_stats;
	off_t newpos;

	//================= seek in MPEG-TS ==========================

	ts_dump_streams(demuxer->priv);
	reset_fifos(priv, sh_audio != NULL, sh_video != NULL, demuxer->sub->id > 0);


	if(sh_audio != NULL)
		ds_free_packs(d_audio);
	if(sh_video != NULL)
		ds_free_packs(d_video);
	if(demuxer->sub->id > 0)
		ds_free_packs(d_sub);


	video_stats = (sh_video != NULL);
	if(video_stats)
		video_stats = sh_video->i_bps;

	newpos = (flags & 1) ? demuxer->movi_start : demuxer->filepos;
	if(flags & 2) // float seek 0..1
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

#ifdef _LARGEFILE_SOURCE
	newpos &= ~((long long) (STREAM_BUFFER_SIZE - 1));  /* sector boundary */
#else
	newpos &= ~(STREAM_BUFFER_SIZE - 1);  /* sector boundary */
#endif

	stream_seek(demuxer->stream, newpos);

	videobuf_code_len = 0;

	if(sh_video != NULL)
		ds_fill_buffer(d_video);

	if(sh_audio != NULL)
	{
		ds_fill_buffer(d_audio);
		resync_audio_stream(sh_audio);
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
		else //MPEG4
		{
			if(i==0x1B6) break; // found it!
		}

		if(!i || !skip_video_packet(d_video)) break; // EOF?
	}
}


int demux_ts_fill_buffer(demuxer_t * demuxer)
{
	ES_stream_t es;
	char packet[TS_FEC_PACKET_SIZE];

	return -ts_parse(demuxer, &es, packet, 0);
}



