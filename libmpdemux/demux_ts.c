/*
 * Demultiplexer for MPEG2 Transport Streams.
 *
 * For the purposes of playing video, we make some assumptions about the
 * kinds of TS we have to process. The most important simplification is to
 * assume that the TS contains a single program (SPTS) because this then
 * allows significant simplifications to be made in processing PATs.
 * 
 * WARNING: Quite a hack was required in order to get files by MultiDec played back correctly.
 * If it breaks anything else, just comment out the "#define DEMUX_PVA_MULTIDEC_HACK" below
 * and it will not be compiled in.
 *
 * Feedback is appreciated.
 *
 * written by Matteo Giani
 * based on FFmpeg (libavformat) sources
 *
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
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
#define MAX_PROBE_SIZE	1000000
#define NUM_CONSECUTIVE_TS_PACKETS 32


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

int ts_parse(demuxer_t *demuxer, ES_stream_t *es, unsigned char *packet);

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
	unsigned char buf[buf_size], c, done = 0, *ptr;
	uint32_t _read, i=1, count = 0;
	int cc[NB_PID_MAX], last_cc[NB_PID_MAX], pid, cc_ok;
	uint8_t size = 0;
	off_t pos = 0;
		
	mp_msg(MSGT_DEMUX, MSGL_V, "************Checking for TS************\n");
	
	while(! done)
	{	
	    while(((c=stream_read_char(demuxer->stream)) != 0x47) 
			&& (i < MAX_PROBE_SIZE)
			&& ! demuxer->stream->eof
		) i++; 
	    
	    if(c != 0x47)
	    {
			mp_msg(MSGT_DEMUX, MSGL_V, "NOT A TS FILE1\n");
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
		  done = 1;
	}
	
	mp_msg(MSGT_DEMUX, MSGL_V, "TRIED UP TO POSITION %u, FOUND %x, packet_size= %d\n", i, c, size);
	stream_seek(demuxer->stream, pos);
	
	//return size;

	for(count = 0; count < NB_PID_MAX; count++)
	{
	  cc[count] = last_cc[count] = -1;
	}
	
	done = 0;
	for(count = 0; count < NUM_CONSECUTIVE_TS_PACKETS; count++)
	{
	  ptr = &(buf[size * count]);  
	  pid = ((ptr[1] & 0x1f) << 8) | ptr[2];
	  mp_msg(MSGT_DEMUX, MSGL_V, "BUF: %02x %02x %02x %02x, PID %d, SIZE: %d \n", 
		ptr[0], ptr[1], ptr[2], ptr[3], pid, size);
	
	  if(pid == 8191)
		continue;	
		
	  cc[pid] = (ptr[3] & 0xf);
	  cc_ok = (last_cc[pid] < 0) || ((((last_cc[pid] + 1) & 0x0f) == cc[pid]));
	  mp_msg(MSGT_DEMUX, MSGL_V, "PID %d, COMPARE CC %d AND LAST_CC %d\n", pid, cc[pid], last_cc[pid]);
	  if(! cc_ok)
		return 0;
	
	  last_cc[pid] = cc[pid];
	  done++;
	}
	
    return size;
}


void ts_detect_streams(demuxer_t *demuxer)
{
	int video_found = 0, audio_found = 0;
	off_t pos=0;
	ES_stream_t es;
	int *apid, *vpid, *spid, fapid, fvpid, fspid;
	unsigned char tmp[TS_FEC_PACKET_SIZE];
	sh_video_t *sh_video = demuxer->video->sh;
	sh_audio_t *sh_audio = demuxer->audio->sh;
	
	apid = &(demuxer->audio->id);
	vpid = &(demuxer->video->id);
	spid = &(demuxer->sub->id);
	
	
	mp_msg(MSGT_DEMUXER, MSGL_INFO, "PROBING UP TO %u\n", MAX_PROBE_SIZE);
	while(pos <= MAX_PROBE_SIZE)
	{
	    if(ts_parse(demuxer, &es, tmp))
	    {
		mp_msg(MSGT_DEMUXER, MSGL_V, "TYPE: %x, PID: %d\n", es.type, es.pid);
		if(es.type == VIDEO_MPEG2)
		{
		    sh_video->format = VIDEO_MPEG2;	//MPEG2 video
		    if(*vpid == -1)
			*vpid = fvpid = es.pid;
		    video_found = 1;
		}
		    
		if(es.type == AUDIO_MP2)
		{
		    sh_audio->format = AUDIO_MP2;	//MPEG1L2 audio
		    if(*apid == -1)
			  *apid = fapid = es.pid;
		    audio_found = 1;
		}
		    
		if(es.type == AUDIO_A52)
		{
		    sh_audio->format = AUDIO_A52;	//A52 audio
		    if(*apid == -1)
			  *apid = fapid = es.pid;
		    audio_found = 1;
		  }
		
		  if(es.type == AUDIO_LPCM_BE)		//LPCM AUDIO
		  {
		    sh_audio->format = AUDIO_LPCM_BE;	
		    if(*apid == -1)
			  *apid = fapid = es.pid;
		    audio_found = 1;
		}
		
		pos = stream_tell(demuxer->stream);
		if(video_found && audio_found)
		    break;
	    }
	}
	
	if(video_found)
	    mp_msg(MSGT_DEMUXER, MSGL_INFO, "VIDEO MPEG2(pid=%d)...", fvpid);
	else
	{
	    *vpid = -2;		//WE DIDN'T MATCH ANY VIDEO STREAM, SO WE FORCE THE DEMUXER TO IGNORE VIDEO
	    mp_msg(MSGT_DEMUXER, MSGL_INFO, "NO VIDEO!\n");
	}
	    
	if(sh_audio->format == AUDIO_MP2)
	    mp_msg(MSGT_DEMUXER, MSGL_INFO, "AUDIO MP2(pid=%d)\n", fapid);
	else if(sh_audio->format == AUDIO_A52)
	    mp_msg(MSGT_DEMUXER, MSGL_INFO, "AUDIO A52(pid=%d)\n", fapid);
	else if(sh_audio->format == AUDIO_LPCM_BE)
	    mp_msg(MSGT_DEMUXER, MSGL_INFO, "AUDIO LPCM(pid=%d)\n", fapid);	
	else    
	{
	    *apid = -2;		//WE DIDN'T MATCH ANY AUDIO STREAM, SO WE FORCE THE DEMUXER TO IGNORE AUDIO
	    mp_msg(MSGT_DEMUXER, MSGL_INFO, "NO AUDIO!\n");
	}
}



demuxer_t *demux_open_ts(demuxer_t * demuxer)
{
	int i;
	uint8_t packet_size;
	sh_video_t *sh_video;
	sh_audio_t *sh_audio;
	ts_priv_t * priv = (ts_priv_t*) demuxer->priv;
	
	
	mp_msg(MSGT_DEMUX, MSGL_V, "DEMUX OPEN, AUDIO_ID: %d, VIDEO_ID: %d, SUBTITLE_ID: %d,\n", 
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
		
	
	sh_video = new_sh_video(demuxer, 0);
	sh_video->ds = demuxer->video;
	demuxer->video->sh = sh_video;
	
	sh_audio = new_sh_audio(demuxer, 0);		
	sh_audio->ds = demuxer->audio;
	demuxer->audio->sh = sh_audio;
	
	
	mp_msg(MSGT_DEMUXER,MSGL_INFO, "Opened TS demuxer...");

	if(! ts_fastparse)	
	    ts_detect_streams(demuxer);
	
	
	/*
	demuxer->movi_start = 0;
	demuxer->movi_end = demuxer->stream->end_pos;
	*/
		
	
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
			



