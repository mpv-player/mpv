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
#include "stheader.h"

#include "bswap.h"


#define TS_FEC_PACKET_SIZE 204
#define TS_PACKET_SIZE 188
#define NB_PID_MAX 8192

#define MAX_HEADER_SIZE 6			/* enough for PES header + length */
#define MAX_CHECK_SIZE	65535
#define MAX_PROBE_SIZE	2000000
#define NUM_CONSECUTIVE_TS_PACKETS 32
#define NUM_CONSECUTIVE_AUDIO_PACKETS 348


int ts_prog;

typedef enum
{
	UNKNOWN		= -1,
	VIDEO_MPEG1 	= 0x10000001,
	VIDEO_MPEG2 	= 0x10000002,
	VIDEO_MPEG4 	= 0x10000004,
	AUDIO_MP2   	= 0x50,
	AUDIO_A52   	= 0x2000,
	AUDIO_LPCM_BE  	= 0x10001,
	AUDIO_AAC	= (('A' << 24) | ('4' << 16) | ('P' << 8) | 'M')
	/*,
	SPU_DVD		= 0x3000000,
	SPU_DVB		= 0x3000001,
	*/
} es_stream_type_t;


typedef struct {
	int size;
	unsigned char *start;
	uint16_t payload_size;
	es_stream_type_t type;
	float pts, last_pts;
	int pid;
	int last_cc;				// last cc code (-1 if first packet)
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
	uint8_t buffer_len;
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
	uint8_t buffer_len;
	uint16_t es_cnt;
	struct pmt_es_t {
		uint16_t pid;
		uint32_t type;	//it's 8 bit long, but cast to the right type as FOURCC
		uint16_t descr_length;
	} *es;
} pmt_t;

typedef struct {
	MpegTSContext ts;
	int is_synced;	//synced to the beginning of priv->last_pid PES header
	int last_afc;	//bytes read from the last adaption field
	int last_pid;
	int is_start;
	int eof;
	av_fifo_t fifo[2];	//0 for audio, 1 for video
	pat_t pat;
	pmt_t *pmt;
	uint16_t pmt_cnt;
	uint32_t prog;
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
	unsigned char buf[buf_size], done = 0, *ptr;
	uint32_t _read, i, count = 0, is_ts;
	int cc[NB_PID_MAX], last_cc[NB_PID_MAX], pid, cc_ok, c, good, bad;
	uint8_t size = 0;
	off_t pos = 0;

	mp_msg(MSGT_DEMUX, MSGL_V, "Checking for MPEG-TS...\n");

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

		if(pos >= MAX_CHECK_SIZE)
		{
			done = 1;
			is_ts = 0;
		}
	}

	mp_msg(MSGT_DEMUX, MSGL_V, "TRIED UP TO POSITION %u, FOUND %x, packet_size= %d\n", pos, c, size);
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


