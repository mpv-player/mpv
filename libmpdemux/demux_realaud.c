/*
    Realaudio demuxer for MPlayer
		(c) 2003 Roberto Togni
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


#define FOURCC_DOTRA mmioFOURCC('.','r','a', 0xfd)
#define FOURCC_144 mmioFOURCC('1','4','_','4')
#define FOURCC_288 mmioFOURCC('2','8','_','8')
#define FOURCC_DNET mmioFOURCC('d','n','e','t')
#define FOURCC_LPCJ mmioFOURCC('l','p','c','J')


typedef struct {
	unsigned short version;
	unsigned int dotranum;
	unsigned int data_size;
	unsigned short version2;
	unsigned int hdr_size;
	unsigned short codec_flavor;
	unsigned int coded_framesize;
	unsigned short sub_packet_h;
	unsigned short frame_size;
	unsigned short sub_packet_size;
	char genr[4];
} ra_priv_t;



int ra_check_file(demuxer_t* demuxer)
{
	unsigned int chunk_id;
  
	chunk_id = stream_read_dword_le(demuxer->stream);
	if (chunk_id == FOURCC_DOTRA)
		return 1;
	else
		return 0;
}



void hexdump(char *, unsigned long);

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
int demux_ra_fill_buffer(demuxer_t *demuxer)
{
	ra_priv_t *ra_priv = demuxer->priv;
	int len;
	int timestamp;
	int flags;
	demux_stream_t *ds = demuxer->audio;
	sh_audio_t *sh = ds->sh;
	WAVEFORMATEX *wf = sh->wf;
	demux_packet_t *dp;

  if (demuxer->stream->eof)
    return 0;

	len = wf->nBlockAlign;
	demuxer->filepos = stream_tell(demuxer->stream);

	dp = new_demux_packet(len);
	stream_read(demuxer->stream, dp->buffer, len);

	dp->pts = demuxer->filepos / ra_priv->data_size;
	dp->pos = demuxer->filepos;
	dp->flags = 0;
	ds_add_packet(ds, dp);

	return 1;
}



extern void print_wave_header(WAVEFORMATEX *h);



int demux_open_ra(demuxer_t* demuxer)
{
	ra_priv_t* ra_priv = demuxer->priv;
	sh_audio_t *sh;
	int i;
	char *buf;

  if ((ra_priv = (ra_priv_t *)malloc(sizeof(ra_priv_t))) == NULL) {
    mp_msg(MSGT_DEMUX, MSGL_ERR, "[RealAudio] Can't allocate memory for private data.\n");
    return 0;
  }
	memset(ra_priv, 0, sizeof(ra_priv_t));

	demuxer->priv = ra_priv;
	sh = new_sh_audio(demuxer, 0);
	demuxer->audio->id = 0;
	sh->ds=demuxer->audio;
	demuxer->audio->sh = sh;

	ra_priv->version = stream_read_word(demuxer->stream);
	mp_msg(MSGT_DEMUX,MSGL_V,"[RealAudio] File version: %d\n", ra_priv->version);
	if ((ra_priv->version < 3) || (ra_priv->version > 4)) {
		mp_msg(MSGT_DEMUX,MSGL_WARN,"[RealAudio] ra version %d is not supported yet, please "
			"contact MPlayer developers\n", ra_priv->version);
		return 0;
	}
	if (ra_priv->version == 3) {
		ra_priv->hdr_size = stream_read_word(demuxer->stream);
		stream_skip(demuxer->stream, 10);
		ra_priv->data_size = stream_read_dword(demuxer->stream);
	} else {
		stream_skip(demuxer->stream, 2);
		ra_priv->dotranum = stream_read_dword(demuxer->stream);
		ra_priv->data_size = stream_read_dword(demuxer->stream);
		ra_priv->version2 = stream_read_word(demuxer->stream);
		ra_priv->hdr_size = stream_read_dword(demuxer->stream);
		ra_priv->codec_flavor = stream_read_word(demuxer->stream);
		ra_priv->coded_framesize = stream_read_dword(demuxer->stream);
		stream_skip(demuxer->stream, 4); // data size?
		stream_skip(demuxer->stream, 8);
		ra_priv->sub_packet_h = stream_read_word(demuxer->stream);
		ra_priv->frame_size = stream_read_word(demuxer->stream);
		mp_msg(MSGT_DEMUX,MSGL_V,"[RealAudio] Frame size: %d\n", ra_priv->frame_size);
		ra_priv->sub_packet_size = stream_read_word(demuxer->stream);
		mp_msg(MSGT_DEMUX,MSGL_V,"[RealAudio] Sub packet size: %d\n", ra_priv->sub_packet_size);
		stream_skip(demuxer->stream, 2);
		sh->samplerate = stream_read_word(demuxer->stream);
		stream_skip(demuxer->stream, 2);
		sh->samplesize = stream_read_word(demuxer->stream);
		sh->channels = stream_read_word(demuxer->stream);
		mp_msg(MSGT_DEMUX,MSGL_V,"[RealAudio] %d channel, %d bit, %dHz\n", sh->channels,
			sh->samplesize, sh->samplerate);
		i = stream_read_char(demuxer->stream);
		*((unsigned int *)(ra_priv->genr)) = stream_read_dword(demuxer->stream);
		if (i != 4) {
			mp_msg(MSGT_DEMUX,MSGL_WARN,"[RealAudio] Genr size is not 4 (%d), please report to "
				"MPlayer developers\n", i);
			stream_skip(demuxer->stream, i - 4);
		}
		i = stream_read_char(demuxer->stream);
		sh->format = stream_read_dword_le(demuxer->stream);
		if (i != 4) {
			mp_msg(MSGT_DEMUX,MSGL_WARN,"[RealAudio] FourCC size is not 4 (%d), please report to "
				"MPlayer developers\n", i);
			stream_skip(demuxer->stream, i - 4);
		}
		stream_skip(demuxer->stream, 3);
	}

	if ((i = stream_read_char(demuxer->stream)) != 0) {
		buf = malloc(i+1);
		stream_read(demuxer->stream, buf, i);
		buf[i] = 0;
		demux_info_add(demuxer, "Title", buf);
		free(buf);
	}
	if ((i = stream_read_char(demuxer->stream)) != 0) {
		buf = malloc(i+1);
		stream_read(demuxer->stream, buf, i);
		buf[i] = 0;
		demux_info_add(demuxer, "Author", buf);
		free(buf);
	}
	if ((i = stream_read_char(demuxer->stream)) != 0) {
		buf = malloc(i+1);
		stream_read(demuxer->stream, buf, i);
		buf[i] = 0;
		demux_info_add(demuxer, "Copyright", buf);
		free(buf);
	}

	if ((i = stream_read_char(demuxer->stream)) != 0)
		mp_msg(MSGT_DEMUX,MSGL_WARN,"[RealAudio] Last header byte is not zero!\n");

	if (ra_priv->version == 3) {
		stream_skip(demuxer->stream, 1);
		i = stream_read_char(demuxer->stream);
		sh->format = stream_read_dword_le(demuxer->stream);
		if (i != 4) {
			mp_msg(MSGT_DEMUX,MSGL_WARN,"[RealAudio] FourCC size is not 4 (%d), please report to "
				"MPlayer developers\n", i);
			stream_skip(demuxer->stream, i - 4);
		}
//		stream_skip(demuxer->stream, 3);

		if (sh->format != FOURCC_LPCJ) {
			mp_msg(MSGT_DEMUX,MSGL_WARN,"[RealAudio] Version 3 with FourCC %8x, please report to "
				"MPlayer developers\n", sh->format);
		}

		sh->channels = 1;
		sh->samplesize = 16;
		sh->samplerate = 8000;
		ra_priv->frame_size = 240;
		sh->format = FOURCC_144;
	}

	/* Fill WAVEFORMATEX */
	sh->wf = malloc(sizeof(WAVEFORMATEX));
	memset(sh->wf, 0, sizeof(WAVEFORMATEX));
	sh->wf->nChannels = sh->channels;
	sh->wf->wBitsPerSample = sh->samplesize;
	sh->wf->nSamplesPerSec = sh->samplerate;
	sh->wf->nAvgBytesPerSec = sh->samplerate*sh->samplesize/8;
	sh->wf->nBlockAlign = ra_priv->frame_size;
	sh->wf->cbSize = 0;
	sh->wf->wFormatTag = sh->format;

	switch (sh->format) {
		case FOURCC_144:
			mp_msg(MSGT_DEMUX,MSGL_V,"Audio: 14_4\n");
			    sh->wf->cbSize = 10/*+codecdata_length*/;
			    sh->wf = realloc(sh->wf, sizeof(WAVEFORMATEX)+sh->wf->cbSize);
			    ((short*)(sh->wf+1))[0]=0;
			    ((short*)(sh->wf+1))[1]=240;
			    ((short*)(sh->wf+1))[2]=0;
			    ((short*)(sh->wf+1))[3]=0x14;
			    ((short*)(sh->wf+1))[4]=0;
			break;
		case FOURCC_288:
			mp_msg(MSGT_DEMUX,MSGL_V,"Audio: 28_8\n");
			    sh->wf->cbSize = 10/*+codecdata_length*/;
			    sh->wf = realloc(sh->wf, sizeof(WAVEFORMATEX)+sh->wf->cbSize);
			    ((short*)(sh->wf+1))[0]=0;
			    ((short*)(sh->wf+1))[1]=ra_priv->sub_packet_h;
			    ((short*)(sh->wf+1))[2]=ra_priv->codec_flavor;
			    ((short*)(sh->wf+1))[3]=ra_priv->coded_framesize;
			    ((short*)(sh->wf+1))[4]=0;
			break;
		case FOURCC_DNET:
			mp_msg(MSGT_DEMUX,MSGL_V,"Audio: DNET -> AC3\n");
			break;
		default:
			mp_msg(MSGT_DEMUX,MSGL_V,"Audio: Unknown (%d)\n", sh->format);
	}

	print_wave_header(sh->wf);

	/* disable seeking */
	demuxer->seekable = 0;

	if(!ds_fill_buffer(demuxer->audio))
		mp_msg(MSGT_DEMUXER,MSGL_INFO,"[RealAudio] No data.\n");

    return 1;
}



void demux_close_ra(demuxer_t *demuxer)
{
	ra_priv_t* ra_priv = demuxer->priv;
 
	if (ra_priv)
		free(ra_priv);

	return;
}


#if 0
/* please upload RV10 samples WITH INDEX CHUNK */
int demux_seek_ra(demuxer_t *demuxer, float rel_seek_secs, int flags)
{
    real_priv_t *priv = demuxer->priv;
    demux_stream_t *d_audio = demuxer->audio;
    sh_audio_t *sh_audio = d_audio->sh;
    int aid = d_audio->id;
    int next_offset = 0;
    int rel_seek_frames = 0;
    int streams = 0;

    return stream_seek(demuxer->stream, next_offset);
}
#endif
