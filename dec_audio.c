
#define USE_G72X
//#define USE_LIBAC3

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

extern int verbose; // defined in mplayer.c

#include "stream.h"
#include "demuxer.h"

#include "codec-cfg.h"
#include "stheader.h"

#include "dec_audio.h"

#include "roqav.h"

//==========================================================================

#include "libao2/afmt.h"

#include "dll_init.h"

#include "mp3lib/mp3.h"

#ifdef USE_LIBAC3
#include "libac3/ac3.h"
#endif

#include "liba52/a52.h"
#include "liba52/mm_accel.h"
static sample_t * a52_samples;
static a52_state_t a52_state;
static uint32_t a52_accel=0;
static uint32_t a52_flags=0;

#ifdef USE_G72X
#include "g72x/g72x.h"
static G72x_DATA g72x_data;
#endif

#include "alaw.h"

#include "xa/xa_gsm.h"

#include "ac3-iec958.h"

#include "adpcm.h"

#include "cpudetect.h"

/* used for ac3surround decoder - set using -channels option */
int audio_output_channels = 2;

#ifdef USE_FAKE_MONO
int fakemono=0;
#endif

#ifdef USE_DIRECTSHOW
#include "loader/dshow/DS_AudioDecoder.h"
static DS_AudioDecoder* ds_adec=NULL;
#endif

#ifdef HAVE_OGGVORBIS
/* XXX is math.h really needed? - atmos */
#include <math.h>
#include <vorbis/codec.h>

// This struct is also defined in demux_ogg.c => common header ?
typedef struct ov_struct_st {
  vorbis_info      vi; /* struct that stores all the static vorbis bitstream
			  settings */
  vorbis_comment   vc; /* struct that stores all the bitstream user comments */
  vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
  vorbis_block     vb; /* local working space for packet->PCM decode */
} ov_struct_t;
#endif

#ifdef HAVE_FAAD
#include <faad.h>
static faacDecHandle faac_hdec;
static faacDecFrameInfo faac_finfo;
static int faac_bytesconsumed = 0;
static unsigned char *faac_buffer;
/* configure maximum supported channels, *
 * this is theoretically max. 64 chans   */
#define FAAD_MAX_CHANNELS 6
#define FAAD_BUFFLEN (FAAD_MIN_STREAMSIZE*FAAD_MAX_CHANNELS)		       
#endif

#ifdef USE_LIBAVCODEC
#ifdef USE_LIBAVCODEC_SO
#include <libffmpeg/avcodec.h>
#else
#include "libavcodec/avcodec.h"
#endif
    static AVCodec *lavc_codec=NULL;
    static AVCodecContext lavc_context;
    extern int avcodec_inited;
#endif



#ifdef USE_LIBMAD
#include <mad.h>

#define MAD_SINGLE_BUFFER_SIZE 8192
#define MAD_TOTAL_BUFFER_SIZE  ((MAD_SINGLE_BUFFER_SIZE)*3)

static struct mad_stream mad_stream;
static struct mad_frame  mad_frame;
static struct mad_synth  mad_synth;
static char*  mad_in_buffer = 0; /* base pointer of buffer */

// ensure buffer is filled with some data
static void mad_prepare_buffer(sh_audio_t* sh_audio, struct mad_stream* ms, int length)
{
  if(sh_audio->a_in_buffer_len < length) {
    int len = demux_read_data(sh_audio->ds, sh_audio->a_in_buffer+sh_audio->a_in_buffer_len, length-sh_audio->a_in_buffer_len);
    sh_audio->a_in_buffer_len += len;
//    printf("mad_prepare_buffer: read %d bytes\n", len);
  }
}

static void mad_postprocess_buffer(sh_audio_t* sh_audio, struct mad_stream* ms)
{
  /* rotate buffer while possible, in order to reduce the overhead of endless memcpy */
  int delta = (unsigned char*)ms->next_frame - (unsigned char *)sh_audio->a_in_buffer;
  if((unsigned long)(sh_audio->a_in_buffer) - (unsigned long)mad_in_buffer < 
     (MAD_TOTAL_BUFFER_SIZE - MAD_SINGLE_BUFFER_SIZE - delta)) {
    sh_audio->a_in_buffer += delta;
    sh_audio->a_in_buffer_len -= delta;
  } else {
    sh_audio->a_in_buffer = mad_in_buffer;
    sh_audio->a_in_buffer_len -= delta;
    memcpy(sh_audio->a_in_buffer, ms->next_frame, sh_audio->a_in_buffer_len);
  }
}

static inline
signed short mad_scale(mad_fixed_t sample)
{
  /* round */
  sample += (1L << (MAD_F_FRACBITS - 16));

  /* clip */
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  /* quantize */
  return sample >> (MAD_F_FRACBITS + 1 - 16);

}

static void mad_sync(sh_audio_t* sh_audio, struct mad_stream* ms)
{
    int len;
#if 1
    int skipped = 0;

//    printf("buffer len: %d\n", sh_audio->a_in_buffer_len);    
    while(sh_audio->a_in_buffer_len - skipped)
    {
	len = mp_decode_mp3_header(sh_audio->a_in_buffer+skipped);
	if (len != -1)
	{
//	    printf("Frame len=%d\n", len);
	    break;
	}
	else
	    skipped++;
    }
    if (skipped)
    {
	mp_msg(MSGT_DECAUDIO, MSGL_INFO, "mad: audio synced, skipped bytes: %d\n", skipped);
//	ms->skiplen += skipped;
//	printf("skiplen: %d (skipped: %d)\n", ms->skiplen, skipped);

//	if (sh_audio->a_in_buffer_len - skipped < MAD_BUFFER_GUARD)
//	    printf("Mad reports: too small buffer\n");

//	mad_stream_buffer(ms, sh_audio->a_in_buffer+skipped, sh_audio->a_in_buffer_len-skipped);
//	mad_prepare_buffer(sh_audio, ms, sh_audio->a_in_buffer_len-skipped);

	/* move frame to the beginning of the buffer and fill up to a_in_buffer_size */
	sh_audio->a_in_buffer_len -= skipped;
	memcpy(sh_audio->a_in_buffer, sh_audio->a_in_buffer+skipped, sh_audio->a_in_buffer_len);
	mad_prepare_buffer(sh_audio, ms, sh_audio->a_in_buffer_size);
	mad_stream_buffer(ms, sh_audio->a_in_buffer, sh_audio->a_in_buffer_len);
//	printf("bufflen: %d\n", sh_audio->a_in_buffer_len);
	
//	len = mp_decode_mp3_header(sh_audio->a_in_buffer);
//	printf("len: %d\n", len);
	ms->md_len = len;
    }
#else
    len = mad_stream_sync(&ms);
    if (len == -1)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Mad sync failed\n");
    }
#endif
}

