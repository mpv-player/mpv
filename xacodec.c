/*
  xacodec.c
  XAnim Video Codec DLL support

  (C) 2001 Alex Beregszaszi <alex@naxine.org>
*/

#include <stdio.h>
#include <stdlib.h>

#include <dlfcn.h> /* dlsym, dlopen, dlclose */
#include <stdarg.h> /* va_alist, va_start, va_end */
#include <errno.h> /* strerror, errno */

#include "config.h"
#define XACODEC_PATH "/usr/lib/xanim/mods"

#include "mp_msg.h"
#include "bswap.h"

#include "stream.h"
#include "demuxer.h"
#include "codec-cfg.h"
#include "stheader.h"

#include "libvo/img_format.h"
#include "xacodec.h"

typedef char xaBYTE;
typedef short xaSHORT;
typedef int xaLONG;

typedef unsigned char xaUBYTE;
typedef unsigned short xaUSHORT;
typedef unsigned int xaULONG;

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

extern int verbose;

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
int XA_Print(char *fmt, ...)
{
    va_list vallist;
    char buf[1024];

    va_start(vallist, fmt);
    vsnprintf(buf, 1024, fmt, vallist);
    mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "[xacodec] %s\n", buf);
    va_end(vallist);
}

/* 0 is no debug (needed by 3ivX) */
long xa_debug = 2;

int TheEnd1(char *err_mess)
{
    XA_Print("error: %s - exiting\n", err_mess);
    xacodec_exit();
}

int XA_Add_Func_To_Free_Chain(XA_ANIM_HDR *anim_hdr, void (*function)())
{
    XA_Print("XA_Add_Func_To_Free_Chain('anim_hdr: %08x', 'function: %08x')",
	    anim_hdr, function);
    xacodec_driver->close_func[xa_close_func] = function;
    if (xa_close_func+1 < XA_CLOSE_FUNCS)
	xa_close_func++;
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
	    mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: failed to init %s while %s\n", filename, error);
	else
	    mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: failed ot init (dlopen) %s\n", filename);
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
	mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: 'What_The' function failed in %s\n", filename);
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
	mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: not supported api revision in %s\n", filename);
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
    
    mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "dump: funcs: 0x%08x (num %d)\n", mod_hdr->funcs, mod_hdr->num_funcs);
    for (i = 0; i < mod_hdr->num_funcs; i++)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "dump(%d): %d %d [iq:0x%08x d:0x%08x]\n", i,
		func[i].what, func[i].id, func[i].iq_func, func[i].dec_func);
	if (func[i].what & XAVID_AVI_QUERY)
	{
	    mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "0x%08x: avi init/query func (id: %d)\n",
		func[i].iq_func, func[i].id);
	    codec_driver->iq_func = func[i].iq_func;
	}
	if (func[i].what & XAVID_QT_QUERY)
	{
	    mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "0x%08x: qt init/query func (id: %d)\n",
		func[i].iq_func, func[i].id);
	    codec_driver->iq_func = func[i].iq_func;
	}
	if (func[i].what & XAVID_DEC_FUNC)
	{
	    mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "0x%08x: decoder func (init/query: 0x%08x) (id: %d)\n",
		func[i].dec_func, func[i].iq_func, func[i].id);
	    codec_driver->dec_func = func[i].dec_func;
	}
    }
    return(1);
}

