#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#ifdef USE_LIBAVCODEC

#include "bswap.h"

#include "vd_internal.h"

static vd_info_t info = {
	"FFmpeg's libavcodec codec family",
	"ffmpeg",
	"A'rpi",
	"A'rpi, Michael, Alex",
	"native codecs (http://ffmpeg.sf.net/)"
};

LIBVD_EXTERN(ffmpeg)

#include "../postproc/rgb2rgb.h"


#ifdef USE_LIBAVCODEC_SO
#include <ffmpeg/avcodec.h>
#else
#include "libavcodec/avcodec.h"
#endif

#if LIBAVCODEC_BUILD < 4641
#error we dont support libavcodec prior to build 4641, get the latest libavcodec CVS
#endif

#if LIBAVCODEC_BUILD < 4645
#warning your version of libavcodec is old, u might want to get a newer one
#endif

#if LIBAVCODEC_BUILD < 4645
#define AVFrame AVVideoFrame
#define coded_frame coded_picture
#endif

int avcodec_inited=0;

#if defined(FF_POSTPROCESS) && defined(MBR)
int quant_store[MBR+1][MBC+1];
#endif

typedef struct {
    AVCodecContext *avctx;
    AVFrame *pic;
    float last_aspect;
    int do_slices;
    int do_dr1;
    int vo_inited;
    int convert;
    int best_csp;
    int b_age;
    int ip_age[2];
    int qp_stat[32];
    double qp_sum;
    double inv_qp_sum;
} vd_ffmpeg_ctx;

//#ifdef FF_POSTPROCESS
//unsigned int lavc_pp=0;
//#endif

#include "cfgparser.h"

static int get_buffer(AVCodecContext *avctx, AVFrame *pic);
static void release_buffer(AVCodecContext *avctx, AVFrame *pic);

static int lavc_param_workaround_bugs= FF_BUG_AUTODETECT;
static int lavc_param_error_resilience=2;
static int lavc_param_error_concealment=3;
static int lavc_param_gray=0;
static int lavc_param_vstats=0;
static int lavc_param_idct_algo=0;
static int lavc_param_debug=0;

struct config lavc_decode_opts_conf[]={
	{"bug", &lavc_param_workaround_bugs, CONF_TYPE_INT, CONF_RANGE, -1, 99, NULL},
	{"er", &lavc_param_error_resilience, CONF_TYPE_INT, CONF_RANGE, 0, 99, NULL},
	{"gray", &lavc_param_gray, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_PART, NULL},
	{"idct", &lavc_param_idct_algo, CONF_TYPE_INT, CONF_RANGE, 0, 99, NULL},
	{"ec", &lavc_param_error_concealment, CONF_TYPE_INT, CONF_RANGE, 0, 99, NULL},
	{"vstats", &lavc_param_vstats, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#if LIBAVCODEC_BUILD >= 4642
	{"debug", &lavc_param_debug, CONF_TYPE_INT, CONF_RANGE, 0, 9999999, NULL},
#endif
	{NULL, NULL, 0, 0, 0, 0, NULL}
};

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    vd_ffmpeg_ctx *ctx = sh->context;
    AVCodecContext *avctx = ctx->avctx;
    switch(cmd){
    case VDCTRL_QUERY_FORMAT:
	if( (*((int*)arg)) == ctx->best_csp ) return CONTROL_TRUE;//supported
	// possible conversions:
	switch( (*((int*)arg)) ){
        case IMGFMT_YV12:
        case IMGFMT_IYUV:
        case IMGFMT_I420:
	    // "converted" using pointer/stride modification
	    if(avctx->pix_fmt==PIX_FMT_YUV420P) return CONTROL_TRUE;// u/v swap
	    if(avctx->pix_fmt==PIX_FMT_YUV422P) return CONTROL_TRUE;// half stride
	    break;
#if 1
        case IMGFMT_YUY2:
	    // converted using yuv422ptoyuy2()
	    if(avctx->pix_fmt==PIX_FMT_YUV422P) return CONTROL_TRUE;
	    break;
	}
#endif
        return CONTROL_FALSE;
    }
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    AVCodecContext *avctx;
    vd_ffmpeg_ctx *ctx;
    AVCodec *lavc_codec;

    if(!avcodec_inited){
      avcodec_init();
      avcodec_register_all();
      avcodec_inited=1;
    }