static void mad_print_error(struct mad_stream *mad_stream)
{
    printf("error (0x%x): ", mad_stream->error);
    switch(mad_stream->error)
    {
	case MAD_ERROR_BUFLEN:	printf("buffer too small");		break;
	case MAD_ERROR_BUFPTR:	printf("invalid buffer pointer"); 	break;
	case MAD_ERROR_NOMEM:	printf("not enought memory");		break;
	case MAD_ERROR_LOSTSYNC:	printf("lost sync");		break;
	case MAD_ERROR_BADLAYER:	printf("bad layer");		break;
	case MAD_ERROR_BADBITRATE:	printf("bad bitrate");		break;
	case MAD_ERROR_BADSAMPLERATE:	printf("bad samplerate");	break;
	case MAD_ERROR_BADEMPHASIS:	printf("bad emphasis");		break;
	case MAD_ERROR_BADCRC:		printf("bad crc");		break;
	case MAD_ERROR_BADBITALLOC:	printf("forbidden bit alloc val"); break;
	case MAD_ERROR_BADSCALEFACTOR:	printf("bad scalefactor index"); break;
	case MAD_ERROR_BADFRAMELEN:	printf("bad frame length");	break;
	case MAD_ERROR_BADBIGVALUES:	printf("bad bigvalues count");	break;
	case MAD_ERROR_BADBLOCKTYPE:	printf("reserved blocktype");	break;
	case MAD_ERROR_BADSCFSI:	printf("bad scalefactor selinfo"); break;
	case MAD_ERROR_BADDATAPTR:	printf("bad maindatabegin ptr"); break;
	case MAD_ERROR_BADPART3LEN:	printf("bad audio data len");	break;
	case MAD_ERROR_BADHUFFTABLE:	printf("bad huffman table sel"); break;
	case MAD_ERROR_BADHUFFDATA:	printf("huffman data overrun");	break;
	case MAD_ERROR_BADSTEREO:	printf("incomp. blocktype for JS"); break;
	default:
	    printf("unknown error");
    }
    printf("\n");
}
#endif


static int a52_fillbuff(sh_audio_t *sh_audio){
int length=0;
int flags=0;
int sample_rate=0;
int bit_rate=0;

    sh_audio->a_in_buffer_len=0;
    // sync frame:
while(1){
    while(sh_audio->a_in_buffer_len<7){
	int c=demux_getc(sh_audio->ds);
	if(c<0) return -1; // EOF
        sh_audio->a_in_buffer[sh_audio->a_in_buffer_len++]=c;
    }
    length = a52_syncinfo (sh_audio->a_in_buffer, &flags, &sample_rate, &bit_rate);
    if(length>=7 && length<=3840) break; // we're done.
    // bad file => resync
    memcpy(sh_audio->a_in_buffer,sh_audio->a_in_buffer+1,6);
    --sh_audio->a_in_buffer_len;
}
    mp_msg(MSGT_DECAUDIO,MSGL_DBG2,"a52: len=%d  flags=0x%X  %d Hz %d bit/s\n",length,flags,sample_rate,bit_rate);
    sh_audio->samplerate=sample_rate;
    sh_audio->i_bps=bit_rate/8;
    demux_read_data(sh_audio->ds,sh_audio->a_in_buffer+7,length-7);
    
    if(crc16_block(sh_audio->a_in_buffer+2,length-2)!=0)
	mp_msg(MSGT_DECAUDIO,MSGL_STATUS,"a52: CRC check failed!  \n");
    
    return length;
}

// returns: number of available channels
static int a52_printinfo(sh_audio_t *sh_audio){
int flags, sample_rate, bit_rate;
char* mode="unknown";
int channels=0;
  a52_syncinfo (sh_audio->a_in_buffer, &flags, &sample_rate, &bit_rate);
  switch(flags&A52_CHANNEL_MASK){
    case A52_CHANNEL: mode="channel"; channels=2; break;
    case A52_MONO: mode="mono"; channels=1; break;
    case A52_STEREO: mode="stereo"; channels=2; break;
    case A52_3F: mode="3f";channels=3;break;
    case A52_2F1R: mode="2f+1r";channels=3;break;
    case A52_3F1R: mode="3f+1r";channels=4;break;
    case A52_2F2R: mode="2f+2r";channels=4;break;
    case A52_3F2R: mode="3f+2r";channels=5;break;
    case A52_CHANNEL1: mode="channel1"; channels=2; break;
    case A52_CHANNEL2: mode="channel2"; channels=2; break;
    case A52_DOLBY: mode="dolby"; channels=2; break;
  }
  mp_msg(MSGT_DECAUDIO,MSGL_INFO,"AC3: %d.%d (%s%s)  %d Hz  %3.1f kbit/s\n",
	channels, (flags&A52_LFE)?1:0,
	mode, (flags&A52_LFE)?"+lfe":"",
	sample_rate, bit_rate*0.001f);
  return (flags&A52_LFE) ? (channels+1) : channels;
}

int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen);


static sh_audio_t* dec_audio_sh=NULL;

#ifdef USE_LIBAC3
// AC3 decoder buffer callback:
static void ac3_fill_buffer(uint8_t **start,uint8_t **end){
    int len=ds_get_packet(dec_audio_sh->ds,start);
    //printf("<ac3:%d>\n",len);
    if(len<0)
          *start = *end = NULL;
    else
          *end = *start + len;
}
#endif

// MP3 decoder buffer callback:
int mplayer_audio_read(char *buf,int size){
  int len;
  len=demux_read_data(dec_audio_sh->ds,buf,size);
  return len;
}

int init_audio(sh_audio_t *sh_audio){
int driver=sh_audio->codec->driver;

sh_audio->samplesize=2;
#ifdef WORDS_BIGENDIAN
sh_audio->sample_format=AFMT_S16_BE;
#else
sh_audio->sample_format=AFMT_S16_LE;
#endif
sh_audio->samplerate=0;
//sh_audio->pcm_bswap=0;
sh_audio->o_bps=0;

sh_audio->a_buffer_size=0;
sh_audio->a_buffer=NULL;

sh_audio->a_in_buffer_len=0;

// setup required min. in/out buffer size:
sh_audio->audio_out_minsize=8192;// default size, maybe not enough for Win32/ACM

switch(driver){
case AFM_ACM:
#ifndef	USE_WIN32DLL
  mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_NoACMSupport);
  driver=0;
#else
  // Win32 ACM audio codec:
  if(init_acm_audio_codec(sh_audio)){
    sh_audio->i_bps=sh_audio->wf->nAvgBytesPerSec;
    sh_audio->channels=sh_audio->o_wf.nChannels;
    sh_audio->samplerate=sh_audio->o_wf.nSamplesPerSec;
//    if(sh_audio->audio_out_minsize>16384) sh_audio->audio_out_minsize=16384;
//    sh_audio->a_buffer_size=sh_audio->audio_out_minsize;
//    if(sh_audio->a_buffer_size<sh_audio->audio_out_minsize+MAX_OUTBURST)
//        sh_audio->a_buffer_size=sh_audio->audio_out_minsize+MAX_OUTBURST;
  } else {
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_ACMiniterror);
    driver=0;
  }
#endif
  break;
case AFM_DSHOW:
#ifndef USE_DIRECTSHOW
  mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_NoDShowAudio);
  driver=0;
#else
  // Win32 DShow audio codec:
//  printf("DShow_audio: channs=%d  rate=%d\n",sh_audio->channels,sh_audio->samplerate);
  if(!(ds_adec=DS_AudioDecoder_Open(sh_audio->codec->dll,&sh_audio->codec->guid,sh_audio->wf))){
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_MissingDLLcodec,sh_audio->codec->dll);
    driver=0;
  } else {
    sh_audio->i_bps=sh_audio->wf->nAvgBytesPerSec;
    sh_audio->channels=sh_audio->wf->nChannels;
    sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
    sh_audio->audio_in_minsize=2*sh_audio->wf->nBlockAlign;
    if(sh_audio->audio_in_minsize<8192) sh_audio->audio_in_minsize=8192;
    sh_audio->a_in_buffer_size=sh_audio->audio_in_minsize;
    sh_audio->a_in_buffer=malloc(sh_audio->a_in_buffer_size);
    sh_audio->a_in_buffer_len=0;
    sh_audio->audio_out_minsize=16384;
  }
#endif
  break;
case AFM_VORBIS:
#ifndef	HAVE_OGGVORBIS
  mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_NoOggVorbis);
  driver=0;
#else
  /* OggVorbis audio via libvorbis, compatible with files created by nandub and zorannt codec */
  // Is there always 1024 samples/frame ? ***** Albeu
  sh_audio->audio_out_minsize=1024*4; // 1024 samples/frame
