/*
    Real parser & demuxer
    
    (C) Alex Beregszaszi <alex@naxine.org>
    
    Based on FFmpeg's libav/rm.c.
*/

/*
Some codecs for Real (from Mike Melanson):

RV10: As has been mentioned, H.263-based; not an unreasonable guess
RV20: RealVideo 2.0, nothing known
RV30: RealVideo 3.0,nothing known, but I don't believe I've ever seen any
media encoded with it
DNET: apparently one of their original audio codecs, to be used with music
SIPR: SiprNet, based on ACELP, and is great for compressing voice
COKR(?): Cooker, the fabled G2 audio codec

New infos:

Audio codecs: (supported by RealPlayer8 for Linux)
    ATRC - RealAudio 8 (ATRAC3)
    COOK - RealAudio G2
    DNET - RealAudio 3.0
    SIPR - SiproLab's audio codec

Video codecs: (supported by RealPlayer8 for Linux)
    RV10
    RV20
    RV30
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "bswap.h"

#define MKTAG(a, b, c, d) (a | (b << 8) | (c << 16) | (d << 24))

#define MAX_STREAMS 10

typedef struct {
    int		data_chunk_offset; /* i think for seeking */
    int		num_of_packets;
    int		last_a_stream;
    int 	a_streams[MAX_STREAMS];
    int		last_v_stream;
    int 	v_streams[MAX_STREAMS];
} real_priv_t;

static void get_str(int isbyte, demuxer_t *demuxer, char *buf, int buf_size)
{
    int len, i;
    char *q;
    
    if (isbyte)
	len = stream_read_char(demuxer->stream);
    else
	len = stream_read_word(demuxer->stream);
#if 1
    q = buf;
    for (i = 0; i < len; i++)
	if (i < (buf_size - 1))
	    *q++ = stream_read_char(demuxer->stream);
    *q = 0;
#else
    stream_read(demuxer->stream, buf, buf_size);
#endif
}

static void skip_str(int isbyte, demuxer_t *demuxer)
{
    int len;

    if (isbyte)
	len = stream_read_char(demuxer->stream);
    else
	len = stream_read_word(demuxer->stream);

    stream_skip(demuxer->stream, len);    

    printf("skip_str: %d bytes skipped\n", len);
}

int real_check_file(demuxer_t* demuxer)
{
    real_priv_t *priv;
    int c;

    mp_msg(MSGT_DEMUX,MSGL_V,"Checking for REAL\n");
    
    c = stream_read_dword_le(demuxer->stream);
    if (c == -256)
	return 0; /* EOF */
    if (c != MKTAG('.', 'R', 'M', 'F'))
	return 0; /* bad magic */

    priv = malloc(sizeof(real_priv_t));
    memset(priv, 0, sizeof(real_priv_t));
    demuxer->priv = priv;

    return 1;
}

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
int demux_real_fill_buffer(demuxer_t *demuxer)
{
    real_priv_t *priv = demuxer->priv;
    demux_stream_t *ds = NULL;
    int len;
    int timestamp;
    int stream_id;
    int i;
    int flags;

//    printf("num_of_packets: %d\n", priv->num_of_packets);

loop:
    if (priv->num_of_packets == 0)
	return 0; /* EOF */
    stream_skip(demuxer->stream, 2); /* version */
    len = stream_read_word(demuxer->stream);
    if (len == -256) /* EOF */
	return 0;
    if (len < 12)
    {
	printf("bad packet len (%d)\n", len);
	stream_skip(demuxer->stream, len);
	goto loop;
    }
    stream_id = stream_read_word(demuxer->stream);
    timestamp = stream_read_dword(demuxer->stream);

    stream_skip(demuxer->stream, 1); /* reserved */
    flags = stream_read_char(demuxer->stream);
    /* flags:		*/
    /* 	0x2 - keyframe	*/

//    printf("packet#%d: len: %d, stream_id: %d, timestamp: %d, flags: %x\n",
//	priv->num_of_packets, len, stream_id, timestamp, flags);

    priv->num_of_packets--;
    len -= 12;    

    /* check if stream_id is audio stream */
    for (i = 0; i < priv->last_a_stream; i++)
    {
	if (priv->a_streams[i] == stream_id)
	{
//	    printf("packet is audio (id: %d)\n", stream_id);
	    ds = demuxer->audio; /* FIXME */
	    break;
	}
    }
    /* check if stream_id is video stream */
    for (i = 0; i < priv->last_v_stream; i++)
    {
	if (priv->v_streams[i] == stream_id)
	{
//	    printf("packet is video (id: %d)\n", stream_id);
	    ds = demuxer->video; /* FIXME */
	    break;
	}
    }

    /* id not found */
    if (ds == NULL)
    {
	printf("unknown stream id (%d)\n", stream_id);
	stream_skip(demuxer->stream, len);
	goto loop;	
    }

    demuxer->filepos = stream_tell(demuxer->stream);
    ds_read_packet(ds, demuxer->stream, len, timestamp/90000.0f,
	demuxer->filepos, (flags & 0x2) ? 0x10 : 0);

    return 1;
}

