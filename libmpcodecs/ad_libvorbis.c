
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <math.h>

#include "config.h"
#include "ad_internal.h"
#include "libaf/reorder_ch.h"

static ad_info_t info = 
{
	"Ogg/Vorbis audio decoder",
#ifdef CONFIG_TREMOR
	"tremor",
#else
	"libvorbis",
#endif
	"Felix Buenemann, A'rpi",
	"libvorbis",
	""
};

LIBAD_EXTERN(libvorbis)

#ifdef CONFIG_TREMOR
#include <tremor/ivorbiscodec.h>
#else
#include <vorbis/codec.h>
#endif

// This struct is also defined in demux_ogg.c => common header ?
typedef struct ov_struct_st {
  vorbis_info      vi; /* struct that stores all the static vorbis bitstream
			  settings */
  vorbis_comment   vc; /* struct that stores all the bitstream user comments */
  vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
  vorbis_block     vb; /* local working space for packet->PCM decode */
  float            rg_scale; /* replaygain scale */
#ifdef CONFIG_TREMOR
  int              rg_scale_int;
#endif
} ov_struct_t;

static int read_vorbis_comment( char* ptr, const char* comment, const char* format, ... ) {
  va_list va;
  int clen, ret;

  va_start( va, format );
  clen = strlen( comment );
  ret = strncasecmp( ptr, comment, clen) == 0 ? vsscanf( ptr+clen, format, va ) : 0;
  va_end( va );

  return ret;
}

static int preinit(sh_audio_t *sh)
{
  sh->audio_out_minsize=1024*4; // 1024 samples/frame
  return 1;
}