#endif
  break;
case AFM_AAC:
  // AAC (MPEG2 Audio, MPEG4 Audio)
#ifndef HAVE_FAAD
  mp_msg(MSGT_DECAUDIO,MSGL_ERR,"Error: Cannot decode AAC data, because MPlayer was compiled without FAAD support\n"/*MSGTR_NoFAAD*/);
  driver=0;
#else  
  mp_msg(MSGT_DECAUDIO,MSGL_V,"Using FAAD to decode AAC content!\n"/*MSGTR_UseFAAD*/);
  // Samples per frame * channels per frame, this might not work with >2 chan AAC, need test samples! ::atmos
  sh_audio->audio_out_minsize=2048*2;
#endif  
  break;
case AFM_PCM:
case AFM_DVDPCM:
case AFM_ALAW:
  // PCM, aLaw
  sh_audio->audio_out_minsize=2048;
  break;
case AFM_AC3:
case AFM_A52:
  // Dolby AC3 audio:
  // however many channels, 2 bytes in a word, 256 samples in a block, 6 blocks in a frame
  sh_audio->audio_out_minsize=audio_output_channels*2*256*6;
  break;
case AFM_HWAC3:
  // Dolby AC3 audio:
  sh_audio->audio_out_minsize=4*256*6;
//  sh_audio->sample_format = AFMT_AC3;
//  sh_audio->sample_format = AFMT_S16_LE;
  sh_audio->channels=2;
  break;
case AFM_GSM:
  // MS-GSM audio codec:
  sh_audio->audio_out_minsize=4*320;
  break;
case AFM_IMAADPCM:
  sh_audio->audio_out_minsize=4096;
  sh_audio->ds->ss_div=IMA_ADPCM_SAMPLES_PER_BLOCK;
  sh_audio->ds->ss_mul=IMA_ADPCM_BLOCK_SIZE * sh_audio->wf->nChannels;
  break;
case AFM_MSADPCM:
  sh_audio->audio_out_minsize=sh_audio->wf->nBlockAlign * 8;
  sh_audio->ds->ss_div = MS_ADPCM_SAMPLES_PER_BLOCK;
  sh_audio->ds->ss_mul = sh_audio->wf->nBlockAlign;
  break;
case AFM_DK4ADPCM:
  sh_audio->audio_out_minsize=DK4_ADPCM_SAMPLES_PER_BLOCK * 4;
  sh_audio->ds->ss_div=DK4_ADPCM_SAMPLES_PER_BLOCK;
  sh_audio->ds->ss_mul=sh_audio->wf->nBlockAlign;
  break;
case AFM_DK3ADPCM:
  sh_audio->audio_out_minsize=DK3_ADPCM_SAMPLES_PER_BLOCK * 4;
  sh_audio->ds->ss_div=DK3_ADPCM_SAMPLES_PER_BLOCK;
  sh_audio->ds->ss_mul=DK3_ADPCM_BLOCK_SIZE;
  break;
case AFM_ROQAUDIO:
  // minsize was stored in wf->nBlockAlign by the RoQ demuxer
  sh_audio->audio_out_minsize=sh_audio->wf->nBlockAlign;
  sh_audio->ds->ss_div=DK3_ADPCM_SAMPLES_PER_BLOCK;
  sh_audio->ds->ss_mul=DK3_ADPCM_BLOCK_SIZE;
  sh_audio->context = roq_decode_audio_init();
  break;
case AFM_MPEG:
  // MPEG Audio:
  sh_audio->audio_out_minsize=4608;
  break;
#ifdef USE_G72X
case AFM_G72X:
//  g72x_reader_init(&g72x_data,G723_16_BITS_PER_SAMPLE);
  g72x_reader_init(&g72x_data,G723_24_BITS_PER_SAMPLE);
//  g72x_reader_init(&g72x_data,G721_32_BITS_PER_SAMPLE);
//  g72x_reader_init(&g72x_data,G721_40_BITS_PER_SAMPLE);
  sh_audio->audio_out_minsize=g72x_data.samplesperblock*4;
  break;
#endif
case AFM_FFMPEG:
#ifndef USE_LIBAVCODEC
   mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_NoLAVCsupport);
   return 0;
#else
  // FFmpeg Audio:
  sh_audio->audio_out_minsize=AVCODEC_MAX_AUDIO_FRAME_SIZE;
  break;
#endif

#ifdef USE_LIBMAD
 case AFM_MAD:
   mp_msg(MSGT_DECVIDEO, MSGL_V, "mad: setting minimum outputsize\n");
   sh_audio->audio_out_minsize=4608;
   if(sh_audio->audio_in_minsize<MAD_SINGLE_BUFFER_SIZE) sh_audio->audio_in_minsize=MAD_SINGLE_BUFFER_SIZE;
   sh_audio->a_in_buffer_size=sh_audio->audio_in_minsize;
   mad_in_buffer = sh_audio->a_in_buffer = malloc(MAD_TOTAL_BUFFER_SIZE);
   sh_audio->a_in_buffer_len=0;
   break;
#endif
}

if(!driver) return 0;

// allocate audio out buffer:
sh_audio->a_buffer_size=sh_audio->audio_out_minsize+MAX_OUTBURST; // worst case calc.

mp_msg(MSGT_DECAUDIO,MSGL_V,"dec_audio: Allocating %d + %d = %d bytes for output buffer\n",
    sh_audio->audio_out_minsize,MAX_OUTBURST,sh_audio->a_buffer_size);

sh_audio->a_buffer=malloc(sh_audio->a_buffer_size);
if(!sh_audio->a_buffer){
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_CantAllocAudioBuf);
    return 0;
}
memset(sh_audio->a_buffer,0,sh_audio->a_buffer_size);
sh_audio->a_buffer_len=0;

