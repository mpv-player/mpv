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

typedef struct {
	uint8_t *buf;
	uint64_t size;	/// amount of time of data packets pushed to demuxer->audio (in bytes)
	float time;	/// amount of time elapsed based upon samples_per_frame/sample_rate (in milliseconds)
	float last_pts; /// last pts seen
	int bitrate;	/// bitrate computed as size/time
} aac_priv_t;

/// \param srate (out) sample rate
/// \param num (out) number of audio frames in this ADTS frame
/// \return size of the ADTS frame in bytes
/// aac_parse_frames needs a buffer at least 8 bytes long
int aac_parse_frame(uint8_t *buf, int *srate, int *num)
{
	int i = 0, sr, fl = 0, id;
	static int srates[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 0, 0, 0};
	
	if((buf[i] != 0xFF) || ((buf[i+1] & 0xF6) != 0xF0))
		return 0;
	
	id = (buf[i+1] >> 3) & 0x01;	//id=1 mpeg2, 0: mpeg4
	sr = (buf[i+2] >> 2)  & 0x0F;
	if(sr > 11)
		return 0;
	*srate = srates[sr];

	fl = ((buf[i+3] & 0x03) << 11) | (buf[i+4] << 3) | ((buf[i+5] >> 5) & 0x07);
	*num = (buf[i+6] & 0x02) + 1;

	return fl;
}

static int demux_aac_init(demuxer_t *demuxer)
{
	aac_priv_t *priv;
	
	priv = calloc(1, sizeof(aac_priv_t));
	if(!priv)
		return 0;
	
	priv->buf = (uint8_t*) malloc(8);
	if(!priv->buf)
	{
		free(priv);
		return 0;
	}

	demuxer->priv = priv;
	return 1;
}

static void demux_close_aac(demuxer_t *demuxer)
{
	aac_priv_t *priv = (aac_priv_t *) demuxer->priv;
	
	if(!priv)
		return;

	if(priv->buf)
		free(priv->buf);

	free(demuxer->priv);

	return;
}

/// returns DEMUXER_TYPE_AAC if it finds 8 ADTS frames in 32768 bytes, 0 otherwise
static int demux_aac_probe(demuxer_t *demuxer)
{
	int cnt = 0, c, len, srate, num;
	off_t init, probed;
	aac_priv_t *priv;
	
	if(! demux_aac_init(demuxer))
	{
		mp_msg(MSGT_DEMUX, MSGL_ERR, "COULDN'T INIT aac_demux, exit\n");
		return 0;
	}
	
	priv = (aac_priv_t *) demuxer->priv;

	init = probed = stream_tell(demuxer->stream);
	while(probed-init <= 32768 && cnt < 8)
	{
		c = 0;
		while(c != 0xFF)
		{
			c = stream_read_char(demuxer->stream);
			if(c < 0)
				goto fail;
		}
		priv->buf[0] = 0xFF;
		if(stream_read(demuxer->stream, &(priv->buf[1]), 7) < 7)
			goto fail;
		
		len = aac_parse_frame(priv->buf, &srate, &num);
		if(len > 0)
		{
			cnt++;
			stream_skip(demuxer->stream, len - 8);
		}
		probed = stream_tell(demuxer->stream);
	}

	stream_seek(demuxer->stream, init);
	if(cnt < 8)
		goto fail;
	
	mp_msg(MSGT_DEMUX, MSGL_V, "demux_aac_probe, INIT: %"PRIu64", PROBED: %"PRIu64", cnt: %d\n", init, probed, cnt);
	return DEMUXER_TYPE_AAC;

fail:
	mp_msg(MSGT_DEMUX, MSGL_V, "demux_aac_probe, failed to detect an AAC stream\n");
	return 0;
}

static demuxer_t* demux_aac_open(demuxer_t *demuxer)
{
	sh_audio_t *sh;

	sh = new_sh_audio(demuxer, 0);
	sh->ds = demuxer->audio;
	sh->format = mmioFOURCC('M', 'P', '4', 'A');
	demuxer->audio->sh = sh;

	demuxer->filepos = stream_tell(demuxer->stream);
	
	return demuxer;
}

