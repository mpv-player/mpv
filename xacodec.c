/*
  xacodec.c -- XAnim Video Codec DLL support

  (C) 2001 Alex Beregszaszi <alex@naxine.org>
       and Arpad Gereoffy <arpi@thot.banki.hu>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* strerror */

#include "config.h"

#ifdef USE_XANIM

#ifdef __FreeBSD__
#include <unistd.h>
#endif

#include <dlfcn.h> /* dlsym, dlopen, dlclose */
#include <stdarg.h> /* va_alist, va_start, va_end */
#include <errno.h> /* strerror, errno */

#include "mp_msg.h"
#include "bswap.h"

#include "stream.h"
#include "demuxer.h"
#include "codec-cfg.h"
#include "stheader.h"

#include "libvo/img_format.h"
#include "linux/timer.h"
#include "xacodec.h"

#include "fastmemcpy.h"

#if 0
typedef char xaBYTE;
typedef short xaSHORT;
typedef int xaLONG;

typedef unsigned char xaUBYTE;
typedef unsigned short xaUSHORT;
typedef unsigned int xaULONG;
#endif

#define xaFALSE 0
#define xaTRUE 1

#ifndef RTLD_NOW
#define RLTD_NOW 2
#endif
#ifndef RTLD_LAZY
#define RLTD_LAZY 1
#endif
#ifndef RTLD_GLOBAL
#define RLTD_GLOBAL 256
#endif

#define XA_CLOSE_FUNCS 5
int xa_close_func = 0;

typedef struct xacodec_driver
{
    XA_DEC_INFO *decinfo;
    void *file_handler;
    long (*iq_func)(XA_CODEC_HDR *codec_hdr);
    unsigned int (*dec_func)(unsigned char *image, unsigned char *delta,
	unsigned int dsize, XA_DEC_INFO *dec_info);
    void *close_func[XA_CLOSE_FUNCS];
    xacodec_image_t image;
} xacodec_driver_t;

xacodec_driver_t *xacodec_driver = NULL;

/* Needed by XAnim DLLs */
void XA_Print(char *fmt, ...)
{
    va_list vallist;
    char buf[1024];

    va_start(vallist, fmt);
    vsnprintf(buf, 1024, fmt, vallist);
    mp_msg(MSGT_XACODEC, MSGL_DBG2, "[xacodec] %s\n", buf);
    va_end(vallist);

    return;
}

/* 0 is no debug (needed by 3ivX) */
long xa_debug = 0;

void TheEnd1(char *err_mess)
{
    XA_Print("error: %s - exiting\n", err_mess);
    xacodec_exit();

    return;
}

void XA_Add_Func_To_Free_Chain(XA_ANIM_HDR *anim_hdr, void (*function)())
{
//    XA_Print("XA_Add_Func_To_Free_Chain('anim_hdr: %08x', 'function: %08x')",
//	    anim_hdr, function);
    xacodec_driver->close_func[xa_close_func] = function;
    if (xa_close_func+1 < XA_CLOSE_FUNCS)
	xa_close_func++;

    return;
}
/* end of crap */