switch(driver){
#ifdef USE_WIN32DLL
case AFM_ACM: {
    int ret=acm_decode_audio(sh_audio,sh_audio->a_buffer,4096,sh_audio->a_buffer_size);
    if(ret<0){
        mp_msg(MSGT_DECAUDIO,MSGL_INFO,"ACM decoding error: %d\n",ret);
        driver=0;
    }
    sh_audio->a_buffer_len=ret;
    break;
}
#endif
case AFM_PCM: {
    // AVI PCM Audio:
    WAVEFORMATEX *h=sh_audio->wf;
    sh_audio->i_bps=h->nAvgBytesPerSec;
    sh_audio->channels=h->nChannels;
    sh_audio->samplerate=h->nSamplesPerSec;
    sh_audio->samplesize=(h->wBitsPerSample+7)/8;
    switch(sh_audio->format){ // hardware formats:
    case 0x6:  sh_audio->sample_format=AFMT_A_LAW;break;
    case 0x7:  sh_audio->sample_format=AFMT_MU_LAW;break;
    case 0x11: sh_audio->sample_format=AFMT_IMA_ADPCM;break;
    case 0x50: sh_audio->sample_format=AFMT_MPEG;break;
    case 0x736F7774: sh_audio->sample_format=AFMT_S16_LE;sh_audio->codec->driver=AFM_DVDPCM;break;
//    case 0x2000: sh_audio->sample_format=AFMT_AC3;
    default: sh_audio->sample_format=(sh_audio->samplesize==2)?AFMT_S16_LE:AFMT_U8;
    }
    break;
}
case AFM_DVDPCM: {
    // DVD PCM Audio:
    sh_audio->channels=2;
    sh_audio->samplerate=48000;
    sh_audio->i_bps=2*2*48000;
//    sh_audio->pcm_bswap=1;
    break;
}
case AFM_AC3: {
#ifndef USE_LIBAC3
  mp_msg(MSGT_DECAUDIO,MSGL_WARN,"WARNING: libac3 support is disabled. (hint: upgrade codecs.conf)\n");
  driver=0;
#else
  // Dolby AC3 audio:
  dec_audio_sh=sh_audio; // save sh_audio for the callback:
  ac3_config.fill_buffer_callback = ac3_fill_buffer;
  ac3_config.num_output_ch = audio_output_channels;
  ac3_config.flags = 0;
if(gCpuCaps.hasMMX){
  ac3_config.flags |= AC3_MMX_ENABLE;
}
if(gCpuCaps.has3DNow){
  ac3_config.flags |= AC3_3DNOW_ENABLE;
}
  ac3_init();
  sh_audio->ac3_frame = ac3_decode_frame();
  if(sh_audio->ac3_frame){
    ac3_frame_t* fr=(ac3_frame_t*)sh_audio->ac3_frame;
    sh_audio->samplerate=fr->sampling_rate;
    sh_audio->channels=ac3_config.num_output_ch;
    // 1 frame: 6*256 samples     1 sec: sh_audio->samplerate samples
    //sh_audio->i_bps=fr->frame_size*fr->sampling_rate/(6*256);
    sh_audio->i_bps=fr->bit_rate*(1000/8);
  } else {
    driver=0; // bad frame -> disable audio
  }
#endif
  break;
}
case AFM_A52: {
  sample_t level=1, bias=384;
  int flags=0;
  // Dolby AC3 audio:
  if(gCpuCaps.hasSSE) a52_accel|=MM_ACCEL_X86_SSE;
  if(gCpuCaps.hasMMX) a52_accel|=MM_ACCEL_X86_MMX;
  if(gCpuCaps.hasMMX2) a52_accel|=MM_ACCEL_X86_MMXEXT;
  if(gCpuCaps.has3DNow) a52_accel|=MM_ACCEL_X86_3DNOW;
  if(gCpuCaps.has3DNowExt) a52_accel|=MM_ACCEL_X86_3DNOWEXT;
  a52_samples=a52_init (a52_accel);
  if (a52_samples == NULL) {
	mp_msg(MSGT_DECAUDIO,MSGL_ERR,"A52 init failed\n");
	driver=0;break;
  }
   sh_audio->a_in_buffer_size=3840;
   sh_audio->a_in_buffer=malloc(sh_audio->a_in_buffer_size);
   sh_audio->a_in_buffer_len=0;
  if(a52_fillbuff(sh_audio)<0){
	mp_msg(MSGT_DECAUDIO,MSGL_ERR,"A52 sync failed\n");
	driver=0;break;
  }
  // 'a52 cannot upmix' hotfix:
  a52_printinfo(sh_audio);
//  if(audio_output_channels<sh_audio->channels)
//      sh_audio->channels=audio_output_channels;
  // channels setup:
  sh_audio->channels=audio_output_channels;
while(sh_audio->channels>0){
  switch(sh_audio->channels){
	    case 1: a52_flags=A52_MONO; break;
//	    case 2: a52_flags=A52_STEREO; break;
	    case 2: a52_flags=A52_DOLBY; break;
//	    case 3: a52_flags=A52_3F; break;
	    case 3: a52_flags=A52_2F1R; break;
	    case 4: a52_flags=A52_2F2R; break; // 2+2
	    case 5: a52_flags=A52_3F2R; break;
	    case 6: a52_flags=A52_3F2R|A52_LFE; break; // 5.1
  }
  // test:
  flags=a52_flags|A52_ADJUST_LEVEL;
  mp_msg(MSGT_DECAUDIO,MSGL_V,"A52 flags before a52_frame: 0x%X\n",flags);
  if (a52_frame (&a52_state, sh_audio->a_in_buffer, &flags, &level, bias)){
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,"a52: error decoding frame -> nosound\n");
    driver=0;break;
  }
  mp_msg(MSGT_DECAUDIO,MSGL_V,"A52 flags after a52_frame: 0x%X\n",flags);
  // frame decoded, let's init resampler:
  if(a52_resample_init(a52_accel,flags,sh_audio->channels)) break;
  --sh_audio->channels; // try to decrease no. of channels
}
  if(sh_audio->channels<=0){
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,"a52: no resampler. try different channel setup!\n");
    driver=0;break;
  }
  break;
}
case AFM_HWAC3: {
  // Dolby AC3 passthrough:
  a52_samples=a52_init (a52_accel);
  if (a52_samples == NULL) {
       mp_msg(MSGT_DECAUDIO,MSGL_ERR,"A52 init failed\n");
       driver=0;break;
  }
  sh_audio->a_in_buffer_size=3840;
  sh_audio->a_in_buffer=malloc(sh_audio->a_in_buffer_size);
  sh_audio->a_in_buffer_len=0;
  if(a52_fillbuff(sh_audio)<0) {
       mp_msg(MSGT_DECAUDIO,MSGL_ERR,"A52 sync failed\n");
       driver=0;break;
  }
  
  //sh_audio->samplerate=ai.samplerate;   // SET by a52_fillbuff()
  //sh_audio->samplesize=ai.framesize;
  //sh_audio->i_bps=ai.bitrate*(1000/8);  // SET by a52_fillbuff()
  //sh_audio->ac3_frame=malloc(6144);
  //sh_audio->o_bps=sh_audio->i_bps;  // XXX FIXME!!! XXX

  // o_bps is calculated from samplesize*channels*samplerate
  // a single ac3 frame is always translated to 6144 byte packet. (zero padding)
  sh_audio->channels=2;
  sh_audio->samplesize=2;   // 2*2*(6*256) = 6144 (very TRICKY!)

  break;
}
case AFM_ALAW: {
  // aLaw audio codec:
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  sh_audio->i_bps=sh_audio->channels*sh_audio->samplerate;
  break;
}
#ifdef USE_G72X
case AFM_G72X: {
  // GSM 723 audio codec:
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  sh_audio->i_bps=(sh_audio->samplerate/g72x_data.samplesperblock)*g72x_data.blocksize;
  break;
}
#endif
#ifdef USE_LIBAVCODEC
case AFM_FFMPEG: {
   int x;
   mp_msg(MSGT_DECAUDIO,MSGL_V,"FFmpeg's libavcodec audio codec\n");
    if(!avcodec_inited){
      avcodec_init();
      avcodec_register_all();
      avcodec_inited=1;
    }
    lavc_codec = (AVCodec *)avcodec_find_decoder_by_name(sh_audio->codec->dll);
    if(!lavc_codec){
	mp_msg(MSGT_DECAUDIO,MSGL_ERR,MSGTR_MissingLAVCcodec,sh_audio->codec->dll);
	return 0;
    }
    memset(&lavc_context, 0, sizeof(lavc_context));
    /* open it */
    if (avcodec_open(&lavc_context, lavc_codec) < 0) {
        mp_msg(MSGT_DECAUDIO,MSGL_ERR, MSGTR_CantOpenCodec);
        return 0;
    }
   mp_msg(MSGT_DECAUDIO,MSGL_V,"INFO: libavcodec init OK!\n");

   // Decode at least 1 byte:  (to get header filled)
   x=decode_audio(sh_audio,sh_audio->a_buffer,1,sh_audio->a_buffer_size);
   if(x>0) sh_audio->a_buffer_len=x;

#if 1
  sh_audio->channels=lavc_context.channels;
  sh_audio->samplerate=lavc_context.sample_rate;
  sh_audio->i_bps=lavc_context.bit_rate/8;
#else
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  sh_audio->i_bps=sh_audio->wf->nAvgBytesPerSec;
#endif
  break;
}
#endif
case AFM_GSM: {
  // MS-GSM audio codec:
  GSM_Init();
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  // decodes 65 byte -> 320 short
  // 1 sec: sh_audio->channels*sh_audio->samplerate  samples
  // 1 frame: 320 samples
  sh_audio->i_bps=65*(sh_audio->channels*sh_audio->samplerate)/320;  // 1:10
  break;
}
case AFM_IMAADPCM:
  // IMA-ADPCM 4:1 audio codec:
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  // decodes 34 byte -> 64 short
  sh_audio->i_bps=IMA_ADPCM_BLOCK_SIZE*(sh_audio->channels*sh_audio->samplerate)/IMA_ADPCM_SAMPLES_PER_BLOCK;  // 1:4
  break;
