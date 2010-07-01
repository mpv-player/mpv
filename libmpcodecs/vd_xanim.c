/*
 * XAnim Video Codec DLL support
 *
 * It partly emulates the Xanim codebase.
 * You need the -rdynamic flag to use this with gcc.
 *
 * Copyright (C) 2001-2002 Alex Beregszaszi
 *                         Arpad Gereoffy <arpi@thot.banki.hu>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* strerror */

#include "config.h"
#include "path.h"
#include "mp_msg.h"

#include "vd_internal.h"

static const vd_info_t info = {
	"XAnim codecs",
	"xanim",
	"A'rpi & Alex",
	"Xanim (http://xanim.va.pubnix.com/)",
	"binary codec plugins"
};

LIBVD_EXTERN(xanim)

#ifdef __FreeBSD__
#include <unistd.h>
#endif

#include <dlfcn.h> /* dlsym, dlopen, dlclose */
#include <stdarg.h> /* va_alist, va_start, va_end */
#include <errno.h> /* strerror, errno */

#include "mp_msg.h"
#include "mpbswap.h"

#include "osdep/timer.h"

#if 0
/* this should be removed */
#ifndef RTLD_NOW
#define RLTD_NOW 2
#endif
#ifndef RTLD_LAZY
#define RLTD_LAZY 1
#endif
#ifndef RTLD_GLOBAL
#define RLTD_GLOBAL 256
#endif
#endif

struct XA_CODEC_HDR;
struct XA_DEC_INFO;

typedef long init_function(struct XA_CODEC_HDR *);
typedef unsigned int decode_function(unsigned char *, unsigned char *,
                                      unsigned int, struct XA_DEC_INFO *);

typedef struct
{
  unsigned int		what;
  unsigned int		id;
  init_function        *iq_func;    /* init/query function */
  decode_function      *dec_func;   /* opt decode function */
} XAVID_FUNC_HDR;

#define XAVID_WHAT_NO_MORE	0x0000
#define XAVID_AVI_QUERY		0x0001
#define XAVID_QT_QUERY		0x0002
#define XAVID_DEC_FUNC		0x0100

#define XAVID_API_REV		0x0003

typedef struct
{
  unsigned int		api_rev;
  char			*desc;
  char			*rev;
  char			*copyright;
  char			*mod_author;
  char			*authors;
  unsigned int		num_funcs;
  XAVID_FUNC_HDR	*funcs;
} XAVID_MOD_HDR;

/* XA CODEC .. */
typedef struct XA_CODEC_HDR
{
  void			*anim_hdr;
  unsigned int		compression;
  unsigned int		x, y;
  unsigned int		depth;
  void			*extra;
  unsigned int		xapi_rev;
  decode_function	*decoder;
  char			*description;
  unsigned int		avi_ctab_flag;
  unsigned int		(*avi_read_ext)(void);
} XA_CODEC_HDR;

#define CODEC_SUPPORTED 1
#define CODEC_UNKNOWN 0
#define CODEC_UNSUPPORTED -1

/* fuckin colormap structures for xanim */
typedef struct
{
  unsigned short	red;
  unsigned short	green;
  unsigned short	blue;
  unsigned short	gray;
} ColorReg;

typedef struct XA_ACTION_STRUCT
{
  int			type;
  int			cmap_rev;
  unsigned char		*data;
  struct XA_ACTION_STRUCT *next;
  struct XA_CHDR_STRUCT	*chdr;
  ColorReg		*h_cmap;
  unsigned int		*map;
  struct XA_ACTION_STRUCT *next_same_chdr;
} XA_ACTION;

typedef struct XA_CHDR_STRUCT
{
  unsigned int		rev;
  ColorReg		*cmap;
  unsigned int		csize, coff;
  unsigned int		*map;
  unsigned int		msize, moff;
  struct XA_CHDR_STRUCT	*next;
  XA_ACTION		*acts;
  struct XA_CHDR_STRUCT	*new_chdr;
} XA_CHDR;

typedef struct XA_DEC_INFO
{
  unsigned int		cmd;
  unsigned int		skip_flag;
  unsigned int		imagex, imagey;	/* image buffer size */
  unsigned int		imaged;		/* image depth */
  XA_CHDR		*chdr;		/* color map header */
  unsigned int		map_flag;
  unsigned int		*map;
  unsigned int		xs, ys;
  unsigned int		xe, ye;
  unsigned int		special;
  void			*extra;
} XA_DEC_INFO;