void demux_open_real(demuxer_t* demuxer)
{
    real_priv_t* priv = demuxer->priv;
    int num_of_headers;
    int i;

    stream_skip(demuxer->stream, 4); /* header size */
    stream_skip(demuxer->stream, 2);
    stream_skip(demuxer->stream, 4);
    num_of_headers = stream_read_dword(demuxer->stream);
//    stream_skip(demuxer->stream, 4); /* number of headers */

    /* parse chunks */
    for (i = 1; i < num_of_headers; i++)
    {
	int chunk, chunk_size;
	
	chunk = stream_read_dword_le(demuxer->stream);
	chunk_size = stream_read_dword(demuxer->stream);

	stream_skip(demuxer->stream, 2);
	
	if (chunk_size < 10)
	    goto fail;
	
	printf("Chunk: %.4s (size: %d)\n",
	    (char *)&chunk, chunk_size);
	
	switch(chunk)
	{
	    case MKTAG('P', 'R', 'O', 'P'):
//		printf("Properties chunk\n");
		stream_skip(demuxer->stream, 4); /* max bitrate */
		stream_skip(demuxer->stream, 4); /* avg bitrate */
		stream_skip(demuxer->stream, 4); /* max packet size */
		stream_skip(demuxer->stream, 4); /* avg packet size */
		stream_skip(demuxer->stream, 4); /* nb packets */
		stream_skip(demuxer->stream, 4); /* duration */
		stream_skip(demuxer->stream, 4); /* preroll */
		stream_skip(demuxer->stream, 4); /* index offset */
//		stream_skip(demuxer->stream, 4); /* data offset */
		priv->data_chunk_offset = stream_read_dword(demuxer->stream)+10;
		printf("Data chunk offset: 0x%x\n", priv->data_chunk_offset);
		stream_skip(demuxer->stream, 2); /* nb streams */
#if 0
		stream_skip(demuxer->stream, 2); /* flags */
#else
		{
		    int flags = stream_read_word(demuxer->stream);
		    printf("Flags (%x): ", flags);
		    if (flags & 0x1)
			printf("[save allowed] ");
		    if (flags & 0x2)
			printf("[perfect play (?)] ");
		    if (flags & 0x4)
			printf("[live broadcast] ");
		    printf("\n");
		}
#endif
		break;
	    case MKTAG('C', 'O', 'N', 'T'):
	    {
		char *buf;
		int len;

//		printf("Broadcasting informations (title, author, copyright, comment)\n");

		len = stream_read_word(demuxer->stream);
		if (len > 0)
		{
		    buf = malloc(len+1);
		    stream_read(demuxer->stream, buf, len);
		    demux_info_add(demuxer, "name", buf);
		    free(buf);
		}

		len = stream_read_word(demuxer->stream);
		if (len > 0)
		{
		    buf = malloc(len+1);
		    stream_read(demuxer->stream, buf, len);
		    demux_info_add(demuxer, "author", buf);
		    free(buf);
		}

		len = stream_read_word(demuxer->stream);
		if (len > 0)
		{
		    buf = malloc(len+1);
		    stream_read(demuxer->stream, buf, len);
		    demux_info_add(demuxer, "copyright", buf);
		    free(buf);
		}

		len = stream_read_word(demuxer->stream);
		if (len > 0)
		{
		    buf = malloc(len+1);
	    	    stream_read(demuxer->stream, buf, len);
		    demux_info_add(demuxer, "comment", buf);
		    free(buf);
		}

//		skip_str(0, demuxer);	/* title */
//		skip_str(0, demuxer);	/* author */
//		skip_str(0, demuxer);	/* copyright */
//		skip_str(0, demuxer);	/* comment */
		break;
	    }
	    case MKTAG('M', 'D', 'P', 'R'):
	    {
		/* new stream */
		int stream_id;
		int bitrate;
		int codec_data_size;
		int codec_pos;
		int tmp;

		stream_id = stream_read_word(demuxer->stream);
		printf("Found new stream (id: %d)\n", stream_id);
		
		stream_skip(demuxer->stream, 4); /* max bitrate */
		bitrate = stream_read_dword(demuxer->stream); /* bitrate */
		stream_skip(demuxer->stream, 4); /* max packet size */
		stream_skip(demuxer->stream, 4); /* avg packet size */
		stream_skip(demuxer->stream, 4); /* start time */
		stream_skip(demuxer->stream, 4); /* preroll */
		stream_skip(demuxer->stream, 4); /* duration */
		
		skip_str(1, demuxer);	/* stream description */
		skip_str(1, demuxer);	/* mimetype */
		
		codec_data_size = stream_read_dword(demuxer->stream);
		codec_pos = stream_tell(demuxer->stream);

		tmp = stream_read_dword(demuxer->stream);
		if (tmp == MKTAG(0xfd, 'a', 'r', '.')) /* audio header */
		{
		    sh_audio_t *sh = new_sh_audio(demuxer, stream_id);
		    char buf[128]; /* for codec name */
		    int frame_size;

		    printf("Found audio stream!\n");
		    stream_skip(demuxer->stream, 2); /* version (4 or 5) */
		    stream_skip(demuxer->stream, 2);
		    stream_skip(demuxer->stream, 4); /* .ra4 or .ra5 */
		    stream_skip(demuxer->stream, 4);
		    stream_skip(demuxer->stream, 2); /* version (4 or 5) */
		    stream_skip(demuxer->stream, 4); /* header size */
		    stream_skip(demuxer->stream, 2); /* add codec info */
		    stream_skip(demuxer->stream, 4); /* coded frame size */
		    stream_skip(demuxer->stream, 4);
		    stream_skip(demuxer->stream, 4);
		    stream_skip(demuxer->stream, 4);
		    stream_skip(demuxer->stream, 2); /* 1 */
//		    stream_skip(demuxer->stream, 2); /* coded frame size */
		    frame_size = stream_read_word(demuxer->stream);
		    printf("frame_size: %d\n", frame_size);
		    stream_skip(demuxer->stream, 4);

		    sh->samplerate = stream_read_word(demuxer->stream);
		    stream_skip(demuxer->stream, 4);
		    sh->channels = stream_read_word(demuxer->stream);
		    printf("samplerate: %d, channels: %d\n",
			sh->samplerate, sh->channels);
		
		    /* Desc #1 */
		    skip_str(1, demuxer);
		    /* Desc #2 */
		    get_str(1, demuxer, buf, sizeof(buf));

		    tmp = 1; /* supported audio codec */
		    switch (MKTAG(buf[0], buf[1], buf[2], buf[3]))
		    {
			case MKTAG('a', 't', 'r', 'c'):
			    break;
			case MKTAG('d', 'n', 'e', 't'):
			    printf("Audio: DNET -> AC3\n");
			    sh->format = 0x2000;
			    break;
			case MKTAG('s', 'i', 'p', 'r'):
			    printf("Audio: SiproLab's ACELP.net\n");
			    sh->format = 0x130;
			    break;
			case MKTAG('c', 'o', 'o', 'k'):
			    printf("Audio: Real's GeneralCooker (unsupported)\n");
			    tmp = 0;
			    break;
			default:
			    printf("Audio: Unknown (%s)\n", buf);
			    tmp = 0;
			    sh->format = MKTAG(buf[0], buf[1], buf[2], buf[3]);
		    }

		    if (tmp)
		    {
			/* Emulate WAVEFORMATEX struct: */
			sh->wf = malloc(sizeof(WAVEFORMATEX));
			memset(sh->wf, 0, sizeof(WAVEFORMATEX));
			sh->wf->wFormatTag = sh->format;
			sh->wf->nChannels = sh->channels;
			sh->wf->wBitsPerSample = 16;
			sh->wf->nSamplesPerSec = sh->samplerate;
			sh->wf->nAvgBytesPerSec = bitrate;
			sh->wf->nBlockAlign = frame_size; /* 19 for acelp, pff */
			sh->wf->cbSize = 0;
		    
			/* insert as stream */
			demuxer->audio->sh = sh;
			sh->ds = demuxer->audio;
			demuxer->audio->id = stream_id;
		    
			if (priv->last_a_stream+1 < MAX_STREAMS)
			{
			    priv->a_streams[priv->last_a_stream] = stream_id;
			    priv->last_a_stream++;
			}
		    }
		}
		else
		{
		    /* video header */
		    sh_video_t *sh = new_sh_video(demuxer, stream_id);

		    tmp = stream_read_dword_le(demuxer->stream);
		    printf("video: %.4s (%x)\n", (char *)&tmp, tmp);
		    if (tmp != MKTAG('V', 'I', 'D', 'O'))
		    {
			mp_msg(MSGT_DEMUX, MSGL_ERR, "Unsupported video codec\n");
			goto skip_this_chunk;
		    }
		    
		    sh->format = stream_read_dword_le(demuxer->stream); /* fourcc */
		    printf("video fourcc: %.4s (%x)\n", (char *)&sh->format, sh->format);

		    /* emulate BITMAPINFOHEADER */
		    sh->bih = malloc(sizeof(BITMAPINFOHEADER));
		    memset(sh->bih, 0, sizeof(BITMAPINFOHEADER));
	    	    sh->bih->biSize = 40;
		    sh->disp_w = sh->bih->biWidth = stream_read_word(demuxer->stream);
		    sh->disp_h = sh->bih->biHeight = stream_read_word(demuxer->stream);
		    sh->bih->biPlanes = 1;
		    sh->bih->biBitCount = 24;
		    sh->bih->biCompression = sh->format;
		    sh->bih->biSizeImage= sh->bih->biWidth*sh->bih->biHeight*3;

		    sh->fps = stream_read_word(demuxer->stream);
		    sh->frametime = 1.0f/sh->fps;
		    
		    stream_skip(demuxer->stream, 4);
		    stream_skip(demuxer->stream, 2);
		    stream_skip(demuxer->stream, 4);
		    stream_skip(demuxer->stream, 2);

		    /* h263 hack */
		    tmp = stream_read_dword(demuxer->stream);
		    switch (tmp)
		    {
			case 0x10000000:
			    /* sub id: 0 */
			    /* codec id: rv10 */
			    break;
			case 0x10003000:
			case 0x10003001:
			    /* sub id: 3 */
			    /* codec id: rv10 */
			    sh->bih->biCompression = sh->format = mmioFOURCC('R', 'V', '1', '3');
			    break;
			case 0x20001000:
			case 0x20100001:
			    /* codec id: rv20 */
			    break;
			default:
			    /* codec id: none */
			    printf("unknown id: %x\n", tmp);
		    }
		        
		    /* insert as stream */
		    demuxer->video->sh = sh;
		    sh->ds = demuxer->video;
		    demuxer->video->id = stream_id;
		    if (priv->last_v_stream+1 < MAX_STREAMS)
		    {
			priv->v_streams[priv->last_v_stream] = stream_id;
			priv->last_v_stream++;
		    }
		}

skip_this_chunk:
		/* skip codec info */
		tmp = stream_tell(demuxer->stream) - codec_pos;
		stream_skip(demuxer->stream, codec_data_size - tmp);
		break;
	    }
	    case MKTAG('D', 'A', 'T', 'A'):
		goto header_end;
	    case MKTAG('I', 'N', 'D', 'X'):
	    default:
		printf("Unknown chunk: %x\n", chunk);
		stream_skip(demuxer->stream, chunk_size - 10);
		break;
	}
    }

header_end:
    priv->num_of_packets = stream_read_dword(demuxer->stream);
//    stream_skip(demuxer->stream, 4); /* number of packets */
    stream_skip(demuxer->stream, 4); /* next data header */

    printf("Packets in file: %d\n", priv->num_of_packets);

    /* disable seeking */
    demuxer->seekable = 0;

fail:
    return;
}

void demux_close_real(demuxer_t *demuxer)
{
    real_priv_t* priv = demuxer->priv;
 
    if (priv)
	free(priv);

    return;
}
