
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
#include "vf.h"

#include "dec_video.h"

// ===================================================================

extern double video_time_usage;
extern double vout_time_usage;

#include "postproc/postprocess.h"

#include "cpudetect.h"

int divx_quality=0;

vd_functions_t* mpvdec=NULL;

int get_video_quality_max(sh_video_t *sh_video){
  vf_instance_t* vf=sh_video->vfilter;
  if(vf){
    int ret=vf->control(vf,VFCTRL_QUERY_MAX_PP_LEVEL,NULL);
    if(ret>0){
      mp_msg(MSGT_DECVIDEO,MSGL_INFO,MSGTR_UsingExternalPP,ret);
      return ret;
    }
  }
  if(mpvdec){
    int ret=mpvdec->control(sh_video,VDCTRL_QUERY_MAX_PP_LEVEL,NULL);
    if(ret>0){
      mp_msg(MSGT_DECVIDEO,MSGL_INFO,MSGTR_UsingCodecPP,ret);
      return ret;
    }
  }
//  mp_msg(MSGT_DECVIDEO,MSGL_INFO,"[PP] Sorry, postprocessing is not available\n");
  return 0;
}

void set_video_quality(sh_video_t *sh_video,int quality){
  vf_instance_t* vf=sh_video->vfilter;
  if(vf){
    int ret=vf->control(vf,VFCTRL_SET_PP_LEVEL, (void*)(&quality));
    if(ret==CONTROL_TRUE) return; // success
  }
  if(mpvdec)
    mpvdec->control(sh_video,VDCTRL_SET_PP_LEVEL, (void*)(&quality));
}

int set_video_colors(sh_video_t *sh_video,char *item,int value)
{
    vf_instance_t* vf=sh_video->vfilter;
    vf_equalizer_t data;

    data.item = item;
    data.value = value;

    mp_dbg(MSGT_DECVIDEO,MSGL_V,"set video colors %s=%d \n", item, value);
    if (vf)
    {
	int ret = vf->control(vf, VFCTRL_SET_EQUALIZER, &data);
	if (ret == CONTROL_TRUE)
	    return(1);
    }
    /* try software control */
    if(mpvdec)
	if( mpvdec->control(sh_video,VDCTRL_SET_EQUALIZER, item, (int *)value)
	    == CONTROL_OK) return 1;
    mp_msg(MSGT_DECVIDEO,MSGL_INFO,MSGTR_VideoAttributeNotSupportedByVO_VD,item);
    return 0;
}

int get_video_colors(sh_video_t *sh_video,char *item,int *value)
{
    vf_instance_t* vf=sh_video->vfilter;
    vf_equalizer_t data;

    data.item = item;

    mp_dbg(MSGT_DECVIDEO,MSGL_V,"get video colors %s \n", item);
    if (vf)
    {
        int ret = vf->control(vf, VFCTRL_GET_EQUALIZER, &data);
	if (ret == CONTROL_TRUE){
	    *value = data.value;
	    return(1);
	}
    }
    /* try software control */
    if(mpvdec) return mpvdec->control(sh_video,VDCTRL_GET_EQUALIZER, item, value);
    return 0;
}

int set_rectangle(sh_video_t *sh_video,int param,int value)
{
    vf_instance_t* vf=sh_video->vfilter;
    int data[] = {param, value};

    mp_dbg(MSGT_DECVIDEO,MSGL_V,"set rectangle \n");
    if (vf)
    {
        int ret = vf->control(vf, VFCTRL_CHANGE_RECTANGLE, data);
	if (ret)
	    return(1);
    }
    return 0;
}

void uninit_video(sh_video_t *sh_video){
    if(!sh_video->inited) return;
    mp_msg(MSGT_DECVIDEO,MSGL_V,MSGTR_UninitVideoStr,sh_video->codec->drv);
    mpvdec->uninit(sh_video);
    vf_uninit_filter_chain(sh_video->vfilter);
    sh_video->inited=0;
}