/* load, init and query */
int xacodec_init(char *filename, xacodec_driver_t *codec_driver)
{
    void *(*what_the)();
    char *error;
    XAVID_MOD_HDR *mod_hdr;
    XAVID_FUNC_HDR *func;
    int i;

    codec_driver->file_handler = dlopen(filename, RTLD_NOW|RTLD_GLOBAL);
    if (!codec_driver->file_handler)
    {
	error = dlerror();
	if (error)
	    mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: failed to dlopen %s while %s\n", filename, error);
	else
	    mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: failed to dlopen %s\n", filename);
	return(0);
    }

    what_the = dlsym(codec_driver->file_handler, "What_The");
    if ((error = dlerror()) != NULL)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: failed to init %s while %s\n", filename, error);
	dlclose(codec_driver->file_handler);
	return(0);
    }
	
    mod_hdr = what_the();
    if (!mod_hdr)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: initializer function failed in %s\n", filename);
	dlclose(codec_driver->file_handler);
	return(0);
    }
    
    mp_msg(MSGT_DECVIDEO, MSGL_INFO, "=== XAnim Codec ===\n");
    mp_msg(MSGT_DECVIDEO, MSGL_INFO, " Filename: %s (API revision: %x)\n", filename, mod_hdr->api_rev);
    mp_msg(MSGT_DECVIDEO, MSGL_INFO, " Codec: %s. Rev: %s\n", mod_hdr->desc, mod_hdr->rev);
    if (mod_hdr->copyright)
	mp_msg(MSGT_DECVIDEO, MSGL_INFO, " %s\n", mod_hdr->copyright);
    if (mod_hdr->mod_author)
	mp_msg(MSGT_DECVIDEO, MSGL_INFO, " Module Author(s): %s\n", mod_hdr->mod_author);
    if (mod_hdr->authors)
	mp_msg(MSGT_DECVIDEO, MSGL_INFO, " Codec Author(s): %s\n", mod_hdr->authors);

    if (mod_hdr->api_rev > XAVID_API_REV)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: not supported api revision (%d) in %s\n",
	    mod_hdr->api_rev, filename);
	dlclose(codec_driver->file_handler);
	return(0);
    }

    func = mod_hdr->funcs;
    if (!func)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: function table error in %s\n", filename);
	dlclose(codec_driver->file_handler);
	return(0);
    }
    
    mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "Exported functions by codec: [functable: 0x%08x entries: %d]\n",
	mod_hdr->funcs, mod_hdr->num_funcs);
    for (i = 0; i < mod_hdr->num_funcs; i++)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_DBG2, " %d: %d %d [iq:0x%08x d:0x%08x]\n",
		i, func[i].what, func[i].id, func[i].iq_func, func[i].dec_func);
	if (func[i].what & XAVID_AVI_QUERY)
	{
	    mp_msg(MSGT_DECVIDEO, MSGL_DBG2, " 0x%08x: avi init/query func (id: %d)\n",
		func[i].iq_func, func[i].id);
	    codec_driver->iq_func = (void *)func[i].iq_func;
	}
	if (func[i].what & XAVID_QT_QUERY)
	{
	    mp_msg(MSGT_DECVIDEO, MSGL_DBG2, " 0x%08x: qt init/query func (id: %d)\n",
		func[i].iq_func, func[i].id);
	    codec_driver->iq_func = (void *)func[i].iq_func;
	}
	if (func[i].what & XAVID_DEC_FUNC)
	{
	    mp_msg(MSGT_DECVIDEO, MSGL_DBG2, " 0x%08x: decoder func (init/query: 0x%08x) (id: %d)\n",
		func[i].dec_func, func[i].iq_func, func[i].id);
	    codec_driver->dec_func = (void *)func[i].dec_func;
	}
    }
    return(1);
}

int xacodec_query(xacodec_driver_t *codec_driver, XA_CODEC_HDR *codec_hdr)
{
    long codec_ret;

#if 0
    /* the brute one */
    if (codec_driver->dec_func)
    {
	codec_hdr->decoder = codec_driver->dec_func;
	mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "We got decoder's address at init! %p\n", codec_hdr->decoder);
	return(1);
    }
#endif
    codec_ret = codec_driver->iq_func(codec_hdr);
    switch(codec_ret)
    {
	case CODEC_SUPPORTED:
	    codec_driver->dec_func = (void *)codec_hdr->decoder;
	    mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "Codec is supported: found decoder for %s at 0x%08x\n",
		codec_hdr->description, codec_hdr->decoder);
	    return(1);
	case CODEC_UNSUPPORTED:
	    mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "Codec (%s) is unsupported by driver\n",
		codec_hdr->description);
	    return(0);
	case CODEC_UNKNOWN:
	default:
	    mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "Codec (%s) is unknown by driver\n",
		codec_hdr->description);
	    return(0);
    }
}

char *xacodec_def_path = XACODEC_PATH;

