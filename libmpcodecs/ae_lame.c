#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include "m_option.h"
#include "mp_msg.h"
#include "libmpdemux/aviheader.h"
#include "libmpdemux/ms_hdr.h"
#include "stream/stream.h"
#include "libmpdemux/muxer.h"
#include "help_mp.h"
#include "ae_pcm.h"
#include "libaf/af_format.h"
#include "libmpdemux/mp3_hdr.h"

#undef CDECL
#include <lame/lame.h>

lame_global_flags *lame;
static int lame_param_quality=0; // best
static int lame_param_algqual=5; // same as old default
static int lame_param_vbr=vbr_default;
static int lame_param_mode=-1; // unset
static int lame_param_padding=-1; // unset
static int lame_param_br=-1; // unset
static int lame_param_ratio=-1; // unset
static float lame_param_scale=-1; // unset
static int lame_param_lowpassfreq = 0; //auto
static int lame_param_highpassfreq = 0; //auto
static int lame_param_free_format = 0; //disabled
static int lame_param_br_min = 0; //not specified
static int lame_param_br_max = 0; //not specified

#ifdef CONFIG_MP3LAME_PRESET
int lame_param_fast=0; // unset
static char* lame_param_preset=NULL; // unset
static int  lame_presets_set( lame_t gfp, int fast, int cbr, const char* preset_name );
#endif


