#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "codec-cfg.h"
//#include "mp_image.h"

#include "img_format.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "vd.h"
#include "vf.h"

//#include "vd_internal.h"

extern vd_functions_t mpcodecs_vd_null;
extern vd_functions_t mpcodecs_vd_cinepak;
extern vd_functions_t mpcodecs_vd_qtrpza;
extern vd_functions_t mpcodecs_vd_ffmpeg;
extern vd_functions_t mpcodecs_vd_dshow;
extern vd_functions_t mpcodecs_vd_vfw;
extern vd_functions_t mpcodecs_vd_vfwex;
extern vd_functions_t mpcodecs_vd_odivx;
extern vd_functions_t mpcodecs_vd_divx4;
extern vd_functions_t mpcodecs_vd_raw;
extern vd_functions_t mpcodecs_vd_xanim;
extern vd_functions_t mpcodecs_vd_msrle;
extern vd_functions_t mpcodecs_vd_msvidc;
extern vd_functions_t mpcodecs_vd_fli;
extern vd_functions_t mpcodecs_vd_qtrle;
extern vd_functions_t mpcodecs_vd_qtsmc;
extern vd_functions_t mpcodecs_vd_roqvideo;
extern vd_functions_t mpcodecs_vd_cyuv;
extern vd_functions_t mpcodecs_vd_nuv;
extern vd_functions_t mpcodecs_vd_mpng;
extern vd_functions_t mpcodecs_vd_ijpg;
extern vd_functions_t mpcodecs_vd_libmpeg2;
extern vd_functions_t mpcodecs_vd_huffyuv;
extern vd_functions_t mpcodecs_vd_zlib;
extern vd_functions_t mpcodecs_vd_mpegpes;

vd_functions_t* mpcodecs_vd_drivers[] = {
        &mpcodecs_vd_null,
        &mpcodecs_vd_cinepak,
        &mpcodecs_vd_qtrpza,
#ifdef USE_LIBAVCODEC
        &mpcodecs_vd_ffmpeg,
#endif
#ifdef USE_WIN32DLL
#ifdef USE_DIRECTSHOW
        &mpcodecs_vd_dshow,
#endif
        &mpcodecs_vd_vfw,
        &mpcodecs_vd_vfwex,
#endif
#ifdef USE_DIVX
        &mpcodecs_vd_odivx,
#ifdef NEW_DECORE
        &mpcodecs_vd_divx4,
#endif
#endif
        &mpcodecs_vd_raw,
        &mpcodecs_vd_msrle,
        &mpcodecs_vd_msvidc,
        &mpcodecs_vd_fli,
        &mpcodecs_vd_qtrle,
        &mpcodecs_vd_qtsmc,
        &mpcodecs_vd_roqvideo,
        &mpcodecs_vd_cyuv,
        &mpcodecs_vd_nuv,
#ifdef USE_XANIM
        &mpcodecs_vd_xanim,
#endif
#ifdef HAVE_PNG
        &mpcodecs_vd_mpng,
#endif
#ifdef HAVE_JPEG
	&mpcodecs_vd_ijpg,
#endif
        &mpcodecs_vd_libmpeg2,
        &mpcodecs_vd_huffyuv,
#ifdef HAVE_ZLIB
        &mpcodecs_vd_zlib,
#endif
        &mpcodecs_vd_mpegpes,
	NULL
};

#include "libvo/video_out.h"

// libvo opts:
int fullscreen=0;
int vidmode=0;
int softzoom=0;
int flip=-1;
int opt_screen_size_x=0;
int opt_screen_size_y=0;
int screen_size_xy=0;
float movie_aspect=-1.0;
int vo_flags=0;
int vd_use_slices=1;

extern vd_functions_t* mpvdec; // FIXME!
extern int divx_quality;