int xacodec_init_video(sh_video_t *vidinfo, int out_format)
{
    char dll[1024];
    XA_CODEC_HDR codec_hdr;
    int i;
    
    xacodec_driver = realloc(xacodec_driver, sizeof(struct xacodec_driver));
    if (xacodec_driver == NULL)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: memory allocation error: %s\n",
	    strerror(errno));
	return(0);
    }

    xacodec_driver->iq_func = NULL;
    xacodec_driver->dec_func = NULL;

    for (i=0; i < XA_CLOSE_FUNCS; i++)
	xacodec_driver->close_func[i] = NULL;

    if (getenv("XANIM_MOD_DIR"))
	xacodec_def_path = getenv("XANIM_MOD_DIR");

    snprintf(dll, 1024, "%s/%s", xacodec_def_path, vidinfo->codec->dll);
    if (xacodec_init(dll, xacodec_driver) == 0)
	return(0);

    codec_hdr.xapi_rev = XAVID_API_REV;
    codec_hdr.anim_hdr = malloc(4096);
    codec_hdr.description = vidinfo->codec->info;
    codec_hdr.compression = bswap_32(vidinfo->bih->biCompression);
    codec_hdr.decoder = NULL;
    codec_hdr.x = vidinfo->bih->biWidth; /* ->disp_w */
    codec_hdr.y = vidinfo->bih->biHeight; /* ->disp_h */
    /* extra fields to store palette */
    codec_hdr.avi_ctab_flag = 0;
    codec_hdr.avi_read_ext = NULL;
    codec_hdr.extra = NULL;

    switch(out_format)
    {
	case IMGFMT_IYUV:
	case IMGFMT_I420:
	case IMGFMT_YV12:
	    codec_hdr.depth = 12;
	    break;
	case IMGFMT_YVU9:
	    if (vidinfo->bih->biCompression == mmioFOURCC('I','V','3','2') ||
		vidinfo->bih->biCompression == mmioFOURCC('i','v','3','2') ||
		vidinfo->bih->biCompression == mmioFOURCC('I','V','3','1') ||
		vidinfo->bih->biCompression == mmioFOURCC('i','v','3','2'))
	    {
		mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: not supporting YVU9 output with Indeo3\n");
		return(0);
	    }
	    codec_hdr.depth = 9;
	    break;
	default:
	    mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: not supported image out format (%s)\n",
		vo_format_name(out_format));
	    return(0);
    }
    mp_msg(MSGT_DECVIDEO, MSGL_INFO, "xacodec: querying for input %dx%d %dbit [fourcc: %4x] (%s)...\n",
	codec_hdr.x, codec_hdr.y, codec_hdr.depth, codec_hdr.compression, codec_hdr.description);

    if (xacodec_query(xacodec_driver, &codec_hdr) == 0)
	return(0);

//    free(codec_hdr.anim_hdr);

    xacodec_driver->decinfo = malloc(sizeof(XA_DEC_INFO));
    if (xacodec_driver->decinfo == NULL)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: memory allocation error: %s\n",
	    strerror(errno));
	return(0);
    }
    xacodec_driver->decinfo->cmd = 0;
    xacodec_driver->decinfo->skip_flag = 0;
    xacodec_driver->decinfo->imagex = xacodec_driver->decinfo->xe = codec_hdr.x;
    xacodec_driver->decinfo->imagey = xacodec_driver->decinfo->ye = codec_hdr.y;
    xacodec_driver->decinfo->imaged = codec_hdr.depth;
    xacodec_driver->decinfo->chdr = NULL;
    xacodec_driver->decinfo->map_flag = 0; /* xaFALSE */
    xacodec_driver->decinfo->map = NULL;
    xacodec_driver->decinfo->xs = xacodec_driver->decinfo->ys = 0;
    xacodec_driver->decinfo->special = 0;
    xacodec_driver->decinfo->extra = codec_hdr.extra;
    mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "decinfo->extra, filled by codec: 0x%08x [%s]\n",
	&xacodec_driver->decinfo->extra, xacodec_driver->decinfo->extra);