static int init(sh_audio_t *sh)
{
  unsigned int offset, i, length, hsizes[3];
  void *headers[3];
  unsigned char* extradata;
  ogg_packet op;
  vorbis_comment vc;
  struct ov_struct_st *ov;
#define ERROR() { \
    vorbis_comment_clear(&vc); \
    vorbis_info_clear(&ov->vi); \
    free(ov); \
    return 0; \
  }

  /// Init the decoder with the 3 header packets
  ov = malloc(sizeof(struct ov_struct_st));
  vorbis_info_init(&ov->vi);
  vorbis_comment_init(&vc);

  if(! sh->wf) {
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,"ad_vorbis, extradata seems to be absent! exit\n");
    ERROR();
  }

  if(! sh->wf->cbSize) {
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,"ad_vorbis, extradata seems to be absent!, exit\n");
    ERROR();
  }

  mp_msg(MSGT_DECAUDIO,MSGL_V,"ad_vorbis, extradata seems is %d bytes long\n", sh->wf->cbSize);
  extradata = (char*) (sh->wf+1);
  if(!extradata) {
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,"ad_vorbis, extradata seems to be NULL!, exit\n");
    ERROR();
  }

  if(*extradata != 2) {
    mp_msg (MSGT_DEMUX, MSGL_WARN, "ad_vorbis: Vorbis track does not contain valid headers.\n");
    ERROR();
  }

  offset = 1;
  for (i=0; i < 2; i++) {
    length = 0;
    while ((extradata[offset] == (unsigned char) 0xFF) && length < sh->wf->cbSize) {
      length += 255;
      offset++;
    }
    if(offset >= (sh->wf->cbSize - 1)) {
      mp_msg (MSGT_DEMUX, MSGL_WARN, "ad_vorbis: Vorbis track does not contain valid headers.\n");
      ERROR();
    }
    length += extradata[offset];
    offset++;
    mp_msg (MSGT_DEMUX, MSGL_V, "ad_vorbis, offset: %u, length: %u\n", offset, length);
    hsizes[i] = length;
  }

  headers[0] = &extradata[offset];
  headers[1] = &extradata[offset + hsizes[0]];
  headers[2] = &extradata[offset + hsizes[0] + hsizes[1]];
  hsizes[2] = sh->wf->cbSize - offset - hsizes[0] - hsizes[1];
  mp_msg (MSGT_DEMUX, MSGL_V, "ad_vorbis, header sizes: %d %d %d\n", hsizes[0], hsizes[1], hsizes[2]);

  for(i=0; i<3; i++) {
    op.bytes = hsizes[i];
    op.packet = headers[i];
    op.b_o_s  = (i == 0);
    if(vorbis_synthesis_headerin(&ov->vi,&vc,&op) <0) {
      mp_msg(MSGT_DECAUDIO,MSGL_ERR,"OggVorbis: header n. %d broken! len=%ld\n", i, op.bytes);
      ERROR();
    }
    if(i == 2) {
      float rg_gain=0.f, rg_peak=0.f;
    char **ptr=vc.user_comments;
    while(*ptr){
      mp_msg(MSGT_DECAUDIO,MSGL_V,"OggVorbisComment: %s\n",*ptr);
      /* replaygain */
      read_vorbis_comment( *ptr, "replaygain_album_gain=", "%f", &rg_gain );
      read_vorbis_comment( *ptr, "rg_audiophile=", "%f", &rg_gain );
      if( !rg_gain ) {
	read_vorbis_comment( *ptr, "replaygain_track_gain=", "%f", &rg_gain );
	read_vorbis_comment( *ptr, "rg_radio=", "%f", &rg_gain );
      }
      read_vorbis_comment( *ptr, "replaygain_album_peak=", "%f", &rg_peak );
      if( !rg_peak ) {
	read_vorbis_comment( *ptr, "replaygain_track_peak=", "%f", &rg_peak );
	read_vorbis_comment( *ptr, "rg_peak=", "%f", &rg_peak );
      }
      ++ptr;
    }
    /* replaygain: scale */
    if(!rg_gain)
      ov->rg_scale = 1.f; /* just in case pow() isn't standard-conformant */
    else
      ov->rg_scale = pow(10.f, rg_gain/20);
    /* replaygain: anticlip */
    if(ov->rg_scale * rg_peak > 1.f)
      ov->rg_scale = 1.f / rg_peak;
    /* replaygain: security */
    if(ov->rg_scale > 15.) 
      ov->rg_scale = 15.;
#ifdef CONFIG_TREMOR
    ov->rg_scale_int = (int)(ov->rg_scale*64.f);
#endif
    mp_msg(MSGT_DECAUDIO,MSGL_V,"OggVorbis: Bitstream is %d channel%s, %dHz, %dbit/s %cBR\n",(int)ov->vi.channels,ov->vi.channels>1?"s":"",(int)ov->vi.rate,(int)ov->vi.bitrate_nominal,
	(ov->vi.bitrate_lower!=ov->vi.bitrate_nominal)||(ov->vi.bitrate_upper!=ov->vi.bitrate_nominal)?'V':'C');
    if(rg_gain || rg_peak)
      mp_msg(MSGT_DECAUDIO,MSGL_V,"OggVorbis: Gain = %+.2f dB, Peak = %.4f, Scale = %.2f\n", rg_gain, rg_peak, ov->rg_scale);
    mp_msg(MSGT_DECAUDIO,MSGL_V,"OggVorbis: Encoded by: %s\n",vc.vendor);
    }
  }

  vorbis_comment_clear(&vc);

//  printf("lower=%d  upper=%d  \n",(int)ov->vi.bitrate_lower,(int)ov->vi.bitrate_upper);

  // Setup the decoder
  sh->channels=ov->vi.channels; 
  sh->samplerate=ov->vi.rate;
  sh->samplesize=2;
  // assume 128kbit if bitrate not specified in the header
  sh->i_bps=((ov->vi.bitrate_nominal>0) ? ov->vi.bitrate_nominal : 128000)/8;
  sh->context = ov;

  /// Finish the decoder init
  vorbis_synthesis_init(&ov->vd,&ov->vi);
  vorbis_block_init(&ov->vd,&ov->vb);
  mp_msg(MSGT_DECAUDIO,MSGL_V,"OggVorbis: Init OK!\n");

  return 1;
}

static void uninit(sh_audio_t *sh)
{
  struct ov_struct_st *ov = sh->context;
  vorbis_dsp_clear(&ov->vd);
  vorbis_block_clear(&ov->vb);
  vorbis_info_clear(&ov->vi);
  free(ov);
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
    switch(cmd)
    {
#if 0      
      case ADCTRL_RESYNC_STREAM:
	  return CONTROL_TRUE;
      case ADCTRL_SKIP_FRAME:
	  return CONTROL_TRUE;
#endif
    }
  return CONTROL_UNKNOWN;
}