int init_video(sh_video_t *sh_video,char* codecname,char* vfm,int status){
    unsigned int orig_fourcc=sh_video->bih?sh_video->bih->biCompression:0;
    sh_video->codec=NULL;
    sh_video->vf_inited=0;

    while(1){
	int i;
	// restore original fourcc:
	if(sh_video->bih) sh_video->bih->biCompression=orig_fourcc;
	if(!(sh_video->codec=find_codec(sh_video->format,
          sh_video->bih?((unsigned int*) &sh_video->bih->biCompression):NULL,
          sh_video->codec,0) )) break;
	// ok we found one codec
	if(sh_video->codec->flags&CODECS_FLAG_SELECTED) continue; // already tried & failed
	if(codecname && strcmp(sh_video->codec->name,codecname)) continue; // -vc
	if(vfm && strcmp(sh_video->codec->drv,vfm)) continue; // vfm doesn't match
	if(sh_video->codec->status<status) continue; // too unstable
	sh_video->codec->flags|=CODECS_FLAG_SELECTED; // tagging it
	// ok, it matches all rules, let's find the driver!
	for (i=0; mpcodecs_vd_drivers[i] != NULL; i++)
//	    if(mpcodecs_vd_drivers[i]->info->id==sh_video->codec->driver) break;
	    if(!strcmp(mpcodecs_vd_drivers[i]->info->short_name,sh_video->codec->drv)) break;
	mpvdec=mpcodecs_vd_drivers[i];
	if(!mpvdec){ // driver not available (==compiled in)
	    mp_msg(MSGT_DECVIDEO,MSGL_WARN,MSGTR_VideoCodecFamilyNotAvailableStr,
		sh_video->codec->name, sh_video->codec->drv);
	    continue;
	}
	// it's available, let's try to init!
	if(sh_video->codec->flags & CODECS_FLAG_ALIGN16){
	    // align width/height to n*16
	    // FIXME: save orig w/h, and restore if codec init failed!
	    if(sh_video->bih){
		sh_video->disp_w=sh_video->bih->biWidth=(sh_video->bih->biWidth+15)&(~15);
		sh_video->disp_h=sh_video->bih->biHeight=(sh_video->bih->biHeight+15)&(~15);
	    } else {
		sh_video->disp_w=(sh_video->disp_w+15)&(~15);
		sh_video->disp_h=(sh_video->disp_h+15)&(~15);
	    }
	}
	// init()
	mp_msg(MSGT_DECVIDEO,MSGL_INFO,MSGTR_OpeningVideoDecoder,mpvdec->info->short_name,mpvdec->info->name);
	if(!mpvdec->init(sh_video)){
	    mp_msg(MSGT_DECVIDEO,MSGL_INFO,MSGTR_VDecoderInitFailed);
	    continue; // try next...
	}
	// Yeah! We got it!
	sh_video->inited=1;
	return 1;
    }
    return 0;
}

extern int vo_directrendering;

int decode_video(sh_video_t *sh_video,unsigned char *start,int in_size,int drop_frame){
vf_instance_t* vf=sh_video->vfilter;
mp_image_t *mpi=NULL;
unsigned int t=GetTimer();
unsigned int t2;
double tt;

//if(!(sh_video->ds->flags&1) || sh_video->ds->pack_no<5)
mpi=mpvdec->decode(sh_video, start, in_size, drop_frame);

//------------------------ frame decoded. --------------------

#ifdef ARCH_X86
	// some codecs are broken, and doesn't restore MMX state :(
	// it happens usually with broken/damaged files.
if(gCpuCaps.has3DNow){
	__asm __volatile ("femms\n\t":::"memory");
}
else if(gCpuCaps.hasMMX){
	__asm __volatile ("emms\n\t":::"memory");
}
#endif

t2=GetTimer();t=t2-t;
tt = t*0.000001f;
video_time_usage+=tt;

if(!mpi || drop_frame) return 0; // error / skipped frame

//vo_draw_image(video_out,mpi);
vf->put_image(vf,mpi);
vf->control(vf,VFCTRL_DRAW_OSD,NULL);

    t2=GetTimer()-t2;
    tt=t2*0.000001f;
    vout_time_usage+=tt;

  return 1;
}
