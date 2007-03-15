/*
    Realaudio demuxer for MPlayer
		(c) 2003, 2005 Roberto Togni
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"


#define FOURCC_DOTRA mmioFOURCC('.','r','a', 0xfd)
#define FOURCC_144 mmioFOURCC('1','4','_','4')
#define FOURCC_288 mmioFOURCC('2','8','_','8')
#define FOURCC_DNET mmioFOURCC('d','n','e','t')
#define FOURCC_LPCJ mmioFOURCC('l','p','c','J')
#define FOURCC_SIPR mmioFOURCC('s','i','p','r')
#define INTLID_INT4 mmioFOURCC('I','n','t','4')
#define INTLID_SIPR mmioFOURCC('s','i','p','r')


static unsigned char sipr_swaps[38][2]={
    {0,63},{1,22},{2,44},{3,90},{5,81},{7,31},{8,86},{9,58},{10,36},{12,68},
    {13,39},{14,73},{15,53},{16,69},{17,57},{19,88},{20,34},{21,71},{24,46},
    {25,94},{26,54},{28,75},{29,50},{32,70},{33,92},{35,74},{38,85},{40,56},
    {42,87},{43,65},{45,59},{48,79},{49,93},{51,89},{55,95},{61,76},{67,83},
    {77,80} };

// Map flavour to bytes per second
static int sipr_fl2bps[4] = {813, 1062, 625, 2000}; // 6.5, 8.5, 5, 16 kbit per second


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
	unsigned intl_id;
	unsigned char *audio_buf;
} ra_priv_t;



static int ra_check_file(demuxer_t* demuxer)
{
	unsigned int chunk_id;
  
	chunk_id = stream_read_dword_le(demuxer->stream);
	if (chunk_id == FOURCC_DOTRA)
		return DEMUXER_TYPE_REALAUDIO;
	else
		return 0;
}



// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
static int demux_ra_fill_buffer(demuxer_t *demuxer, demux_stream_t *dsds)
{
	ra_priv_t *ra_priv = demuxer->priv;
	int len;
	demux_stream_t *ds = demuxer->audio;
	sh_audio_t *sh = ds->sh;
	WAVEFORMATEX *wf = sh->wf;
	demux_packet_t *dp;
	int x, y;

  if (demuxer->stream->eof)
    return 0;

	len = wf->nBlockAlign;
	demuxer->filepos = stream_tell(demuxer->stream);

    if ((ra_priv->intl_id == INTLID_INT4) || (ra_priv->intl_id == INTLID_SIPR)) {
      if (ra_priv->intl_id == INTLID_SIPR) {
        int n;
        int bs = ra_priv->sub_packet_h * ra_priv->frame_size * 2 / 96;  // nibbles per subpacket
        stream_read(demuxer->stream, ra_priv->audio_buf, ra_priv->sub_packet_h * ra_priv->frame_size);
        // Perform reordering
        for(n = 0; n < 38; n++) {
            int j;
            int i = bs * sipr_swaps[n][0];
            int o = bs * sipr_swaps[n][1];
            // swap nibbles of block 'i' with 'o'      TODO: optimize
            for(j = 0; j < bs; j++) {
                int x = (i & 1) ? (ra_priv->audio_buf[i >> 1] >> 4) : (ra_priv->audio_buf[i >> 1] & 0x0F);
                int y = (o & 1) ? (ra_priv->audio_buf[o >> 1] >> 4) : (ra_priv->audio_buf[o >> 1] & 0x0F);
                if(o & 1)
                    ra_priv->audio_buf[o >> 1] = (ra_priv->audio_buf[o >> 1] & 0x0F) | (x << 4);
                else
                    ra_priv->audio_buf[o >> 1] = (ra_priv->audio_buf[o >> 1] & 0xF0) | x;
                if(i & 1)
                    ra_priv->audio_buf[i >> 1] = (ra_priv->audio_buf[i >> 1] & 0x0F) | (y << 4);
                else
                    ra_priv->audio_buf[i >> 1] = (ra_priv->audio_buf[i >> 1] & 0xF0) | y;
                ++i; ++o;
            }
        }
      } else {
        for (y = 0; y < ra_priv->sub_packet_h; y++)
            for (x = 0; x < ra_priv->sub_packet_h / 2; x++)
                stream_read(demuxer->stream, ra_priv->audio_buf + x * 2 *ra_priv->frame_size +
                            y * ra_priv->coded_framesize, ra_priv->coded_framesize);
      }
        // Release all the audio packets
        for (x = 0; x < ra_priv->sub_packet_h * ra_priv->frame_size / len; x++) {
            dp = new_demux_packet(len);
            memcpy(dp->buffer, ra_priv->audio_buf + x * len, len);
            dp->pts = x ? 0 : demuxer->filepos / ra_priv->data_size;
            dp->pos = demuxer->filepos; // all equal
            dp->flags = x ? 0 : 0x10; // Mark first packet as keyframe
            ds_add_packet(ds, dp);
        }
    } else {
	dp = new_demux_packet(len);
	stream_read(demuxer->stream, dp->buffer, len);

	dp->pts = demuxer->filepos / ra_priv->data_size;
	dp->pos = demuxer->filepos;
	dp->flags = 0;
	ds_add_packet(ds, dp);
    }

	return 1;
}



extern void print_wave_header(WAVEFORMATEX *h, int verbose_level);



static demuxer_t* demux_open_ra(demuxer_t* demuxer)
{
	ra_priv_t* ra_priv = demuxer->priv;
	sh_audio_t *sh;
	int i;
	char *buf;

  if ((ra_priv = malloc(sizeof(ra_priv_t))) == NULL) {
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
		mp_msg(MSGT_DEMUX,MSGL_V,"[RealAudio] Flavor: %d\n", ra_priv->codec_flavor);
		ra_priv->coded_framesize = stream_read_dword(demuxer->stream);
		mp_msg(MSGT_DEMUX,MSGL_V,"[RealAudio] Coded frame size: %d\n", ra_priv->coded_framesize);
		stream_skip(demuxer->stream, 4); // data size?
		stream_skip(demuxer->stream, 8);
		ra_priv->sub_packet_h = stream_read_word(demuxer->stream);
		mp_msg(MSGT_DEMUX,MSGL_V,"[RealAudio] Sub packet h: %d\n", ra_priv->sub_packet_h);
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
		ra_priv->intl_id = stream_read_dword_le(demuxer->stream);
		if (i != 4) {
			mp_msg(MSGT_DEMUX,MSGL_WARN,"[RealAudio] Interleaver Id size is not 4 (%d), please report to "
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

	if ((i = stream_read_char(demuxer->stream)) != 0) {
		buf = malloc(i+1);
		stream_read(demuxer->stream, buf, i);
		buf[i] = 0;
		demux_info_add(demuxer, "Comment", buf);
		free(buf);
	}

	if (ra_priv->version == 3) {
	    if(ra_priv->hdr_size + 8 > stream_tell(demuxer->stream)) {
		stream_skip(demuxer->stream, 1);
		i = stream_read_char(demuxer->stream);
		sh->format = stream_read_dword_le(demuxer->stream);
		if (i != 4) {
			mp_msg(MSGT_DEMUX,MSGL_WARN,"[RealAudio] FourCC size is not 4 (%d), please report to "
				"MPlayer developers\n", i);
			stream_skip(demuxer->stream, i - 4);
		}

		if (sh->format != FOURCC_LPCJ) {
			mp_msg(MSGT_DEMUX,MSGL_WARN,"[RealAudio] Version 3 with FourCC %8x, please report to "
				"MPlayer developers\n", sh->format);
		}
	    } else
		// If a stream does not have fourcc, let's assume it's 14.4
		sh->format = FOURCC_LPCJ;

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
            sh->wf->nBlockAlign = 0x14;
			break;
		case FOURCC_288:
			mp_msg(MSGT_DEMUX,MSGL_V,"Audio: 28_8\n");
            sh->wf->nBlockAlign = ra_priv->coded_framesize;
            ra_priv->audio_buf = calloc(ra_priv->sub_packet_h, ra_priv->frame_size);
			break;
		case FOURCC_DNET:
			mp_msg(MSGT_DEMUX,MSGL_V,"Audio: DNET -> AC3\n");
			break;
		case FOURCC_SIPR:
			mp_msg(MSGT_DEMUX,MSGL_V,"Audio: SIPR\n");
			sh->wf->nBlockAlign = ra_priv->coded_framesize;
			sh->wf->nAvgBytesPerSec = sipr_fl2bps[ra_priv->codec_flavor];
			ra_priv->audio_buf = calloc(ra_priv->sub_packet_h, ra_priv->frame_size);
			break;
		default:
			mp_msg(MSGT_DEMUX,MSGL_V,"Audio: Unknown (%d)\n", sh->format);
	}

	print_wave_header(sh->wf, MSGL_V);

	/* disable seeking */
	demuxer->seekable = 0;

	if(!ds_fill_buffer(demuxer->audio))
		mp_msg(MSGT_DEMUXER,MSGL_INFO,"[RealAudio] No data.\n");

    return demuxer;
}



static void demux_close_ra(demuxer_t *demuxer)
{
	ra_priv_t* ra_priv = demuxer->priv;
 
    if (ra_priv) {
	    if (ra_priv->audio_buf)
	        free (ra_priv->audio_buf);
		free(ra_priv);
    }
	return;
}


#if 0
/* please upload RV10 samples WITH INDEX CHUNK */
int demux_seek_ra(demuxer_t *demuxer, float rel_seek_secs, float audio_delay, int flags)
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


demuxer_desc_t demuxer_desc_realaudio = {
  "Realaudio demuxer",
  "realaudio",
  "REALAUDIO",
  "Roberto Togni",
  "handles old audio only .ra files",
  DEMUXER_TYPE_REALAUDIO,
  1, // safe autodetect
  ra_check_file,
  demux_ra_fill_buffer,
  demux_open_ra,
  demux_close_ra,
  NULL,
  NULL
};