//    vidinfo->our_out_buffer = malloc(codec_hdr.y * codec_hdr.x * ((codec_hdr.depth+7)/8));
//    vidinfo->our_out_buffer = malloc(codec_hdr.y * codec_hdr.x * codec_hdr.depth);
    xacodec_driver->image.out_fmt = out_format;
    xacodec_driver->image.bpp = codec_hdr.depth;
    xacodec_driver->image.width = codec_hdr.x;
    xacodec_driver->image.height = codec_hdr.y;
    xacodec_driver->image.mem = malloc(codec_hdr.y * codec_hdr.x * ((codec_hdr.depth+7)/8));

    if (xacodec_driver->image.mem == NULL)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: memory allocation error: %s\n",
	    strerror(errno));
	return(0);
    }

    return(1);
}

#define ACT_DLTA_NORM	0x00000000
#define ACT_DLTA_BODY	0x00000001
#define ACT_DLTA_XOR	0x00000002
#define ACT_DLTA_NOP	0x00000004
#define ACT_DLTA_MAPD	0x00000008
#define ACT_DLTA_DROP	0x00000010
#define ACT_DLTA_BAD	0x80000000

//    unsigned int (*dec_func)(unsigned char *image, unsigned char *delta,
//	unsigned int dsize, XA_DEC_INFO *dec_info);

xacodec_image_t* xacodec_decode_frame(uint8_t *frame, int frame_size, int skip_flag)
{
    unsigned int ret;
    xacodec_image_t *image=&xacodec_driver->image;

// ugyis kiirja a vegen h dropped vagy nem.. 
//    if (skip_flag > 0)
//	mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "frame will be dropped..\n");

    xacodec_driver->decinfo->skip_flag = skip_flag;

    image->planes[0]=image->mem;
    image->stride[0]=image->width;
    switch(image->out_fmt){
    case IMGFMT_YV12:
	image->planes[2]=image->planes[0]+image->width*image->height;
	image->planes[1]=image->planes[2]+image->width*image->height/4;
	image->stride[1]=image->stride[2]=image->width/2;
	break;
    case IMGFMT_I420:
    case IMGFMT_IYUV:
	image->planes[1]=image->planes[0]+image->width*image->height;
	image->planes[2]=image->planes[1]+image->width*image->height/4;
	image->stride[1]=image->stride[2]=image->width/2;
	break;
    case IMGFMT_YVU9:
	image->planes[2]=image->planes[0]+image->width*image->height;
	image->planes[1]=image->planes[2]+(image->width>>2)*(image->height>>2);
	image->stride[1]=image->stride[2]=image->width/4;
	break;
    }

//    printf("frame: %08x (size: %d) - dest: %08x\n", frame, frame_size, dest);

    ret = xacodec_driver->dec_func((uint8_t*)&xacodec_driver->image, frame, frame_size, xacodec_driver->decinfo);

//    printf("ret: %lu : ", ret);
    

    if (ret == ACT_DLTA_NORM)
    {
//	mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "norm\n");
	return &xacodec_driver->image;
    }

    if (ret & ACT_DLTA_MAPD)
	mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "mapd\n");
/*
    if (!(ret & ACT_DLT_MAPD))
	xacodec_driver->decinfo->map_flag = 0;
    else
    {
	xacodec_driver->decinfo->map_flag = 1;
	xacodec_driver->decinfo->map = ...
    }
*/

    if (ret & ACT_DLTA_XOR)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "xor\n");
	return &xacodec_driver->image;
    }

    /* nothing changed */
    if (ret & ACT_DLTA_NOP)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "nop\n");
	return NULL;
    }

    /* frame dropped (also display latest frame) */
    if (ret & ACT_DLTA_DROP)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "drop\n");
	return NULL;
    }

    if (ret & ACT_DLTA_BAD)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "bad\n");
	return NULL;
    }

    /* used for double buffer */
    if (ret & ACT_DLTA_BODY)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "body\n");
	return NULL;
    }
    
    return NULL;
}