int xacodec_query(xacodec_driver_t *codec_driver, XA_CODEC_HDR *codec_hdr)
{
    long codec_ret;

    codec_ret = codec_driver->iq_func(codec_hdr);
    switch(codec_ret)
    {
	case CODEC_SUPPORTED:
	    codec_driver->dec_func = codec_hdr->decoder;
	    mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "Codec is supported: found decoder for %s at 0x%08x\n",
		codec_hdr->description, codec_hdr->decoder);
	    return(1);
	case CODEC_UNSUPPORTED:
	    mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "Codec is unsupported by driver\n");
	    return(0);
	case CODEC_UNKNOWN:
	default:
	    mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "Codec is unknown by driver\n");
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
	return(0);

    xacodec_driver->iq_func = NULL;
    xacodec_driver->dec_func = NULL;

    for (i=0; i < XA_CLOSE_FUNCS; i++)
	xacodec_driver->close_func[i] = NULL;

    snprintf(dll, 1024, "%s/%s", xacodec_def_path, vidinfo->codec->dll);
    if (xacodec_init(dll, xacodec_driver) == 0)
	return(0);

    codec_hdr.anim_hdr = malloc(4096);
    codec_hdr.description = vidinfo->codec->info;
    codec_hdr.compression = bswap_32(vidinfo->format);
    codec_hdr.decoder = NULL;
    codec_hdr.avi_ctab_flag = 0;
    codec_hdr.avi_read_ext = NULL;
    codec_hdr.xapi_rev = 0x0001;
    codec_hdr.extra = NULL;
    codec_hdr.x = vidinfo->disp_w;
    codec_hdr.y = vidinfo->disp_h;

    switch(out_format)
    {
	case IMGFMT_RGB8:
	    codec_hdr.depth = 8;
	    break;
	case IMGFMT_RGB15:
	    codec_hdr.depth = 15;
	    break;
	case IMGFMT_RGB16:
	    codec_hdr.depth = 16;
	    break;
	case IMGFMT_RGB24:
	    codec_hdr.depth = 24;
	    break;
	case IMGFMT_RGB32:
	    codec_hdr.depth = 32;
	    break;
	case IMGFMT_BGR8:
	    codec_hdr.depth = 8;
	    break;
	case IMGFMT_BGR15:
	    codec_hdr.depth = 15;
	    break;
	case IMGFMT_BGR16:
	    codec_hdr.depth = 16;
	    break;
	case IMGFMT_BGR24:
	    codec_hdr.depth = 24;
	    break;
	case IMGFMT_BGR32:
	    codec_hdr.depth = 32;
	    break;
	case IMGFMT_IYUV:
	case IMGFMT_I420:
	case IMGFMT_YV12:
	    codec_hdr.depth = 12;
	    break;
	default:
	    mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: not supported image format (%s)\n",
		vo_format_name(out_format));
	    return(0);
    }
    mp_msg(MSGT_DECVIDEO, MSGL_INFO, "xacodec: querying for %dx%d %dbit [fourcc: %4x] (%s)...\n",
	codec_hdr.x, codec_hdr.y, codec_hdr.depth, codec_hdr.compression,
	codec_hdr.description);

    if (xacodec_query(xacodec_driver, &codec_hdr) == 0)
	return(0);

//    free(codec_hdr.anim_hdr);

    xacodec_driver->decinfo = malloc(sizeof(XA_DEC_INFO));
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

//    vidinfo->our_out_buffer = malloc(codec_hdr.y * codec_hdr.x * ((codec_hdr.depth+7)/8));
//    vidinfo->our_out_buffer = malloc(codec_hdr.y * codec_hdr.x * codec_hdr.depth);
    xacodec_driver->image.out_fmt=out_format;
    xacodec_driver->image.bpp=codec_hdr.depth;
    xacodec_driver->image.width=codec_hdr.x;
    xacodec_driver->image.height=codec_hdr.y;
    xacodec_driver->image.mem=malloc(codec_hdr.y * codec_hdr.x * ((codec_hdr.depth+7)/8));

//    printf("out_buf size: %d\n", codec_hdr.y * codec_hdr.x * codec_hdr.depth);

#if 0
    if (vidinfo->our_out_buffer == NULL)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_ERR, "cannot allocate memory for output: %s",
	    strerror(errno));
	return(0);
    }
#endif

    mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "extra: %08x - %dx%d %dbit\n", codec_hdr.extra,
	codec_hdr.x, codec_hdr.y, codec_hdr.depth);
    return(1);
}

#define ACT_DLT_NORM	0x00000000
#define ACT_DLT_BODY	0x00000001
#define ACT_DLT_XOR	0x00000002
#define ACT_DLT_NOP	0x00000004
#define ACT_DLT_MAPD	0x00000008
#define ACT_DLT_DROP	0x00000010
#define ACT_DLT_BAD	0x80000000

//    unsigned int (*dec_func)(unsigned char *image, unsigned char *delta,
//	unsigned int dsize, XA_DEC_INFO *dec_info);