case AFM_MSADPCM:
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  sh_audio->i_bps = sh_audio->wf->nBlockAlign *
    (sh_audio->channels*sh_audio->samplerate) / MS_ADPCM_SAMPLES_PER_BLOCK;
  break;
case AFM_DK4ADPCM:
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  sh_audio->i_bps = sh_audio->wf->nBlockAlign *
    (sh_audio->channels*sh_audio->samplerate) / DK4_ADPCM_SAMPLES_PER_BLOCK;
  break;
case AFM_DK3ADPCM:
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  sh_audio->i_bps=DK3_ADPCM_BLOCK_SIZE*
    (sh_audio->channels*sh_audio->samplerate) / DK3_ADPCM_SAMPLES_PER_BLOCK;
  break;
case AFM_ROQAUDIO:
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  sh_audio->i_bps = (sh_audio->channels * 22050) / 2;
  break;
case AFM_MPEG: {
  // MPEG Audio:
  dec_audio_sh=sh_audio; // save sh_audio for the callback:
#ifdef USE_FAKE_MONO
  MP3_Init(fakemono);
#else
  MP3_Init();
#endif
  MP3_samplerate=MP3_channels=0;
  sh_audio->a_buffer_len=MP3_DecodeFrame(sh_audio->a_buffer,-1);
  sh_audio->channels=2; // hack
  sh_audio->samplerate=MP3_samplerate;
  sh_audio->i_bps=MP3_bitrate*(1000/8);
  MP3_PrintHeader();
  break;
}
#ifdef HAVE_OGGVORBIS
case AFM_VORBIS: {
  ogg_packet op;
  vorbis_comment vc;
  struct ov_struct_st *ov;

  /// Init the decoder with the 3 header packets
  ov = (struct ov_struct_st*)malloc(sizeof(struct ov_struct_st));
  vorbis_info_init(&ov->vi);
  vorbis_comment_init(&vc);
  op.bytes = ds_get_packet(sh_audio->ds,&op.packet);
  op.b_o_s  = 1;
  /// Header
  if(vorbis_synthesis_headerin(&ov->vi,&vc,&op) <0) {
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,"OggVorbis: initial (identification) header broken!\n");
    driver = 0;
    free(ov);
    break;
  }
  op.bytes = ds_get_packet(sh_audio->ds,&op.packet);
  op.b_o_s  = 0;
  /// Comments
  if(vorbis_synthesis_headerin(&ov->vi,&vc,&op) <0) {
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,"OggVorbis: comment header broken!\n");
    driver = 0;
    free(ov);
    break;
  }
  op.bytes = ds_get_packet(sh_audio->ds,&op.packet);
  //// Codebook
  if(vorbis_synthesis_headerin(&ov->vi,&vc,&op)<0) {
    mp_msg(MSGT_DECAUDIO,MSGL_WARN,"OggVorbis: codebook header broken!\n");
    driver = 0;
    free(ov);
    break;
  } else { /// Print the infos
    char **ptr=vc.user_comments;
    while(*ptr){
      mp_msg(MSGT_DECAUDIO,MSGL_V,"OggVorbisComment: %s\n",*ptr);
      ++ptr;
    }
    mp_msg(MSGT_DECAUDIO,MSGL_V,"OggVorbis: Bitstream is %d channel, %ldHz, %ldkbit/s %cBR\n",ov->vi.channels,ov->vi.rate,ov->vi.bitrate_nominal/1000, (ov->vi.bitrate_lower!=ov->vi.bitrate_nominal)||(ov->vi.bitrate_upper!=ov->vi.bitrate_nominal)?'V':'C');
    mp_msg(MSGT_DECAUDIO,MSGL_V,"OggVorbis: Encoded by: %s\n",vc.vendor);
  }

  // Setup the decoder
  sh_audio->channels=ov->vi.channels; 
  sh_audio->samplerate=ov->vi.rate;
  sh_audio->i_bps=ov->vi.bitrate_nominal/8;
  sh_audio->context = ov;

  /// Finish the decoder init
  vorbis_synthesis_init(&ov->vd,&ov->vi);
  vorbis_block_init(&ov->vd,&ov->vb);
  mp_msg(MSGT_DECAUDIO,MSGL_V,"OggVorbis: Init OK!\n");
} break;
#endif

#ifdef HAVE_FAAD
case AFM_AAC: {
  unsigned long faac_samplerate, faac_channels;
  faacDecConfigurationPtr faac_conf;
  faac_hdec = faacDecOpen();

#if 0
  /* Set the default object type and samplerate */
  /* This is useful for RAW AAC files */
  faac_conf = faacDecGetCurrentConfiguration(faac_hdec);
  if(sh_audio->samplerate)
    faac_conf->defSampleRate = sh_audio->samplerate;
  /* XXX: is outputFormat samplesize of compressed data or samplesize of
   * decoded data, maybe upsampled? Also, FAAD support FLOAT output,
   * how do we handle that (FAAD_FMT_FLOAT)? ::atmos
   */
  if(sh_audio->samplesize)
    switch(sh_audio->samplesize){
      case 1: // 8Bit
	mp_msg(MSGT_DECAUDIO,MSGL_WARN,"FAAD: 8Bit samplesize not supported by FAAD, assuming 16Bit!\n");
      default:
      case 2: // 16Bit
	faac_conf->outputFormat = FAAD_FMT_16BIT;
	break;
      case 3: // 24Bit
	faac_conf->outputFormat = FAAD_FMT_24BIT;
	break;
      case 4: // 32Bit
	faac_conf->outputFormat = FAAD_FMT_32BIT;
	break;
    }
  faac_conf->defObjectType = LTP; // => MAIN, LC, SSR, LTP available.

  faacDecSetConfiguration(faac_hdec, faac_conf);
#endif

  if(faac_buffer == NULL)
    faac_buffer = (unsigned char*)malloc(FAAD_BUFFLEN);
  memset(faac_buffer, 0, FAAD_BUFFLEN);
  demux_read_data(sh_audio->ds, faac_buffer, FAAD_BUFFLEN);

  /* init the codec */
  if((faac_bytesconsumed = faacDecInit(faac_hdec, faac_buffer, &faac_samplerate, &faac_channels)) < 0) {
    mp_msg(MSGT_DECAUDIO,MSGL_WARN,"FAAD: Failed to initialize the decoder!\n"); // XXX: deal with cleanup!
    faacDecClose(faac_hdec);
    free(faac_buffer);
    faac_buffer = NULL;
    driver = 0;
  } else {
    mp_msg(MSGT_DECAUDIO,MSGL_V,"FAAD: Decoder init done (%dBytes)!\n", faac_bytesconsumed); // XXX: remove or move to debug!
    mp_msg(MSGT_DECAUDIO,MSGL_V,"FAAD: Negotiated samplerate: %dHz  channels: %d\n", faac_samplerate, faac_channels);
    sh_audio->channels = faac_channels;
    sh_audio->samplerate = faac_samplerate;
    sh_audio->i_bps = 128*1000/8; // XXX: HACK!!! There's currently no way to get bitrate from libfaad2! ::atmos
  }  
	    
} break;		
#endif