int xacodec_exit()
{
    int i;
    void (*close_func)();

    for (i=0; i < XA_CLOSE_FUNCS; i++)
	if (xacodec_driver->close_func[i])
	{
	    close_func = xacodec_driver->close_func[i];
	    close_func();
	}
    dlclose(xacodec_driver->file_handler);
    if (xacodec_driver->decinfo != NULL)
	free(xacodec_driver->decinfo);
    free(xacodec_driver);
    return(TRUE);
}


/* *** XANIM Conversions *** */
/* like loader/win32.c - mini XANIM library */

unsigned long XA_Time_Read()
{
    return GetTimer(); //(GetRelativeTime());
}

void XA_dummy()
{
    XA_Print("dummy() called");
}

void XA_Gen_YUV_Tabs(XA_ANIM_HDR *anim_hdr)
{
    XA_Print("XA_Gen_YUV_Tabs('anim_hdr: %08x')", anim_hdr);

//    XA_Print("anim type: %d - img[x: %d, y: %d, c: %d, d: %d]",
//	anim_hdr->anim_type, anim_hdr->imagex, anim_hdr->imagey,
//	anim_hdr->imagec, anim_hdr->imaged);
    return;
}

void JPG_Setup_Samp_Limit_Table(XA_ANIM_HDR *anim_hdr)
{
    XA_Print("JPG_Setup_Samp_Limit_Table('anim_hdr: %08x')", anim_hdr);
//    xa_byte_limit = jpg_samp_limit + (MAXJSAMPLE + 1);
    return;
}

void JPG_Alloc_MCU_Bufs(XA_ANIM_HDR *anim_hdr, unsigned int width,
	unsigned int height, unsigned int full_flag)
{
    XA_Print("JPG_Alloc_MCU_Bufs('anim_hdr: %08x', 'width: %d', 'height: %d', 'full_flag: %d')",
	    anim_hdr, width, height, full_flag);
    return;
}

/* ---------------  4x4 pixel YUV block fillers [CVID] ----------------- */

typedef struct
{
    unsigned char r0, g0, b0;
    unsigned char r1, g1, b1;
    unsigned char r2, g2, b2;
    unsigned char r3, g3, b3;
    unsigned int clr0_0, clr0_1, clr0_2, clr0_3;
    unsigned int clr1_0, clr1_1, clr1_2, clr1_3;
    unsigned int clr2_0, clr2_1, clr2_2, clr2_3;
    unsigned int clr3_0, clr3_1, clr3_2, clr3_3;
} XA_2x2_Color;

#define SET_4_YUV_PIXELS(image,x,y,cmap2x2) \
    image->planes[0][((x)+0)+((y)+0)*image->stride[0]]=cmap2x2->clr0_0;\
    image->planes[0][((x)+1)+((y)+0)*image->stride[0]]=cmap2x2->clr0_1;\
    image->planes[0][((x)+0)+((y)+1)*image->stride[0]]=cmap2x2->clr0_2;\
    image->planes[0][((x)+1)+((y)+1)*image->stride[0]]=cmap2x2->clr0_3;\
    image->planes[1][((x)>>1)+((y)>>1)*image->stride[1]]=cmap2x2->clr1_0;\
    image->planes[2][((x)>>1)+((y)>>1)*image->stride[2]]=cmap2x2->clr1_1;

void XA_2x2_OUT_1BLK_Convert(unsigned char *image_p, unsigned int x, unsigned int y,
    unsigned int imagex, XA_2x2_Color *cmap2x2)
{
    xacodec_image_t *image=(xacodec_image_t*)image_p;

#if 0
    SET_4_YUV_PIXELS(image,x,y,cmap2x2)
#else
    SET_4_YUV_PIXELS(image,x,y,cmap2x2)
    SET_4_YUV_PIXELS(image,x+2,y,cmap2x2)
    SET_4_YUV_PIXELS(image,x,y+2,cmap2x2)
    SET_4_YUV_PIXELS(image,x+2,y+2,cmap2x2)
#endif
    return;
}