xacodec_image_t* xacodec_decode_frame(uint8_t *frame, int frame_size, int skip_flag)
{
    unsigned int ret;
    int i;

    if (skip_flag > 0)
	printf("frame will be dropped..\n");

    xacodec_driver->decinfo->skip_flag = skip_flag;

//    printf("frame: %08x (size: %d) - dest: %08x\n", frame, frame_size, dest);

    ret = xacodec_driver->dec_func((uint8_t*)&xacodec_driver->image, frame, frame_size, xacodec_driver->decinfo);

//    printf("ret: %lu : ", ret);
    
#if 0
    for (i = 0; i < 10; i++)
    {
	if (frame[i] != dest[i])
	    printf("%d: [%02x] != [%02x] ", i, frame[i], dest[i]);
	else
	    printf("%d: [%02x] ", frame[i]);
    }
#endif

    if (ret == ACT_DLT_NORM)
    {
	printf("norm\n");
	return &xacodec_driver->image;
    }

/*
    if (!(ret & ACT_DLT_MAPD))
	xacodec_driver->decinfo->map_flag = 0;
    else
    {
	xacodec_driver->decinfo->map_flag = 1;
	xacodec_driver->decinfo->map = ...
    }
*/

    if (ret & ACT_DLT_NOP)
    {
	printf("nop\n");
	return NULL; /* dst = 0 */
    }

    if (ret & ACT_DLT_DROP) /* by skip frames and errors */
    {
	printf("drop\n");
	return NULL;
    }


    if (ret & ACT_DLT_BAD)
    {
	printf("bad\n");
	return NULL;
    }

    if (ret & ACT_DLT_BODY)
    {
	printf("body\n");
	return NULL;
    }
    
    if (ret & ACT_DLT_XOR)
    {
	printf("xor\n");
	return &xacodec_driver->image;
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
//    if (xacodec_driver->close_func)
//	xacodec_driver->close_func();
    dlclose(xacodec_driver->file_handler);
    if (xacodec_driver->decinfo != NULL)
	free(xacodec_driver->decinfo);
    if (xacodec_driver != NULL)
	free(xacodec_driver);
    return(TRUE);
}


/* *** XANIM SHIT *** */
/* like loader/win32.c - mini XANIM library */


unsigned long XA_Time_Read()
{
    return GetTimer(); //(GetRelativeTime());
}

void XA_dummy()
{
    XA_Print("dummy() called");
}

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

#define ip_OUT_2x2_1BLK(ip, CAST, cmap2x2, rinc) { register CAST d0, d1; \
 *ip++ = d0 = (CAST)(cmap2x2->clr0_0); *ip++ = d0; \
 *ip++ = d1 = (CAST)(cmap2x2->clr1_0); *ip = d1; \
  ip += rinc; \
 *ip++ = d0; *ip++ = d0; *ip++ = d1; *ip = d1; ip += rinc; \
 *ip++ = d0 = (CAST)(cmap2x2->clr2_0); *ip++ = d0; \
 *ip++ = d1 = (CAST)(cmap2x2->clr3_0); *ip = d1; \
  ip += rinc; *ip++ = d0; *ip++ = d0; *ip++ = d1; *ip++ = d1; }

#define ip_OUT_2x2_2BLKS(ip, CAST, c2x2map0, c2x2map1, rinc) { \
 *ip++ = (CAST)(c2x2map0->clr0_0); \
 *ip++ = (CAST)(c2x2map0->clr1_0); \
 *ip++ = (CAST)(c2x2map1->clr0_0); \
 *ip   = (CAST)(c2x2map1->clr1_0); ip += rinc; \
 *ip++ = (CAST)(c2x2map0->clr2_0); \
 *ip++ = (CAST)(c2x2map0->clr3_0); \
 *ip++ = (CAST)(c2x2map1->clr2_0); \
 *ip   = (CAST)(c2x2map1->clr3_0); }

