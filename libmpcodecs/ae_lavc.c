#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include "config.h"
#include "m_option.h"
#include "mp_msg.h"
#include "libmpdemux/aviheader.h"
#include "libmpdemux/ms_hdr.h"
#include "stream/stream.h"
#include "libmpdemux/muxer.h"
#include "ae_lavc.h"
#include "help_mp.h"
#include "libaf/af_format.h"
#include "libaf/reorder_ch.h"
#include "libavcodec/avcodec.h"
#include "libavutil/intreadwrite.h"

static AVCodec        *lavc_acodec;
static AVCodecContext *lavc_actx;
extern char *lavc_param_acodec;
extern int  lavc_param_abitrate;
extern int  lavc_param_atag;
extern int  lavc_param_audio_global_header;
extern int  avcodec_initialized;
static int compressed_frame_size = 0;
#ifdef CONFIG_LIBAVFORMAT
#include "libavformat/avformat.h"
extern const struct AVCodecTag *mp_wav_taglists[];
#endif

static int bind_lavc(audio_encoder_t *encoder, muxer_stream_t *mux_a)
{
	mux_a->wf = malloc(sizeof(WAVEFORMATEX)+lavc_actx->extradata_size+256);
	mux_a->wf->wFormatTag = lavc_param_atag;
	mux_a->wf->nChannels = lavc_actx->channels;
	mux_a->wf->nSamplesPerSec = lavc_actx->sample_rate;
	mux_a->wf->nAvgBytesPerSec = (lavc_actx->bit_rate / 8);
        mux_a->avg_rate= lavc_actx->bit_rate;
	mux_a->h.dwRate = mux_a->wf->nAvgBytesPerSec;
	if(lavc_actx->block_align)
		mux_a->h.dwSampleSize = mux_a->h.dwScale = lavc_actx->block_align;
	else 
	{
		mux_a->h.dwScale = (mux_a->wf->nAvgBytesPerSec * lavc_actx->frame_size)/ mux_a->wf->nSamplesPerSec; /* for cbr */
	
		if ((mux_a->wf->nAvgBytesPerSec *
			lavc_actx->frame_size) % mux_a->wf->nSamplesPerSec) 
		{
			mux_a->h.dwScale = lavc_actx->frame_size;
			mux_a->h.dwRate = lavc_actx->sample_rate;
			mux_a->h.dwSampleSize = 0; // Blocksize not constant
		} 
		else 
			mux_a->h.dwSampleSize = 0;
	}
        if(mux_a->h.dwSampleSize)
                mux_a->wf->nBlockAlign = mux_a->h.dwSampleSize;
        else
                mux_a->wf->nBlockAlign = 1;
	mux_a->h.dwSuggestedBufferSize = (encoder->params.audio_preload*mux_a->wf->nAvgBytesPerSec)/1000;
	mux_a->h.dwSuggestedBufferSize -= mux_a->h.dwSuggestedBufferSize % mux_a->wf->nBlockAlign;

	switch(lavc_param_atag) 
	{
		case 0x11: /* imaadpcm */
			mux_a->wf->wBitsPerSample = 4;
			mux_a->wf->cbSize = 2;
			AV_WL16(mux_a->wf+1, lavc_actx->frame_size);
			break;
		case 0x55: /* mp3 */
			mux_a->wf->cbSize = 12;
			mux_a->wf->wBitsPerSample = 0; /* does not apply */
			((MPEGLAYER3WAVEFORMAT *) (mux_a->wf))->wID = 1;
			((MPEGLAYER3WAVEFORMAT *) (mux_a->wf))->fdwFlags = 2;
			((MPEGLAYER3WAVEFORMAT *) (mux_a->wf))->nBlockSize = mux_a->wf->nBlockAlign;
			((MPEGLAYER3WAVEFORMAT *) (mux_a->wf))->nFramesPerBlock = 1;
			((MPEGLAYER3WAVEFORMAT *) (mux_a->wf))->nCodecDelay = 0;
			break;
		default:
			mux_a->wf->wBitsPerSample = 0; /* Unknown */
			if (lavc_actx->extradata && (lavc_actx->extradata_size > 0))
			{
				memcpy(mux_a->wf+1, lavc_actx->extradata, lavc_actx->extradata_size);
				mux_a->wf->cbSize = lavc_actx->extradata_size;
			}
			else
				mux_a->wf->cbSize = 0;
			break;
	}

	// Fix allocation    
	mux_a->wf = realloc(mux_a->wf, sizeof(WAVEFORMATEX)+mux_a->wf->cbSize);
	
	encoder->input_format = AF_FORMAT_S16_NE;
	encoder->min_buffer_size = mux_a->h.dwSuggestedBufferSize;
	encoder->max_buffer_size = mux_a->h.dwSuggestedBufferSize*2;
	
	return 1;
}