typedef struct
{
    unsigned int	file_num;
    unsigned int	anim_type;
    unsigned int	imagex;
    unsigned int	imagey;
    unsigned int	imagec;
    unsigned int	imaged;
} XA_ANIM_HDR;

typedef struct {
    XA_DEC_INFO *decinfo;
    void *file_handler;
    init_function *iq_func;
    decode_function *dec_func;
    mp_image_t *mpi;
} vd_xanim_ctx;

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

#define ACT_DLTA_NORM	0x00000000
#define ACT_DLTA_BODY	0x00000001
#define ACT_DLTA_XOR	0x00000002
#define ACT_DLTA_NOP	0x00000004
#define ACT_DLTA_MAPD	0x00000008
#define ACT_DLTA_DROP	0x00000010
#define ACT_DLTA_BAD	0x80000000

#define XA_CLOSE_FUNCS 5
int xa_close_funcs = 0;
void *xa_close_func[XA_CLOSE_FUNCS];

/* load, init and query */
static int xacodec_load(sh_video_t *sh, char *filename)
{
    vd_xanim_ctx *priv = sh->context;
    void *(*what_the)(void);
    char *error;
    XAVID_MOD_HDR *mod_hdr;
    XAVID_FUNC_HDR *func;
    int i;

//    priv->file_handler = dlopen(filename, RTLD_NOW|RTLD_GLOBAL);
    priv->file_handler = dlopen(filename, RTLD_LAZY);
    if (!priv->file_handler)
    {
	error = dlerror();
	if (error)
	    mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: failed to dlopen %s while %s\n", filename, error);
	else
	    mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: failed to dlopen %s\n", filename);
	return 0;
    }

    what_the = dlsym(priv->file_handler, "What_The");
    if ((error = dlerror()) != NULL)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: failed to init %s while %s\n", filename, error);
	dlclose(priv->file_handler);
	return 0;
    }

    mod_hdr = what_the();
    if (!mod_hdr)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: initializer function failed in %s\n", filename);
	dlclose(priv->file_handler);
	return 0;
    }

    mp_msg(MSGT_DECVIDEO, MSGL_V, "=== XAnim Codec ===\n");
    mp_msg(MSGT_DECVIDEO, MSGL_V, " Filename: %s (API revision: %x)\n", filename, mod_hdr->api_rev);
    mp_msg(MSGT_DECVIDEO, MSGL_V, " Codec: %s. Rev: %s\n", mod_hdr->desc, mod_hdr->rev);
    if (mod_hdr->copyright)
	mp_msg(MSGT_DECVIDEO, MSGL_V, " %s\n", mod_hdr->copyright);
    if (mod_hdr->mod_author)
	mp_msg(MSGT_DECVIDEO, MSGL_V, " Module Author(s): %s\n", mod_hdr->mod_author);
    if (mod_hdr->authors)
	mp_msg(MSGT_DECVIDEO, MSGL_V, " Codec Author(s): %s\n", mod_hdr->authors);

    if (mod_hdr->api_rev > XAVID_API_REV)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: not supported api revision (%d) in %s\n",
	    mod_hdr->api_rev, filename);
	dlclose(priv->file_handler);
	return 0;
    }

    func = mod_hdr->funcs;
    if (!func)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: function table error in %s\n", filename);
	dlclose(priv->file_handler);
	return 0;
    }

    mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "Exported functions by codec: [functable: %p entries: %d]\n",
	mod_hdr->funcs, mod_hdr->num_funcs);
    for (i = 0; i < (int)mod_hdr->num_funcs; i++)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_DBG2, " %d: %d %d [iq:%p d:%p]\n",
		i, func[i].what, func[i].id, func[i].iq_func, func[i].dec_func);
	if (func[i].what & XAVID_AVI_QUERY)
	{
	    mp_msg(MSGT_DECVIDEO, MSGL_DBG2, " %p: avi init/query func (id: %d)\n",
		func[i].iq_func, func[i].id);
	    priv->iq_func = func[i].iq_func;
	}
	if (func[i].what & XAVID_QT_QUERY)
	{
	    mp_msg(MSGT_DECVIDEO, MSGL_DBG2, " %p: qt init/query func (id: %d)\n",
		func[i].iq_func, func[i].id);
	    priv->iq_func = func[i].iq_func;
	}
	if (func[i].what & XAVID_DEC_FUNC)
	{
	    mp_msg(MSGT_DECVIDEO, MSGL_DBG2, " %p: decoder func (init/query: %p) (id: %d)\n",
		func[i].dec_func, func[i].iq_func, func[i].id);
	    priv->dec_func = func[i].dec_func;
	}
    }
    return 1;
}