static void ts_detect_streams(demuxer_t *demuxer, uint32_t *a,  uint32_t *v, int *fapid, int *fvpid, int32_t *prog)
{
	int video_found = 0, audio_found = 0, i, num_packets = 0, req_apid, req_vpid;
	int is_audio, is_video, has_tables;
	int32_t p, chosen_pid;
	off_t pos=0;
	ES_stream_t es;
	unsigned char tmp[TS_FEC_PACKET_SIZE];
	ts_priv_t *priv = (ts_priv_t*) demuxer->priv;

	priv->is_synced = 0;
	priv->last_afc = 0;
	priv->last_pid = 8192;		//invalid pid
	priv->is_start = 0;
	priv->eof = 0;

	req_apid = *fapid;
	req_vpid = *fvpid;

	has_tables = 0;
	mp_msg(MSGT_DEMUXER, MSGL_INFO, "PROBING UP TO %u, PROG: %d\n", MAX_PROBE_SIZE, *prog);
	while(pos <= MAX_PROBE_SIZE)
	{
		pos = stream_tell(demuxer->stream);
		if(ts_parse(demuxer, &es, tmp, 1))
		{
			is_audio = ((es.type == AUDIO_MP2) || (es.type == AUDIO_A52) || (es.type == AUDIO_LPCM_BE) || (es.type == AUDIO_AAC));
			is_video = ((es.type == VIDEO_MPEG1) || (es.type == VIDEO_MPEG2) || (es.type == VIDEO_MPEG4));

			if((! is_audio) && (! is_video))
				continue;

			if(is_video)
			{
    				chosen_pid = (req_vpid == es.pid);
				if((! chosen_pid) && (req_vpid > 0))
					continue;
			}
			else if(is_audio)
			{
				chosen_pid = (req_apid == es.pid);
				if((! chosen_pid) && (req_apid > 0))
					continue;
			}

			if(req_vpid < 0 && req_apid < 0)
				chosen_pid = 1;

			p = progid_for_pid(priv, es.pid, *prog);
			if(p != -1)
				has_tables++;

			if((*prog == 0) && (p != -1))
			{
				if(chosen_pid)
					*prog = p;
			}

			if((*prog > 0) && (*prog != p))
			{
				if(audio_found)
				{
					if(is_video && (req_vpid == es.pid))
					{
						*v = es.type;
						*fvpid = es.pid;
						video_found = 1;
						break;
					}
				}

				if(video_found)
				{
					if(is_audio && (req_apid == es.pid))
					{
						*a = es.type;
						*fapid = es.pid;
						audio_found = 1;
						break;
					}
				}


				continue;
			}


			mp_msg(MSGT_DEMUXER, MSGL_DBG2, "TYPE: %x, PID: %d, PROG FOUND: %d\n", es.type, es.pid, *prog);


			if(is_video)
			{
				if((req_vpid == -1) || (req_vpid == es.pid))
				{
					*v = es.type;
					*fvpid = es.pid;
					video_found = 1;
				}
			}


			if(((req_vpid == -2) || (num_packets >= NUM_CONSECUTIVE_AUDIO_PACKETS)) && audio_found)
			{
				//novideo or we have at least 348 audio packets (64 KB) without video (TS with audio only)
				*v = 0;
				break;
			}


			if(is_audio)
			{
				if((req_apid == -1) || (req_apid == es.pid))
				{
					*a = es.type;
					*fapid = es.pid;
					audio_found = 1;
				}
			}

			if(audio_found && (*fapid == es.pid) && (! video_found))
				num_packets++;

			if((req_apid == -2) && video_found)
			{
				*a = 0;
				break;
			}

			if((has_tables==0) && (video_found && audio_found) && (pos >= 1000000))
				break;
		}
	}

	if(video_found)
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "VIDEO MPEG%d(pid=%d)...", (*v == VIDEO_MPEG1 ? 1 : (*v == VIDEO_MPEG2 ? 2 : 4)), *fvpid);
	else
	{
	    //WE DIDN'T MATCH ANY VIDEO STREAM
	    mp_msg(MSGT_DEMUXER, MSGL_INFO, "NO VIDEO! ");
	}

	if(*a == AUDIO_MP2)
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "AUDIO MP2(pid=%d)", *fapid);
	else if(*a == AUDIO_A52)
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "AUDIO A52(pid=%d)", *fapid);
	else if(*a == AUDIO_LPCM_BE)
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "AUDIO LPCM(pid=%d)", *fapid);
	else if(*a == AUDIO_AAC)
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "AUDIO AAC(pid=%d)", *fapid);
	else
	{
		//WE DIDN'T MATCH ANY AUDIO STREAM, SO WE FORCE THE DEMUXER TO IGNORE AUDIO
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "NO AUDIO! ");
	}

	mp_msg(MSGT_DEMUXER, MSGL_INFO, " PROGRAM N. %d\n", *prog);

	for(i=0; i<8192; i++)
	{
		if(priv->ts.pids[i] != NULL)
		{
			priv->ts.pids[i]->payload_size = 0;
			priv->ts.pids[i]->pts = priv->ts.pids[i]->last_pts = 0;
			//priv->ts.pids[i]->type = UNKNOWN;
		}
	}
}