    ctx = sh->context = malloc(sizeof(vd_ffmpeg_ctx));
    if (!ctx)
	return(0);
    memset(ctx, 0, sizeof(vd_ffmpeg_ctx));
    
    lavc_codec = (AVCodec *)avcodec_find_decoder_by_name(sh->codec->dll);
    if(!lavc_codec){
	mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_MissingLAVCcodec,sh->codec->dll);
	return 0;
    }

    if(vd_use_slices && lavc_codec->capabilities&CODEC_CAP_DRAW_HORIZ_BAND)
	ctx->do_slices=1;
 
    if(lavc_codec->capabilities&CODEC_CAP_DR1)
	ctx->do_dr1=1;
    //XXX:FIXME:HACK:UGLY 422P with direct rendering is buggy cuz of that chroma stride trick ...
    if(sh->format == mmioFOURCC('H','F','Y','U'))
        ctx->do_dr1=0;
    ctx->b_age= ctx->ip_age[0]= ctx->ip_age[1]= 256*256*256*64;

#if LIBAVCODEC_BUILD >= 4645
    ctx->pic = avcodec_alloc_frame();
#else
    ctx->pic = avcodec_alloc_picture();
#endif
    ctx->avctx = avcodec_alloc_context();
    avctx = ctx->avctx;

    if(ctx->do_dr1){
        avctx->flags|= CODEC_FLAG_EMU_EDGE; 
        avctx->get_buffer= get_buffer;
        avctx->release_buffer= release_buffer;
    }

#ifdef CODEC_FLAG_NOT_TRUNCATED
    avctx->flags|= CODEC_FLAG_NOT_TRUNCATED;
#endif
    
    avctx->width = sh->disp_w;
    avctx->height= sh->disp_h;
    avctx->workaround_bugs= lavc_param_workaround_bugs;
    avctx->error_resilience= lavc_param_error_resilience;
    if(lavc_param_gray) avctx->flags|= CODEC_FLAG_GRAY;
    avctx->fourcc= sh->format;
    avctx->idct_algo= lavc_param_idct_algo;
    avctx->error_concealment= lavc_param_error_concealment;
#if LIBAVCODEC_BUILD >= 4642
    avctx->debug= lavc_param_debug;
#endif    
    mp_dbg(MSGT_DECVIDEO,MSGL_DBG2,"libavcodec.size: %d x %d\n",avctx->width,avctx->height);
    /* AVRn stores huffman table in AVI header */
    /* Pegasus MJPEG stores it also in AVI header, but it uses the common
       MJPG fourcc :( */
    if (sh->bih && (sh->bih->biSize != sizeof(BITMAPINFOHEADER)) &&
	(sh->format == mmioFOURCC('A','V','R','n') ||
	sh->format == mmioFOURCC('M','J','P','G')))
    {
	avctx->flags |= CODEC_FLAG_EXTERN_HUFF;
	avctx->extradata_size = sh->bih->biSize-sizeof(BITMAPINFOHEADER);
	avctx->extradata = malloc(avctx->extradata_size);
	memcpy(avctx->extradata, sh->bih+sizeof(BITMAPINFOHEADER),
	    avctx->extradata_size);

#if 0
	{
	    int x;
	    uint8_t *p = avctx->extradata;
	    
	    for (x=0; x<avctx->extradata_size; x++)
		printf("[%x] ", p[x]);
	    printf("\n");
	}
#endif
    }
    if(   sh->format == mmioFOURCC('R', 'V', '1', '0')
       || sh->format == mmioFOURCC('R', 'V', '1', '3')){
        avctx->extradata_size= 8;
        avctx->extradata = malloc(avctx->extradata_size);
        if(sh->bih->biSize!=sizeof(*sh->bih)+8){
            /* only 1 packet per frame & sub_id from fourcc */
	    ((uint32_t*)avctx->extradata)[0] = 0;
	    avctx->sub_id=
	    ((uint32_t*)avctx->extradata)[1] =
        	(sh->format == mmioFOURCC('R', 'V', '1', '3')) ? 0x10003001 : 0x10000000;
        } else {
	    /* has extra slice header (demux_rm or rm->avi streamcopy) */
	    unsigned int* extrahdr=(unsigned int*)(sh->bih+1);
	    ((uint32_t*)avctx->extradata)[0] = extrahdr[0];
	    avctx->sub_id=
	    ((uint32_t*)avctx->extradata)[1] = extrahdr[1];
	}

//        printf("%X %X %d %d\n", extrahdr[0], extrahdr[1]);
    }
    if (sh->bih && (sh->bih->biSize != sizeof(BITMAPINFOHEADER)) &&
	(sh->format == mmioFOURCC('M','4','S','2') ||
	 sh->format == mmioFOURCC('M','P','4','S') ||
	 sh->format == mmioFOURCC('H','F','Y','U') ||
	 sh->format == mmioFOURCC('W','M','V','2')
         ))
    {
	avctx->extradata_size = sh->bih->biSize-sizeof(BITMAPINFOHEADER);
	avctx->extradata = malloc(avctx->extradata_size);
	memcpy(avctx->extradata, sh->bih+1, avctx->extradata_size);
    }
    
    if(sh->bih)
	avctx->bits_per_sample= sh->bih->biBitCount;

    /* open it */
    if (avcodec_open(avctx, lavc_codec) < 0) {
        mp_msg(MSGT_DECVIDEO,MSGL_ERR, MSGTR_CantOpenCodec);
        return 0;
    }
    mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: libavcodec init OK!\n");
    ctx->last_aspect=-3;
    return 1; //mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YV12);
}