static int xacodec_query(sh_video_t *sh, XA_CODEC_HDR *codec_hdr)
{
    vd_xanim_ctx *priv = sh->context;
    long ret;

#if 0
    /* the brute one */
    if (priv->dec_func)
    {
	codec_hdr->decoder = priv->dec_func;
	mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "We got decoder's address at init! %p\n", codec_hdr->decoder);
	return 1;
    }
#endif

    ret = priv->iq_func(codec_hdr);
    switch(ret)
    {
	case CODEC_SUPPORTED:
	    priv->dec_func = codec_hdr->decoder;
	    mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "Codec is supported: found decoder for %s at %p\n",
		codec_hdr->description, codec_hdr->decoder);
	    return 1;
	case CODEC_UNSUPPORTED:
	    mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "Codec (%s) is unsupported by dll\n",
		codec_hdr->description);
	    return 0;
	case CODEC_UNKNOWN:
	default:
	    mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "Codec (%s) is unknown by dll\n",
		codec_hdr->description);
	    return 0;
    }
}

/* These functions are required for loading XAnim binary libs.
 * Add forward declarations to avoid warnings with -Wmissing-prototypes. */
void XA_Print(char *fmt, ...);
void TheEnd1(char *err_mess);
void XA_Add_Func_To_Free_Chain(XA_ANIM_HDR *anim_hdr, void (*function)(void));
unsigned long XA_Time_Read(void);
void XA_Gen_YUV_Tabs(XA_ANIM_HDR *anim_hdr);
void JPG_Setup_Samp_Limit_Table(XA_ANIM_HDR *anim_hdr);
void JPG_Alloc_MCU_Bufs(XA_ANIM_HDR *anim_hdr, unsigned int width,
                        unsigned int height, unsigned int full_flag);
void *YUV2x2_Blk_Func(unsigned int image_type, int blks,
                      unsigned int dith_flag);
void *YUV2x2_Map_Func(unsigned int image_type, unsigned int dith_type);
void *XA_YUV1611_Func(unsigned int image_type);
void *XA_YUV221111_Func(unsigned int image_type);

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
    /* we should exit here... */

    return;
}

void XA_Add_Func_To_Free_Chain(XA_ANIM_HDR *anim_hdr, void (*function)(void))
{
//    XA_Print("XA_Add_Func_To_Free_Chain('anim_hdr: %08x', 'function: %08x')",
//	    anim_hdr, function);
    xa_close_func[xa_close_funcs] = function;
    if (xa_close_funcs+1 < XA_CLOSE_FUNCS)
	xa_close_funcs++;

    return;
}

unsigned long XA_Time_Read(void)
{
    return GetTimer(); //(GetRelativeTime());
}

static void XA_dummy(void)
{
    XA_Print("dummy() called");
}

void XA_Gen_YUV_Tabs(XA_ANIM_HDR *anim_hdr)
{
    XA_Print("XA_Gen_YUV_Tabs('anim_hdr: %08x')", anim_hdr);
    return;
}