void XA_2x2_OUT_4BLKS_Convert(unsigned char *image_p, unsigned int x, unsigned int y,
    unsigned int imagex, XA_2x2_Color *cm0, XA_2x2_Color *cm1, XA_2x2_Color *cm2,
    XA_2x2_Color *cm3)
{
    xacodec_image_t *image=(xacodec_image_t*)image_p;

    SET_4_YUV_PIXELS(image,x,y,cm0)
    SET_4_YUV_PIXELS(image,x+2,y,cm1)
    SET_4_YUV_PIXELS(image,x,y+2,cm2)
    SET_4_YUV_PIXELS(image,x+2,y+2,cm3)
    return;
}

void *YUV2x2_Blk_Func(unsigned int image_type, int blks, unsigned int dith_flag)
{
    mp_dbg(MSGT_DECVIDEO,MSGL_DBG3, "YUV2x2_Blk_Func(image_type=%d, blks=%d, dith_flag=%d)\n",
	image_type, blks, dith_flag);
    switch(blks){
    case 1:
	return (void*) XA_2x2_OUT_1BLK_Convert;
    case 4:
	return (void*) XA_2x2_OUT_4BLKS_Convert;
    }

    mp_msg(MSGT_DECVIDEO,MSGL_WARN,"Unimplemented: YUV2x2_Blk_Func(image_type=%d  blks=%d  dith=%d)\n",image_type,blks,dith_flag);
    return (void*) XA_dummy;
}

//  Take Four Y's and UV and put them into a 2x2 Color structure.

void XA_YUV_2x2_clr(XA_2x2_Color *cmap2x2, unsigned int Y0, unsigned int Y1,
    unsigned int Y2, unsigned int Y3, unsigned int U, unsigned int V,
    unsigned int map_flag, unsigned int *map, XA_CHDR *chdr)
{

  mp_dbg(MSGT_DECVIDEO,MSGL_DBG3, "XA_YUV_2x2_clr(%p [%d,%d,%d,%d][%d][%d] %d %p %p)\n",
          cmap2x2,Y0,Y1,Y2,Y3,U,V,map_flag,map,chdr);

  cmap2x2->clr0_0=Y0;
  cmap2x2->clr0_1=Y1;
  cmap2x2->clr0_2=Y2;
  cmap2x2->clr0_3=Y3;
  cmap2x2->clr1_0=U;
  cmap2x2->clr1_1=V;
  return;
}

void *YUV2x2_Map_Func(unsigned int image_type, unsigned int dith_type)
{
    mp_dbg(MSGT_DECVIDEO,MSGL_DBG3, "YUV2x2_Map_Func('image_type: %d', 'dith_type: %d')",
	    image_type, dith_type);
    return((void*)XA_YUV_2x2_clr);
}

/* -------------------- whole YUV frame converters ------------------------- */

typedef struct
{
    unsigned char *Ybuf;
    unsigned char *Ubuf;
    unsigned char *Vbuf;
    unsigned char *the_buf;
    unsigned int the_buf_size;
    unsigned short y_w, y_h;
    unsigned short uv_w, uv_h;
} YUVBufs;

typedef struct
{
    unsigned long Uskip_mask;
    long	*YUV_Y_tab;
    long	*YUV_UB_tab;
    long	*YUV_VR_tab;
    long	*YUV_UG_tab;
    long	*YUV_VG_tab;
} YUVTabs;

YUVBufs jpg_YUVBufs;
YUVTabs def_yuv_tabs;

/* -------------- YUV 4x4 1x1 1x1  (4:1:0 aka YVU9) [Indeo 3,4,5] ------------------ */