static int pes_parse2(unsigned char *buf, uint16_t packet_len, int is_start, ES_stream_t *es) 
{
    unsigned char *p;
    uint32_t       header_len;
    int64_t        pts;
    uint32_t       stream_id;
    uint32_t       pkt_len;

    mp_msg(MSGT_DEMUX, MSGL_DBG2, "pes_parse2(%X, %d, %d, ): \n", buf, packet_len, is_start);

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

    if(! is_start)
    {
	  es->pts = 0.0f;
	es->start = p;
	es->size = packet_len;
	return es->size;
    }
  
    /* we should have a PES packet here */

	mp_msg(MSGT_DEMUX, MSGL_DBG2, "pes_parse2: HEADER %02x %02x %02x %02x\n", p[0], p[1], p[2], p[3]);
    if (p[0] || p[1] || (p[2] != 1)) 
    {
        mp_msg(MSGT_DEMUX, MSGL_DBG2, "pes_parse2: error HEADER %02x %02x %02x (should be 0x000001) \n", p[0], p[1], p[2]);
	return 0 ;
    }

    packet_len -= 6;
  
	es->payload_size = p[4] << 8 | p[5];
    if (es->payload_size == 0)
        es->payload_size = 65535;
		
    stream_id  = p[3];

    if(packet_len==0)
	return 0;


    if (p[7] & 0x80) 
    { /* pts available */
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

    /* sometimes corruption on header_len causes segfault in memcpy below */
    if (header_len + 9 > pkt_len) 
    {
	mp_msg(MSGT_DEMUX, MSGL_DBG2, "demux_ts: illegal value for PES_header_data_length (0x%02x)\n", header_len);
	return 0;
    }

    p += header_len + 9;
    packet_len -= header_len + 3;

    if (stream_id == 0xbd) 
    {
	  /* hack : ac3 track */
	int track, spu_id;
    
	  mp_msg(MSGT_DEMUX, MSGL_V, "pes_parse2: audio buf = %02X %02X %02X %02X %02X %02X %02X %02X, 80: %d\n",
	    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[0] & 0x80);
    
	  track = p[0] & 0x0F; 
	  mp_msg(MSGT_DEMUX, MSGL_DBG2, "AC3 TRACK: %d\n", track);
	  
    
	/*
        * we check the descriptor tag first because some stations
        * do not include any of the ac3 header info in their audio tracks
        * these "raw" streams may begin with a byte that looks like a stream type.
        */
	  if(		   /* ac3 - raw or syncword */ 
    	    (p[0] == 0x0B && p[1] == 0x77))   
	{ 						
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "AC3 SYNCWORD\n");
    	    es->start = p;
    	    es->size = packet_len;
    	    es->type = AUDIO_A52;
    	    return es->size;
	} 
	  /*
	else if (//m->descriptor_tag == 0x06 && 
	    p[0] == 0x20 && p[1] == 0x00) 
	{
    	    // DVBSUB 
    	    long payload_len = ((buf[4] << 8) | buf[5]) - header_len - 3;
    	    es->start = p;
    	    es->size  = packet_len;
    	    es->type = SPU_DVB + payload_len;
    	    return es->size;
	} 
	   
	else if ((p[0] & 0xE0) == 0x20) 
	{
    	    spu_id      = (p[0] & 0x1f);
            es->start   = p+1;
    	    es->size    = packet_len-1;
    		es->type   = SPU_DVD + spu_id;
    	    return es->size;
	} 
	  */
	else if ((p[0] & 0xF0) == 0x80) 
	{
			mp_msg(MSGT_DEMUX, MSGL_DBG2, "AC3 WITH HEADER\n");
    	    es->start   = p+4;
    	    es->size    = packet_len - 4;
    	    es->type = AUDIO_A52; 
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
    	    es->type = AUDIO_LPCM_BE;
    	    return es->size;
	}
    } 
    else if ((stream_id >= 0xbc) && ((stream_id & 0xf0) == 0xe0)) 
    {
	es->start   = p;
	es->size    = packet_len;
	  es->type 		= VIDEO_MPEG2;
	return es->size;
    } 
    else if ((stream_id & 0xe0) == 0xc0) 
    {
	int track;
	track 	    = stream_id & 0x1f;
	es->start   = p;
	es->size    = packet_len;
	  es->type 		= AUDIO_MP2;
	return es->size;
    } 
    else 
    {
	mp_msg(MSGT_DEMUX, MSGL_DBG2, "pes_parse2: unknown packet, id: %x\n", stream_id);
    }

    return 0;
}