void JPG_Setup_Samp_Limit_Table(XA_ANIM_HDR *anim_hdr)
{
    XA_Print("JPG_Setup_Samp_Limit_Table('anim_hdr: %08x')", anim_hdr);
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

static void XA_2x2_OUT_1BLK_Convert(unsigned char *image_p, unsigned int x, unsigned int y,
    unsigned int imagex, XA_2x2_Color *cmap2x2)
{
    mp_image_t *mpi = (mp_image_t *)image_p;

#if 0
    SET_4_YUV_PIXELS(mpi,x,y,cmap2x2)
#else
    SET_4_YUV_PIXELS(mpi,x,y,cmap2x2)
    SET_4_YUV_PIXELS(mpi,x+2,y,cmap2x2)
    SET_4_YUV_PIXELS(mpi,x,y+2,cmap2x2)
    SET_4_YUV_PIXELS(mpi,x+2,y+2,cmap2x2)
#endif

    return;
}

static void XA_2x2_OUT_4BLKS_Convert(unsigned char *image_p, unsigned int x, unsigned int y,
    unsigned int imagex, XA_2x2_Color *cm0, XA_2x2_Color *cm1, XA_2x2_Color *cm2,
    XA_2x2_Color *cm3)
{
    mp_image_t *mpi = (mp_image_t *)image_p;

    SET_4_YUV_PIXELS(mpi,x,y,cm0)
    SET_4_YUV_PIXELS(mpi,x+2,y,cm1)
    SET_4_YUV_PIXELS(mpi,x,y+2,cm2)
    SET_4_YUV_PIXELS(mpi,x+2,y+2,cm3)
    return;
}

void *YUV2x2_Blk_Func(unsigned int image_type, int blks, unsigned int dith_flag)
{
    mp_dbg(MSGT_DECVIDEO,MSGL_DBG2, "YUV2x2_Blk_Func(image_type=%d, blks=%d, dith_flag=%d)\n",
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

static void XA_YUV_2x2_clr(XA_2x2_Color *cmap2x2, unsigned int Y0, unsigned int Y1,
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
    mp_dbg(MSGT_DECVIDEO,MSGL_DBG2, "YUV2x2_Map_Func('image_type: %d', 'dith_type: %d')",
	    image_type, dith_type);
    return (void*)XA_YUV_2x2_clr;
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

static void XA_YUV1611_Convert(unsigned char *image_p, unsigned int imagex, unsigned int imagey,
    unsigned int i_x, unsigned int i_y, YUVBufs *yuv, YUVTabs *yuv_tabs,
    unsigned int map_flag, unsigned int *map, XA_CHDR *chdr)
{
    sh_video_t *sh = (sh_video_t*)image_p;
    vd_xanim_ctx *priv = sh->context;
    mp_image_t *mpi;
    int y;
    int ystride=(yuv->y_w)?yuv->y_w:imagex;
    int uvstride=(yuv->uv_w)?yuv->uv_w:(imagex/4);

    mp_dbg(MSGT_DECVIDEO,MSGL_DBG3, "YUVTabs:  %d %p %p %p %p %p\n",yuv_tabs->Uskip_mask,
	yuv_tabs->YUV_Y_tab,
	yuv_tabs->YUV_UB_tab,
	yuv_tabs->YUV_VR_tab,
	yuv_tabs->YUV_UG_tab,
	yuv_tabs->YUV_VG_tab );

    mp_dbg(MSGT_DECVIDEO,MSGL_DBG3, "XA_YUV1611_Convert('image: %08x', 'imagex: %d', 'imagey: %d', 'i_x: %d', 'i_y: %d', 'yuv_bufs: %08x', 'yuv_tabs: %08x', 'map_flag: %d', 'map: %08x', 'chdr: %08x')",
	image_p, imagex, imagey, i_x, i_y, yuv, yuv_tabs, map_flag, map, chdr);

    mp_dbg(MSGT_DECVIDEO,MSGL_DBG3, "YUV: %p %p %p %X (%d) %dx%d %dx%d\n",
	yuv->Ybuf,yuv->Ubuf,yuv->Vbuf,yuv->the_buf,yuv->the_buf_size,
	yuv->y_w,yuv->y_h,yuv->uv_w,yuv->uv_h);

    if(!yuv_tabs->YUV_Y_tab){
	// standard YVU9 - simply export it!
	mpi = mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, 0,
	    sh->disp_w, sh->disp_h);
	priv->mpi=mpi; if(!mpi) return; // ERROR!
	mpi->planes[0]=yuv->Ybuf;
	mpi->planes[1]=yuv->Ubuf;
	mpi->planes[2]=yuv->Vbuf;
	mpi->width=imagex;
	mpi->stride[0]=ystride; //i_x; // yuv->y_w
	mpi->stride[1]=mpi->stride[2]=uvstride; //i_x/4; // yuv->uv_w
	return;
    }

    // allocate TEMP buffer and convert the image:
    mpi = mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
	sh->disp_w, sh->disp_h);
    priv->mpi=mpi; if(!mpi) return; // ERROR!

    // convert the Y plane:
    for(y=0;y<(int)imagey;y++){
	unsigned int x;
	unsigned char* s=yuv->Ybuf+ystride*y;
	unsigned char* d=mpi->planes[0]+mpi->stride[0]*y;
	for(x=0;x<imagex;x++) d[x]=s[x]<<1;
    }

    imagex>>=2;
    imagey>>=2;

    // convert the U plane:
    for(y=0;y<(int)imagey;y++){
	unsigned int x;
	unsigned char* s=yuv->Ubuf+uvstride*y;
	unsigned char* d=mpi->planes[1]+mpi->stride[1]*y;
	for(x=0;x<imagex;x++) d[x]=s[x]<<1;
    }

    // convert the V plane:
    for(y=0;y<(int)imagey;y++){
	unsigned int x;
	unsigned char* s=yuv->Vbuf+uvstride*y;
	unsigned char* d=mpi->planes[2]+mpi->stride[2]*y;
	for(x=0;x<imagex;x++) d[x]=s[x]<<1;
    }
}

void *XA_YUV1611_Func(unsigned int image_type)
{
    mp_dbg(MSGT_DECVIDEO,MSGL_DBG2, "XA_YUV1611_Func('image_type: %d')", image_type);
    return (void *)XA_YUV1611_Convert;
}

/* --------------- YUV 2x2 1x1 1x1 (4:2:0 aka YV12) [3ivX,H263] ------------ */

static void XA_YUV221111_Convert(unsigned char *image_p, unsigned int imagex, unsigned int imagey,
    unsigned int i_x, unsigned int i_y, YUVBufs *yuv, YUVTabs *yuv_tabs, unsigned int map_flag,
    unsigned int *map, XA_CHDR *chdr)
{
    sh_video_t *sh = (sh_video_t*)image_p;
    vd_xanim_ctx *priv = sh->context;
    mp_image_t *mpi;
    // note: 3ivX codec doesn't set y_w, uv_w, they are random junk :(
    int ystride=imagex; //(yuv->y_w)?yuv->y_w:imagex;
    int uvstride=imagex/2; //(yuv->uv_w)?yuv->uv_w:(imagex/2);

    mp_dbg(MSGT_DECVIDEO,MSGL_DBG3, "XA_YUV221111_Convert(%p  %dx%d %d;%d [%dx%d]  %p %p %d %p %p)\n",
	image_p,imagex,imagey,i_x,i_y, sh->disp_w, sh->disp_h,
	yuv,yuv_tabs,map_flag,map,chdr);

    mp_dbg(MSGT_DECVIDEO,MSGL_DBG3, "YUV: %p %p %p %X (%X) %Xx%X %Xx%X\n",
	yuv->Ybuf,yuv->Ubuf,yuv->Vbuf,yuv->the_buf,yuv->the_buf_size,
	yuv->y_w,yuv->y_h,yuv->uv_w,yuv->uv_h);

    // standard YV12 - simply export it!
    mpi = mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, 0, sh->disp_w, sh->disp_h);
    priv->mpi=mpi; if(!mpi) return; // ERROR!
    mpi->planes[0]=yuv->Ybuf;
    mpi->planes[1]=yuv->Ubuf;
    mpi->planes[2]=yuv->Vbuf;
    mpi->width=imagex;
    mpi->stride[0]=ystride; //i_x; // yuv->y_w
    mpi->stride[1]=mpi->stride[2]=uvstride;  //=i_x/4; // yuv->uv_w
}

void *XA_YUV221111_Func(unsigned int image_type)
{
    mp_dbg(MSGT_DECVIDEO,MSGL_DBG2, "XA_YUV221111_Func('image_type: %d')\n",image_type);
    return (void *)XA_YUV221111_Convert;
}

/* *** EOF XANIM *** */

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh)
{
    vd_xanim_ctx *priv;
    char dll[1024];
    XA_CODEC_HDR codec_hdr;
    int i;

    priv = malloc(sizeof(vd_xanim_ctx));
    if (!priv)
	return 0;
    sh->context = priv;
    memset(priv, 0, sizeof(vd_xanim_ctx));

    if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YV12)) return 0;

    priv->iq_func = NULL;
    priv->dec_func = NULL;

    for (i=0; i < XA_CLOSE_FUNCS; i++)
	xa_close_func[i] = NULL;

    snprintf(dll, 1024, "%s/%s", codec_path, sh->codec->dll);
    if (xacodec_load(sh, dll) == 0)
	return 0;

    codec_hdr.xapi_rev = XAVID_API_REV;
    codec_hdr.anim_hdr = malloc(4096);
    codec_hdr.description = sh->codec->info;
    codec_hdr.compression = bswap_32(sh->bih->biCompression);
    codec_hdr.decoder = NULL;
    codec_hdr.x = sh->bih->biWidth; /* ->disp_w */
    codec_hdr.y = sh->bih->biHeight; /* ->disp_h */
    /* extra fields to store palette */
    codec_hdr.avi_ctab_flag = 0;
    codec_hdr.avi_read_ext = NULL;
    codec_hdr.extra = NULL;

    switch(sh->codec->outfmt[sh->outfmtidx])
    {
	case IMGFMT_BGR32:
	    codec_hdr.depth = 32;
	    break;
	case IMGFMT_BGR24:
	    codec_hdr.depth = 24;
	    break;
	case IMGFMT_IYUV:
	case IMGFMT_I420:
	case IMGFMT_YV12:
	    codec_hdr.depth = 12;
	    break;
	case IMGFMT_YVU9:
	    codec_hdr.depth = 9;
	    break;
	default:
	    mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: not supported image out format (%s)\n",
		vo_format_name(sh->codec->outfmt[sh->outfmtidx]));
	    return 0;
    }
    mp_msg(MSGT_DECVIDEO, MSGL_INFO, "xacodec: querying for input %dx%d %dbit [fourcc: %4x] (%s)...\n",
	codec_hdr.x, codec_hdr.y, codec_hdr.depth, codec_hdr.compression, codec_hdr.description);

    if (xacodec_query(sh, &codec_hdr) == 0)
	return 0;

