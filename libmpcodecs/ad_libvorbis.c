
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "ad_internal.h"

#ifdef HAVE_OGGVORBIS

static ad_info_t info = 
{
	"Ogg/Vorbis audio decoder",
	"libvorbis",
	"Felix Buenemann, A'rpi",
	"libvorbis",
	""
};

LIBAD_EXTERN(libvorbis)

#include <vorbis/codec.h>

// This struct is also defined in demux_ogg.c => common header ?
typedef struct ov_struct_st {
  vorbis_info      vi; /* struct that stores all the static vorbis bitstream
			  settings */
  vorbis_comment   vc; /* struct that stores all the bitstream user comments */
  vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
  vorbis_block     vb; /* local working space for packet->PCM decode */
} ov_struct_t;

static int preinit(sh_audio_t *sh)
{
  sh->audio_out_minsize=1024*4; // 1024 samples/frame
  return 1;
}

static int init(sh_audio_t *sh)
{
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
  ov = (struct ov_struct_st*)malloc(sizeof(struct ov_struct_st));
  vorbis_info_init(&ov->vi);
  vorbis_comment_init(&vc);
  op.bytes = ds_get_packet(sh->ds,&op.packet);
  op.b_o_s  = 1;
  /// Header
  if(vorbis_synthesis_headerin(&ov->vi,&vc,&op) <0) {
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,"OggVorbis: initial (identification) header broken!\n");
    ERROR();
  }
  op.bytes = ds_get_packet(sh->ds,&op.packet);
  op.b_o_s  = 0;
  /// Comments
  if(vorbis_synthesis_headerin(&ov->vi,&vc,&op) <0) {
    mp_msg(MSGT_DECAUDIO,MSGL_ERR,"OggVorbis: comment header broken!\n");
    ERROR();
  }
  op.bytes = ds_get_packet(sh->ds,&op.packet);
  //// Codebook
  if(vorbis_synthesis_headerin(&ov->vi,&vc,&op)<0) {
    mp_msg(MSGT_DECAUDIO,MSGL_WARN,"OggVorbis: codebook header broken!\n");
    ERROR();
  } else { /// Print the infos
    char **ptr=vc.user_comments;
    while(*ptr){
      mp_msg(MSGT_DECAUDIO,MSGL_V,"OggVorbisComment: %s\n",*ptr);
      ++ptr;
    }
    mp_msg(MSGT_DECAUDIO,MSGL_V,"OggVorbis: Bitstream is %d channel, %dHz, %dbit/s %cBR\n",(int)ov->vi.channels,(int)ov->vi.rate,(int)ov->vi.bitrate_nominal,
	(ov->vi.bitrate_lower!=ov->vi.bitrate_nominal)||(ov->vi.bitrate_upper!=ov->vi.bitrate_nominal)?'V':'C');
    mp_msg(MSGT_DECAUDIO,MSGL_V,"OggVorbis: Encoded by: %s\n",vc.vendor);
  }
  vorbis_comment_clear(&vc);

//  printf("lower=%d  upper=%d  \n",(int)ov->vi.bitrate_lower,(int)ov->vi.bitrate_upper);

  // Setup the decoder
  sh->channels=ov->vi.channels; 
  sh->samplerate=ov->vi.rate;
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
        float **pcm;
        ogg_packet op;
        struct ov_struct_st *ov = sh->context;
        op.b_o_s =  op.e_o_s = 0;
	while(len < minlen) {
	  op.bytes = ds_get_packet(sh->ds,&op.packet);
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



  return len;
}

#endif /* !HAVE_OGGVORBIS */

