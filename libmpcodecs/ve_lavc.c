#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"

#ifdef USE_LIBAVCODEC

#include "../mp_msg.h"
#include "../help_mp.h"

#include "codec-cfg.h"
#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "aviwrite.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#ifdef HAVE_DIVX4ENCORE
#include "divx4_vbr.h"
extern char* passtmpfile;
#endif

extern int pass;

//===========================================================================//

#ifdef USE_LIBAVCODEC_SO
#include <ffmpeg/avcodec.h>
#else
#include "libavcodec/avcodec.h"
#endif

#if LIBAVCODEC_BUILD < 4601
#error your version of libavcodec is too old, get a newer one, and dont send a bugreport, THIS IS NO BUG
#endif

#if LIBAVCODEC_BUILD < 4620
#warning your version of libavcodec is old, u might want to get a newer one
#endif

extern int avcodec_inited;

/* video options */
static char *lavc_param_vcodec = NULL;
static int lavc_param_vbitrate = -1;
static int lavc_param_vrate_tolerance = 1000*8;
static int lavc_param_vhq = 0; /* default is realtime encoding */
static int lavc_param_v4mv = 0;
static int lavc_param_vme = 4;
static int lavc_param_vqscale = 0;
static int lavc_param_vqmin = 2;
static int lavc_param_vqmax = 31;
static int lavc_param_vqdiff = 3;
static float lavc_param_vqcompress = 0.5;
static float lavc_param_vqblur = 0.5;
static float lavc_param_vb_qfactor = 1.25;
static float lavc_param_vb_qoffset = 1.25;
static float lavc_param_vi_qfactor = 0.8;
static float lavc_param_vi_qoffset = 0.0;
static int lavc_param_vmax_b_frames = 0;
static int lavc_param_keyint = -1;
static int lavc_param_vpass = 0;
static int lavc_param_vrc_strategy = 2;
static int lavc_param_vb_strategy = 0;
static int lavc_param_luma_elim_threshold = 0;
static int lavc_param_chroma_elim_threshold = 0;
static int lavc_param_packet_size= 0;
static int lavc_param_strict= 0;
static int lavc_param_data_partitioning= 0;
static int lavc_param_gray=0;
static float lavc_param_rc_qsquish=1.0;
static float lavc_param_rc_qmod_amp=0;
static int lavc_param_rc_qmod_freq=0;
static char *lavc_param_rc_override_string=NULL;
static char *lavc_param_rc_eq="tex^qComp";
static int lavc_param_rc_buffer_size=0;
static float lavc_param_rc_buffer_aggressivity=1.0;
static int lavc_param_rc_max_rate=0;
static int lavc_param_rc_min_rate=0;
static float lavc_param_rc_initial_cplx=0;
static int lavc_param_mpeg_quant=0;

#include "cfgparser.h"