int mpcodecs_config_vo(sh_video_t *sh, int w, int h, unsigned int preferred_outfmt){
    int i,j;
    unsigned int out_fmt=0;
    int screen_size_x=0;//SCREEN_SIZE_X;
    int screen_size_y=0;//SCREEN_SIZE_Y;
//    vo_functions_t* video_out=sh->video_out;
    vf_instance_t* vf=sh->vfilter;

#if 1
    if(!(sh->disp_w && sh->disp_h))
        mp_msg(MSGT_DECVIDEO,MSGL_WARN,
            "VDec: codec didn't set sh->disp_w and sh->disp_h, trying to workaround!\n");
    /* XXX: HACK, if sh->disp_* aren't set,
     * but we have w and h, set them :: atmos */
    if(!sh->disp_w && w)
        sh->disp_w=w;
    if(!sh->disp_h && h)
        sh->disp_h=h;
#endif

    mp_msg(MSGT_DECVIDEO,MSGL_INFO,"VDec: vo config request - %d x %d, %s  \n",
	w,h,vo_format_name(preferred_outfmt));

    if(!vf) return 1; // temp hack
    
    if(get_video_quality_max(sh)<=0 && divx_quality){
	// user wants postprocess but no pp filter yet:
	sh->vfilter=vf=vf_open_filter(vf,"pp",NULL);
    }

    // check if libvo and codec has common outfmt (no conversion):
csp_again:
    j=-1;
    for(i=0;i<CODECS_MAX_OUTFMT;i++){
	int flags;
	out_fmt=sh->codec->outfmt[i];
	if(out_fmt==(signed int)0xFFFFFFFF) continue;
	flags=vf->query_format(vf,out_fmt);
	mp_msg(MSGT_CPLAYER,MSGL_V,"vo_debug: query(%s) returned 0x%X (i=%d) \n",vo_format_name(out_fmt),flags,i);
	if((flags&2) || (flags && j<0)){
	    // check (query) if codec really support this outfmt...
	    if(mpvdec->control(sh,VDCTRL_QUERY_FORMAT,&out_fmt)==CONTROL_FALSE)
		continue;
	    j=i; vo_flags=flags; if(flags&2) break;
	}
    }
    if(j<0){
	// TODO: no match - we should use conversion...
	if(strcmp(vf->info->name,"scale")){	
	    mp_msg(MSGT_DECVIDEO,MSGL_INFO,"Couldn't find matching colorspace - retrying with -vop scale...\n");
	    vf=vf_open_filter(vf,"scale",NULL);
	    goto csp_again;
	}
	mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_VOincompCodec);
	return 0;	// failed
    }
    out_fmt=sh->codec->outfmt[j];
    sh->outfmtidx=j;
    sh->vfilter=vf;

    // autodetect flipping
    if(flip==-1){
	flip=0;
	if(sh->codec->outflags[j]&CODECS_FLAG_FLIP)
	    if(!(sh->codec->outflags[j]&CODECS_FLAG_NOFLIP))
		flip=1;
    }
    if(vo_flags&VFCAP_FLIPPED) flip^=1;
    if(flip && !(vo_flags&VFCAP_FLIP)){
	// we need to flip, but no flipping filter avail.
	sh->vfilter=vf=vf_open_filter(vf,"flip",NULL);
    }

    // time to do aspect ratio corrections...

  if(movie_aspect>-1.0) sh->aspect = movie_aspect; // cmdline overrides autodetect
