/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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

#define MEncoderMP3LameHelp _("\n\n"\
" vbr=<0-4>     variable bitrate method\n"\
"                0: cbr (constant bitrate)\n"\
"                1: mt (Mark Taylor VBR algorithm)\n"\
"                2: rh (Robert Hegemann VBR algorithm - default)\n"\
"                3: abr (average bitrate)\n"\
"                4: mtrh (Mark Taylor Robert Hegemann VBR algorithm)\n"\
"\n"\
" abr           average bitrate\n"\
"\n"\
" cbr           constant bitrate\n"\
"               Also forces CBR mode encoding on subsequent ABR presets modes.\n"\
"\n"\
" br=<0-1024>   specify bitrate in kBit (CBR and ABR only)\n"\
"\n"\
" q=<0-9>       quality (0-highest, 9-lowest) (only for VBR)\n"\
"\n"\
" aq=<0-9>      algorithmic quality (0-best/slowest, 9-worst/fastest)\n"\
"\n"\
" ratio=<1-100> compression ratio\n"\
"\n"\
" vol=<0-10>    set audio input gain\n"\
"\n"\
" mode=<0-3>    (default: auto)\n"\
"                0: stereo\n"\
"                1: joint-stereo\n"\
"                2: dualchannel\n"\
"                3: mono\n"\
"\n"\
" padding=<0-2>\n"\
"                0: no\n"\
"                1: all\n"\
"                2: adjust\n"\
"\n"\
" fast          Switch on faster encoding on subsequent VBR presets modes,\n"\
"               slightly lower quality and higher bitrates.\n"\
"\n"\
" preset=<value> Provide the highest possible quality settings.\n"\
"                 medium: VBR  encoding,  good  quality\n"\
"                 (150-180 kbps bitrate range)\n"\
"                 standard:  VBR encoding, high quality\n"\
"                 (170-210 kbps bitrate range)\n"\
"                 extreme: VBR encoding, very high quality\n"\
"                 (200-240 kbps bitrate range)\n"\
"                 insane:  CBR  encoding, highest preset quality\n"\
"                 (320 kbps bitrate)\n"\
"                 <8-320>: ABR encoding at average given kbps bitrate.\n\n")