#ifdef USE_LIBMAD
 case AFM_MAD:
   {
     printf("%s %s %s (%s)\n", mad_version, mad_copyright, mad_author, mad_build);

     mp_msg(MSGT_DECVIDEO, MSGL_V, "mad: initialising\n");
     mad_frame_init(&mad_frame);
     mad_stream_init(&mad_stream);

     mp_msg(MSGT_DECVIDEO, MSGL_V, "mad: preparing buffer\n");
     mad_prepare_buffer(sh_audio, &mad_stream, sh_audio->a_in_buffer_size);
     mad_stream_buffer(&mad_stream, (unsigned char*)(sh_audio->a_in_buffer), sh_audio->a_in_buffer_len);
//     mad_stream_sync(&mad_stream);
     mad_sync(sh_audio, &mad_stream);
     mad_synth_init(&mad_synth);

     if(mad_frame_decode(&mad_frame, &mad_stream) == 0)
       {
	 mp_msg(MSGT_DECVIDEO, MSGL_V, "mad: post processing buffer\n");
	 mad_postprocess_buffer(sh_audio, &mad_stream);
       }
     else
       {
	 mp_msg(MSGT_DECVIDEO, MSGL_V, "mad: frame decoding failed\n");
	 mad_print_error(&mad_stream);
       }
     
     switch (mad_frame.header.mode)
     {
        case MAD_MODE_SINGLE_CHANNEL:
	    sh_audio->channels=1;
	    break;
	case MAD_MODE_DUAL_CHANNEL:
	case MAD_MODE_JOINT_STEREO:
	case MAD_MODE_STEREO:
	    sh_audio->channels=2;
	    break;
	default:
	    mp_msg(MSGT_DECAUDIO, MSGL_FATAL, "mad: unknown number of channels\n");
     }
     mp_msg(MSGT_DECAUDIO, MSGL_HINT, "mad: channels: %d (mad channel mode: %d)\n",
        sh_audio->channels, mad_frame.header.mode);
/* var. name changed in 0.13.0 (beta) (libmad/CHANGES) -- alex */
#if (MAD_VERSION_MAJOR >= 0) && (MAD_VERSION_MINOR >= 13)
     sh_audio->samplerate=mad_frame.header.samplerate;
#else
     sh_audio->samplerate=mad_frame.header.sfreq;
#endif
     sh_audio->i_bps=mad_frame.header.bitrate;
     mp_msg(MSGT_DECVIDEO, MSGL_V, "mad: continuing\n");
     break;
   }
#endif
}

if(!sh_audio->channels || !sh_audio->samplerate){
  mp_msg(MSGT_DECAUDIO,MSGL_WARN,MSGTR_UnknownAudio);
  driver=0;
}

  if(!driver){
      if(sh_audio->a_buffer) free(sh_audio->a_buffer);
      sh_audio->a_buffer=NULL;
      return 0;
  }

  if(!sh_audio->o_bps)
  sh_audio->o_bps=sh_audio->channels*sh_audio->samplerate*sh_audio->samplesize;
  return driver;
}

// Audio decoding:

// Decode a single frame (mp3,acm etc) or 'minlen' bytes (pcm/alaw etc)
// buffer length is 'maxlen' bytes, it shouldn't be exceeded...