//  if(!sh->aspect) sh->aspect=1.0;

  if(opt_screen_size_x||opt_screen_size_y){
    screen_size_x = opt_screen_size_x;
    screen_size_y = opt_screen_size_y;
    if(!vidmode){
     if(!screen_size_x) screen_size_x=SCREEN_SIZE_X;
     if(!screen_size_y) screen_size_y=SCREEN_SIZE_Y;
     if(screen_size_x<=8) screen_size_x*=sh->disp_w;
     if(screen_size_y<=8) screen_size_y*=sh->disp_h;
    }
  } else {
    // check source format aspect, calculate prescale ::atmos
    screen_size_x=sh->disp_w;
    screen_size_y=sh->disp_h;
    if(screen_size_xy>0){
     if(screen_size_xy<=8){
       // -xy means x+y scale
       screen_size_x*=screen_size_xy;
       screen_size_y*=screen_size_xy;
     } else {
       // -xy means forced width while keeping correct aspect
       screen_size_x=screen_size_xy;
       screen_size_y=screen_size_xy*sh->disp_h/sh->disp_w;
     }
    }
    if(sh->aspect>0.01){
      int w;
      mp_msg(MSGT_CPLAYER,MSGL_INFO,"Movie-Aspect is %.2f:1 - prescaling to correct movie aspect.\n",
             sh->aspect);
      w=(int)((float)screen_size_y*sh->aspect); w+=w%2; // round
      // we don't like horizontal downscale || user forced width:
      if(w<screen_size_x || screen_size_xy>8){
        screen_size_y=(int)((float)screen_size_x*(1.0/sh->aspect));
        screen_size_y+=screen_size_y%2; // round
      } else screen_size_x=w; // keep new width
    } else {
      mp_msg(MSGT_CPLAYER,MSGL_INFO,"Movie-Aspect is undefined - no prescaling applied.\n");
    }
  }

#if 0
  if(video_out->get_info)
  { const vo_info_t *info = video_out->get_info();
    mp_msg(MSGT_CPLAYER,MSGL_INFO,"VO: [%s] %dx%d => %dx%d %s %s%s%s%s\n",info->short_name,
         sh->disp_w,sh->disp_h,
         screen_size_x,screen_size_y,
	 vo_format_name(out_fmt),
         fullscreen?"fs ":"",
         vidmode?"vm ":"",
         softzoom?"zoom ":"",
         (flip==1)?"flip ":"");
    mp_msg(MSGT_CPLAYER,MSGL_V,"VO: Description: %s\n",info->name);
    mp_msg(MSGT_CPLAYER,MSGL_V,"VO: Author: %s\n", info->author);
    if(info->comment && strlen(info->comment) > 0)
        mp_msg(MSGT_CPLAYER,MSGL_V,"VO: Comment: %s\n", info->comment);
  }
#endif

    // Time to config libvo!
    mp_msg(MSGT_CPLAYER,MSGL_V,"video_out->init(%dx%d->%dx%d,flags=%d,'%s',0x%X)\n",
                      sh->disp_w,sh->disp_h,
                      screen_size_x,screen_size_y,
                      fullscreen|(vidmode<<1)|(softzoom<<2)|(flip<<3),
                      "MPlayer",out_fmt);

//    memset(&vtune,0,sizeof(vo_tune_info_t));
    if(vf->config(vf,sh->disp_w,sh->disp_h,
                      screen_size_x,screen_size_y,
                      fullscreen|(vidmode<<1)|(softzoom<<2)|(flip<<3),
                      out_fmt)==0){
//                      "MPlayer",out_fmt,&vtune)){
	mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_CannotInitVO);
	return 0; // exit_player(MSGTR_Exit_error);
    }

#if 0
#define FREE_MPI(mpi) if(mpi){if(mpi->flags&MP_IMGFLAG_ALLOCATED) free(mpi->planes[0]); free(mpi); mpi=NULL;}
    FREE_MPI(static_images[0])
    FREE_MPI(static_images[1])
    FREE_MPI(temp_images[0])
    FREE_MPI(export_images[0])
#undef FREE_MPI
#endif

    return 1;
}

// mp_imgtype: buffering type, see mp_image.h
// mp_imgflag: buffer requirements (read/write, preserve, stride limits), see mp_image.h
// returns NULL or allocated mp_image_t*
// Note: buffer allocation may be moved to mpcodecs_config_vo() later...
mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h){
  return vf_get_image(sh->vfilter,sh->codec->outfmt[sh->outfmtidx],mp_imgtype,mp_imgflag,w,h);
}