// uninit driver
static void uninit(sh_video_t *sh){
    vd_ffmpeg_ctx *ctx = sh->context;
    AVCodecContext *avctx = ctx->avctx;
    
    if(lavc_param_vstats){
        int i;
        for(i=1; i<32; i++){
            printf("QP: %d, count: %d\n", i, ctx->qp_stat[i]);
        }
        printf("Arithmetic mean of QP: %2.4f, Harmonic mean of QP: %2.4f\n", 
            ctx->qp_sum / avctx->coded_frame->coded_picture_number,
            1.0/(ctx->inv_qp_sum / avctx->coded_frame->coded_picture_number)
            );
    }

    if (avcodec_close(avctx) < 0)
    	    mp_msg(MSGT_DECVIDEO,MSGL_ERR, MSGTR_CantCloseCodec);

    if (avctx->extradata_size)
	free(avctx->extradata);
    avctx->extradata=NULL;
    if(avctx->slice_offset!=NULL) 
        free(avctx->slice_offset);
    avctx->slice_offset=NULL;

    if (avctx)
	free(avctx);
    if (ctx->pic)
	free(ctx->pic);
    if (ctx)
	free(ctx);
}

static void draw_slice(struct AVCodecContext *s,
                	UINT8 **src, int linesize,
                	int y, int width, int height){
    sh_video_t * sh = s->opaque;
    int stride[3];
    int start=0, i;
    int skip_stride= (s->width+15)>>4;
    UINT8 *skip= &s->coded_frame->mbskip_table[(y>>4)*skip_stride];
    int threshold= s->coded_frame->age;

    stride[0]=linesize;
    if(s->coded_frame->linesize[1]){
        stride[1]= s->coded_frame->linesize[1];
        stride[2]= s->coded_frame->linesize[2];
    }else
        stride[1]=stride[2]=stride[0]/2;
#if 0
    if(s->pict_type!=B_TYPE){
        for(i=0; i*16<width+16; i++){ 
            if(i*16>=width || skip[i]>=threshold){
                if(start==i) start++;
                else{
                    UINT8 *src2[3]= {src[0] + start*16, 
                                     src[1] + start*8, 
                                     src[2] + start*8};
//printf("%2d-%2d x %d\n", start, i, y);
                    mpcodecs_draw_slice (sh,src2, stride, (i-start)*16, height, start*16, y);
                    start= i+1;
                }
            }       
        }
    }else
#endif
        mpcodecs_draw_slice (sh,src, stride, width, height, 0, y);
}

static int init_vo(sh_video_t *sh){
    vd_ffmpeg_ctx *ctx = sh->context;
    AVCodecContext *avctx = ctx->avctx;

    if (avctx->aspect_ratio != ctx->last_aspect ||
	avctx->width != sh->disp_w ||
	avctx->height != sh->disp_h ||
	!ctx->vo_inited)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_V, "[ffmpeg] aspect_ratio: %f\n", avctx->aspect_ratio);
        ctx->last_aspect = avctx->aspect_ratio;