m_option_t lameopts_conf[]={
	{"q", &lame_param_quality, CONF_TYPE_INT, CONF_RANGE, 0, 9, NULL},
	{"aq", &lame_param_algqual, CONF_TYPE_INT, CONF_RANGE, 0, 9, NULL},
	{"vbr", &lame_param_vbr, CONF_TYPE_INT, CONF_RANGE, 0, vbr_max_indicator, NULL},
	{"cbr", &lame_param_vbr, CONF_TYPE_FLAG, 0, 0, 0, NULL},
	{"abr", &lame_param_vbr, CONF_TYPE_FLAG, 0, 0, vbr_abr, NULL},
	{"mode", &lame_param_mode, CONF_TYPE_INT, CONF_RANGE, 0, MAX_INDICATOR, NULL},
	{"padding", &lame_param_padding, CONF_TYPE_INT, CONF_RANGE, 0, PAD_MAX_INDICATOR, NULL},
	{"br", &lame_param_br, CONF_TYPE_INT, CONF_RANGE, 0, 1024, NULL},
	{"ratio", &lame_param_ratio, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
	{"vol", &lame_param_scale, CONF_TYPE_FLOAT, CONF_RANGE, 0, 10, NULL},
	{"lowpassfreq",&lame_param_lowpassfreq, CONF_TYPE_INT, CONF_RANGE, -1, 48000,0},
	{"highpassfreq",&lame_param_highpassfreq, CONF_TYPE_INT, CONF_RANGE, -1, 48000,0},
	{"nofree", &lame_param_free_format, CONF_TYPE_FLAG, 0, 0, 0, NULL},
	{"free", &lame_param_free_format, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"br_min", &lame_param_br_min, CONF_TYPE_INT, CONF_RANGE, 0, 1024, NULL},
	{"br_max", &lame_param_br_max, CONF_TYPE_INT, CONF_RANGE, 0, 1024, NULL},
#ifdef CONFIG_MP3LAME_PRESET
	{"fast", &lame_param_fast, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"preset", &lame_param_preset, CONF_TYPE_STRING, 0, 0, 0, NULL},
#else
	{"fast", "MPlayer was built without -lameopts fast support (requires libmp3lame >=3.92).\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"preset", "MPlayer was built without -lameopts preset support (requires libmp3lame >=3.92).\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif
	{"help", MSGTR_MEncoderMP3LameHelp, CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};


static int bind_lame(audio_encoder_t *encoder, muxer_stream_t *mux_a)
{
    mp_msg(MSGT_MENCODER, MSGL_INFO, MSGTR_MP3AudioSelected);
    mux_a->h.dwSampleSize=0; // VBR
    mux_a->h.dwRate=encoder->params.sample_rate;
    mux_a->h.dwScale=encoder->params.samples_per_frame; // samples/frame
    if(sizeof(MPEGLAYER3WAVEFORMAT)!=30) mp_msg(MSGT_MENCODER,MSGL_WARN,MSGTR_MP3WaveFormatSizeNot30,sizeof(MPEGLAYER3WAVEFORMAT));
    mux_a->wf=malloc(sizeof(MPEGLAYER3WAVEFORMAT)); // should be 30
    mux_a->wf->wFormatTag=0x55; // MP3
    mux_a->wf->nChannels= (lame_param_mode<0) ? encoder->params.channels : ((lame_param_mode==3) ? 1 : 2);
    mux_a->wf->nSamplesPerSec=mux_a->h.dwRate;
    if(! lame_param_vbr)
        mux_a->wf->nAvgBytesPerSec=lame_param_br * 125;
    else
        mux_a->wf->nAvgBytesPerSec=192000/8; // FIXME!
    mux_a->wf->nBlockAlign=encoder->params.samples_per_frame; // required for l3codeca.acm + WMP 6.4
    mux_a->wf->wBitsPerSample=0; //16;
    // from NaNdub:  (requires for l3codeca.acm)
    mux_a->wf->cbSize=12;
    ((MPEGLAYER3WAVEFORMAT*)(mux_a->wf))->wID=1;
    ((MPEGLAYER3WAVEFORMAT*)(mux_a->wf))->fdwFlags=2;
    ((MPEGLAYER3WAVEFORMAT*)(mux_a->wf))->nBlockSize=encoder->params.samples_per_frame; // ???
    ((MPEGLAYER3WAVEFORMAT*)(mux_a->wf))->nFramesPerBlock=1;
    ((MPEGLAYER3WAVEFORMAT*)(mux_a->wf))->nCodecDelay=0;
    
    encoder->input_format = AF_FORMAT_S16_NE;
    encoder->min_buffer_size = 4608;
    encoder->max_buffer_size = mux_a->h.dwRate * mux_a->wf->nChannels * 2;
    
    return 1;
}

#define min(a, b) ((a) <= (b) ? (a) : (b))

static int get_frame_size(audio_encoder_t *encoder)
{
    int sz;
    if(encoder->stream->buffer_len < 4)
        return 0;
    sz = mp_decode_mp3_header(encoder->stream->buffer);
    if(sz <= 0)
        return 0;
    return sz;
}

static int encode_lame(audio_encoder_t *encoder, uint8_t *dest, void *src, int len, int max_size)
{
    int n = 0;
    if(encoder->params.channels == 1)
        n = lame_encode_buffer(lame, (short *)src, (short *)src, len/2, dest, max_size);
    else
        n = lame_encode_buffer_interleaved(lame,(short *)src, len/4, dest, max_size);

    return n < 0 ? 0 : n;
}


static int close_lame(audio_encoder_t *encoder)
{
    return 1;
}

static void fixup(audio_encoder_t *encoder)
{
    // fixup CBR mp3 audio header:
    if(!lame_param_vbr) {
        encoder->stream->h.dwSampleSize=1;
        if (encoder->stream->h.dwLength)
        ((MPEGLAYER3WAVEFORMAT*)(encoder->stream->wf))->nBlockSize=
            (encoder->stream->size+(encoder->stream->h.dwLength>>1))/encoder->stream->h.dwLength;
        encoder->stream->h.dwLength=encoder->stream->size;
        encoder->stream->h.dwRate=encoder->stream->wf->nAvgBytesPerSec;
        encoder->stream->h.dwScale=1;
        encoder->stream->wf->nBlockAlign=1;
        mp_msg(MSGT_MENCODER, MSGL_V, MSGTR_CBRAudioByterate,
            encoder->stream->h.dwRate,((MPEGLAYER3WAVEFORMAT*)(encoder->stream->wf))->nBlockSize);
    }
}

int mpae_init_lame(audio_encoder_t *encoder)
{
    encoder->params.bitrate = lame_param_br * 125;
    encoder->params.samples_per_frame = encoder->params.sample_rate < 32000 ? 576 : 1152;
    encoder->decode_buffer_size = 2304;

    lame=lame_init();
    lame_set_bWriteVbrTag(lame,0);
    lame_set_in_samplerate(lame,encoder->params.sample_rate);
    //lame_set_in_samplerate(lame,sh_audio->samplerate); // if resampling done by lame
    lame_set_num_channels(lame,encoder->params.channels);
    lame_set_out_samplerate(lame,encoder->params.sample_rate);
    lame_set_quality(lame,lame_param_algqual); // 0 = best q
    if(lame_param_free_format) lame_set_free_format(lame,1);
    if(lame_param_vbr){  // VBR:
        lame_set_VBR(lame,lame_param_vbr); // vbr mode
        lame_set_VBR_q(lame,lame_param_quality); // 0 = best vbr q  5=~128k
        if(lame_param_br>0) lame_set_VBR_mean_bitrate_kbps(lame,lame_param_br);
        if(lame_param_br_min>0) lame_set_VBR_min_bitrate_kbps(lame,lame_param_br_min);
        if(lame_param_br_max>0) lame_set_VBR_max_bitrate_kbps(lame,lame_param_br_max);
    } else {    // CBR:
        if(lame_param_br>0) lame_set_brate(lame,lame_param_br);
    }
    if(lame_param_mode>=0) lame_set_mode(lame,lame_param_mode); // j-st
    if(lame_param_ratio>0) lame_set_compression_ratio(lame,lame_param_ratio);
    if(lame_param_scale>0) {
        mp_msg(MSGT_MENCODER, MSGL_V, MSGTR_SettingAudioInputGain, lame_param_scale);
        lame_set_scale(lame,lame_param_scale);
    }
    if(lame_param_lowpassfreq>=-1) lame_set_lowpassfreq(lame,lame_param_lowpassfreq);
    if(lame_param_highpassfreq>=-1) lame_set_highpassfreq(lame,lame_param_highpassfreq);
#ifdef CONFIG_MP3LAME_PRESET
    if(lame_param_preset != NULL) {
        mp_msg(MSGT_MENCODER, MSGL_V, MSGTR_LamePresetEquals,lame_param_preset);
        if(lame_presets_set(lame,lame_param_fast, (lame_param_vbr==0), lame_param_preset) < 0)
            return 0;
    }
#endif
    if(lame_init_params(lame) == -1) {
        mp_msg(MSGT_MENCODER, MSGL_FATAL, MSGTR_LameCantInit); 
        return 0;
    }
    if( mp_msg_test(MSGT_MENCODER,MSGL_V) ) {
        lame_print_config(lame);
        lame_print_internals(lame);
    }
    
    encoder->bind = bind_lame;
    encoder->get_frame_size = get_frame_size;
    encoder->encode = encode_lame;
    encoder->fixup = fixup;
    encoder->close = close_lame;
    return 1;
}

#ifdef CONFIG_MP3LAME_PRESET
/* lame_presets_set 
   taken out of presets_set in lame-3.93.1/frontend/parse.c and modified */
static int  lame_presets_set( lame_t gfp, int fast, int cbr, const char* preset_name )
{
    int mono = 0;

    if (strcmp(preset_name, "help") == 0) {
        mp_msg(MSGT_MENCODER, MSGL_FATAL, MSGTR_LameVersion, get_lame_version(), get_lame_url());
        mp_msg(MSGT_MENCODER, MSGL_FATAL, MSGTR_LamePresetsLongInfo);
        return -1;
    }

    //aliases for compatibility with old presets

    if (strcmp(preset_name, "phone") == 0) {
        preset_name = "16";
        mono = 1;
    }
    if ( (strcmp(preset_name, "phon+") == 0) ||
         (strcmp(preset_name, "lw") == 0) ||
         (strcmp(preset_name, "mw-eu") == 0) ||
         (strcmp(preset_name, "sw") == 0)) {
        preset_name = "24";
        mono = 1;
    }
    if (strcmp(preset_name, "mw-us") == 0) {
        preset_name = "40";
        mono = 1;
    }
    if (strcmp(preset_name, "voice") == 0) {
        preset_name = "56";
        mono = 1;
    }
    if (strcmp(preset_name, "fm") == 0) {
        preset_name = "112";
    }
    if ( (strcmp(preset_name, "radio") == 0) ||
         (strcmp(preset_name, "tape") == 0)) {
        preset_name = "112";
    }
    if (strcmp(preset_name, "hifi") == 0) {
        preset_name = "160";
    }
    if (strcmp(preset_name, "cd") == 0) {
        preset_name = "192";
    }
    if (strcmp(preset_name, "studio") == 0) {
        preset_name = "256";
    }

#ifdef CONFIG_MP3LAME_PRESET_MEDIUM
    if (strcmp(preset_name, "medium") == 0) {
        if (fast > 0)
           lame_set_preset(gfp, MEDIUM_FAST);
        else
           lame_set_preset(gfp, MEDIUM);

        return 0;
    }
#endif
    
    if (strcmp(preset_name, "standard") == 0) {
        if (fast > 0)
           lame_set_preset(gfp, STANDARD_FAST);
        else
           lame_set_preset(gfp, STANDARD);

        return 0;
    }
    
    else if (strcmp(preset_name, "extreme") == 0){
        if (fast > 0)
           lame_set_preset(gfp, EXTREME_FAST);
        else
           lame_set_preset(gfp, EXTREME);

        return 0;
    }
    					
    else if (((strcmp(preset_name, "insane") == 0) || 
              (strcmp(preset_name, "320"   ) == 0))   && (fast < 1)) {

        lame_set_preset(gfp, INSANE);
 
        return 0;
    }

    // Generic ABR Preset
    if (((atoi(preset_name)) > 0) &&  (fast < 1)) {
        if ((atoi(preset_name)) >= 8 && (atoi(preset_name)) <= 320){
            lame_set_preset(gfp, atoi(preset_name));

            if (cbr == 1 )
                lame_set_VBR(gfp, vbr_off);

            if (mono == 1 ) {
                lame_set_mode(gfp, MONO);
            }

            return 0;

        }
        else {
            mp_msg(MSGT_MENCODER, MSGL_FATAL, MSGTR_LameVersion, get_lame_version(), get_lame_url());
            mp_msg(MSGT_MENCODER, MSGL_FATAL, MSGTR_InvalidBitrateForLamePreset);
            return -1;
        }
    }

    mp_msg(MSGT_MENCODER, MSGL_FATAL, MSGTR_LameVersion, get_lame_version(), get_lame_url());
    mp_msg(MSGT_MENCODER, MSGL_FATAL, MSGTR_InvalidLamePresetOptions);
    return -1;
}
#endif
