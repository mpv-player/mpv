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
#define MAX_PROBE_SIZE	1000000
#define NUM_CONSECUTIVE_TS_PACKETS 32
#define NUM_CONSECUTIVE_AUDIO_PACKETS 348


int ts_fastparse = 0;

typedef enum
{
	UNKNOWN		= -1,
	VIDEO_MPEG2 	= 0x10000002,
	AUDIO_MP2   	= 0x50,
	AUDIO_A52   	= 0x2000,
	AUDIO_LPCM_BE  	= 0x10001
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
	MpegTSContext ts;
} ts_priv_t;


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
	int cc[NB_PID_MAX], last_cc[NB_PID_MAX], pid, cc_ok, c;
	uint8_t size = 0;
	off_t pos = 0;

	mp_msg(MSGT_DEMUX, MSGL_V, "Checking for TS...\n");

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
			mp_msg(MSGT_DEMUX, MSGL_V, "NOT A TS FILE1\n");
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
			return 0;

		last_cc[pid] = cc[pid];
		done++;
	}

	return size;
}




static void ts_detect_streams(demuxer_t *demuxer, uint32_t *a,  uint32_t *v, int *fapid, int *fvpid)
{
	int video_found = 0, audio_found = 0, i, num_packets = 0;
	off_t pos=0;
	ES_stream_t es;
	unsigned char tmp[TS_FEC_PACKET_SIZE];
	ts_priv_t *priv = (ts_priv_t*) demuxer->priv;


	mp_msg(MSGT_DEMUXER, MSGL_INFO, "PROBING UP TO %u\n", MAX_PROBE_SIZE);
	while(pos <= MAX_PROBE_SIZE)
	{
		if(ts_parse(demuxer, &es, tmp, 1))
		{
			mp_msg(MSGT_DEMUXER, MSGL_DBG2, "TYPE: %x, PID: %d\n", es.type, es.pid);


			if((*fvpid == -1) || (*fvpid == es.pid))
			{
				if(es.type == VIDEO_MPEG2)
				{
					*v = VIDEO_MPEG2;
					*fvpid = es.pid;
					video_found = 1;
				}
			}

			if(((*fvpid == -2) || (num_packets >= NUM_CONSECUTIVE_AUDIO_PACKETS)) && audio_found)
			{
				//novideo or we have at least 348 audio packets (64 KB) without video (TS with audio only)
				*v = 0;
				break;
			}


			if((*fapid == -1) || (*fapid == es.pid))
			{
				if(es.type == AUDIO_MP2)
				{
					*a = AUDIO_MP2;	//MPEG1L2 audio
					*fapid = es.pid;
					audio_found = 1;
				}

				if(es.type == AUDIO_A52)
				{
					*a = AUDIO_A52;	//A52 audio
					*fapid = es.pid;
					audio_found = 1;
				}

				if(es.type == AUDIO_LPCM_BE)		//LPCM AUDIO
				{
					*a = AUDIO_LPCM_BE;
					*fapid = es.pid;
					audio_found = 1;
				}
			}

			if(audio_found && (*fapid == es.pid) && (! video_found))
			  num_packets++;

			if((*fapid == -2) && video_found)
			{
				*a = 0;
				break;
			}

			pos = stream_tell(demuxer->stream);
			if(video_found && audio_found)
			break;
		}
	}

	if(video_found)
	    mp_msg(MSGT_DEMUXER, MSGL_INFO, "VIDEO MPEG2(pid=%d)...", *fvpid);
	else
	{
	    //WE DIDN'T MATCH ANY VIDEO STREAM
	    mp_msg(MSGT_DEMUXER, MSGL_INFO, "NO VIDEO!\n");
	}

	if(*a == AUDIO_MP2)
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "AUDIO MP2(pid=%d)\n", *fapid);
	else if(*a == AUDIO_A52)
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "AUDIO A52(pid=%d)\n", *fapid);
	else if(*a == AUDIO_LPCM_BE)
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "AUDIO LPCM(pid=%d)\n", *fapid);
	else
	{
		//WE DIDN'T MATCH ANY AUDIO STREAM, SO WE FORCE THE DEMUXER TO IGNORE AUDIO
		mp_msg(MSGT_DEMUXER, MSGL_INFO, "NO AUDIO!\n");
	}

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

	mp_msg(MSGT_DEMUX, MSGL_DBG2, "DEMUX OPEN, AUDIO_ID: %d, VIDEO_ID: %d, SUBTITLE_ID: %d,\n",
		demuxer->audio->id, demuxer->video->id, demuxer->sub->id);


	demuxer->type= DEMUXER_TYPE_MPEG_TS;

	stream_reset(demuxer->stream);
	stream_seek(demuxer->stream, 0);

	packet_size = ts_check_file(demuxer);
	if(!packet_size)
	    return NULL;

	priv = malloc(sizeof(ts_priv_t));

	for(i=0; i < 8192; i++)
	    priv->ts.pids[i] = NULL;
	priv->ts.packet_size = packet_size;


	demuxer->priv = priv;

	if(demuxer->stream->type != STREAMTYPE_FILE) demuxer->seekable=0;
	else demuxer->seekable = 1;


	ts_detect_streams(demuxer, &at, &vt, &demuxer->audio->id, &demuxer->video->id);
	mp_msg(MSGT_DEMUXER,MSGL_INFO, "Opened TS demuxer2, audio: %x(pid %d), video: %x(pid %d)...\n", at, demuxer->audio->id, vt, demuxer->video->id);


	if(vt)
	{
		sh_video = new_sh_video(demuxer, 0);
		sh_video->ds = demuxer->video;
		sh_video->format = vt;
		demuxer->video->sh = sh_video;

		mp_msg(MSGT_DEMUXER,MSGL_INFO, "OPENED_SH_VIDEO, VD: %x\n");
	}

	if(at)
	{
		sh_audio = new_sh_audio(demuxer, 0);
		sh_audio->ds = demuxer->audio;
		sh_audio->format = at;
		demuxer->audio->sh = sh_audio;

		mp_msg(MSGT_DEMUXER,MSGL_INFO, "OPENED_SH_AUDIO\n");
	}


	mp_msg(MSGT_DEMUXER,MSGL_INFO, "Opened TS demuxer...");


	demuxer->movi_start = 0;
	demuxer->movi_end = demuxer->stream->end_pos;

	stream_seek(demuxer->stream, 0);		//IF IT'S FROM A PIPE IT WILL FAIL, BUT WHO CARES?

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