int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen){
    int len=-1;
    switch(sh_audio->codec->driver){
#ifdef USE_LIBAVCODEC
      case AFM_FFMPEG: {
          unsigned char *start=NULL;
	  int y;
	  while(len<minlen){
	    int len2=0;
	    int x=ds_get_packet(sh_audio->ds,&start);
	    if(x<=0) break; // error
	    y=avcodec_decode_audio(&lavc_context,(INT16*)buf,&len2,start,x);
	    if(y<0){ mp_msg(MSGT_DECAUDIO,MSGL_V,"lavc_audio: error\n");break; }
	    if(y<x) sh_audio->ds->buffer_pos+=y-x;  // put back data (HACK!)
	    if(len2>0){
	      //len=len2;break;
	      if(len<0) len=len2; else len+=len2;
	      buf+=len2;
	    }
            mp_dbg(MSGT_DECAUDIO,MSGL_DBG2,"Decoded %d -> %d  \n",y,len2);
	  }
        }
        break;
#endif
      case AFM_MPEG: // MPEG layer 2 or 3
        len=MP3_DecodeFrame(buf,-1);
//        len=MP3_DecodeFrame(buf,3);
        break;
#ifdef HAVE_OGGVORBIS
      case AFM_VORBIS: { // Vorbis
        int samples;
        float **pcm;
        ogg_packet op;
        char* np;
        struct ov_struct_st *ov = sh_audio->context;
        len = 0;
        op.b_o_s =  op.e_o_s = 0;
	while(len < minlen) {
	  op.bytes = ds_get_packet(sh_audio->ds,&op.packet);
	  if(!op.packet)
	    break;
	  if(vorbis_synthesis(&ov->vb,&op)==0) /* test for success! */
	    vorbis_synthesis_blockin(&ov->vd,&ov->vb);
	  while((samples=vorbis_synthesis_pcmout(&ov->vd,&pcm))>0){
	    int i,j;
	    int clipflag=0;
	    int convsize=(maxlen-len)/(2*ov->vi.channels); // max size!
	    int bout=(samples<convsize?samples:convsize);
	  
	    if(bout<=0) break;

	    /* convert floats to 16 bit signed ints (host order) and
	       interleave */
	    for(i=0;i<ov->vi.channels;i++){
	      ogg_int16_t *convbuffer=(ogg_int16_t *)(&buf[len]);
	      ogg_int16_t *ptr=convbuffer+i;
	      float  *mono=pcm[i];
	      for(j=0;j<bout;j++){
#if 1
		int val=mono[j]*32767.f;
#else /* optional dither */
		int val=mono[j]*32767.f+drand48()-0.5f;
#endif
		/* might as well guard against clipping */
		if(val>32767){
		  val=32767;
		  clipflag=1;
		}
		if(val<-32768){
		  val=-32768;
		  clipflag=1;
		}
		*ptr=val;
		ptr+=ov->vi.channels;
	      }
	    }
		
	    if(clipflag)
	      mp_msg(MSGT_DECAUDIO,MSGL_DBG2,"Clipping in frame %ld\n",(long)(ov->vd.sequence));
	    len+=2*ov->vi.channels*bout;
	    mp_msg(MSGT_DECAUDIO,MSGL_DBG2,"\n[decoded: %d / %d ]\n",bout,samples);
	    vorbis_synthesis_read(&ov->vd,bout); /* tell libvorbis how
						    many samples we
						    actually consumed */
	  }
	}
      } break;
#endif
		       
#ifdef HAVE_FAAD		       
      case AFM_AAC: {
	int /*i,*/ k, j = 0;	      
	void *faac_sample_buffer;
	
	len = 0;
	while(len < minlen) {
	  /* update buffer */
    	  if (faac_bytesconsumed > 0) {
	    for (k = 0; k < (FAAD_BUFFLEN - faac_bytesconsumed); k++)
	      faac_buffer[k] = faac_buffer[k + faac_bytesconsumed];
	    demux_read_data(sh_audio->ds, faac_buffer + (FAAD_BUFFLEN) - faac_bytesconsumed, faac_bytesconsumed);
	    faac_bytesconsumed = 0;
	  }
	  /*for (i = 0; i < 16; i++)
	    printf ("%02X ", faac_buffer[i]);
	  printf ("\n");*/
	  do {
	    faac_sample_buffer = faacDecDecode(faac_hdec, &faac_finfo, faac_buffer+j);
	    /* update buffer index after faacDecDecode */
	    faac_bytesconsumed += faac_finfo.bytesconsumed;
	    if(faac_finfo.error > 0) {
	      mp_msg(MSGT_DECAUDIO,MSGL_WARN,"FAAD: Trying to resync!\n");
	      j++;
	    } else
	      break;
	  } while(j < FAAD_BUFFLEN);	  


	  if(faac_finfo.error > 0) {
	    mp_msg(MSGT_DECAUDIO,MSGL_WARN,"FAAD: Failed to decode frame: %s \n",
	      faacDecGetErrorMessage(faac_finfo.error));
	  } else if (faac_finfo.samples == 0)
	    mp_msg(MSGT_DECAUDIO,MSGL_DBG2,"FAAD: Decoded zero samples!\n");
	  else {
	    mp_msg(MSGT_DECAUDIO,MSGL_DBG2,"FAAD: Successfully decoded frame (%dBytes)!\n", faac_finfo.samples*faac_finfo.channels);
	    memcpy(buf+len,faac_sample_buffer, faac_finfo.samples*faac_finfo.channels);
	    len += faac_finfo.samples*faac_finfo.channels;
	  }
	}

      } break;
#endif		    
      case AFM_PCM: // AVI PCM
        len=demux_read_data(sh_audio->ds,buf,minlen);
        break;
      case AFM_DVDPCM: // DVD PCM
      { int j;
        len=demux_read_data(sh_audio->ds,buf,minlen);
          //if(i&1){ printf("Warning! pcm_audio_size&1 !=0  (%d)\n",i);i&=~1; }
          // swap endian:
          for(j=0;j<len;j+=2){
            char x=buf[j];
            buf[j]=buf[j+1];
            buf[j+1]=x;
          }
        break;
      }
      case AFM_ALAW:  // aLaw decoder
      { int l=demux_read_data(sh_audio->ds,buf,minlen/2);
        unsigned short *d=(unsigned short *) buf;
        unsigned char *s=buf;
        len=2*l;
        if(sh_audio->format==6){
        // aLaw
          while(l>0){ --l; d[l]=alaw2short[s[l]]; }
        } else {
        // uLaw
          while(l>0){ --l; d[l]=ulaw2short[s[l]]; }
        }
        break;
      }
      case AFM_GSM:  // MS-GSM decoder
      { unsigned char ibuf[65]; // 65 bytes / frame
        if(demux_read_data(sh_audio->ds,ibuf,65)!=65) break; // EOF
        XA_MSGSM_Decoder(ibuf,(unsigned short *) buf); // decodes 65 byte -> 320 short
//  	    XA_GSM_Decoder(buf,(unsigned short *) &sh_audio->a_buffer[sh_audio->a_buffer_len]); // decodes 33 byte -> 160 short
        len=2*320;
        break;
      }
#ifdef USE_G72X
      case AFM_G72X:  // GSM 723 decoder
      { if(demux_read_data(sh_audio->ds,g72x_data.block, g72x_data.blocksize)!=g72x_data.blocksize) break; // EOF
        g72x_decode_block(&g72x_data);
	len=2*g72x_data.samplesperblock;
	memcpy(buf,g72x_data.samples,len);
        break;
      }
#endif
      case AFM_IMAADPCM:
      { unsigned char ibuf[IMA_ADPCM_BLOCK_SIZE * 2]; // bytes / stereo frame
        if (demux_read_data(sh_audio->ds, ibuf,
          IMA_ADPCM_BLOCK_SIZE * sh_audio->wf->nChannels) != 
          IMA_ADPCM_BLOCK_SIZE * sh_audio->wf->nChannels) 
          break; // EOF
        len=2*ima_adpcm_decode_block((unsigned short*)buf,ibuf, sh_audio->wf->nChannels);
        break;
      }
      case AFM_MSADPCM:
      { static unsigned char *ibuf = NULL;
        if (!ibuf)
          ibuf = (unsigned char *)malloc
            (sh_audio->wf->nBlockAlign * sh_audio->wf->nChannels);
        if (demux_read_data(sh_audio->ds, ibuf,
          sh_audio->wf->nBlockAlign) != 
          sh_audio->wf->nBlockAlign) 
          break; // EOF
        len= 2 * ms_adpcm_decode_block(
          (unsigned short*)buf,ibuf, sh_audio->wf->nChannels,
          sh_audio->wf->nBlockAlign);
        break;
      }
      case AFM_DK4ADPCM:
      { static unsigned char *ibuf = NULL;
        if (!ibuf)
          ibuf = (unsigned char *)malloc(sh_audio->wf->nBlockAlign);
        if (demux_read_data(sh_audio->ds, ibuf, sh_audio->wf->nBlockAlign) != 
          sh_audio->wf->nBlockAlign)
          break; // EOF
        len=2*dk4_adpcm_decode_block((unsigned short*)buf,ibuf,
          sh_audio->wf->nChannels, sh_audio->wf->nBlockAlign);
        break;
      }
      case AFM_DK3ADPCM:
      { unsigned char ibuf[DK3_ADPCM_BLOCK_SIZE * 2]; // bytes / stereo frame
        if (demux_read_data(sh_audio->ds, ibuf,
          DK3_ADPCM_BLOCK_SIZE * sh_audio->wf->nChannels) != 
          DK3_ADPCM_BLOCK_SIZE * sh_audio->wf->nChannels) 
          break; // EOF
        len = 2 * dk3_adpcm_decode_block(
          (unsigned short*)buf,ibuf);
        break;
      }
      case AFM_ROQAUDIO:
      {
        static unsigned char *ibuf = NULL;
        unsigned char header_data[6];
        int read_len;

        if (!ibuf)
          ibuf = (unsigned char *)malloc(sh_audio->audio_out_minsize / 2);

        // figure out how much data to read
        if (demux_read_data(sh_audio->ds, header_data, 6) != 6)
          break; // EOF
        read_len = (header_data[5] << 24) | (header_data[4] << 16) |
          (header_data[3] << 8) | header_data[2];
        read_len += 2;  // 16-bit arguments
        if (demux_read_data(sh_audio->ds, ibuf, read_len) != read_len)
          break;
        len = 2 * roq_decode_audio((unsigned short *)buf, ibuf,
          read_len, sh_audio->channels, sh_audio->context);          
        break;
      }
#ifdef USE_LIBAC3
      case AFM_AC3: // AC3 decoder
        //printf("{1:%d}",avi_header.idx_pos);fflush(stdout);
        if(!sh_audio->ac3_frame) sh_audio->ac3_frame=ac3_decode_frame();
        //printf("{2:%d}",avi_header.idx_pos);fflush(stdout);
        if(sh_audio->ac3_frame){
          len = 256 * 6 *sh_audio->channels*sh_audio->samplesize;
          memcpy(buf,((ac3_frame_t*)sh_audio->ac3_frame)->audio_data,len);
          sh_audio->ac3_frame=NULL;
        }
        //printf("{3:%d}",avi_header.idx_pos);fflush(stdout);
        break;
#endif
      case AFM_A52: { // AC3 decoder
	sample_t level=1, bias=384;
        int flags=a52_flags|A52_ADJUST_LEVEL;
	int i;
        if(!sh_audio->a_in_buffer_len) 
	    if(a52_fillbuff(sh_audio)<0) break; // EOF
	sh_audio->a_in_buffer_len=0;
	if (a52_frame (&a52_state, sh_audio->a_in_buffer, &flags, &level, bias)){
	    mp_msg(MSGT_DECAUDIO,MSGL_WARN,"a52: error decoding frame\n");
	    break;
	}
	// a52_dynrng (&state, NULL, NULL); // disable dynamic range compensation

	// frame decoded, let's resample:
	//a52_resample_init(a52_accel,flags,sh_audio->channels);
	len=0;
	for (i = 0; i < 6; i++) {
	    if (a52_block (&a52_state, a52_samples)){
		mp_msg(MSGT_DECAUDIO,MSGL_WARN,"a52: error at resampling\n");
		break;
	    }
	    len+=2*a52_resample(a52_samples,&buf[len]);
	}
	// printf("len = %d      \n",len); // 6144 on all vobs I tried so far... (5.1 and 2.0) ::atmos
	break;
      }
      case AFM_HWAC3: // AC3 through SPDIF
        if(!sh_audio->a_in_buffer_len)
	    if((len=a52_fillbuff(sh_audio))<0) break; //EOF
	sh_audio->a_in_buffer_len=0;
	len = ac3_iec958_build_burst(len, 0x01, 1, sh_audio->a_in_buffer, buf);
	// len = 6144 = 4*(6*256)
	break;
#ifdef USE_WIN32DLL
      case AFM_ACM:
//        len=sh_audio->audio_out_minsize; // optimal decoded fragment size
//        if(len<minlen) len=minlen; else
//        if(len>maxlen) len=maxlen;
//        len=acm_decode_audio(sh_audio,buf,len);
        len=acm_decode_audio(sh_audio,buf,minlen,maxlen);
        break;
#endif

#ifdef USE_DIRECTSHOW
      case AFM_DSHOW: // DirectShow
      { int size_in=0;
        int size_out=0;
        int srcsize=DS_AudioDecoder_GetSrcSize(ds_adec, maxlen);
        mp_msg(MSGT_DECAUDIO,MSGL_DBG3,"DShow says: srcsize=%d  (buffsize=%d)  out_size=%d\n",srcsize,sh_audio->a_in_buffer_size,maxlen);
        if(srcsize>sh_audio->a_in_buffer_size) srcsize=sh_audio->a_in_buffer_size; // !!!!!!
        if(sh_audio->a_in_buffer_len<srcsize){
          sh_audio->a_in_buffer_len+=
            demux_read_data(sh_audio->ds,&sh_audio->a_in_buffer[sh_audio->a_in_buffer_len],
            srcsize-sh_audio->a_in_buffer_len);
        }
        DS_AudioDecoder_Convert(ds_adec, sh_audio->a_in_buffer,sh_audio->a_in_buffer_len,
            buf,maxlen, &size_in,&size_out);
        mp_dbg(MSGT_DECAUDIO,MSGL_DBG2,"DShow: audio %d -> %d converted  (in_buf_len=%d of %d)  %d\n",size_in,size_out,sh_audio->a_in_buffer_len,sh_audio->a_in_buffer_size,ds_tell_pts(sh_audio->ds));
        if(size_in>=sh_audio->a_in_buffer_len){
          sh_audio->a_in_buffer_len=0;
        } else {
          sh_audio->a_in_buffer_len-=size_in;
          memcpy(sh_audio->a_in_buffer,&sh_audio->a_in_buffer[size_in],sh_audio->a_in_buffer_len);
        }
        len=size_out;
        break;
      }
#endif

#ifdef USE_LIBMAD
    case AFM_MAD:
      {
	mad_prepare_buffer(sh_audio, &mad_stream, sh_audio->a_in_buffer_size);
	mad_stream_buffer(&mad_stream, sh_audio->a_in_buffer, sh_audio->a_in_buffer_len);
//        mad_stream_sync(&mad_stream);
	mad_sync(sh_audio, &mad_stream);
	if(mad_frame_decode(&mad_frame, &mad_stream) == 0)
	  {
	    mad_synth_frame(&mad_synth, &mad_frame);
	    mad_postprocess_buffer(sh_audio, &mad_stream);
	    
	    /* and fill buffer */
	    
	    {
	      int i;
	      int end_size = mad_synth.pcm.length;
	      signed short* samples = (signed short*)buf;
	      if(end_size > maxlen/4)
		end_size=maxlen/4;
	      
	      for(i=0; i<mad_synth.pcm.length; ++i) {
		*samples++ = mad_scale(mad_synth.pcm.samples[0][i]);
		*samples++ = mad_scale(mad_synth.pcm.samples[0][i]);
		//		*buf++ = mad_scale(mad_synth.pcm.sampAles[1][i]);
	      }
	      len = end_size*4;
	    }
	  }
	else
	  {
	    mp_msg(MSGT_DECVIDEO, MSGL_ERR, "mad: frame decoding failed (error: %d)\n",
		mad_stream.error);
	    mad_print_error(&mad_stream);
	  }
	
	break;
      }
#endif
    }
    return len;
}