static int decode_audio(sh_audio_t *sh,unsigned char *buf,int minlen,int maxlen)
{
        int len = 0;
        int samples;
#ifdef CONFIG_TREMOR
        ogg_int32_t **pcm;
#else
        float scale;
        float **pcm;
#endif
        struct ov_struct_st *ov = sh->context;
	while(len < minlen) {
	  while((samples=vorbis_synthesis_pcmout(&ov->vd,&pcm))<=0){
	    ogg_packet op;
	    double pts;
	    memset(&op,0,sizeof(op)); //op.b_o_s = op.e_o_s = 0;
	    op.bytes = ds_get_packet_pts(sh->ds,&op.packet, &pts);
	    if(op.bytes<=0) break;
	    if (pts != MP_NOPTS_VALUE) {
		sh->pts = pts;
		sh->pts_bytes = 0;
	    }
	    if(vorbis_synthesis(&ov->vb,&op)==0) /* test for success! */
	      vorbis_synthesis_blockin(&ov->vd,&ov->vb);
	  }
	  if(samples<=0) break; // error/EOF
	  while(samples>0){
	    int i,j;
	    int clipflag=0;
	    int convsize=(maxlen-len)/(2*ov->vi.channels); // max size!
	    int bout=((samples<convsize)?samples:convsize);
	  
	    if(bout<=0) break; // no buffer space

	    /* convert floats to 16 bit signed ints (host order) and
	       interleave */
#ifdef CONFIG_TREMOR
           if (ov->rg_scale_int == 64) {
	    for(i=0;i<ov->vi.channels;i++){
	      ogg_int16_t *convbuffer=(ogg_int16_t *)(&buf[len]);
	      ogg_int16_t *ptr=convbuffer+i;
	      ogg_int32_t  *mono=pcm[i];
	      for(j=0;j<bout;j++){
		int val=mono[j]>>9;
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
	   } else
#endif /* CONFIG_TREMOR */
	   {
#ifndef CONFIG_TREMOR
            scale = 32767.f * ov->rg_scale;
#endif
	    for(i=0;i<ov->vi.channels;i++){
	      ogg_int16_t *convbuffer=(ogg_int16_t *)(&buf[len]);
	      ogg_int16_t *ptr=convbuffer+i;
#ifdef CONFIG_TREMOR
	      ogg_int32_t  *mono=pcm[i];
	      for(j=0;j<bout;j++){
		int val=(mono[j]*ov->rg_scale_int)>>(9+6);
#else
	      float  *mono=pcm[i];
	      for(j=0;j<bout;j++){
		int val=mono[j]*scale;
		/* might as well guard against clipping */
		if(val>32767){
		  val=32767;
		  clipflag=1;
		}
		if(val<-32768){
		  val=-32768;
		  clipflag=1;
		}
#endif /* CONFIG_TREMOR */
		*ptr=val;
		ptr+=ov->vi.channels;
	      }
	    }
	   }
		
	    if(clipflag)
	      mp_msg(MSGT_DECAUDIO,MSGL_DBG2,"Clipping in frame %ld\n",(long)(ov->vd.sequence));
	    len+=2*ov->vi.channels*bout;
	    sh->pts_bytes += 2*ov->vi.channels*bout;
	    mp_msg(MSGT_DECAUDIO,MSGL_DBG2,"\n[decoded: %d / %d ]\n",bout,samples);
	    samples-=bout;
	    vorbis_synthesis_read(&ov->vd,bout); /* tell libvorbis how
						    many samples we
						    actually consumed */
	  } //while(samples>0)
//          if (!samples) break; // why? how?
	}

	if (len > 0 && ov->vi.channels >= 5) {
	  reorder_channel_nch(buf, AF_CHANNEL_LAYOUT_VORBIS_DEFAULT,
	                      AF_CHANNEL_LAYOUT_MPLAYER_DEFAULT,
	                      ov->vi.channels, len / sh->samplesize,
	                      sh->samplesize);
	}


  return len;
}