static int demux_aac_fill_buffer(demuxer_t *demuxer, demux_stream_t *ds)
{
	aac_priv_t *priv = (aac_priv_t *) demuxer->priv;
	demux_packet_t *dp;
	int c1, c2, len, srate, num;
	float tm = 0;

	if(demuxer->stream->eof || (demuxer->movi_end && stream_tell(demuxer->stream) >= demuxer->movi_end))
        	return 0;

	while(! demuxer->stream->eof)
	{
		c1 = c2 = 0;
		while(c1 != 0xFF)
		{
			c1 = stream_read_char(demuxer->stream);
			if(c1 < 0)
				return 0;
		}
		c2 = stream_read_char(demuxer->stream);
		if(c2 < 0)
			return 0;
		if((c2 & 0xF6) != 0xF0)
			continue;

		priv->buf[0] = (unsigned char) c1;
		priv->buf[1] = (unsigned char) c2;
		if(stream_read(demuxer->stream, &(priv->buf[2]), 6) < 6)
			return 0;
		
		len = aac_parse_frame(priv->buf, &srate, &num);
		if(len > 0)
		{
			dp = new_demux_packet(len);
			if(! dp)
			{
				mp_msg(MSGT_DEMUX, MSGL_ERR, "fill_buffer, NEW_ADD_PACKET(%d)FAILED\n", len);
				return 0;
			}
			
			
			memcpy(dp->buffer, priv->buf, 8);
			stream_read(demuxer->stream, &(dp->buffer[8]), len-8);
			if(srate)
				tm = (float) (num * 1024.0/srate);
			priv->last_pts += tm;
			dp->pts = priv->last_pts;
			//fprintf(stderr, "\nPTS: %.3f\n", dp->pts);
			ds_add_packet(demuxer->audio, dp);
			priv->size += len;
			priv->time += tm;
			
			priv->bitrate = (int) (priv->size / priv->time);
			demuxer->filepos = stream_tell(demuxer->stream);
			
			return len;
		}
		else
			stream_skip(demuxer->stream, -6);
	}

	return 0;
}


//This is an almost verbatim copy of high_res_mp3_seek(), from demux_audio.c
static void demux_aac_seek(demuxer_t *demuxer, float rel_seek_secs, float audio_delay, int flags)
{
	aac_priv_t *priv = (aac_priv_t *) demuxer->priv;
	demux_stream_t *d_audio=demuxer->audio;
	sh_audio_t *sh_audio=d_audio->sh;
	float time;

	ds_free_packs(d_audio);

	time = (flags & 1) ? rel_seek_secs - priv->last_pts : rel_seek_secs;
	if(time < 0) 
	{
		stream_seek(demuxer->stream, demuxer->movi_start);
		time = priv->last_pts + time;
		priv->last_pts = 0;
	}

	if(time > 0)
	{
		int len, nf, srate, num;

		nf = time * sh_audio->samplerate/1024;
		
		while(nf > 0) 
		{
			if(stream_read(demuxer->stream,priv->buf, 8) < 8)
				break;
			len = aac_parse_frame(priv->buf, &srate, &num);
			if(len <= 0) 
			{
				stream_skip(demuxer->stream, -7);
				continue;
			}
			stream_skip(demuxer->stream, len - 8);
			priv->last_pts += (float) (num*1024.0/srate);
			nf -= num;
		}
	}
}


demuxer_desc_t demuxer_desc_aac = {
  "AAC demuxer",
  "aac",
  "AAC",
  "Nico Sabbi",
  "Raw AAC files ",
  DEMUXER_TYPE_AAC,
  0, // unsafe autodetect
  demux_aac_probe,
  demux_aac_fill_buffer,
  demux_aac_open,
  demux_close_aac,
  demux_aac_seek,
  NULL
};