void XA_2x2_OUT_1BLK_clr8(unsigned char *image, unsigned int x, unsigned int y,
    unsigned int imagex, XA_2x2_Color *cmap2x2)
{
    unsigned int row_inc = imagex - 3;
    unsigned char *ip = (unsigned char *)(image + 4*(y * imagex + x));

//    XA_Print("XA_2x2_OUT_1BLK_clr8('image: %08x', 'x: %d', 'y: %d', 'imagex: %d', 'cmap2x2: %08x')",
//	image, x, y, imagex, cmap2x2);
    //ip_OUT_2x2_1BLK(ip, unsigned char, cmap2x2, row_inc);
    
    // simplified yv12->rgb32bpp converter (Y only)
    ip[0]=ip[1]=ip[2]=cmap2x2->clr0_0;
    ip[4]=ip[5]=ip[6]=cmap2x2->clr1_0;
    ip+=4*imagex;
    ip[0]=ip[1]=ip[2]=cmap2x2->clr2_0;
    ip[4]=ip[5]=ip[6]=cmap2x2->clr3_0;
    
}

void XA_2x2_OUT_4BLKS_clr8(unsigned char *image, unsigned int x, unsigned int y,
    unsigned int imagex, XA_2x2_Color *cm0, XA_2x2_Color *cm1, XA_2x2_Color *cm2,
    XA_2x2_Color *cm3)
{
/*
    unsigned int row_inc = imagex - 3;
    unsigned char *ip = (unsigned char *)(image + y * imagex + x);

    XA_Print("XA_2x2_OUT_4BLKS_clr8('image: %08x', 'x: %d', 'y: %d', 'imagex: %d', 'cm0: %08x', 'cm1: %08x', 'cm2: %08x', 'cm3: %08x')",
	image, x, y, imagex, cm0, cm1, cm2, cm3);
    ip_OUT_2x2_2BLKS(ip, unsigned char, cm0, cm1, row_inc);
    ip += row_inc;
    ip_OUT_2x2_2BLKS(ip, unsigned char, cm2, cm3, row_inc);
*/

XA_2x2_OUT_1BLK_clr8(image,x,y,imagex,cm0);
XA_2x2_OUT_1BLK_clr8(image,x+2,y,imagex,cm1);
XA_2x2_OUT_1BLK_clr8(image,x,y+2,imagex,cm2);
XA_2x2_OUT_1BLK_clr8(image,x+2,y+2,imagex,cm3);

}


void *YUV2x2_Blk_Func(unsigned int image_type, int blks, unsigned int dith_flag)
{
    void (*color_func)();

//    XA_Print("YUV2x2_Blk_Func('image_type: %d', 'blks: %d', 'dith_flag: %d')",
//	    image_type, blks, dith_flag);

    if (blks == 1)
    {
	switch(image_type)
	{
	    default:
		color_func = XA_2x2_OUT_1BLK_clr8;
		break;
	}
    }
    else if(blks == 4)
    {
	switch(image_type)
	{
	    default:
		color_func = XA_2x2_OUT_4BLKS_clr8;
		break;
	}
    }
    else printf("Ajajj!\n");

//    XA_Print("YUV2x2_Blk_Func -> %08x", color_func);

    return((void *)color_func);
}


/*****************************************************************************
 * Take Four Y's and UV and put them into a 2x2 Color structure.
 * Convert to display clr.
 ******************/

void XA_YUV_2x2_clr(cmap2x2,Y0,Y1,Y2,Y3,U,V,map_flag,map,chdr)
XA_2x2_Color *cmap2x2;
xaULONG Y0,Y1,Y2,Y3,U,V;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaLONG cr,cg,cb; xaULONG r,g,b;

//  printf("XA_YUV_2x2_clr(%p [%d,%d,%d,%d][%d][%d] %d %p %p)\n",
//          cmap2x2,Y0,Y1,Y2,Y3,U,V,map_flag,map,chdr);

  cmap2x2->clr0_0=Y0;
  cmap2x2->clr1_0=Y1;
  cmap2x2->clr2_0=Y2;
  cmap2x2->clr3_0=Y3;
  cmap2x2->clr0_1=U;
  cmap2x2->clr0_2=V;

//  cmap2x2->clr0_0 = 0x1111;
//  cmap2x2->clr1_0 = 0x3535;
//  cmap2x2->clr2_0 = 0x7272;
//  cmap2x2->clr3_0 = 0xbfbf;