#ifdef USE_LIBAVCODEC
struct config lavcopts_conf[]={
	{"vcodec", &lavc_param_vcodec, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"vbitrate", &lavc_param_vbitrate, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
	{"vratetol", &lavc_param_vrate_tolerance, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
	{"vhq", &lavc_param_vhq, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"v4mv", &lavc_param_v4mv, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"vme", &lavc_param_vme, CONF_TYPE_INT, CONF_RANGE, 0, 5, NULL},
	{"vqscale", &lavc_param_vqscale, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
	{"vqmin", &lavc_param_vqmin, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
	{"vqmax", &lavc_param_vqmax, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
	{"vqdiff", &lavc_param_vqdiff, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
	{"vqcomp", &lavc_param_vqcompress, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 1.0, NULL},
	{"vqblur", &lavc_param_vqblur, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 1.0, NULL},
	{"vb_qfactor", &lavc_param_vb_qfactor, CONF_TYPE_FLOAT, CONF_RANGE, -31.0, 31.0, NULL},
	{"vmax_b_frames", &lavc_param_vmax_b_frames, CONF_TYPE_INT, CONF_RANGE, 0, FF_MAX_B_FRAMES, NULL},
	{"vpass", &lavc_param_vpass, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
#if LIBAVCODEC_BUILD < 4620
	{"vrc_strategy", &lavc_param_vrc_strategy, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
#endif
	{"vb_strategy", &lavc_param_vb_strategy, CONF_TYPE_INT, CONF_RANGE, 0, 1, NULL},
#ifdef CODEC_FLAG_PART
	{"vb_qoffset", &lavc_param_vb_qoffset, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 31.0, NULL},
	{"vlelim", &lavc_param_luma_elim_threshold, CONF_TYPE_INT, CONF_RANGE, -99, 99, NULL},
	{"vcelim", &lavc_param_chroma_elim_threshold, CONF_TYPE_INT, CONF_RANGE, -99, 99, NULL},
	{"vpsize", &lavc_param_packet_size, CONF_TYPE_INT, CONF_RANGE, 0, 100000000, NULL},
	{"vstrict", &lavc_param_strict, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"vdpart", &lavc_param_data_partitioning, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_PART, NULL},
#endif
	{"keyint", &lavc_param_keyint, CONF_TYPE_INT, 0, 0, 0, NULL},
#if LIBAVCODEC_BUILD >= 4614
	{"gray", &lavc_param_gray, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_PART, NULL},
#endif
#if LIBAVCODEC_BUILD >= 4619
	{"mpeg_quant", &lavc_param_mpeg_quant, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#endif
#if LIBAVCODEC_BUILD >= 4620
	{"vi_qfactor", &lavc_param_vi_qfactor, CONF_TYPE_FLOAT, CONF_RANGE, -31.0, 31.0, NULL},
	{"vi_qoffset", &lavc_param_vi_qoffset, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 31.0, NULL},
	{"vqsquish", &lavc_param_rc_qsquish, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 99.0, NULL},
	{"vqmod_amp", &lavc_param_rc_qmod_amp, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 99.0, NULL},
	{"vqmod_freq", &lavc_param_rc_qmod_freq, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"vrc_eq", &lavc_param_rc_eq, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"vrc_override", &lavc_param_rc_override_string, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"vrc_maxrate", &lavc_param_rc_max_rate, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
	{"vrc_minrate", &lavc_param_rc_min_rate, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
	{"vrc_buf_size", &lavc_param_rc_min_rate, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
	{"vrc_buf_aggressivity", &lavc_param_rc_buffer_aggressivity, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 99.0, NULL},
	{"vrc_init_cplx", &lavc_param_rc_initial_cplx, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 9999999.0, NULL},
#endif
	{NULL, NULL, 0, 0, 0, 0, NULL}
};
#endif

struct vf_priv_s {
    aviwrite_stream_t* mux;
    AVCodecContext context;
    AVCodec *codec;
    FILE *stats_file;
};

#define stats_file (vf->priv->stats_file)
#define mux_v (vf->priv->mux)
#define lavc_venc_context (vf->priv->context)

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    int size, i;
    void *p;
        
    mux_v->bih->biWidth=width;
    mux_v->bih->biHeight=height;
    mux_v->bih->biSizeImage=mux_v->bih->biWidth*mux_v->bih->biHeight*(mux_v->bih->biBitCount/8);

    memset(&lavc_venc_context, 0, sizeof(lavc_venc_context));

    printf("videocodec: libavcodec (%dx%d fourcc=%x [%.4s])\n",
	mux_v->bih->biWidth, mux_v->bih->biHeight, mux_v->bih->biCompression,
	    (char *)&mux_v->bih->biCompression);

    lavc_venc_context.width = width;
    lavc_venc_context.height = height;
    if (lavc_param_vbitrate > 16000) /* != -1 */
	lavc_venc_context.bit_rate = lavc_param_vbitrate;
    else if (lavc_param_vbitrate >= 0) /* != -1 */
	lavc_venc_context.bit_rate = lavc_param_vbitrate*1000;
    else
	lavc_venc_context.bit_rate = 800000; /* default */
    lavc_venc_context.bit_rate_tolerance= lavc_param_vrate_tolerance*1000;
    lavc_venc_context.frame_rate = (float)mux_v->h.dwRate/mux_v->h.dwScale * FRAME_RATE_BASE;
    lavc_venc_context.qmin= lavc_param_vqmin;
    lavc_venc_context.qmax= lavc_param_vqmax;
    lavc_venc_context.max_qdiff= lavc_param_vqdiff;
    lavc_venc_context.qcompress= lavc_param_vqcompress;
    lavc_venc_context.qblur= lavc_param_vqblur;
    lavc_venc_context.max_b_frames= lavc_param_vmax_b_frames;
    lavc_venc_context.b_quant_factor= lavc_param_vb_qfactor;
    lavc_venc_context.rc_strategy= lavc_param_vrc_strategy;
    lavc_venc_context.b_frame_strategy= lavc_param_vb_strategy;

#ifdef CODEC_FLAG_PART
    lavc_venc_context.b_quant_offset= lavc_param_vb_qoffset;
    lavc_venc_context.luma_elim_threshold= lavc_param_luma_elim_threshold;
    lavc_venc_context.chroma_elim_threshold= lavc_param_chroma_elim_threshold;
    lavc_venc_context.rtp_payload_size= lavc_param_packet_size;
    if(lavc_param_packet_size )lavc_venc_context.rtp_mode=1;
    lavc_venc_context.strict_std_compliance= lavc_param_strict;
#endif
#if LIBAVCODEC_BUILD >= 4620
    lavc_venc_context.i_quant_factor= lavc_param_vi_qfactor;
    lavc_venc_context.i_quant_offset= lavc_param_vi_qoffset;
    lavc_venc_context.rc_qsquish= lavc_param_rc_qsquish;
    lavc_venc_context.rc_qmod_amp= lavc_param_rc_qmod_amp;
    lavc_venc_context.rc_qmod_freq= lavc_param_rc_qmod_freq;
    lavc_venc_context.rc_eq= lavc_param_rc_eq;
    lavc_venc_context.rc_max_rate= lavc_param_rc_max_rate*1000;
    lavc_venc_context.rc_min_rate= lavc_param_rc_min_rate*1000;
    lavc_venc_context.rc_buffer_size= lavc_param_rc_buffer_size*1000;
    lavc_venc_context.rc_buffer_aggressivity= lavc_param_rc_buffer_aggressivity;
    lavc_venc_context.rc_initial_cplx= lavc_param_rc_initial_cplx;
    p= lavc_param_rc_override_string;
    for(i=0; p; i++){
        int start, end, q;
        int e=sscanf(p, "%d,%d,%d", &start, &end, &q);
        if(e!=3){
	    mp_msg(MSGT_MENCODER,MSGL_ERR,"error parsing vrc_q\n");
            return 0;
        }
        lavc_venc_context.rc_override= 
            realloc(lavc_venc_context.rc_override, sizeof(RcOverride)*(i+1));
        lavc_venc_context.rc_override[i].start_frame= start;
        lavc_venc_context.rc_override[i].end_frame  = end;
        if(q>0){
            lavc_venc_context.rc_override[i].qscale= q;
            lavc_venc_context.rc_override[i].quality_factor= 1.0;
        }
        else{
            lavc_venc_context.rc_override[i].qscale= 0;
            lavc_venc_context.rc_override[i].quality_factor= -q/100.0;
        }
        p= strchr(p, '/');
        if(p) p++;
    }
    lavc_venc_context.rc_override_count=i;
#endif

#if LIBAVCODEC_BUILD >= 4619
    lavc_venc_context.mpeg_quant=lavc_param_mpeg_quant;
#endif
    
    /* keyframe interval */
    if (lavc_param_keyint >= 0) /* != -1 */
	lavc_venc_context.gop_size = lavc_param_keyint;
    else
	lavc_venc_context.gop_size = 250; /* default */

    if (lavc_param_vhq)
    {
	printf("High quality encoding selected (non real time)!\n");
	lavc_venc_context.flags = CODEC_FLAG_HQ;
    }
    else
	lavc_venc_context.flags = 0;

     /* 4mv is currently buggy with B frames */
    if (lavc_param_vmax_b_frames > 0 && lavc_param_v4mv) {
        printf("4MV with B-Frames not yet supported -> 4MV disabled\n");
        lavc_param_v4mv = 0;
    }

    lavc_venc_context.flags|= lavc_param_v4mv ? CODEC_FLAG_4MV : 0;
#ifdef CODEC_FLAG_PART
    lavc_venc_context.flags|= lavc_param_data_partitioning;
#endif
#if LIBAVCODEC_BUILD >= 4614
    if(lavc_param_gray) lavc_venc_context.flags|= CODEC_FLAG_GRAY;
#endif


    /* lavc internal 2pass bitrate control */
#ifdef HAVE_DIVX4ENCORE
    switch(lavc_param_vpass){
#else
    switch(lavc_param_vpass?lavc_param_vpass:pass){
#endif
    case 1: 
	lavc_venc_context.flags|= CODEC_FLAG_PASS1; 
	stats_file= fopen(passtmpfile, "w");
	if(stats_file==NULL){
	    mp_msg(MSGT_MENCODER,MSGL_ERR,"2pass failed: filename=%s\n", passtmpfile);
            return 0;
	}
	break;
    case 2:
	lavc_venc_context.flags|= CODEC_FLAG_PASS2; 
	stats_file= fopen(passtmpfile, "r");
	if(stats_file==NULL){
	    mp_msg(MSGT_MENCODER,MSGL_ERR,"2pass failed: filename=%s\n", passtmpfile);
            return 0;
	}
	fseek(stats_file, 0, SEEK_END);
	size= ftell(stats_file);
	fseek(stats_file, 0, SEEK_SET);
	
	lavc_venc_context.stats_in= malloc(size + 1);
	lavc_venc_context.stats_in[size]=0;

	if(fread(lavc_venc_context.stats_in, size, 1, stats_file)<1){
	    mp_msg(MSGT_MENCODER,MSGL_ERR,"2pass failed: reading from filename=%s\n", passtmpfile);
            return 0;
	}
        
	break;
    }

#ifdef ME_ZERO
    // workaround Juanjo's stupid incompatible change:
    motion_estimation_method = lavc_param_vme;
#else
    lavc_venc_context.me_method = ME_ZERO+lavc_param_vme;
#endif

    /* fixed qscale :p */
    if (lavc_param_vqscale)
    {
	printf("Using constant qscale = %d (VBR)\n", lavc_param_vqscale);
	lavc_venc_context.flags |= CODEC_FLAG_QSCALE;
	lavc_venc_context.quality = lavc_param_vqscale;
    }

#ifdef HAVE_DIVX4ENCORE
    switch(pass){
    case 1:
	if (VbrControl_init_2pass_vbr_analysis(passtmpfile, 5) == -1){
	    mp_msg(MSGT_MENCODER,MSGL_ERR,"2pass failed: filename=%s\n", passtmpfile);
	    pass=0;
	}
	break;
    case 2:
        if (VbrControl_init_2pass_vbr_encoding(passtmpfile,
		    lavc_venc_context.bit_rate,
		    (float)mux_v->h.dwRate/mux_v->h.dwScale,
		    100, /* crispness */
		    5) == -1){
	    mp_msg(MSGT_MENCODER,MSGL_ERR,"2pass failed: filename=%s\n", passtmpfile);
	    pass=0;
	} else
	    lavc_venc_context.flags|=CODEC_FLAG_QSCALE|CODEC_FLAG_TYPE; // VBR
	break;
    }
#endif

    if (avcodec_open(&lavc_venc_context, vf->priv->codec) != 0) {
	mp_msg(MSGT_MENCODER,MSGL_ERR,MSGTR_CantOpenCodec);
	return 0;
    }

    if (lavc_venc_context.codec->encode == NULL) {
	mp_msg(MSGT_MENCODER,MSGL_ERR,"avcodec init failed (ctx->codec->encode == NULL)!\n");
	return 0;
    }
    
#if LIBAVCODEC_BUILD >= 4620
    /* free second pass buffer, its not needed anymore */
    if(lavc_venc_context.stats_in) free(lavc_venc_context.stats_in);
    lavc_venc_context.stats_in= NULL;
#endif
    
    return 1;
}

static int control(struct vf_instance_s* vf, int request, void* data){

    return CONTROL_UNKNOWN;
}

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
    switch(fmt){
    case IMGFMT_YV12:
    case IMGFMT_IYUV:
    case IMGFMT_I420:
	return VFCAP_CSP_SUPPORTED | VFCAP_ACCEPT_STRIDE;
    }
    return 0;
}

static void put_image(struct vf_instance_s* vf, mp_image_t *mpi){
    int out_size;
    AVPicture lavc_venc_picture;

    lavc_venc_picture.data[0]=mpi->planes[0];
    lavc_venc_picture.data[1]=mpi->planes[1];
    lavc_venc_picture.data[2]=mpi->planes[2];
    lavc_venc_picture.linesize[0]=mpi->stride[0];
    lavc_venc_picture.linesize[1]=mpi->stride[1];
    lavc_venc_picture.linesize[2]=mpi->stride[2];

#ifdef HAVE_DIVX4ENCORE
    if(pass==2){ // handle 2-pass:
	lavc_venc_context.flags|=CODEC_FLAG_QSCALE; // enable VBR
	lavc_venc_context.quality=VbrControl_get_quant();
	lavc_venc_context.key_frame=VbrControl_get_intra();
	lavc_venc_context.gop_size=0x3fffffff;
	out_size = avcodec_encode_video(&lavc_venc_context, mux_v->buffer, mux_v->buffer_size,
	    &lavc_venc_picture);
	VbrControl_update_2pass_vbr_encoding(lavc_venc_context.mv_bits,
	      lavc_venc_context.i_tex_bits+lavc_venc_context.p_tex_bits,
	      8*out_size);
    } else
#endif
    {
	out_size = avcodec_encode_video(&lavc_venc_context, mux_v->buffer, mux_v->buffer_size,
	    &lavc_venc_picture);
#ifdef HAVE_DIVX4ENCORE
	if(pass==1){
	  VbrControl_update_2pass_vbr_analysis(lavc_venc_context.key_frame,
	      lavc_venc_context.mv_bits,
	      lavc_venc_context.i_tex_bits+lavc_venc_context.p_tex_bits,
	      8*out_size, lavc_venc_context.quality);
	}
#endif
    }

    mencoder_write_chunk(mux_v,out_size,lavc_venc_context.key_frame?0x10:0);
    
#if LIBAVCODEC_BUILD >= 4620
    /* store stats if there are any */
    if(lavc_venc_context.stats_out && stats_file) 
        fprintf(stats_file, "%s", lavc_venc_context.stats_out);
#endif
}

static void uninit(struct vf_instance_s* vf){
    avcodec_close(&lavc_venc_context);

    if(stats_file) fclose(stats_file);

#if LIBAVCODEC_BUILD >= 4620
    /* free rc_override */
    if(lavc_venc_context.rc_override) free(lavc_venc_context.rc_override);
    lavc_venc_context.rc_override= NULL;
#endif
}

//===========================================================================//

static int vf_open(vf_instance_t *vf, char* args){
    vf->uninit=uninit;
    vf->config=config;
    vf->control=control;
    vf->query_format=query_format;
    vf->put_image=put_image;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    memset(vf->priv,0,sizeof(struct vf_priv_s));
    vf->priv->mux=args;

    /* XXX: hack: some of the MJPEG decoder DLL's needs exported huffman
       table, so we define a zero-table, also lavc mjpeg encoder is putting
       huffman tables into the stream, so no problem */
    if (lavc_param_vcodec && !strcasecmp(lavc_param_vcodec, "mjpeg"))
    {
	mux_v->bih=malloc(sizeof(BITMAPINFOHEADER)+28);
	memset(mux_v->bih, 0, sizeof(BITMAPINFOHEADER)+28);
	mux_v->bih->biSize=sizeof(BITMAPINFOHEADER)+28;
    }
    else
    {
	mux_v->bih=malloc(sizeof(BITMAPINFOHEADER));
	memset(mux_v->bih, 0, sizeof(BITMAPINFOHEADER));
	mux_v->bih->biSize=sizeof(BITMAPINFOHEADER);
    }
    mux_v->bih->biWidth=0;
    mux_v->bih->biHeight=0;
    mux_v->bih->biPlanes=1;
    mux_v->bih->biBitCount=24;
    if (!lavc_param_vcodec)
    {
	printf("No libavcodec codec specified! It's required!\n");
	return 0;
    }

    if (!strcasecmp(lavc_param_vcodec, "mpeg1") || !strcasecmp(lavc_param_vcodec, "mpeg1video"))
	mux_v->bih->biCompression = mmioFOURCC('m', 'p', 'g', '1');
    else if (!strcasecmp(lavc_param_vcodec, "h263") || !strcasecmp(lavc_param_vcodec, "h263p"))
	mux_v->bih->biCompression = mmioFOURCC('h', '2', '6', '3');
    else if (!strcasecmp(lavc_param_vcodec, "rv10"))
	mux_v->bih->biCompression = mmioFOURCC('R', 'V', '1', '0');
    else if (!strcasecmp(lavc_param_vcodec, "mjpeg"))
	mux_v->bih->biCompression = mmioFOURCC('M', 'J', 'P', 'G');
    else if (!strcasecmp(lavc_param_vcodec, "mpeg4"))
	mux_v->bih->biCompression = mmioFOURCC('D', 'I', 'V', 'X');
    else if (!strcasecmp(lavc_param_vcodec, "msmpeg4"))
	mux_v->bih->biCompression = mmioFOURCC('d', 'i', 'v', '3');
    else if (!strcasecmp(lavc_param_vcodec, "msmpeg4v2"))
	mux_v->bih->biCompression = mmioFOURCC('M', 'P', '4', '2');
    else if (!strcasecmp(lavc_param_vcodec, "wmv1"))
	mux_v->bih->biCompression = mmioFOURCC('W', 'M', 'V', '1');
    else
	mux_v->bih->biCompression = mmioFOURCC(lavc_param_vcodec[0],
		lavc_param_vcodec[1], lavc_param_vcodec[2], lavc_param_vcodec[3]); /* FIXME!!! */

    if (!avcodec_inited){
	avcodec_init();
	avcodec_register_all();
	avcodec_inited=1;
    }

    vf->priv->codec = (AVCodec *)avcodec_find_encoder_by_name(lavc_param_vcodec);
    if (!vf->priv->codec) {
	mp_msg(MSGT_MENCODER,MSGL_ERR,MSGTR_MissingLAVCcodec, lavc_param_vcodec);
	return 0;
    }

    return 1;
}

vf_info_t ve_info_lavc = {
    "libavcodec encoder",
    "lavc",
    "A'rpi, Alex, Michael",
    "for internal use by mencoder",
    vf_open
};

//===========================================================================//
#endif