void XA_YUV1611_Convert(unsigned char *image_p, unsigned int imagex, unsigned int imagey,
    unsigned int i_x, unsigned int i_y, YUVBufs *yuv, YUVTabs *yuv_tabs,
    unsigned int map_flag, unsigned int *map, XA_CHDR *chdr)
{
    xacodec_image_t *image=(xacodec_image_t*)image_p;
    int y;
    int uvstride;

    mp_dbg(MSGT_DECVIDEO,MSGL_DBG3, "YUVTabs:  %d %p %p %p %p %p\n",yuv_tabs->Uskip_mask,
	yuv_tabs->YUV_Y_tab,
	yuv_tabs->YUV_UB_tab,
	yuv_tabs->YUV_VR_tab,
	yuv_tabs->YUV_UG_tab,
	yuv_tabs->YUV_VG_tab );

    mp_dbg(MSGT_DECVIDEO,MSGL_DBG3, "XA_YUV1611_Convert('image: %08x', 'imagex: %d', 'imagey: %d', 'i_x: %d', 'i_y: %d', 'yuv_bufs: %08x', 'yuv_tabs: %08x', 'map_flag: %d', 'map: %08x', 'chdr: %08x')",
	image, imagex, imagey, i_x, i_y, yuv, yuv_tabs, map_flag, map, chdr);

    mp_dbg(MSGT_DECVIDEO,MSGL_DBG3, "YUV: %p %p %p %X (%d) %dx%d %dx%d\n",
	yuv->Ybuf,yuv->Ubuf,yuv->Vbuf,yuv->the_buf,yuv->the_buf_size,
	yuv->y_w,yuv->y_h,yuv->uv_w,yuv->uv_h);

    if(image->out_fmt == IMGFMT_YVU9 && !yuv_tabs->YUV_Y_tab)
    {
	if(i_x==image->width && i_y==image->height){
	    image->planes[0]=yuv->Ybuf;
	    image->planes[1]=yuv->Ubuf;
	    image->planes[2]=yuv->Vbuf;
	    image->stride[0]=i_x; // yuv->y_w
	    image->stride[1]=image->stride[2]=i_x/4; // yuv->uv_w
	} else {
	    int y;
	    for(y=0;y<i_y;y++)
		memcpy(image->planes[0]+y*image->stride[0],yuv->Ybuf+y*i_x,i_x);
	    i_x>>=2; i_y>>=2;
	    for(y=0;y<i_y;y++){
		memcpy(image->planes[1]+y*image->stride[1],yuv->Ubuf+y*i_x,i_x);
		memcpy(image->planes[2]+y*image->stride[2],yuv->Vbuf+y*i_x,i_x);
	    }
	}
	return;
    }

    // copy Y plane:
    if(yuv_tabs->YUV_Y_tab){     // dirty hack to detect iv32:
	for(y=0;y<imagey*imagex;y++)
	    image->planes[0][y]=yuv->Ybuf[y]<<1;
    } else
	memcpy(image->planes[0],yuv->Ybuf,imagex*imagey);

    // scale U,V planes by 2:
    imagex>>=2;
    imagey>>=2;
    
    uvstride=(yuv->uv_w)?yuv->uv_w:imagex;

    for(y=0;y<imagey;y++){
	unsigned char *su=yuv->Ubuf+uvstride*y;
	unsigned char *sv=yuv->Vbuf+uvstride*y;
	unsigned int strideu=image->stride[1];
	unsigned int stridev=image->stride[2];
	unsigned char *du=image->planes[1]+2*y*strideu;
	unsigned char *dv=image->planes[2]+2*y*stridev;
	int x;
	if(yuv_tabs->YUV_Y_tab){     // dirty hack to detect iv32:
	    for(x=0;x<imagex;x++){
		du[2*x]=du[2*x+1]=du[2*x+strideu]=du[2*x+strideu+1]=su[x]*2;
		dv[2*x]=dv[2*x+1]=dv[2*x+stridev]=dv[2*x+stridev+1]=sv[x]*2;
	    }
	} else {
	    for(x=0;x<imagex;x++){
		du[2*x]=du[2*x+1]=du[2*x+strideu]=du[2*x+strideu+1]=su[x];
		dv[2*x]=dv[2*x+1]=dv[2*x+stridev]=dv[2*x+stridev+1]=sv[x];
	    }
	}
    }
    return;
}