/*
  xaUBYTE *rl = xa_byte_limit;

  cr = YUV2_VR_tab[V];
  cb = YUV2_UB_tab[U];
  cg = YUV2_UG_tab[U] + YUV2_VG_tab[V];

  YUV_TO_RGB(Y0,cr,cg,cb,rl,r,g,b);
  cmap2x2->clr0_0 = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
  YUV_TO_RGB(Y1,cr,cg,cb,rl,r,g,b);
  cmap2x2->clr1_0 = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
  YUV_TO_RGB(Y2,cr,cg,cb,rl,r,g,b);
  cmap2x2->clr2_0 = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
  YUV_TO_RGB(Y3,cr,cg,cb,rl,r,g,b);
  cmap2x2->clr3_0 = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
*/
}



void *YUV2x2_Map_Func(unsigned int image_type, unsigned int dith_type)
{
//    XA_Print("YUV2x2_Map_Func('image_type: %d', 'dith_type: %d')",
//	    image_type, dith_type);
    if(image_type!=0)printf("izeeeeee!\n");
    return (void*)XA_YUV_2x2_clr;
}


int XA_Gen_YUV_Tabs(XA_ANIM_HDR *anim_hdr)
{
    XA_Print("XA_Gen_YUV_Tabs('anim_hdr: %08x')", anim_hdr);

//    XA_Print("anim type: %d - img[x: %d, y: %d, c: %d, d: %d]",
//	anim_hdr->anim_type, anim_hdr->imagex, anim_hdr->imagey,
//	anim_hdr->imagec, anim_hdr->imaged);
}

void JPG_Setup_Samp_Limit_Table(XA_ANIM_HDR *anim_hdr)
{
    XA_Print("JPG_Setup_Samp_Limit_Table('anim_hdr: %08x')", anim_hdr);
//    xa_byte_limit = jpg_samp_limit + (MAXJSAMPLE + 1);
}

void JPG_Alloc_MCU_Bufs(XA_ANIM_HDR *anim_hdr, unsigned int width,
	unsigned int height, unsigned int full_flag)
{
    XA_Print("JPG_Alloc_MCU_Bufs('anim_hdr: %08x', 'width: %d', 'height: %d', 'full_flag: %d')",
	    anim_hdr, width, height, full_flag);
}

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

#define XA_IMTYPE_RGB	0x0001

void XA_YUV1611_To_RGB(unsigned char *image, unsigned int imagex, unsigned int imagey,
    unsigned int i_x, unsigned int i_y, YUVBufs *yuv_bufs, YUVTabs *yuv_tabs,
    unsigned int map_flag, unsigned int *map, XA_CHDR *chdr)
{
    XA_Print("XA_YUV1611_To_RGB('image: %08x', 'imagex: %d', 'imagey: %d', 'i_x: %d', 'i_y: %d', 'yuv_bufs: %08x', 'yuv_tabs: %08x', 'map_flag: %d', 'map: %08x', 'chdr: %08x')",
	image, imagex, imagey, i_x, i_y, yuv_bufs, yuv_tabs, map_flag, map, chdr);
}

void XA_YUV1611_To_CLR8(unsigned char *image, unsigned int imagex, unsigned int imagey,
    unsigned int i_x, unsigned int i_y, YUVBufs *yuv_bufs, YUVTabs *yuv_tabs,
    unsigned int map_flag, unsigned int *map, XA_CHDR *chdr)
{
    XA_Print("XA_YUV1611_To_CLR8('image: %08x', 'imagex: %d', 'imagey: %d', 'i_x: %d', 'i_y: %d', 'yuv_bufs: %08x', 'yuv_tabs: %08x', 'map_flag: %d', 'map: %08x', 'chdr: %08x')",
	image, imagex, imagey, i_x, i_y, yuv_bufs, yuv_tabs, map_flag, map, chdr);
}

void *XA_YUV1611_Func(unsigned int image_type)
{
    void (*color_func)();

    XA_Print("XA_YUV1611_Func('image_type: %d')", image_type);

    switch(image_type)
    {
	case XA_IMTYPE_RGB:
	    color_func = XA_YUV1611_To_RGB;
	    break;
	default:
	    color_func = XA_YUV1611_To_CLR8;
	    break;
    }

    return((void *)color_func);
}