demuxer_t *demux_open_ts(demuxer_t * demuxer)
{
	int i;
	uint8_t packet_size;
	sh_video_t *sh_video;
	sh_audio_t *sh_audio;
	uint32_t at = 0, vt = 0;
	ts_priv_t * priv = (ts_priv_t*) demuxer->priv;
	demuxer_t *od;

	mp_msg(MSGT_DEMUX, MSGL_DBG2, "DEMUX OPEN, AUDIO_ID: %d, VIDEO_ID: %d, SUBTITLE_ID: %d,\n",
		demuxer->audio->id, demuxer->video->id, demuxer->sub->id);


	demuxer->type= DEMUXER_TYPE_MPEG_TS;


	stream_reset(demuxer->stream);
	stream_seek(demuxer->stream, 0);

	packet_size = ts_check_file(demuxer);
	if(!packet_size)
	    return NULL;

	priv = malloc(sizeof(ts_priv_t));
	if(priv == NULL)
	{
		mp_msg(MSGT_DEMUX, MSGL_FATAL, "DEMUX_OPEN_TS, couldn't allocate %lu bytes, exit\n",
			sizeof(ts_priv_t));
		return NULL;
	}

	for(i=0; i < 8192; i++)
	    priv->ts.pids[i] = NULL;
	priv->pat.progs = NULL;
	priv->pat.progs_cnt = 0;

	priv->pmt = NULL;
	priv->pmt_cnt = 0;

	priv->ts.packet_size = packet_size;


	demuxer->priv = priv;
	if(demuxer->stream->type != STREAMTYPE_FILE)
		demuxer->seekable = 0;
	else
		demuxer->seekable = 1;

	stream_seek(demuxer->stream, 0);	//IF IT'S FROM A PIPE IT WILL FAIL, BUT WHO CARES?
	ts_detect_streams(demuxer, &at, &vt, &demuxer->audio->id, &demuxer->video->id, &ts_prog);
	mp_msg(MSGT_DEMUXER,MSGL_INFO, "Opened TS demuxer, audio: %x(pid %d), video: %x(pid %d)...\n", at, demuxer->audio->id, vt, demuxer->video->id);


	if(vt)
	{
		if(vt == VIDEO_MPEG4)
			demuxer->file_format= DEMUXER_TYPE_MPEG4_IN_TS;

		sh_video = new_sh_video(demuxer, 0);
		sh_video->ds = demuxer->video;
		sh_video->format = vt;
		demuxer->video->sh = sh_video;

		mp_msg(MSGT_DEMUXER,MSGL_INFO, "OPENED_SH_VIDEO\n");
	}

	if(at)
	{
		sh_audio = new_sh_audio(demuxer, 0);
		sh_audio->ds = demuxer->audio;
		sh_audio->format = at;
		demuxer->audio->sh = sh_audio;

		mp_msg(MSGT_DEMUXER,MSGL_INFO, "OPENED_SH_AUDIO\n");
	}


	mp_msg(MSGT_DEMUXER,MSGL_INFO, "Opened TS demuxer2\n");

	/*
	demuxer->movi_start = 0;
	demuxer->movi_end = demuxer->stream->end_pos;
	*/

	stream_seek(demuxer->stream, 0);	//IF IT'S FROM A PIPE IT WILL FAIL, BUT WHO CARES?



	priv->is_synced = 0;
	priv->last_afc = 0;
	priv->last_pid = 8192;		//invalid pid
	priv->is_start = 0;

	for(i=0; i< 2; i++)
	{
		priv->fifo[i].pack  = NULL;
		priv->fifo[i].offset = 0;
		priv->fifo[i].broken = 1;
	}
	priv->fifo[0].ds = demuxer->audio;
	priv->fifo[1].ds = demuxer->video;

	priv->fifo[0].buffer_size = 1536;
	priv->fifo[1].buffer_size = 32767;
	priv->eof = 0;

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
	mp_msg(MSGT_DEMUX, MSGL_DBG2, "pes_parse2(%X, %d): \n", buf, packet_len);

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
		return 0;

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

	if(es->payload_size)
		es->payload_size -= header_len + 3;


	if (stream_id == 0xbd)
	{
		int track; //, spu_id;

		mp_msg(MSGT_DEMUX, MSGL_DBG3, "pes_parse2: audio buf = %02X %02X %02X %02X %02X %02X %02X %02X, 80: %d\n",
			p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[0] & 0x80);

		track = p[0] & 0x0F;
		mp_msg(MSGT_DEMUX, MSGL_DBG2, "AC3 TRACK: %d\n", track);


		/*
		* we check the descriptor tag first because some stations
		* do not include any of the ac3 header info in their audio tracks
		* these "raw" streams may begin with a byte that looks like a stream type.
		*/


		if(
			(type_from_pmt == AUDIO_A52) ||		 /* ac3 - raw */
			(p[0] == 0x0B && p[1] == 0x77)		/* ac3 - syncword */
		)
		{
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "AC3 RAW OR SYNCWORD\n");
			es->start = p;
			es->size  = packet_len;
			es->type  = AUDIO_A52;
			es->payload_size -= packet_len;

			return es->size;
		}
		/*
		SUBS, it seems they are not streams in g1, what to do?
		else if (//m->descriptor_tag == 0x06 &&
		p[0] == 0x20 && p[1] == 0x00)
		{
			// DVBSUB
			long payload_len = ((buf[4] << 8) | buf[5]) - header_len - 3;
			es->start = p;
			es->size  = packet_len;
			es->type  = SPU_DVB + payload_len;
			es->payload_size -= packet_len;

			return es->size;
		}

		else if ((p[0] & 0xE0) == 0x20)
		{
			spu_id      = (p[0] & 0x1f);
			es->start   = p+1;
			es->size    = packet_len-1;
			es->type    = SPU_DVD + spu_id;
			es->payload_size -= packet_len;

			return es->size;
		}
		*/
		else if ((p[0] & 0xF0) == 0x80)
		{
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "AC3 WITH HEADER\n");
			es->start   = p+4;
			es->size    = packet_len - 4;
			es->type    = AUDIO_A52;
			es->payload_size -= packet_len;

			return es->size;
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

			return es->size;
		}
	}
	else if ((stream_id >= 0xbc) && ((stream_id & 0xf0) == 0xe0))
	{
		es->start   = p;
		es->size    = packet_len;
		es->type    = VIDEO_MPEG2;
		if(es->payload_size)
			es->payload_size -= packet_len;

		return es->size;
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

			return es->size;
		}
	}
	else if ((stream_id & 0xe0) == 0xc0)
	{
		int track;

		track 	    = stream_id & 0x1f;
		es->start   = p;
		es->size    = packet_len;
		es->type    = AUDIO_MP2;
		es->payload_size -= packet_len;

		return es->size;
	}
	else if (type_from_pmt != -1)	//as a last resort here we trust the PMT, if present
	{
		es->start   = p;
		es->size    = packet_len;
		es->type    = type_from_pmt;
		es->payload_size -= packet_len;

		return es->size;
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

	for(i = 0; i < 2; i++)
	{
		if((priv->fifo[i].pack != NULL) && (priv->fifo[i].offset != 0))
		{
			resize_demux_packet(priv->fifo[i].pack, priv->fifo[i].offset);
			ds_add_packet(priv->fifo[i].ds, priv->fifo[i].pack);
		}
	}

	priv->eof = 1;
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


static int parse_pat(ts_priv_t * priv, int is_start, unsigned char *buff, int size)
{
	uint8_t skip, m = 0;
	unsigned char *ptr;
	unsigned char *base; //, *crc;
	int entries, i, sections;
	uint16_t progid;
	//uint32_t o_crc, calc_crc;

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

	/*CRC CHECK
	crc = &(priv->pat.buffer[priv->pat.section_length - 4]);
	o_crc = (crc[0] << 24) | (crc[1] << 16) | (crc[2] << 8) | crc[3];
	calc_crc = CalcCRC32(0xFFFFFFFFL, priv->pat.buffer, priv->pat.section_length);
	printf("CRC ORIGINALE: %x, CALCOLATO: %x\n", o_crc, calc_crc);
	*/


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
	int i;

	if(pmt == NULL)
		return -1;

	if(pmt->es == NULL)
		return -1;

	for(i = 0; i < pmt->es_cnt; i++)
	{
		if(pmt->es[i].pid == pid)
			return i;
	}

	return -1;
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
			mp_msg(MSGT_DEMUX, MSGL_ERR, "FILL_PMT: COULDN'T REALLOC %d bytes, NEXT\n", sz);
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

	memcpy(&(pmt->buffer[pmt->buffer_len]), &buff[m], size - m);
	pmt->progid = progid;
	pmt->buffer_len += size - m;


	mp_msg(MSGT_DEMUX, MSGL_V, "FILL_PMT(prog=%d), PMT_len: %d, IS_START: %d, TSPID: %d\n",
		progid, pmt->buffer_len, is_start, pid);

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
			pmt->es_cnt++;
		}

		pmt->es[idx].descr_length = ((es_base[3] & 0xf) << 8) | es_base[4];


		if(pmt->es[idx].descr_length > section_bytes - 5)
		{
			mp_msg(MSGT_DEMUX, MSGL_ERR, "PARSE_PMT, ES_DESCR_LENGTH TOO LARGE %d > %d, EXIT %d bytes for PMT_ES\n",
				pmt->es[idx].descr_length, section_bytes - 5);
			return -1;
		}


		pmt->es[idx].pid = es_pid;
		pmt->es[idx].type = es_type;

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
			{
				int j;
				for(j = 5; j < pmt->es[idx].descr_length; j += es_base[j+1] + 2)	//possible DVB-AC3
					if(es_base[j] == 0x6a)
						pmt->es[idx].type = AUDIO_A52;
			}
			break;

			case 0x10:
				pmt->es[idx].type = VIDEO_MPEG4;
				break;
			case 0x11:
				pmt->es[idx].type = AUDIO_AAC;
				break;

			case 0x81:
				pmt->es[idx].type = AUDIO_A52;
				break;
		}


		section_bytes -= 5 + pmt->es[idx].descr_length;
		mp_msg(MSGT_DEMUX, MSGL_V, "PARSE_PMT(%d INDEX %d), STREAM: %d, FOUND pid=0x%x (%d), type=0x%x, ES_DESCR_LENGTH: %d, bytes left: %d\n",
			progid, idx, es_count, pmt->es[idx].pid, pmt->es[idx].pid, pmt->es[idx].type, pmt->es[idx].descr_length, section_bytes);


		es_base += 5 + pmt->es[idx].descr_length;

		es_count++;
	}

	return 1;
}