int ts_sync(demuxer_t *demuxer)
{
    uint8_t c=0;
    
    mp_msg(MSGT_DEMUX, MSGL_DBG2, "TS_SYNC \n");
    
    while(((c=stream_read_char(demuxer->stream)) != 0x47) && ! demuxer->stream->eof);
    
    if(c == 0x47)
	return c;
    else
	return 0;
}






// 0 = EOF or no stream found
// 1 = successfully read a packet
int ts_parse(demuxer_t * demuxer , ES_stream_t *es, unsigned char *packet)
{
	ES_stream_t *tss;
    uint8_t done = 0;
    uint16_t buf_size, is_start; 
    int len, pid, cc, cc_ok, afc;
    unsigned char *p;
	ts_priv_t * priv = (ts_priv_t*) demuxer->priv;
	
    
    while(! done)
    {
	if(! ts_sync(demuxer))
	{	
	    mp_msg(MSGT_DEMUX, MSGL_V, "TS_FILL_BUFFER: COULDN'T SYNC\n");        
	    return 0;
        }
	
	  len = stream_read(demuxer->stream, &packet[1], priv->ts.packet_size-1);
      if (len != priv->ts.packet_size-1)
            return 0;
        
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
		priv->ts.pids[pid] 	= tss;
            mp_msg(MSGT_DEMUX, MSGL_DBG2, "new TS pid=%u\n", pid);
        }
	
	cc = (packet[3] & 0xf);
        cc_ok = (tss->last_cc < 0) || ((((tss->last_cc + 1) & 0x0f) == cc));
	if(! cc_ok)
	{
	    mp_msg(MSGT_DEMUX, MSGL_DBG2, "ts_parse: CCCheck NOT OK: %d -> %d\n", tss->last_cc, cc);	
	}
        tss->last_cc = cc;        
    
	
        
        /* skip adaptation field */
        afc = (packet[3] >> 4) & 3;
        p = packet + 4;
        if (afc == 0) /* reserved value */
            continue;
        if (afc == 2) /* adaptation field only */
            continue;
        if (afc == 3) 
	{
            /* skip adapation field */
            p += p[0] + 1;
        }
        /* if past the end of packet, ignore */
        if (p >= packet + TS_PACKET_SIZE)
            continue;
    
	// PES CONTENT STARTS HERE
	
	buf_size = TS_PACKET_SIZE - (p - packet);
	
	is_start = packet[1] & 0x40;
	  len = pes_parse2(p, buf_size, is_start, es);
	  
	  if(len)
	{
	    es->pid = tss->pid;
	    
		if(! is_start)
	    es->type = tss->type;
		else
		{
		  tss->type = es->type;
		  tss->payload_size = es->payload_size;
		}
		
		if(es->pts == 0.0f)
		  es->pts = tss->pts = tss->last_pts;
		else
		  tss->pts = tss->last_pts = es->pts;
            
	    mp_msg(MSGT_DEMUX, MSGL_V, "ts_parse, pid=%d, PSIZE: %u, type=%X, start=%X, len=%d\n", es->pid, es->payload_size, es->type, es->start, es->size);
	
	    return len;
	}    
    }
    
    return 0;
}