//	if(ctx->last_aspect>=0.01 && ctx->last_aspect<100)
	    sh->aspect = ctx->last_aspect;
	sh->disp_w = avctx->width;
	sh->disp_h = avctx->height;
	ctx->vo_inited=1;
	switch(avctx->pix_fmt){
	case PIX_FMT_YUV410P: ctx->best_csp=IMGFMT_YVU9;break; //svq1
	case PIX_FMT_YUV420P: ctx->best_csp=IMGFMT_YV12;break; //mpegs
	case PIX_FMT_YUV422P: ctx->best_csp=IMGFMT_422P;break; //mjpeg / huffyuv
	case PIX_FMT_YUV444P: ctx->best_csp=IMGFMT_444P;break; //photo jpeg
	case PIX_FMT_YUV411P: ctx->best_csp=IMGFMT_411P;break; //dv ntsc
	case PIX_FMT_YUV422:  ctx->best_csp=IMGFMT_YUY2;break; //huffyuv perhaps in the future
	case PIX_FMT_BGR24 :  ctx->best_csp=IMGFMT_BGR24;break; //huffyuv
	case PIX_FMT_BGRA32:  ctx->best_csp=IMGFMT_BGR32;break; //huffyuv
	default:
	    ctx->best_csp=0;
	}
    	if (!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h, ctx->best_csp))
    		return -1;
	ctx->convert=(sh->codec->outfmt[sh->outfmtidx]==IMGFMT_YUY2
	    && ctx->best_csp!=IMGFMT_YUY2); // yuv422p->yuy2 conversion
    }
    return 0;
}

static int get_buffer(AVCodecContext *avctx, AVFrame *pic){
    sh_video_t * sh = avctx->opaque;
    vd_ffmpeg_ctx *ctx = sh->context;
    mp_image_t* mpi=NULL;
    int flags= MP_IMGFLAG_ACCEPT_STRIDE | MP_IMGFLAG_PREFER_ALIGNED_STRIDE;
    int type= MP_IMGTYPE_IPB;
    int width= avctx->width;
    int height= avctx->height;
    int align=15;

    if(avctx->pix_fmt == PIX_FMT_YUV410P)
        align=63; //yes seriously, its really needed (16x16 chroma blocks in SVQ1 -> 64x64)

    if(init_vo(sh)<0){
        printf("init_vo failed\n");

        avctx->get_buffer= avcodec_default_get_buffer;
        avctx->release_buffer= avcodec_default_release_buffer;
        return avctx->get_buffer(avctx, pic);
    }

    if(!pic->reference)
        flags|=(!avctx->hurry_up && ctx->do_slices) ?
                MP_IMGFLAG_DRAW_CALLBACK:0;
    else
        flags|= MP_IMGFLAG_PRESERVE|MP_IMGFLAG_READABLE
                | (ctx->do_slices ? MP_IMGFLAG_DRAW_CALLBACK : 0);

    if(avctx->has_b_frames){
        type= MP_IMGTYPE_IPB;
    }else{
        type= MP_IMGTYPE_IP;
    }
    mp_msg(MSGT_DECVIDEO,MSGL_DBG2, type== MP_IMGTYPE_IPB ? "using IPB\n" : "using IP\n");

    mpi= mpcodecs_get_image(sh,type, flags,
			(width+align)&(~align), (height+align)&(~align));

    // ok, lets see what did we get:
    if(  mpi->flags&MP_IMGFLAG_DRAW_CALLBACK &&
       !(mpi->flags&MP_IMGFLAG_DIRECT)){
	// nice, filter/vo likes draw_callback :)
	avctx->draw_horiz_band= draw_slice;
    } else
	avctx->draw_horiz_band= NULL;
    pic->data[0]= mpi->planes[0];
    pic->data[1]= mpi->planes[1];
    pic->data[2]= mpi->planes[2];
    
    assert(mpi->width  >= ((width +align)&(~align)));   
    assert(mpi->height >= ((height+align)&(~align)));
    assert(mpi->stride[0] >= mpi->width);
    if(mpi->imgfmt==IMGFMT_I420 || mpi->imgfmt==IMGFMT_YV12 || mpi->imgfmt==IMGFMT_IYUV){
        const int y_size= mpi->stride[0] * mpi->height;
        const int c_size= mpi->stride[1] * mpi->chroma_height;
        
        assert(mpi->planes[0] > mpi->planes[1] || mpi->planes[0] + y_size <= mpi->planes[1]);
        assert(mpi->planes[0] > mpi->planes[2] || mpi->planes[0] + y_size <= mpi->planes[2]);
        assert(mpi->planes[1] > mpi->planes[0] || mpi->planes[1] + c_size <= mpi->planes[0]);
        assert(mpi->planes[1] > mpi->planes[2] || mpi->planes[1] + c_size <= mpi->planes[2]);
        assert(mpi->planes[2] > mpi->planes[0] || mpi->planes[2] + c_size <= mpi->planes[0]);
        assert(mpi->planes[2] > mpi->planes[1] || mpi->planes[2] + c_size <= mpi->planes[1]);
    }

    /* Note, some (many) codecs in libavcodec must have stride1==stride2 && no changes between frames
     * lavc will check that and die with an error message, if its not true
     */
    pic->linesize[0]= mpi->stride[0];
    pic->linesize[1]= mpi->stride[1];
    pic->linesize[2]= mpi->stride[2];

    pic->opaque = mpi;
//printf("%X\n", (int)mpi->planes[0]);
#if 0
if(mpi->flags&MP_IMGFLAG_DIRECT)
    printf("D");
else if(mpi->flags&MP_IMGFLAG_DRAW_CALLBACK)
    printf("S");
else
    printf(".");
#endif
    if(pic->reference){
        pic->age= ctx->ip_age[0];
        
        ctx->ip_age[0]= ctx->ip_age[1]+1;
        ctx->ip_age[1]= 1;
        ctx->b_age++;
    }else{
        pic->age= ctx->b_age;
    
        ctx->ip_age[0]++;
        ctx->ip_age[1]++;
        ctx->b_age=1;
    }
#if LIBAVCODEC_BUILD >= 4644
    pic->type= FF_BUFFER_TYPE_USER;
#endif
    return 0;
}