// 0 = EOF or no stream found
// else = [-] number of bytes written to the packet
static int ts_parse(demuxer_t *demuxer , ES_stream_t *es, unsigned char *packet, int probe)
{
	ES_stream_t *tss;
	uint8_t done = 0;
	int buf_size, is_start;
	int len, pid, cc, cc_ok, afc, _read;
	ts_priv_t * priv = (ts_priv_t*) demuxer->priv;
	stream_t *stream = demuxer->stream;
	char *p, tmp[TS_FEC_PACKET_SIZE];
	demux_stream_t *ds = NULL;
	demux_packet_t **dp = NULL;
	int *dp_offset = 0, *buffer_size = 0, *broken = NULL;
	int32_t progid, pid_idx, pid_type;


	while(! done)
	{
		if(stream_eof(stream))
		{
			if(! priv->eof)
			{
				ts_dump_streams(priv);
				demuxer->filepos = stream_tell(demuxer->stream);
				return -1;
			}
			else
				return 0;
		}


		if(! priv->is_synced)
		{
			if(! ts_sync(stream))
			{
				mp_msg(MSGT_DEMUX, MSGL_V, "TS_PARSE: COULDN'T SYNC\n");
				return 0;
			}

			len = stream_read(stream, &packet[1], 3);
			if (len != 3)
				return 0;
		}
		_read = 4;

		if(! priv->is_synced)
		{
			is_start = packet[1] & 0x40;
			pid = ((packet[1] & 0x1f) << 8) | packet[2];
		}
		else
		{
			is_start = priv->is_start;
			pid = priv->last_pid;
		}

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
			priv->ts.pids[pid] 	= tss;
			mp_msg(MSGT_DEMUX, MSGL_INFO, "\nNew TS pid=%u\n", pid);
		}



		if(((pid > 1) && (pid < 16)) || (pid == 8191))		//invalid pid
			continue;

		cc = (packet[3] & 0xf);
		cc_ok = (tss->last_cc < 0) || ((((tss->last_cc + 1) & 0x0f) == cc));
		if(! cc_ok)
		{
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "ts_parse: CCCheck NOT OK: %d -> %d\n", tss->last_cc, cc);
		}
		tss->last_cc = cc;


		if(! priv->is_synced)
		{
			priv->last_afc = 0;
			/* skip adaptation field */
			afc = (packet[3] >> 4) & 3;
			if (afc == 0) /* reserved value */
				continue;
			if (afc == 2) /* adaptation field only */
				continue;
			if (afc == 3)
			{
				int c;
				stream_read(stream, &packet[_read], 1);
				c = packet[_read];
				_read++;

				c = min(c, priv->ts.packet_size - _read);
				stream_read(stream, &packet[_read], c);
				_read += c;

				priv->last_afc = c + 1;
				mp_msg(MSGT_DEMUX, MSGL_DBG2, "AFC: %d\n", priv->last_afc);

				if(_read == priv->ts.packet_size)
				continue;
			}
		}
		else
		{
			_read += priv->last_afc;
			priv->last_afc = 0;
		}

		// PES CONTENT STARTS HERE

		buf_size = priv->ts.packet_size - _read;

		//LET'S PARSE TABLES

		if(pid  == 0)
		{
			stream_read(stream,&packet[_read], buf_size);
			parse_pat(priv, is_start, &packet[_read], buf_size);
			priv->is_synced = 0;
			continue;
		}
		else
		{
			progid = prog_id_in_pat(priv, pid);
			if(((progid != -1) || (pid == 16)))
			{
				stream_read(stream,&packet[_read], buf_size);
				parse_pmt(priv, progid, pid, is_start, &packet[_read], buf_size);
				priv->is_synced = 0;
				continue;
			}
		}



		if(! probe)
		{
			if(((tss->type == VIDEO_MPEG1) || (tss->type == VIDEO_MPEG2) || (tss->type == VIDEO_MPEG4))
				&& (demuxer->video->id == tss->pid))
			{
				ds = demuxer->video;

				dp = &priv->fifo[1].pack;
				dp_offset = &priv->fifo[1].offset;
				buffer_size = &priv->fifo[1].buffer_size;
				broken = &priv->fifo[1].broken;
			}
			else if(((tss->type == AUDIO_MP2) || (tss->type == AUDIO_A52) || (tss->type == AUDIO_LPCM_BE) || (tss->type == AUDIO_AAC))
					&& (demuxer->audio->id == tss->pid))
			{
				ds = demuxer->audio;

				dp = &priv->fifo[0].pack;
				dp_offset = &priv->fifo[0].offset;
				buffer_size = &priv->fifo[0].buffer_size;
				broken = &priv->fifo[0].broken;
			}
			else
			{
				stream_skip(stream, buf_size);
				_read += buf_size;
				continue;
			}

			//IS IT TIME TO QUEUE DATA to the dp_packet?
			if(is_start && (*dp != NULL) && (*dp_offset > 0))
			{
				priv->last_pid = pid;
				priv->is_synced = 1;
				priv->is_start = is_start;

				if(! *broken)
				{
					int ret = *dp_offset;
					resize_demux_packet(*dp, ret);	//shrinked to the right size

					ds_add_packet(ds, *dp);
					mp_msg(MSGT_DEMUX, MSGL_V, "ADDED %d  bytes to %s fifo, PTS=%f\n", ret, (ds == demuxer->audio ? "audio" : "video"), (*dp)->pts);


					*dp = NULL;
					*dp_offset = 0;

					return -ret;
				}
				else
				{
					mp_msg(MSGT_DEMUX, MSGL_V, "BROKEN PES, DISCARDING\n");
					free_demux_packet(*dp);

					*dp = NULL;
					*dp_offset = 0;

					continue;
				}
			}

			priv->last_pid = pid;

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

		priv->is_synced = 0;


		if(is_start)
		{
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "IS_START\n");

			//priv->is_synced = 0;

			p = &packet[_read];
			stream_read(stream, p, buf_size);
			_read += buf_size;

			pid_idx = es_pid_in_pmt(priv->pmt, pid);
			if(pid_idx == -1)
				pid_type = UNKNOWN;
			else
				pid_type = priv->pmt->es[pid_idx].type;


			len = pes_parse2(p, buf_size, es, pid_type);

			if(len)
			{
				if(es->type == UNKNOWN)
					continue;

				es->pid = tss->pid;
				tss->type = es->type;

				if((es->pts < tss->last_pts) && es->pts)
					mp_msg(MSGT_DEMUX, MSGL_V, "BACKWARDS PTS! : NEW: %f -> LAST: %f, PID %d\n", es->pts, tss->last_pts, tss->pid);

				if(es->pts == 0.0f)
					es->pts = tss->pts = tss->last_pts;
				else
					tss->pts = tss->last_pts = es->pts;

				mp_msg(MSGT_DEMUX, MSGL_DBG2, "ts_parse, NEW pid=%d, PSIZE: %u, type=%X, start=%X, len=%d\n",
					es->pid, es->payload_size, es->type, es->start, es->size);

				tss->payload_size = es->payload_size;

				mp_msg(MSGT_DEMUX, MSGL_DBG2, "ts_parse, NOW tss->PSIZE=%u\n", tss->payload_size);

				demuxer->filepos = stream_tell(demuxer->stream) - es->size;

				if(probe)
					return es->size;
				else
				{
					*broken = 0;
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
					(*dp)->pts = es->pts;
					(*dp)->pos = stream_tell(demuxer->stream);
					*broken = 0;
					mp_msg(MSGT_DEMUX, MSGL_DBG2, "INIT PACKET, TYPE=%x, PTS: %f\n", es->type, es->pts);

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
			es->start = &packet[_read];


			if(tss->payload_size)
			{
				sz = min(tss->payload_size, buf_size);
				tss->payload_size -= sz;
				es->size = sz;
			}
			else
			{
				if((es->type == VIDEO_MPEG1) || (es->type == VIDEO_MPEG2) || (es->type == VIDEO_MPEG4))
					sz = es->size = buf_size;
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
				}


				stream_read(stream, &((*dp)->buffer[*dp_offset]), sz);
				*dp_offset += sz;

				if(buf_size - sz)
					stream_skip(stream, buf_size - sz);

				continue;
			}
			else
			{
				stream_read(stream, es->start, sz);
				if(buf_size - sz) stream_read(stream, tmp, buf_size-sz);

				_read += buf_size;

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

static void reset_fifos(ts_priv_t* priv, int a, int v)
{
	if(a)
	{
		priv->fifo[0].pack = NULL;
		priv->fifo[0].offset = 0;
		priv->fifo[0].buffer_size = 0;
		priv->fifo[0].broken = 1;
		priv->is_synced = 0;
	}

	if(v)
	{
		priv->fifo[1].pack = NULL;
		priv->fifo[1].offset = 0;
		priv->fifo[1].buffer_size = 0;
		priv->fifo[1].broken = 1;
		priv->is_synced = 0;
	}
}

void demux_seek_ts(demuxer_t *demuxer,float rel_seek_secs,int flags)
{
	demux_stream_t *d_audio=demuxer->audio;
	demux_stream_t *d_video=demuxer->video;
	sh_audio_t *sh_audio=d_audio->sh;
	sh_video_t *sh_video=d_video->sh;
	ts_priv_t * priv = (ts_priv_t*) demuxer->priv;
	int i, video_stats;

	//================= seek in MPEG ==========================
	off_t newpos = (flags&1)?demuxer->movi_start:demuxer->filepos;

	ts_dump_streams(demuxer->priv);
	reset_fifos(priv, sh_audio != NULL, sh_video != NULL);


	video_stats = (sh_video != NULL);
	if(video_stats)
		video_stats = sh_video->i_bps;

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


	if(newpos<demuxer->movi_start)
  		newpos = demuxer->movi_start = 0;	//begininng of stream

#ifdef _LARGEFILE_SOURCE
	newpos &= ~((long long) (STREAM_BUFFER_SIZE - 1));  /* sector boundary */
#else
	newpos &= ~(STREAM_BUFFER_SIZE - 1);  /* sector boundary */
#endif

	reset_fifos(priv, sh_audio != NULL, sh_video != NULL);

	if(sh_audio != NULL)
		ds_free_packs(d_audio);
	if(sh_video != NULL)
		ds_free_packs(d_video);

	stream_seek(demuxer->stream, newpos);


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
			if(d_video->pts>a_pts)
			{
				skip_audio_frame(sh_audio);  // sync audio
				continue;
			}
		}


		i = sync_video_packet(d_video);
		if(sh_video->format == VIDEO_MPEG1 || sh_video->format == VIDEO_MPEG2)
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