int demux_ts_fill_buffer(demuxer_t * demuxer)
{
    ES_stream_t es;
    demux_packet_t *dp;
    int len;
    unsigned char packet[TS_FEC_PACKET_SIZE];
    int *apid, *vpid, *spid;
    
    apid = &(demuxer->audio->id);
    vpid = &(demuxer->video->id);
    spid = &(demuxer->sub->id);
    
    while((len = ts_parse(demuxer, &es, packet)) > 0)
    {
	    mp_msg(MSGT_DEMUX, MSGL_V, "NEW_FILL_BUFFER, NEW_ADD_PACKET(%x, %d) type: %x, PTS: %f\n", es.start, es.size, es.type, es.pts);    
	
	    if(es.type == VIDEO_MPEG2)
	    {
		if(ts_fastparse)
		{
		    if(*vpid == -2)
			continue;
		    
		    if(*vpid == -1)
			*vpid = es.pid;
		}
		    
		if(*vpid != es.pid)
	    	    continue;
	
		dp = new_demux_packet(es.size);
		if(! dp || ! dp->buffer)
		{
	    	    fprintf(stderr, "fill_buffer, NEW_ADD_PACKET(%d) FAILED\n", es.size);    
	    	    continue;
		}		
		memcpy(dp->buffer, es.start, es.size); 
		dp->pts = es.pts;
		dp->flags = 0;
		dp->pos = stream_tell(demuxer->stream);
		ds_add_packet(demuxer->video, dp);
		mp_msg(MSGT_DEMUX, MSGL_V, "VIDEO pts=%f\n", es.pts);
		return len;
	    }
	
	    if((es.type == AUDIO_MP2) || (es.type == AUDIO_A52) || (es.type == AUDIO_LPCM_BE))
	    {
		  mp_msg(MSGT_DEMUX, MSGL_V, "FILL_AUDIO %x	\n", es.type);
		  
		if(ts_fastparse)
		{
		    if(*apid == -2)
			continue;
		    
		    if(*apid == -1)
	    		*apid = es.pid;
		}
		        
		if(*apid != es.pid)
	    	    continue;
		    
		dp = new_demux_packet(es.size);
		if(! dp || ! dp->buffer)
		{
	    	    fprintf(stderr, "fill_buffer, NEW_ADD_PACKET(%d) FAILED\n", es.size);    
	    	    continue;
		}
		memcpy(dp->buffer, es.start, es.size);    
		dp->flags = 0;
		dp->pts = es.pts;
		dp->pos = stream_tell(demuxer->stream);
		ds_add_packet(demuxer->audio, dp);    
		mp_msg(MSGT_DEMUX, MSGL_V, "AUDIO pts=%f\r\n", es.pts);
		return len;
	    }
	
	    mp_msg(MSGT_DEMUX, MSGL_V, "SKIP--------\n");    
    }
    return 0;
}