static int encode_lavc(audio_encoder_t *encoder, uint8_t *dest, void *src, int size, int max_size)
{
	int n;
	if ((encoder->params.channels == 6 || encoder->params.channels == 5) &&
			(!strcmp(lavc_acodec->name,"ac3") ||
			!strcmp(lavc_acodec->name,"libfaac"))) {
		int isac3 = !strcmp(lavc_acodec->name,"ac3");
		reorder_channel_nch(src, AF_CHANNEL_LAYOUT_MPLAYER_DEFAULT,
		                    isac3 ? AF_CHANNEL_LAYOUT_LAVC_AC3_DEFAULT
		                          : AF_CHANNEL_LAYOUT_AAC_DEFAULT,
		                    encoder->params.channels,
		                    size / 2, 2);
	}
	n = avcodec_encode_audio(lavc_actx, dest, size, src);
        compressed_frame_size = n;
	return n;
}


static int close_lavc(audio_encoder_t *encoder)
{
	compressed_frame_size = 0;
	return 1;
}

static int get_frame_size(audio_encoder_t *encoder)
{
        int sz = compressed_frame_size;
        compressed_frame_size = 0;
        return sz;
}

#ifndef CONFIG_LIBAVFORMAT
static uint32_t lavc_find_atag(char *codec)
{
	if(codec == NULL)
	        return 0;

	if(! strcasecmp(codec, "mp2"))
		return 0x50;

	if(! strcasecmp(codec, "mp3"))
		return 0x55;

	if(! strcasecmp(codec, "ac3"))
		return 0x2000;

	if(! strcasecmp(codec, "adpcm_ima_wav"))
		return 0x11;

	if(! strncasecmp(codec, "bonk", 4))
		return 0x2048;

	return 0;
}
#endif


int mpae_init_lavc(audio_encoder_t *encoder)
{
	encoder->params.samples_per_frame = encoder->params.sample_rate;
	encoder->params.bitrate = encoder->params.sample_rate * encoder->params.channels * 2 * 8;
	
	if(!lavc_param_acodec)
	{
		mp_msg(MSGT_MENCODER, MSGL_FATAL, MSGTR_NoLavcAudioCodecName);
		return 0;
	}

	if(!avcodec_initialized){
		avcodec_init();
		avcodec_register_all();
		avcodec_initialized=1;
	}

	lavc_acodec = avcodec_find_encoder_by_name(lavc_param_acodec);
	if (!lavc_acodec)
	{
		mp_msg(MSGT_MENCODER, MSGL_FATAL, MSGTR_LavcAudioCodecNotFound, lavc_param_acodec);
		return 0;
	}
	if(lavc_param_atag == 0)
	{
#ifdef CONFIG_LIBAVFORMAT
		lavc_param_atag = av_codec_get_tag(mp_wav_taglists, lavc_acodec->id);
#else
		lavc_param_atag = lavc_find_atag(lavc_param_acodec);
#endif
		if(!lavc_param_atag)
		{
			mp_msg(MSGT_MENCODER, MSGL_FATAL, "Couldn't find wav tag for specified codec, exit\n");
			return 0;
		}
	}

	lavc_actx = avcodec_alloc_context();
	if(lavc_actx == NULL)
	{
		mp_msg(MSGT_MENCODER, MSGL_FATAL, MSGTR_CouldntAllocateLavcContext);
		return 0;
	}
	
	// put sample parameters
	lavc_actx->channels = encoder->params.channels;
	lavc_actx->sample_rate = encoder->params.sample_rate;
        if(lavc_param_abitrate<1000)
                lavc_actx->bit_rate = encoder->params.bitrate = lavc_param_abitrate * 1000;
        else
                lavc_actx->bit_rate = encoder->params.bitrate = lavc_param_abitrate;
	

	/*
	* Special case for adpcm_ima_wav.
	* The bitrate is only dependent on samplerate.
	* We have to known frame_size and block_align in advance,
	* so I just copied the code from libavcodec/adpcm.c
	*
	* However, ms adpcm_ima_wav uses a block_align of 2048,
	* lavc defaults to 1024
	*/
	if(lavc_param_atag == 0x11) {
		int blkalign = 2048;
		int framesize = (blkalign - 4 * lavc_actx->channels) * 8 / (4 * lavc_actx->channels) + 1;
		lavc_actx->bit_rate = lavc_actx->sample_rate*8*blkalign/framesize;
	}
        if((lavc_param_audio_global_header&1)
        /*|| (video_global_header==0 && (oc->oformat->flags & AVFMT_GLOBALHEADER))*/){
                lavc_actx->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }
        if(lavc_param_audio_global_header&2){
                lavc_actx->flags2 |= CODEC_FLAG2_LOCAL_HEADER;
        }

	if(avcodec_open(lavc_actx, lavc_acodec) < 0)
	{
		mp_msg(MSGT_MENCODER, MSGL_FATAL, MSGTR_CouldntOpenCodec, lavc_param_acodec, lavc_param_abitrate);
		return 0;
	}

	if(lavc_param_atag == 0x11) {
		lavc_actx->block_align = 2048;
		lavc_actx->frame_size = (lavc_actx->block_align - 4 * lavc_actx->channels) * 8 / (4 * lavc_actx->channels) + 1;
	}

	encoder->decode_buffer_size = lavc_actx->frame_size * 2 * encoder->params.channels;
	encoder->bind = bind_lavc;
	encoder->get_frame_size = get_frame_size;
	encoder->encode = encode_lavc;
	encoder->close = close_lavc;

	return 1;
}