//    free(codec_hdr.anim_hdr);

    priv->decinfo = malloc(sizeof(XA_DEC_INFO));
    if (priv->decinfo == NULL)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "xacodec: memory allocation error: %s\n",
	    strerror(errno));
	return 0;
    }
    priv->decinfo->cmd = 0;
    priv->decinfo->skip_flag = 0;
    priv->decinfo->imagex = priv->decinfo->xe = codec_hdr.x;
    priv->decinfo->imagey = priv->decinfo->ye = codec_hdr.y;
    priv->decinfo->imaged = codec_hdr.depth;
    priv->decinfo->chdr = NULL;
    priv->decinfo->map_flag = 0; /* xaFALSE */
    priv->decinfo->map = NULL;
    priv->decinfo->xs = priv->decinfo->ys = 0;
    priv->decinfo->special = 0;
    priv->decinfo->extra = codec_hdr.extra;
    mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "decinfo->extra, filled by codec: %p [%s]\n",
	&priv->decinfo->extra, (char *)priv->decinfo->extra);

    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh)
{
    vd_xanim_ctx *priv = sh->context;
    int i;
    void (*close_func)(void);

    for (i=0; i < XA_CLOSE_FUNCS; i++)
	if (xa_close_func[i])
	{
	    close_func = xa_close_func[i];
	    close_func();
	}
    dlclose(priv->file_handler);
    if (priv->decinfo != NULL)
	free(priv->decinfo);
    free(priv);
}