int stringent_ts_sync(demuxer_t *demuxer)
{
    ts_priv_t *priv = demuxer->priv;
    uint8_t c = 0, done = 0, i, buf[TS_FEC_PACKET_SIZE * NUM_CONSECUTIVE_TS_PACKETS];
    off_t pos;
    
    mp_msg(MSGT_DEMUX, MSGL_DBG2, "STRINGENT_TS_SYNC packet_size: %d\n", priv->ts.packet_size);
    
    
    if(! demuxer->seekable)
	return 0;
    
	
    while(! done)
    {   
	while(((c=stream_read_char(demuxer->stream)) != 0x47) && !demuxer->stream->eof);
	
	if(c != 0x47)
	{
	    stream_reset(demuxer->stream);
	    return 0;
	}
	    
	pos = stream_tell(demuxer->stream);
	if(pos < 1)
	    pos = 1;
	mp_msg(MSGT_DEMUX, MSGL_DBG2, "dopo il while, pos=%u\n", pos);
	    
	done = 1;
	buf[0] = c;
	stream_read(demuxer->stream, &buf[1], (priv->ts.packet_size * NUM_CONSECUTIVE_TS_PACKETS) - 1);	
	for(i = 0; i < 5; i++)
	{	
	    if (buf[i * priv->ts.packet_size] != 0x47)
        	done = 0;
	    mp_msg(MSGT_DEMUX, MSGL_DBG2, "i: %d, char:  %x\n", i, buf[i * priv->ts.packet_size]);	
	}
	
	if(done)
	    stream_seek(demuxer->stream, pos); 
	else
	    stream_seek(demuxer->stream, pos); 
    }
    //stream_seek(demuxer->stream, pos+1); 
    mp_msg(MSGT_DEMUX, MSGL_DBG2, "STRINGENT_TS_SYNC, STREAM_POS: %lu\n", stream_tell(demuxer->stream));	
    return 0x47;
}


extern void resync_audio_stream(sh_audio_t *);


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

	/*if(!ts_sync(demuxer))
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "demux_ts: Couldn't seek!\n");
		return 0;
	}
	*/
	
	ds_fill_buffer(d_video);
	if(sh_audio)
	{
	  ds_fill_buffer(d_audio);
	  resync_audio_stream(sh_audio);
	}

	return 1;
}





static int mpegts_read_close(MpegTSContext *ts)
{
    int i;
    for(i=0;i<NB_PID_MAX;i++)
        free(ts->pids[i]);
    return 0;
}