static int pes_parse2(unsigned char *buf, uint16_t packet_len, ES_stream_t *es)
{
	unsigned char  *p;
	uint32_t       header_len;
	int64_t        pts, escr, dts;
	uint32_t       stream_id;
	uint32_t       pkt_len, es_rate;
	//NEXT might be needed:
	//uint8_t        es_rate_flag, escr_flag, pts_flag;

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

	/* sometimes corruption on header_len causes segfault in memcpy below */
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
		/* hack : ac3 track */
		int track, spu_id;

		mp_msg(MSGT_DEMUX, MSGL_DBG3, "pes_parse2: audio buf = %02X %02X %02X %02X %02X %02X %02X %02X, 80: %d\n",
			p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[0] & 0x80);

		track = p[0] & 0x0F;
		mp_msg(MSGT_DEMUX, MSGL_DBG2, "AC3 TRACK: %d\n", track);


		/*
		* we check the descriptor tag first because some stations
		* do not include any of the ac3 header info in their audio tracks
		* these "raw" streams may begin with a byte that looks like a stream type.
		*/
		if(	/* ac3 - raw or syncword */
			(p[0] == 0x0B && p[1] == 0x77))
		{
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "AC3 SYNCWORD\n");
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






// 0 = EOF or no stream found
// 1 = successfully read a packet
static int ts_parse(demuxer_t *demuxer , ES_stream_t *es, unsigned char *packet, int probe)
{
	ES_stream_t *tss;
	uint8_t done = 0;
	int buf_size, is_start;
	int len, pid, last_pid, cc, cc_ok, afc, _read;
	ts_priv_t * priv = (ts_priv_t*) demuxer->priv;
	stream_t *stream = demuxer->stream;
	char *p, tmp[TS_FEC_PACKET_SIZE];
	demux_stream_t *ds;
	demux_packet_t *dp;


	while(! done)	//while pid=last_pid add_to_buffer
	{
		if(! ts_sync(stream))
		{
			mp_msg(MSGT_DEMUX, MSGL_V, "TS_PARSE: COULDN'T SYNC\n");
			return 0;
		}

		len = stream_read(stream, &packet[1], 3);
		if (len != 3)
			return 0;
		_read = 4;

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
			priv->ts.pids[pid] 	= tss;
			mp_msg(MSGT_DEMUX, MSGL_V, "new TS pid=%u\n", pid);
		}

		if((pid < 16) || (pid == 8191))		//invalid pid
			continue;
		cc = (packet[3] & 0xf);
		cc_ok = (tss->last_cc < 0) || ((((tss->last_cc + 1) & 0x0f) == cc));
		if(! cc_ok)
		{
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "ts_parse: CCCheck NOT OK: %d -> %d\n", tss->last_cc, cc);
		}
		tss->last_cc = cc;

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

			if(_read == priv->ts.packet_size)
			continue;
		}

		// PES CONTENT STARTS HERE

		buf_size = priv->ts.packet_size - _read;

		if(! probe)
		{
			if((tss->type == VIDEO_MPEG2) && (demuxer->video->id == tss->pid))
				ds = demuxer->video;
			else if(((tss->type == AUDIO_MP2) || (tss->type == AUDIO_A52) || (tss->type == AUDIO_LPCM_BE))
					&& (demuxer->audio->id == tss->pid))
				ds = demuxer->audio;
			else
			{
				stream_read(stream, tmp, buf_size);
				_read += buf_size;
				continue;
			}
		}


		if(is_start)
		{
			p = &packet[_read];
			stream_read(stream, p, buf_size);
			_read += buf_size;

			len = pes_parse2(p, buf_size, es);

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

				mp_msg(MSGT_DEMUX, MSGL_V, "ts_parse, NOW tss->PSIZE=%u\n", tss->payload_size);


				if(probe)
					return es->size;
				else
				{
					dp = new_demux_packet(es->size);
					if(! dp || ! dp->buffer)
					{
						fprintf(stderr, "fill_buffer, NEW_ADD_PACKET(%d)FAILED\n", es->size);
						continue;
					}

					memcpy(dp->buffer, es->start, es->size);
					dp->flags = 0;
					dp->pts = es->pts;
					dp->pos = stream_tell(demuxer->stream);
					ds_add_packet(ds, dp);

					return -es->size;
				}
			}
		}
		else
		{
			uint16_t sz;

			if(tss->type == UNKNOWN)
			{
				stream_read(stream, tmp, buf_size);
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
				if(es->type == VIDEO_MPEG2)
					sz = es->size = buf_size;
				else
				{
					stream_read(stream, tmp, buf_size);
					continue;
				}
			}



			if(! probe)
			{
				ds_read_packet(ds, stream, sz, tss->last_pts, stream_tell(stream), 0);
				if(buf_size - sz)
					stream_read(stream, tmp, buf_size - sz);

				mp_msg(MSGT_DEMUX, MSGL_V, "DOPO DS_READ_PACKET pid %d, size %d DS=%p\n", tss->pid, sz, ds);
				return -sz;

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


int demux_ts_fill_buffer(demuxer_t * demuxer)
{
	ES_stream_t es;
	char packet[TS_FEC_PACKET_SIZE];

	return -ts_parse(demuxer, &es, packet, 0);
}




int demux_seek_ts(demuxer_t * demuxer, float rel_seek_secs, int flags)
{
	int total_bitrate=0;
	off_t dest_offset;
	ts_priv_t * priv = demuxer->priv;
	int a_bps, v_bps;
	demux_stream_t *d_audio=demuxer->audio;
	demux_stream_t *d_video=demuxer->video;
	sh_audio_t *sh_audio=d_audio->sh;
	sh_video_t *sh_video=d_video->sh;


	/*
	 * Compute absolute offset inside the stream. Approximate total bitrate with sum of bitrates
	 * reported by the audio and video codecs. The seek is not accurate because, just like
	 * with MPEG streams, the bitrate is not constant. Moreover, we do not take into account
	 * the overhead caused by PVA and PES headers.
	 * If the calculated absolute offset is negative, seek to the beginning of the file.

	 */


	if(demuxer->audio->id != -2)
	{
		a_bps = ((sh_audio_t *)demuxer->audio->sh)->i_bps;
		total_bitrate += a_bps;
	}

	if(demuxer->video->id != -2)
	{
		v_bps = ((sh_video_t *)demuxer->video->sh)->i_bps;
		total_bitrate += v_bps;
	}

	if(! total_bitrate)
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "SEEK_TS, couldn't determine bitrate, no seek\n");
		return 0;
	}

	dest_offset = stream_tell(demuxer->stream) + rel_seek_secs*total_bitrate;
	if(dest_offset < 0) dest_offset = 0;

	mp_msg(MSGT_DEMUX, MSGL_V, "SEEK TO: %f, BITRATE: %lu, FINAL_POS: %u \n", rel_seek_secs, total_bitrate, dest_offset);

	stream_seek(demuxer->stream, dest_offset);

	ds_fill_buffer(d_video);
	if(sh_audio)
	{
		ds_fill_buffer(d_audio);
		resync_audio_stream(sh_audio);
	}

	return 1;
}