/* YUV 41 11 11 routines */
void XA_YUV411111_To_RGB(unsigned char *image, unsigned int imagex, unsigned int imagey,
    unsigned int i_x, unsigned int i_y, YUVBufs *yuv_bufs, YUVTabs *yuv_tabs,
    unsigned int map_flag, unsigned int *map, XA_CHDR *chdr)
{
    XA_Print("XA_YUV411111_To_RGB('image: %d', 'imagex: %d', 'imagey: %d', 'i_x: %d', 'i_y: %d', 'yuv_bufs: %08x', 'yuv_tabs: %08x', 'map_flag: %d', 'map: %08x', 'chdr: %08x')",
	    image, imagex, imagey, i_x, i_y, yuv_bufs, yuv_tabs, map_flag, map, chdr);
}


void *XA_YUV411111_Func(unsigned int image_type)
{
    void (*color_func)();

    XA_Print("XA_YUV411111_Func('image_type: %d')", image_type);    

    switch(image_type)
    {
	case XA_IMTYPE_RGB:	color_func = XA_YUV411111_To_RGB;	break;
    }
    
    return((void *)color_func);
}

void XA_YUV221111_To_CLR8(image_p,imagex,imagey,i_x,i_y,yuv,yuv_tabs,map_flag,map,chdr)
xaUBYTE *image_p;		xaULONG imagex,imagey,i_x,i_y;
YUVBufs *yuv;	YUVTabs *yuv_tabs;
xaULONG map_flag,*map;	XA_CHDR *chdr;
{
    xacodec_image_t *image=(xacodec_image_t*)image_p;

#if 0
    printf("XA_YUV221111_To_CLR8(%p  %dx%d %d;%d [%dx%d]  %p %p %d %p %p)\n",
	image,imagex,imagey,i_x,i_y, image->width,image->height,
	yuv,yuv_tabs,map_flag,map,chdr);

    printf("YUV: %p %p %p %X (%X) %Xx%X %Xx%X\n",
	yuv->Ybuf,yuv->Ubuf,yuv->Vbuf,yuv->the_buf,yuv->the_buf_size,
	yuv->y_w,yuv->y_h,yuv->uv_w,yuv->uv_h);
#endif

if(imagex==image->width && imagey==image->height){
//    printf("Direct render!!!\n");
    image->planes[0]=yuv->Ybuf;
    if(image->out_fmt==IMGFMT_YV12){
	image->planes[1]=yuv->Ubuf;
	image->planes[2]=yuv->Vbuf;
    } else {
	image->planes[1]=yuv->Vbuf;
	image->planes[2]=yuv->Ubuf;
    }
    image->stride[0]=imagex; // yuv->y_w
    image->stride[1]=image->stride[2]=imagex/2; // yuv->uv_w
} else {
    int y;
    printf("partial YV12 not implemented!!!!!!\n");
//    image->planes[0]=image->mem;
//    image->planes[1]=image->planes[0]+image->width*image->height;
//    image->planes[2]=image->planes[1]+image->width*image->height/4;
}

//    memcpy(image,yuv->Ybuf,imagex*imagey);
//    memcpy(image+imagex*imagey,yuv->Vbuf,imagex*imagey/4);
//    memcpy(image+imagex*imagey*5/4,yuv->Ubuf,imagex*imagey/4);

/*
    unsigned char *Ybuf;
    unsigned char *Ubuf;
    unsigned char *Vbuf;
    unsigned char *the_buf;
    unsigned int the_buf_size;
    unsigned short y_w, y_h;
    unsigned short uv_w, uv_h;
*/

}

/* YUV 22 11 11 routines */
void *XA_YUV221111_Func(unsigned int image_type)
{
    XA_Print("XA_YUV221111_Func('image_type: %d')", image_type);    
    return((void *)XA_YUV221111_To_CLR8);
}

YUVBufs jpg_YUVBufs;
YUVTabs def_yuv_tabs;

/* *** EOF XANIM SHIT *** */