void *XA_YUV1611_Func(unsigned int image_type)
{
    mp_dbg(MSGT_DECVIDEO,MSGL_DBG3, "XA_YUV1611_Func('image_type: %d')", image_type);
    return((void *)XA_YUV1611_Convert);
}

/* -------------- YUV 4x1 1x1 1x1 (4:1:1 but interleaved) [CYUV] ------------------ */

void XA_YUV411111_Convert(unsigned char *image, unsigned int imagex, unsigned int imagey,
    unsigned int i_x, unsigned int i_y, YUVBufs *yuv_bufs, YUVTabs *yuv_tabs,
    unsigned int map_flag, unsigned int *map, XA_CHDR *chdr)
{
    mp_dbg(MSGT_DECVIDEO,MSGL_DBG3, "XA_YUV411111_Convert('image: %d', 'imagex: %d', 'imagey: %d', 'i_x: %d', 'i_y: %d', 'yuv_bufs: %08x', 'yuv_tabs: %08x', 'map_flag: %d', 'map: %08x', 'chdr: %08x')",
	    image, imagex, imagey, i_x, i_y, yuv_bufs, yuv_tabs, map_flag, map, chdr);
    return;
}

void *XA_YUV411111_Func(unsigned int image_type)
{
    mp_dbg(MSGT_DECVIDEO,MSGL_DBG3, "XA_YUV411111_Func('image_type: %d')", image_type);
    return((void*)XA_YUV411111_Convert);
}

/* --------------- YUV 2x2 1x1 1x1 (4:2:0 aka YV12) [3ivX,H263] ------------ */

void XA_YUV221111_Convert(unsigned char *image_p, unsigned int imagex, unsigned int imagey,
    unsigned int i_x, unsigned int i_y, YUVBufs *yuv, YUVTabs *yuv_tabs, unsigned int map_flag,
    unsigned int *map, XA_CHDR *chdr)
{
    xacodec_image_t *image=(xacodec_image_t*)image_p;

    mp_dbg(MSGT_DECVIDEO,MSGL_DBG3, "XA_YUV221111_Convert(%p  %dx%d %d;%d [%dx%d]  %p %p %d %p %p)\n",
	image,imagex,imagey,i_x,i_y, image->width,image->height,
	yuv,yuv_tabs,map_flag,map,chdr);

    mp_dbg(MSGT_DECVIDEO,MSGL_DBG3, "YUV: %p %p %p %X (%X) %Xx%X %Xx%X\n",
	yuv->Ybuf,yuv->Ubuf,yuv->Vbuf,yuv->the_buf,yuv->the_buf_size,
	yuv->y_w,yuv->y_h,yuv->uv_w,yuv->uv_h);

if(i_x==image->width && i_y==image->height){
//    printf("Direct render!!!\n");
    image->planes[0]=yuv->Ybuf;
    if(image->out_fmt==IMGFMT_YV12){
	image->planes[1]=yuv->Ubuf;
	image->planes[2]=yuv->Vbuf;
    } else {
	image->planes[1]=yuv->Vbuf;
	image->planes[2]=yuv->Ubuf;
    }
    image->stride[0]=i_x; // yuv->y_w
    image->stride[1]=image->stride[2]=i_x/2; // yuv->uv_w
} else {
    int y;
    for(y=0;y<i_y;y++)
	memcpy(image->planes[0]+y*image->stride[0],yuv->Ybuf+y*i_x,i_x);
    i_x>>=1; i_y>>=1;
    for(y=0;y<i_y;y++){
	memcpy(image->planes[1]+y*image->stride[1],yuv->Ubuf+y*i_x,i_x);
	memcpy(image->planes[2]+y*image->stride[2],yuv->Vbuf+y*i_x,i_x);
    }
}
    return;
}

void *XA_YUV221111_Func(unsigned int image_type)
{
    mp_dbg(MSGT_DECVIDEO,MSGL_DBG3, "XA_YUV221111_Func('image_type: %d')\n",image_type);
    return((void *)XA_YUV221111_Convert);
}

/* *** EOF XANIM *** */
#endif