void resync_audio_stream(sh_audio_t *sh_audio){
        switch(sh_audio->codec->driver){
        case AFM_MPEG:
          MP3_DecodeFrame(NULL,-2); // resync
          MP3_DecodeFrame(NULL,-2); // resync
          MP3_DecodeFrame(NULL,-2); // resync
          break;
#ifdef USE_LIBAC3
        case AFM_AC3:
          ac3_bitstream_reset();    // reset AC3 bitstream buffer
    //      if(verbose){ printf("Resyncing AC3 audio...");fflush(stdout);}
          sh_audio->ac3_frame=ac3_decode_frame(); // resync
    //      if(verbose) printf(" OK!\n");
          break;
#endif
#ifdef HAVE_FAAD
	case AFM_AAC:
	  //if(faac_buffer != NULL)
	  faac_bytesconsumed = 0;
	  memset(faac_buffer, 0, FAAD_BUFFLEN);
  	  //demux_read_data(sh_audio->ds, faac_buffer, FAAD_BUFFLEN);
	  break;
#endif
        case AFM_A52:
        case AFM_ACM:
        case AFM_DSHOW:
	case AFM_HWAC3:
          sh_audio->a_in_buffer_len=0;        // reset ACM/DShow audio buffer
          break;

#ifdef USE_LIBMAD
	case AFM_MAD:
	  mad_prepare_buffer(sh_audio, &mad_stream, sh_audio->a_in_buffer_size);
	  mad_stream_buffer(&mad_stream, sh_audio->a_in_buffer, sh_audio->a_in_buffer_len);
//	  mad_stream_sync(&mad_stream);
	  mad_sync(sh_audio, &mad_stream);
	  mad_postprocess_buffer(sh_audio, &mad_stream);
	  break;
#endif	
        }
}

void skip_audio_frame(sh_audio_t *sh_audio){
              switch(sh_audio->codec->driver){
                case AFM_MPEG: MP3_DecodeFrame(NULL,-2);break; // skip MPEG frame
#ifdef USE_LIBAC3
                case AFM_AC3: sh_audio->ac3_frame=ac3_decode_frame();break; // skip AC3 frame
#endif
		case AFM_HWAC3:
                case AFM_A52: a52_fillbuff(sh_audio);break; // skip AC3 frame
		case AFM_ACM:
		case AFM_DSHOW: {
		    int skip=sh_audio->wf->nBlockAlign;
		    if(skip<16){
		      skip=(sh_audio->wf->nAvgBytesPerSec/16)&(~7);
		      if(skip<16) skip=16;
		    }
		    demux_read_data(sh_audio->ds,NULL,skip);
		    break;
		}
		case AFM_PCM:
		case AFM_DVDPCM:
		case AFM_ALAW: {
		    int skip=sh_audio->i_bps/16;
		    skip=skip&(~3);
		    demux_read_data(sh_audio->ds,NULL,skip);
		    break;
		}
#ifdef USE_LIBMAD
	      case AFM_MAD:
		{
		  mad_prepare_buffer(sh_audio, &mad_stream, sh_audio->a_in_buffer_size);
		  mad_stream_buffer(&mad_stream, sh_audio->a_in_buffer, sh_audio->a_in_buffer_len);
		  mad_stream_skip(&mad_stream, 2);
//		  mad_stream_sync(&mad_stream);
		  mad_sync(sh_audio, &mad_stream);
		  mad_postprocess_buffer(sh_audio, &mad_stream);
		  break;
		}
#endif

                default: ds_fill_buffer(sh_audio->ds);  // skip PCM frame
              }
}