//    unsigned int (*dec_func)(unsigned char *image, unsigned char *delta,
//	unsigned int dsize, XA_DEC_INFO *dec_info);

// decode a frame
static mp_image_t* decode(sh_video_t *sh, void *data, int len, int flags)
{
    vd_xanim_ctx *priv = sh->context;
    unsigned int ret;

    if (len <= 0)
	return NULL; // skipped frame

    priv->decinfo->skip_flag = (flags&3)?1:0;

    if(sh->codec->outflags[sh->outfmtidx] & CODECS_FLAG_STATIC){
	// allocate static buffer for cvid-like codecs:
	priv->mpi = mpcodecs_get_image(sh, MP_IMGTYPE_STATIC,
	    MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_PREFER_ALIGNED_STRIDE,
	    (sh->disp_w+3)&(~3), (sh->disp_h+3)&(~3));
	if (!priv->mpi) return NULL;
	ret = priv->dec_func((uint8_t*)priv->mpi, data, len, priv->decinfo);
    } else {
	// left the buffer allocation to the codecs, pass sh_video && priv
	priv->mpi=NULL;
	ret = priv->dec_func((uint8_t*)sh, data, len, priv->decinfo);
    }

    if (ret == ACT_DLTA_NORM)
	return priv->mpi;

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
	return priv->mpi;
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

    return priv->mpi;
}