static void release_buffer(struct AVCodecContext *avctx, AVFrame *pic){
    int i;
    
#if LIBAVCODEC_BUILD >= 4644
    assert(pic->type == FF_BUFFER_TYPE_USER);
#endif
    
    for(i=0; i<4; i++){
        pic->data[i]= NULL;
    }
//printf("R%X %X\n", pic->linesize[0], pic->data[0]);
}

// copypaste from demux_real.c - it should match to get it working!
//FIXME put into some header
typedef struct dp_hdr_s {
    uint32_t chunks;	// number of chunks
    uint32_t timestamp; // timestamp from packet header
    uint32_t len;	// length of actual data
    uint32_t chunktab;	// offset to chunk offset array
} dp_hdr_t;

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    int got_picture=0;
    int ret;
    vd_ffmpeg_ctx *ctx = sh->context;
    AVFrame *pic= ctx->pic;
    AVCodecContext *avctx = ctx->avctx;
    mp_image_t* mpi=NULL;
    int dr1= ctx->do_dr1;

    if(len<=0) return NULL; // skipped frame

    avctx->draw_horiz_band=NULL;
    avctx->opaque=sh;
    if(ctx->vo_inited && !ctx->convert && !(flags&3) && !dr1){
	mpi=mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, MP_IMGFLAG_PRESERVE |
	    (ctx->do_slices?MP_IMGFLAG_DRAW_CALLBACK:0),
	    sh->disp_w, sh->disp_h);
	if(mpi && mpi->flags&MP_IMGFLAG_DRAW_CALLBACK){
	    // vd core likes slices!
	    avctx->draw_horiz_band=draw_slice;
	}
    }

    avctx->hurry_up=(flags&3)?((flags&2)?2:1):0;