const m_option_t lameopts_conf[] = {
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
	{"help", MEncoderMP3LameHelp, CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};


static int bind_lame(audio_encoder_t *encoder, muxer_stream_t *mux_a)
{
    mp_tmsg(MSGT_MENCODER, MSGL_INFO, "MP3 audio selected.\n");
    mux_a->h.dwSampleSize=0; // VBR
    mux_a->h.dwRate=encoder->params.sample_rate;
    mux_a->h.dwScale=encoder->params.samples_per_frame; // samples/frame
    if(sizeof(MPEGLAYER3WAVEFORMAT)!=30) mp_tmsg(MSGT_MENCODER,MSGL_WARN,"sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, maybe broken C compiler?\n",sizeof(MPEGLAYER3WAVEFORMAT));
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
        mp_tmsg(MSGT_MENCODER, MSGL_V, "\n\nCBR audio: %d bytes/sec, %d bytes/block\n",
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
        mp_tmsg(MSGT_MENCODER, MSGL_V, "Setting audio input gain to %f.\n", lame_param_scale);
        lame_set_scale(lame,lame_param_scale);
    }
    if(lame_param_lowpassfreq>=-1) lame_set_lowpassfreq(lame,lame_param_lowpassfreq);
    if(lame_param_highpassfreq>=-1) lame_set_highpassfreq(lame,lame_param_highpassfreq);
#ifdef CONFIG_MP3LAME_PRESET
    if(lame_param_preset != NULL) {
        mp_tmsg(MSGT_MENCODER, MSGL_V, "\npreset=%s\n\n",lame_param_preset);
        if(lame_presets_set(lame,lame_param_fast, (lame_param_vbr==0), lame_param_preset) < 0)
            return 0;
    }
#endif
    if(lame_init_params(lame) == -1) {
        mp_tmsg(MSGT_MENCODER, MSGL_FATAL,
            "Cannot set LAME options, check bitrate/samplerate, some very low bitrates\n"\
            "(<32) need lower samplerates (i.e. -srate 8000).\n"\
            "If everything else fails, try a preset.");
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
        mp_tmsg(MSGT_MENCODER, MSGL_FATAL, "LAME version %s (%s)\n\n", get_lame_version(), get_lame_url());

#define LamePresetsLongInfo _("\n"\
"The preset switches are designed to provide the highest possible quality.\n"\
"\n"\
"They have for the most part been subjected to and tuned via rigorous double\n"\
"blind listening tests to verify and achieve this objective.\n"\
"\n"\
"These are continually updated to coincide with the latest developments that\n"\
"occur and as a result should provide you with nearly the best quality\n"\
"currently possible from LAME.\n"\
"\n"\
"To activate these presets:\n"\
"\n"\
"   For VBR modes (generally highest quality):\n"\
"\n"\
"     \"preset=standard\" This preset should generally be transparent\n"\
"                             to most people on most music and is already\n"\
"                             quite high in quality.\n"\
"\n"\
"     \"preset=extreme\" If you have extremely good hearing and similar\n"\
"                             equipment, this preset will generally provide\n"\
"                             slightly higher quality than the \"standard\"\n"\
"                             mode.\n"\
"\n"\
"   For CBR 320kbps (highest quality possible from the preset switches):\n"\
"\n"\
"     \"preset=insane\"  This preset will usually be overkill for most\n"\
"                             people and most situations, but if you must\n"\
"                             have the absolute highest quality with no\n"\
"                             regard to filesize, this is the way to go.\n"\
"\n"\
"   For ABR modes (high quality per given bitrate but not as high as VBR):\n"\
"\n"\
"     \"preset=<kbps>\"  Using this preset will usually give you good\n"\
"                             quality at a specified bitrate. Depending on the\n"\
"                             bitrate entered, this preset will determine the\n"\
"                             optimal settings for that particular situation.\n"\
"                             While this approach works, it is not nearly as\n"\
"                             flexible as VBR, and usually will not attain the\n"\
"                             same level of quality as VBR at higher bitrates.\n"\
"\n"\
"The following options are also available for the corresponding profiles:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR Mode) - The ABR Mode is implied. To use it,\n"\
"                      simply specify a bitrate. For example:\n"\
"                      \"preset=185\" activates this\n"\
"                      preset and uses 185 as an average kbps.\n"\
"\n"\
"   \"fast\" - Enables the new fast VBR for a particular profile. The\n"\
"            disadvantage to the speed switch is that often times the\n"\
"            bitrate will be slightly higher than with the normal mode\n"\
"            and quality may be slightly lower also.\n"\
"   Warning: with the current version fast presets might result in too\n"\
"            high bitrate compared to regular presets.\n"\
"\n"\
"   \"cbr\"  - If you use the ABR mode (read above) with a significant\n"\
"            bitrate such as 80, 96, 112, 128, 160, 192, 224, 256, 320,\n"\
"            you can use the \"cbr\" option to force CBR mode encoding\n"\
"            instead of the standard abr mode. ABR does provide higher\n"\
"            quality but CBR may be useful in situations such as when\n"\
"            streaming an MP3 over the internet may be important.\n"\
"\n"\
"    For example:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" or \"-lameopts  cbr:preset=192       \"\n"\
" or \"-lameopts      preset=172       \"\n"\
" or \"-lameopts      preset=extreme   \"\n"\
"\n"\
"\n"\
"A few aliases are available for ABR mode:\n"\
"phone => 16kbps/mono        phon+/lw/mw-eu/sw => 24kbps/mono\n"\
"mw-us => 40kbps/mono        voice => 56kbps/mono\n"\
"fm/radio/tape => 112kbps    hifi => 160kbps\n"\
"cd => 192kbps               studio => 256kbps")

        mp_tmsg(MSGT_MENCODER, MSGL_FATAL, LamePresetsLongInfo);
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
            mp_tmsg(MSGT_MENCODER, MSGL_FATAL, "LAME version %s (%s)\n\n", get_lame_version(), get_lame_url());
            mp_tmsg(MSGT_MENCODER, MSGL_FATAL,
                "Error: The bitrate specified is out of the valid range for this preset.\n"\
                "\n"\
                "When using this mode you must enter a value between \"8\" and \"320\".\n"\
                "\n"\
                "For further information try: \"-lameopts preset=help\"\n");
            return -1;
        }
    }

    mp_tmsg(MSGT_MENCODER, MSGL_FATAL, "LAME version %s (%s)\n\n", get_lame_version(), get_lame_url());
#define InvalidLamePresetOptions _("Error: You did not enter a valid profile and/or options with preset.\n"\
"\n"\
"Available profiles are:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR Mode) - The ABR Mode is implied. To use it,\n"\
"                      simply specify a bitrate. For example:\n"\
"                      \"preset=185\" activates this\n"\
"                      preset and uses 185 as an average kbps.\n"\
"\n"\
"    Some examples:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" or \"-lameopts  cbr:preset=192       \"\n"\
" or \"-lameopts      preset=172       \"\n"\
" or \"-lameopts      preset=extreme   \"\n"\
"\n"\
"For further information try: \"-lameopts preset=help\"\n")
    mp_tmsg(MSGT_MENCODER, MSGL_FATAL, InvalidLamePresetOptions);
    return -1;
}
#endif
