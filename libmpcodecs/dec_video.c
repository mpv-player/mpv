
#include "config.h"

#include <stdio.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdlib.h>
#include <unistd.h>

#include "mp_msg.h"
#include "help_mp.h"

#include "linux/timer.h"
#include "linux/shmem.h"

extern int verbose; // defined in mplayer.c

#include "stream.h"
#include "demuxer.h"
#include "parse_es.h"

#include "codec-cfg.h"

#include "libvo/video_out.h"

#include "stheader.h"
#include "vd.h"

#include "dec_video.h"

// ===================================================================

extern int benchmark;
extern double video_time_usage;
extern double vout_time_usage;
extern double max_video_time_usage;
extern double max_vout_time_usage;
extern double cur_video_time_usage;
extern double cur_vout_time_usage;
extern vo_vaa_t vo_vaa;

#include "postproc/postprocess.h"

#include "cpudetect.h"

int divx_quality=0;

static vd_functions_t* mpvdec=NULL;

int get_video_quality_max(sh_video_t *sh_video){
  if(mpvdec){
    int ret=mpvdec->control(sh_video,VDCTRL_QUERY_MAX_PP_LEVEL,NULL);
    if(ret>=0) return ret;
  }
 return 0;
}

void set_video_quality(sh_video_t *sh_video,int quality){
  if(mpvdec)
    mpvdec->control(sh_video,VDCTRL_SET_PP_LEVEL, (void*)(&quality));
}

int set_video_colors(sh_video_t *sh_video,char *item,int value)
{
    if(vo_vaa.get_video_eq)
    {
	vidix_video_eq_t veq;
	if(vo_vaa.get_video_eq(&veq) == 0)
	{
	    int v_hw_equ_cap = veq.cap;
	    if(v_hw_equ_cap != 0)
	    {
		if(vo_vaa.set_video_eq)
		{
		    vidix_video_eq_t veq;
		    veq.flags = VEQ_FLG_ITU_R_BT_601; /* Fixme please !!! */
		    if(strcmp(item,"Brightness") == 0)
		    {
			if(!(v_hw_equ_cap & VEQ_CAP_BRIGHTNESS)) goto try_sw_control;
			veq.brightness = value*10;
			veq.cap = VEQ_CAP_BRIGHTNESS;
		    }
		    else
		    if(strcmp(item,"Contrast") == 0)
		    {
			if(!(v_hw_equ_cap & VEQ_CAP_CONTRAST)) goto try_sw_control;
			veq.contrast = value*10;
			veq.cap = VEQ_CAP_CONTRAST;
		    }
		    else
		    if(strcmp(item,"Saturation") == 0)
		    {
			if(!(v_hw_equ_cap & VEQ_CAP_SATURATION)) goto try_sw_control;
			veq.saturation = value*10;
			veq.cap = VEQ_CAP_SATURATION;
		    }
		    else
		    if(strcmp(item,"Hue") == 0)
		    {
			if(!(v_hw_equ_cap & VEQ_CAP_HUE)) goto try_sw_control;
			veq.hue = value*10;
			veq.cap = VEQ_CAP_HUE;
		    }
		    else goto try_sw_control;;
		    vo_vaa.set_video_eq(&veq);
		}
		return 1;
	    }
	}
    }
    try_sw_control:
    if(mpvdec) return mpvdec->control(sh_video,VDCTRL_SET_EQUALIZER,item,(int)value);
    return 0;
}

void uninit_video(sh_video_t *sh_video){
    if(!sh_video->inited) return;
    mp_msg(MSGT_DECVIDEO,MSGL_V,"uninit video: %d  \n",sh_video->codec->driver);
    mpvdec->uninit(sh_video);
    sh_video->inited=0;
}

int init_video(sh_video_t *sh_video,int *pitches)
{
//unsigned int out_fmt=sh_video->codec->outfmt[sh_video->outfmtidx];
int i;
//pitches[0] = pitches[1] =pitches[2] = 0; /* fake unknown */

//sh_video->our_out_buffer=NULL;
//sh_video->our_out_buffer_size=0U;

  for (i=0; mpcodecs_vd_drivers[i] != NULL; i++)
    if(mpcodecs_vd_drivers[i]->info->id==sh_video->codec->driver){
	mpvdec=mpcodecs_vd_drivers[i]; break;
    }
  if(!mpvdec){
      mp_msg(MSGT_DECVIDEO,MSGL_ERR,"Requested video codec family [%s] (vfm=%d) not available (enable it at compile time!)\n",
          sh_video->codec->name, sh_video->codec->driver);
      return 0; // no such driver
  }
  
  printf("Selecting Video Decoder: [%s] %s\n",mpvdec->info->short_name,mpvdec->info->name);
  
  if(!mpvdec->init(sh_video)){
      printf("VDecoder init failed :(\n");
      return 0;
  }
  
  sh_video->inited=1;
  return 1;
}

extern int vaa_use_dr;

static int use_dr=0,use_dr_422=0;
static bes_da_t bda;
void init_video_vaa( unsigned width )
{
  unsigned adp;
  memset(&bda,0,sizeof(bes_da_t));
  if(vo_vaa.query_bes_da)
    use_dr = vo_vaa.query_bes_da(&bda) ? 0 : 1;
  if(!vaa_use_dr) use_dr = 0;
  if(use_dr)
  {
    uint32_t sstride,dstride;
    sstride=width*2;
    adp = bda.dest.pitch.y-1;
    dstride=(width*2+adp)&~adp;
    if(sstride == dstride) use_dr_422 = 1;
  }
}

int decode_video(vo_functions_t *video_out,sh_video_t *sh_video,unsigned char *start,int in_size,int drop_frame){
mp_image_t *mpi=NULL;
int blit_frame=0;
unsigned int t=GetTimer();
unsigned int t2;
double tt;

sh_video->video_out=video_out;
mpi=mpvdec->decode(sh_video, start, in_size, drop_frame);

//------------------------ frame decoded. --------------------

#ifdef ARCH_X86
	// some codecs is broken, and doesn't restore MMX state :(
	// it happens usually with broken/damaged files.
if(gCpuCaps.has3DNow){
	__asm __volatile ("femms\n\t":::"memory");
}
else if(gCpuCaps.hasMMX){
	__asm __volatile ("emms\n\t":::"memory");
}
#endif

if(!mpi) return 0; // error / skipped frame

t2=GetTimer();t=t2-t;
tt = t*0.000001f;
video_time_usage+=tt;
if(benchmark)
{
    if(tt > max_video_time_usage) max_video_time_usage=tt;
    cur_video_time_usage=tt;
}

if(drop_frame) return 0;

if(!(mpi->flags&(MP_IMGFLAG_DIRECT|MP_IMGFLAG_DRAW_CALLBACK))){
    // blit frame:
    if(mpi->flags&MP_IMGFLAG_PLANAR)
        video_out->draw_slice(mpi->planes,mpi->stride,sh_video->disp_w,sh_video->disp_h,0,0);
    else
        video_out->draw_frame(mpi->planes);
}

    t2=GetTimer()-t2;
    tt=t2*0.000001f;
    vout_time_usage+=tt;
    if(benchmark)
    {
	if(tt > max_vout_time_usage) max_vout_time_usage = tt;
	cur_vout_time_usage=tt;
    }
    blit_frame=1;

  return blit_frame;
}