//    if(sh->ds->demuxer->type == DEMUXER_TYPE_REAL){
    if(   sh->format == mmioFOURCC('R', 'V', '1', '0')
       || sh->format == mmioFOURCC('R', 'V', '1', '3'))
    if(sh->bih->biSize==sizeof(*sh->bih)+8){
        int i;
        dp_hdr_t *hdr= (dp_hdr_t*)data;

        if(avctx->slice_offset==NULL) 
            avctx->slice_offset= malloc(sizeof(int)*1000);
        
//        for(i=0; i<25; i++) printf("%02X ", ((uint8_t*)data)[i]);
        
        avctx->slice_count= hdr->chunks+1;
        for(i=0; i<avctx->slice_count; i++)
            avctx->slice_offset[i]= ((uint32_t*)(data+hdr->chunktab))[2*i+1];
	len=hdr->len;
        data+= sizeof(dp_hdr_t);
    }

    ret = avcodec_decode_video(avctx, pic,
	     &got_picture, data, len);
    if(ret<0) mp_msg(MSGT_DECVIDEO,MSGL_WARN, "Error while decoding frame!\n");

//-- vstats generation
#if LIBAVCODEC_BUILD >= 4643
    while(lavc_param_vstats){ // always one time loop
        static FILE *fvstats=NULL;
        char filename[20];
        static long long int all_len=0;
        static int frame_number=0;
        static double all_frametime=0.0;
        AVFrame *pic= avctx->coded_frame;

        if(!fvstats) {
            time_t today2;
            struct tm *today;
            today2 = time(NULL);
            today = localtime(&today2);
            sprintf(filename, "vstats_%02d%02d%02d.log", today->tm_hour,
                today->tm_min, today->tm_sec);
            fvstats = fopen(filename,"w");
            if(!fvstats) {
                perror("fopen");
                lavc_param_vstats=0; // disable block
                break;
                /*exit(1);*/
            }
        }

        all_len+=len;
        all_frametime+=sh->frametime;
        fprintf(fvstats, "frame= %5d q= %2.2f f_size= %6d s_size= %8.0fkB ",
            ++frame_number, pic->quality, len, (double)all_len/1024);
        fprintf(fvstats, "time= %0.3f br= %7.1fkbits/s avg_br= %7.1fkbits/s ",
           all_frametime, (double)(len*8)/sh->frametime/1000.0,
           (double)(all_len*8)/all_frametime/1000.0);
	switch(pic->pict_type){
	case FF_I_TYPE:
            fprintf(fvstats, "type= I\n");
	    break;
	case FF_P_TYPE:
            fprintf(fvstats, "type= P\n");
	    break;
	case FF_S_TYPE:
            fprintf(fvstats, "type= S\n");
	    break;
	case FF_B_TYPE:
            fprintf(fvstats, "type= B\n");
	    break;
	}
        
        ctx->qp_stat[(int)(pic->quality+0.5)]++;
        ctx->qp_sum += pic->quality;
        ctx->inv_qp_sum += 1.0/pic->quality;
        
        break;
    }
#endif
//--

    if(!got_picture) return NULL;	// skipped image

    if(init_vo(sh)<0) return NULL;

    if(dr1 && pic->opaque){
        mpi= (mp_image_t*)pic->opaque;
    }
        
    if(!mpi && ctx->convert){
	// do yuv422p -> yuy2 conversion:
        mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
	    avctx->width, avctx->height);
	if(!mpi) return NULL;
	yuv422ptoyuy2(pic->data[0],pic->data[1],pic->data[2],
	    mpi->planes[0],avctx->width,avctx->height,
	    pic->linesize[0],pic->linesize[1],mpi->stride[0]);
	return mpi;
    }
    
    if(!mpi)
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, MP_IMGFLAG_PRESERVE,
	avctx->width, avctx->height);
    if(!mpi){	// temporary!
	printf("couldn't allocate image for codec\n");
	return NULL;
    }
    
    if(!dr1){
        mpi->planes[0]=pic->data[0];
        mpi->planes[1]=pic->data[1];
        mpi->planes[2]=pic->data[2];
        mpi->stride[0]=pic->linesize[0];
        mpi->stride[1]=pic->linesize[1];
        mpi->stride[2]=pic->linesize[2];
    }

    if(avctx->pix_fmt==PIX_FMT_YUV422P && mpi->chroma_y_shift==1){
	// we have 422p but user wants 420p
	mpi->stride[1]*=2;
	mpi->stride[2]*=2;
    }
    
/* to comfirm with newer lavc style */
    mpi->qscale =pic->qscale_table;
    mpi->qstride=pic->qstride;
    mpi->pict_type=pic->pict_type;
    
    return mpi;
}

#endif

