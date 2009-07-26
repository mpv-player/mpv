/*
 * video driver for SVGAlib
 * by Zoltan Mark Vician <se7en@sch.bme.hu>
 * Code started: Mon Apr  1 23:25:47 2001
 * Some changes by Matan Ziv-Av <matan@svgalib.org>
 * complete rewrite by Ivan Kalvachev 19 Mar 2003
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

/*
Wrangings:
 -  1bpp doesn't work right for me with '-double' and svgalib 1.4.3,
    but works OK with svgalib 1.9.17
 -  The HW acceleration is not tested - svgalibs supports few chipsets,
    and i don't have any of them. If it works for you then let me know.
    I will remove this warning after confirm its status.
 -  retrace sync works only in doublebuffer mode.
 -  the retrace sync may slow down decoding a lot - mplayer is blocked while
    waiting for retrace
 -  denoise3d fails to find common colorspace, use -vf denoise3d,scale

TODO:
 - let choose_best_mode take aspect into account
 - set palette from mpi->palette or mpi->plane[1]
 - make faster OSD black bars clear - need some OSD changes
 - Make nicer CONFIG parsing
 - change video mode logical width to match img->stride[0] - for HW only
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vga.h>

#include <limits.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "fastmemcpy.h"
#include "osdep/getch2.h"
#ifdef CONFIG_VIDIX
#include "vosub_vidix.h"
#endif

#include "sub.h"

#include "mp_msg.h"
#include "help_mp.h"
//#include "mp_image.h"

#include <assert.h>

//silence warnings, probably it have to go in some global header
#define UNUSED(x) ((void)(x))


static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
                       unsigned char *srca, int stride);
static uint32_t get_image(mp_image_t *mpi);

#define MAXPAGES 16
#define PAGE_EMPTY 0
#define PAGE_BUSY 1

#define CAP_ACCEL_CLEAR 8
#define CAP_ACCEL_PUTIMAGE 4
#define CAP_ACCEL_BACKGR 2
#define CAP_LINEAR 1

static uint8_t zerobuf[8192];//used when clear screen with vga_draw

static int squarepix;
static int force_vm=0;
static int force_native=0;
static int sync_flip=0;
static int blackbar_osd=0;
static int cpage,max_pages,old_page;

static vga_modeinfo * modeinfo;
static int mode_stride; //keep it in case of vga_setlogicalwidth
static int stride_granularity; //not yet used
static int mode_bpp;
static int mode_capabilities;

static int image_width,image_height; // used by OSD
static int x_pos, y_pos;

static struct {
    int yoffset;//y position of the page
    int doffset;//display start of the page
    uint8_t * vbase;//memory start address of the page
    int locks;
}PageStore[MAXPAGES];

static const vo_info_t info = {
    "SVGAlib",
    "svga",
    "Ivan Kalvachev <iive@users.sf.net>",
    ""
};

#ifdef CONFIG_VIDIX
static char vidix_name[32] = "";
static vidix_grkey_t gr_key;
#endif

LIBVO_EXTERN(svga)


//return number of 1'st free page or -1 if no free one
static inline int page_find_free(void){
    int i;
    for(i=0;i<max_pages;i++)
        if(PageStore[i].locks == PAGE_EMPTY) return i;
    return -1;
}

static int preinit(const char *arg)
{
    int i,rez;
    char s[64];

    getch2_disable();
    memset(zerobuf,0,sizeof(zerobuf));
    force_vm=force_native=squarepix=0;
    sync_flip=vo_vsync;
    blackbar_osd=0;

    if(arg)while(*arg) {
#ifdef CONFIG_VIDIX
        if(memcmp(arg,"vidix",5)==0) {
            i=6;
            while(arg[i] && arg[i]!=':') i++;
            strncpy(vidix_name, arg+6, i-6);
            vidix_name[i-5]=0;
            if(arg[i]==':')i++;
            arg+=i;
            vidix_preinit(vidix_name, &video_out_svga);
        }
#endif
        if(!strncmp(arg,"sq",2)) {
            squarepix=1;
            arg+=2;
            if( *arg == ':' ) arg++;
        }

        if(!strncmp(arg,"native",6)) {
            force_native=1;
            arg+=6;
            if( *arg == ':' ) arg++;
        }

        if(!strncmp(arg,"bbosd",5)) {
            blackbar_osd=1;
            arg+=5;
            if( *arg == ':' ) arg++;
        }

        if(!strncmp(arg,"retrace",7)) {
            sync_flip=1;
            arg+=7;
            if( *arg == ':' ) arg++;
        }

        if(*arg) {
            i=0;
            while(arg[i] && arg[i]!=':')i++;
            if(i<64){
                strncpy(s, arg, i);
                s[i]=0;

                force_vm=vga_getmodenumber(s);
                if(force_vm>0) {
                    mp_msg(MSGT_VO,MSGL_V, "vo_svga: Forcing mode %i\n",force_vm);
                }else{
                    force_vm = 0;
                }
            }
            arg+=i;
            if(*arg==':')arg++;
        }
    }

    rez = vga_init();
    if(rez != 0){
        mp_msg(MSGT_VO,MSGL_ERR, "vo_svga: vga_init() returned error=%d\n",rez);
    }
    return !!rez;
}

static void svga_clear_box(int x,int y,int w,int h){
    uint8_t * rgbplane;
    int i;

    if (mode_capabilities&CAP_ACCEL_CLEAR){
        mp_msg(MSGT_VO,MSGL_DBG3, "vo_svga: clearing box %d,%d - %d,%d with HW acceleration\n",
               x,y,w,h);
        if(mode_capabilities&CAP_ACCEL_BACKGR)
            vga_accel(ACCEL_SYNC);
        vga_accel(ACCEL_SETFGCOLOR,0);//black
        vga_accel(ACCEL_FILLBOX,x,y,w,h);
        return;
    }
    if (mode_capabilities & CAP_LINEAR){
        mp_msg(MSGT_VO,MSGL_DBG3, "vo_svga: clearing box %d,%d - %d,%d with memset\n",x,y,w,h);
        rgbplane=PageStore[0].vbase + (y*mode_stride) + (x*modeinfo->bytesperpixel);
        for(i=0;i<h;i++){
//i'm afraid that memcpy is better optimized than memset;)
            fast_memcpy(rgbplane,zerobuf,w*modeinfo->bytesperpixel);
//    memset(rgbplane,0,w*modeinfo->bytesperpixel);
            rgbplane+=mode_stride;
        }
        return;
    }
    //native
    mp_msg(MSGT_VO,MSGL_DBG3, "vo_svga: clearing box %d,%d - %d,%d with native draw \n",x,y,w,h);
    if(modeinfo->bytesperpixel!=0) w*=modeinfo->bytesperpixel;
    for(i=0;i<h;i++){
        vga_drawscansegment(zerobuf,x,y+i,w);
    }
};

static uint32_t svga_draw_image(mp_image_t *mpi){
    int i,x,y,w,h;
    int stride;
    uint8_t *rgbplane, *base;
    int bytesperline;
    int page;

    if(mpi->flags & MP_IMGFLAG_DIRECT){
        mp_msg(MSGT_VO,MSGL_DBG3, "vo_svga: drawing direct rendered surface\n");
        cpage=(uint32_t)mpi->priv;
        assert((cpage>=0)&&(cpage<max_pages));
        return VO_TRUE; //it's already done
    }
//  if (mpi->flags&MP_IMGFLAGS_DRAWBACK)
//  return VO_TRUE;//direct render method 2

//find a free page to draw into
//if there is no one then use the current one
    page = page_find_free();
    if(page>=0) cpage=page;
    PageStore[cpage].locks=PAGE_BUSY;

// these variables are used in loops
    x = mpi->x;
    y = mpi->y;
    w = mpi->w;
    h = mpi->h;
    stride = mpi->stride[0];
    rgbplane = mpi->planes[0] + y*stride + (x*mpi->bpp)/8;
    x+=x_pos;//center
    y+=y_pos;

    if(mpi->bpp >= 8){//for modes<8 use only native
        if( (mode_capabilities&CAP_ACCEL_PUTIMAGE) && (x==0) && (w==mpi->width) &&
            (stride == mode_stride) ){ //only monolite image can be accelerated
            w=(stride*8)/mpi->bpp;//we transfer pixels in the stride so the source
//ACCELERATE
            mp_msg(MSGT_VO,MSGL_DBG3, "vo_svga: using HW PutImage (x=%d,y=%d,w=%d,h=%d)\n",x,y,w,h);
            if(mode_capabilities & CAP_ACCEL_BACKGR)
                vga_accel(ACCEL_SYNC);

            vga_accel(ACCEL_PUTIMAGE,x,y+PageStore[cpage].yoffset,w,h,rgbplane);
            return VO_TRUE;
        }

        if( mode_capabilities&CAP_LINEAR){
//DIRECT
            mp_msg(MSGT_VO,MSGL_DBG3, "vo_svga: using Direct memcpy (x=%d,y=%d,w=%d,h=%d)\n",x,y,w,h);
            bytesperline=(w*mpi->bpp)/8;
            base=PageStore[cpage].vbase + (y*mode_stride) + (x*mpi->bpp)/8;

            for(i=0;i<h;i++){
                mem2agpcpy(base,rgbplane,bytesperline);
                base+=mode_stride;
                rgbplane+=stride;
            }
            return VO_TRUE;
        }
    }//(modebpp>=8


//NATIVE
    {
        int length;
        length=(w*mpi->bpp)/8;
        //one byte per pixel! svgalib innovation
        if(mpi->imgfmt==IMGFMT_RG4B || mpi->imgfmt==IMGFMT_BG4B) length=w;

        mp_msg(MSGT_VO,MSGL_DBG3, "vo_svga: using Native vga_draw(x=%d,y=%d,w=%d,h=%d)\n",x,y,w,h);
        y+=PageStore[cpage].yoffset;//y position of the page beggining
        for(i=0;i<h;i++){
            vga_drawscansegment(rgbplane,x,y+i,length);
            rgbplane+=stride;
        }
    }
    return VO_TRUE;
}

static int bpp_from_vminfo(vga_modeinfo *vminfo){
    switch(vminfo->colors){
        case 2: return 1;
        case 16: return 4;
        case 256: return 8;
        case 32768: return 15;
        case 65536: return 16;
        case 1<<24: return 8*vminfo->bytesperpixel;
    }
    return 0;
}

static int find_best_svga_mode(int req_w,int req_h, int req_bpp){
    int badness,prev_badness;
    int bestmode,lastmode;
    int i;
    vga_modeinfo *vminfo;
//int best aspect mode // best linear mode // best normal mode (no modeX)

    prev_badness = 0;//take care of special case below
    bestmode = 0; //0 is the TEXT mode
    lastmode = vga_lastmodenumber();
    for(i=1;i<=lastmode;i++){
        vminfo = vga_getmodeinfo(i);
        if( vminfo == NULL ) continue;
        mp_msg(MSGT_VO,MSGL_DBG4, "vo_svga: testing mode %d (%s)\n",i,vga_getmodename(i));
        if( vga_hasmode(i) == 0 ) continue;
        if( req_bpp != bpp_from_vminfo(vminfo) )continue;
        if( (vminfo->width < req_w) || (vminfo->height < req_h) ) continue;
            badness=(vminfo->width * vminfo->height) - (req_h * req_w);
        //put here aspect calculations
        if(squarepix)
            if( vminfo->width*3 != vminfo->height*4 ) continue;

        if( bestmode==0 || prev_badness >= badness ){//modeX etc...
            prev_badness=badness;
            bestmode=i;
            mp_msg(MSGT_VO,MSGL_DBG4, "vo_svga: found good mode %d with badness %d\n",i,badness);
        }
    }
    return bestmode;
}

static int control(uint32_t request, void *data, ...)
{
    switch (request) {
        case VOCTRL_QUERY_FORMAT:
            return query_format(*((uint32_t*)data));
        case VOCTRL_DRAW_IMAGE:
            return svga_draw_image( (mp_image_t *)data);
        case VOCTRL_GET_IMAGE:
            return get_image(data);
    }

#ifdef CONFIG_VIDIX
    if (vidix_name[0]) {
        switch (request) {
            case VOCTRL_SET_EQUALIZER:
            {
                va_list ap;
                int value;

                va_start(ap, data);
                value = va_arg(ap, int);
                va_end(ap);

                return vidix_control(request, data, value);
            }
            case VOCTRL_GET_EQUALIZER:
            {
                va_list ap;
                int *value;

                va_start(ap, data);
                value = va_arg(ap, int*);
                va_end(ap);

                return vidix_control(request, data, value);
            }
        }
        return vidix_control(request, data);
    }
#endif

    return VO_NOTIMPL;
}

//
// This function is called to init the video driver for specific mode
//
static int config(uint32_t width, uint32_t height, uint32_t d_width,
                       uint32_t d_height, uint32_t flags, char *title,
                       uint32_t format) {
    int32_t req_w = width;// (d_width > 0 ? d_width : width);
    int32_t req_h = height;// (d_height > 0 ? d_height : height);
    uint16_t vid_mode = 0;
    int32_t req_bpp;

    uint32_t accflags;
    mp_msg(MSGT_VO,MSGL_V, "vo_svga: config(%i, %i, %i, %i, %08x, %s, %08x)\n", width, height,
           d_width, d_height, flags, title, format);
//Only RGB modes supported
    if (!IMGFMT_IS_RGB(format) && !IMGFMT_IS_BGR(format)) {assert(0);return -1;}
    req_bpp = IMGFMT_BGR_DEPTH(format);

    if( vo_dbpp!=0 && vo_dbpp!=req_bpp) {assert(0);return-1;}

    if(!force_vm) {
        mp_msg(MSGT_VO,MSGL_V, "vo_svga: Looking for the best resolution...\n");
        mp_msg(MSGT_VO,MSGL_V, "vo_svga: req_w: %d, req_h: %d, bpp: %d\n",req_w,req_h,req_bpp);
        vid_mode=find_best_svga_mode(req_w,req_h,req_bpp);
        if(vid_mode==0)
            return 1;
        modeinfo=vga_getmodeinfo(vid_mode);
    }else{//force_vm
        vid_mode=force_vm;
        if(vga_hasmode(vid_mode) == 0){
            mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_SVGA_ForcedVidmodeNotAvailable,
                   vid_mode,vga_getmodename(vid_mode));
            return 1; //error;
        }
        modeinfo=vga_getmodeinfo(vid_mode);
        if( (modeinfo->width < req_w) || (modeinfo->height < req_h) ){
            mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_SVGA_ForcedVidmodeTooSmall,
                   vid_mode,vga_getmodename(vid_mode));
            return 1;
        }
    }
    mode_bpp=bpp_from_vminfo(modeinfo);

    mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_SVGA_Vidmode,
           vid_mode,modeinfo->width,modeinfo->height,mode_bpp);

    if (vga_setmode(vid_mode) == -1) {
        mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_SVGA_VgasetmodeFailed,vid_mode);
        uninit();
        return 1; // error
    }
    /* set 332 palette for 8 bpp */
    if(mode_bpp==8){
        int i;
        for(i=0; i<256; i++)
            vga_setpalette(i, ((i>>5)&7)*9, ((i>>2)&7)*9, (i&3)*21);
    }
    /* set 121 palette for 4 bpp */
    else if(mode_bpp==4){
        int i;
        for(i=0; i<16; i++)
        vga_setpalette(i, ((i>>3)&1)*63, ((i>>1)&3)*21, (i&1)*63);
    }
    //if we change the logical width, we should know the granularity
    stride_granularity=8;//according to man vga_logicalwidth
    if(modeinfo->flags & EXT_INFO_AVAILABLE){
        stride_granularity=modeinfo->linewidth_unit;
    }
    //look for hardware acceleration
    mode_capabilities=0;//NATIVE;
    if(!force_native){//if we want to use only native drawers
        if(modeinfo->flags & HAVE_EXT_SET){//support for hwaccel interface
            accflags=vga_ext_set(VGA_EXT_AVAILABLE,VGA_AVAIL_ACCEL);
            if(accflags & ACCELFLAG_FILLBOX) // clear screen
                mode_capabilities|=CAP_ACCEL_CLEAR;
            if(accflags & ACCELFLAG_PUTIMAGE)//support for mem->vid transfer
                mode_capabilities|=CAP_ACCEL_PUTIMAGE;
            if((accflags & ACCELFLAG_SETMODE) && (accflags & ACCELFLAG_SYNC)){
                vga_accel(ACCEL_SETMODE,BLITS_IN_BACKGROUND);
                mode_capabilities|=CAP_ACCEL_BACKGR;//can draw in backgraund
            }
        }
        if(modeinfo->flags & IS_LINEAR){
            mode_capabilities|=CAP_LINEAR; //don't use bank & vga_draw
        }
        else{
            if(modeinfo->flags & CAPABLE_LINEAR){
                int vid_mem_size;
                vid_mem_size = vga_setlinearaddressing();
                if(vid_mem_size != -1){
                    modeinfo=vga_getmodeinfo(vid_mode);//sometimes they change parameters
                    mode_capabilities|=CAP_LINEAR;
                }
            }
        }
    }//fi force native
    if(mode_capabilities&CAP_LINEAR){
        mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_SVGA_VideoModeIsLinearAndMemcpyCouldBeUsed);
    }
    if(mode_capabilities&CAP_ACCEL_PUTIMAGE){
        mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_SVGA_VideoModeHasHardwareAcceleration);
        mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_SVGA_IfItWorksForYouIWouldLikeToKnow);
    }

//here is the place to handle strides for accel_ modes;
    mode_stride=modeinfo->linewidth;
//we may try to set a bigger stride for video mode that will match the mpi->stride,
//this way we will transfer more data, but HW put_image can do it in backgraund!

//now let's see how many pages we can use
    max_pages = modeinfo->maxpixels/(modeinfo->height * modeinfo->width);
    if(max_pages > MAXPAGES) max_pages = MAXPAGES;
    if(!vo_doublebuffering) max_pages=1;
//fill PageStore structs
    {
        int i;
        uint8_t * GRAPH_MEM;
        int dof;
        GRAPH_MEM=vga_getgraphmem();
        for(i=0;i<max_pages;i++){
            //calculate display offset
            dof = i * modeinfo->height * modeinfo->width;
            if(modeinfo->bytesperpixel != 0) dof*=modeinfo->bytesperpixel;
            //check video chip limitations
            if( dof != (dof & modeinfo->startaddressrange) ){
                max_pages=i;//page 0 will never come here
                break;
            }
            PageStore[i].yoffset = i * modeinfo->height;//starting y offset
            PageStore[i].vbase = GRAPH_MEM + i*modeinfo->height*mode_stride; //memory base address
            PageStore[i].doffset = dof; //display offset
            PageStore[i].locks = PAGE_EMPTY;
        }
    }
    assert(max_pages>0);
    mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_SVGA_VideoModeHas,max_pages);
    //15bpp
    if(modeinfo->bytesperpixel!=0)
        vga_claimvideomemory(max_pages * modeinfo->height * modeinfo->width * modeinfo->bytesperpixel);
    else
        vga_claimvideomemory(max_pages * modeinfo->height * modeinfo->width * mode_bpp / 8);

    cpage=old_page=0;
    svga_clear_box(0,0,modeinfo->width,modeinfo->height * max_pages);

    image_height=req_h;
    image_width=req_w;
    x_pos = (modeinfo->width  - req_w) / 2;
    y_pos = (modeinfo->height - req_h) / 2;
    x_pos &= ~(15); //align x offset position to 16 pixels
    mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_SVGA_CenteringImageStartAt,x_pos,y_pos);

#ifdef CONFIG_VIDIX

    if(vidix_name[0]){
        vidix_init(width, height, x_pos, y_pos, modeinfo->width, modeinfo->height,
                   format, mode_bpp, modeinfo->width,modeinfo->height);
        mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_SVGA_UsingVidix,width,height,
               modeinfo->width,modeinfo->height);
        vidix_start();
        /*set colorkey*/
        if(vidix_grkey_support()){
            vidix_grkey_get(&gr_key);
            gr_key.key_op = KEYS_PUT;
            if (!(vo_colorkey & 0xFF000000)) {
                gr_key.ckey.op = CKEY_TRUE;
                gr_key.ckey.red = (vo_colorkey & 0x00FF0000) >> 16;
                gr_key.ckey.green = (vo_colorkey & 0x0000FF00) >> 8;
                gr_key.ckey.blue = vo_colorkey & 0x000000FF;
            } else
                gr_key.ckey.op = CKEY_FALSE;
            vidix_grkey_set(&gr_key);
        }
    }
#endif

    vga_setdisplaystart(0);
    return 0;
}

static int draw_slice(uint8_t *image[],int stride[],
               int w, int h, int x, int y) {
    assert(0);
    UNUSED(image);UNUSED(stride);
    UNUSED(w);UNUSED(h);
    UNUSED(x);UNUSED(y);

    return VO_ERROR;//this is yv12 only -> vf_scale should do all transforms
}

static int draw_frame(uint8_t *src[]) {
    assert(0);
    UNUSED(src);
    return VO_ERROR;//this one should not be called
}

static void draw_osd(void)
{
    mp_msg(MSGT_VO,MSGL_DBG4, "vo_svga: draw_osd()\n");
    //only modes with bytesperpixel>0 can draw OSD
    if(modeinfo->bytesperpixel==0) return;
    if(!(mode_capabilities&CAP_LINEAR)) return;//force_native will remove OSD

    if(blackbar_osd){
//111
//3 4
//222
        svga_clear_box(0,0 + PageStore[cpage].yoffset,
                       modeinfo->width, y_pos);
        svga_clear_box(0, image_height + y_pos + PageStore[cpage].yoffset,
                       modeinfo->width, modeinfo->height-(image_height+ y_pos));
        svga_clear_box(0, y_pos + PageStore[cpage].yoffset,
                       x_pos, image_height);
        svga_clear_box(image_width + x_pos, y_pos + PageStore[cpage].yoffset,
                       modeinfo->width-(x_pos+image_width), image_height);
//    vo_remove_text(modeinfo->width, modeinfo->height, clear_alpha);
        vo_draw_text(modeinfo->width, modeinfo->height, draw_alpha);
    }else{
        vo_draw_text(image_width, image_height, draw_alpha);
    }
}

static void flip_page(void) {
    PageStore[old_page].locks=PAGE_EMPTY;
    PageStore[cpage].locks=PAGE_BUSY;

    mp_msg(MSGT_VO,MSGL_DBG3, "vo_svga: viewing page %d\n",cpage);
    if(sync_flip && old_page!=cpage){
        mp_msg(MSGT_VO,MSGL_DBG3, "vo_svga:vga_waitretrace\n");
        vga_waitretrace();
    }
    vga_setdisplaystart(PageStore[cpage].doffset);

    old_page=cpage;//cpage will be overwriten on next draw_image
}

static void check_events(void) {
}

static void uninit(void) {

#ifdef CONFIG_VIDIX
    if(vidix_name[0])vidix_term();
#endif
    vga_setmode(TEXT);
}

/* --------------------------------------------------------------------- */
static int query_format(uint32_t format) {
    int32_t req_bpp,flags;
    int i,lastmode;
    vga_modeinfo * vminfo;

    mp_msg(MSGT_VO,MSGL_DBG4, "vo_svga: query_format=%X \n",format);
//only RGB modes supported
    if( (!IMGFMT_IS_RGB(format)) && (!IMGFMT_IS_BGR(format)) ) return 0;

// Reject different endian
#if HAVE_BIGENDIAN
    if (IMGFMT_IS_BGR(format)) return 0;
#else
    if (IMGFMT_IS_RGB(format)) return 0;
#endif

    //svgalib supports only BG4B! if we want BGR4 we have to emulate it (sw)
    if( format==IMGFMT_BGR4 || format==IMGFMT_RGB4) return 0;
    req_bpp = IMGFMT_RGB_DEPTH(format);
    if( vo_dbpp>0 && vo_dbpp!=req_bpp ) return 0; //support -bpp options
//scan all modes
    lastmode = vga_lastmodenumber();
    for(i=1;i<=lastmode;i++){
        vminfo = vga_getmodeinfo(i);
        if( vminfo == NULL ) continue;
        if( vga_hasmode(i) == 0 ) continue;
        if( req_bpp != bpp_from_vminfo(vminfo) ) continue;
        if( (force_vm > 0) && (force_vm != i) )  continue;//quick hack
        flags = VFCAP_CSP_SUPPORTED|
                VFCAP_CSP_SUPPORTED_BY_HW|
                VFCAP_ACCEPT_STRIDE|
                0;
        if(req_bpp>8) flags|=VFCAP_OSD;
        return flags;
    }
    return 0;
}

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
                       unsigned char *srca, int stride) {
    char* base;

    mp_msg(MSGT_VO,MSGL_DBG3, "vo_svga: draw_alpha(x0=%d,y0=%d,w=%d,h=%d,src=%p,srca=%p,stride=%d\n",
           x0,y0,w,h,src,srca,stride);
    if(!blackbar_osd) {
        //drawing in the image, so place the stuff there
        x0+=x_pos;
        y0+=y_pos;
    }

    mp_msg(MSGT_VO,MSGL_DBG4, "vo_svga: OSD draw in page %d\n",cpage);
    base=PageStore[cpage].vbase + y0*mode_stride + x0*modeinfo->bytesperpixel;
    switch (mode_bpp) {
        case 32:
            vo_draw_alpha_rgb32(w, h, src, srca, stride, base, mode_stride);
            break;
        case 24:
            vo_draw_alpha_rgb24(w, h, src, srca, stride, base, mode_stride);
            break;
        case 16:
            vo_draw_alpha_rgb16(w, h, src, srca, stride, base, mode_stride);
            break;
        case 15:
            vo_draw_alpha_rgb15(w, h, src, srca, stride, base, mode_stride);
            break;
    }
}

static uint32_t get_image(mp_image_t *mpi){
    int page;

    if(!IMGFMT_IS_BGR(mpi->imgfmt) && !IMGFMT_IS_RGB(mpi->imgfmt) ){
        assert(0);//should never happen
        return VO_FALSE;
    }

    if (
        ( (mpi->type != MP_IMGTYPE_STATIC) && (mpi->type != MP_IMGTYPE_TEMP)) ||
        (mpi->flags & MP_IMGFLAG_PLANAR) ||
        (mpi->flags & MP_IMGFLAG_YUV)
       )
    return VO_FALSE;

//reading from video memory is horribly slow
    if( !(mpi->flags & MP_IMGFLAG_READABLE) && vo_directrendering &&
        (mode_capabilities & CAP_LINEAR) ){

//find free page and reserve it
        page=page_find_free();
        if(page >= 0){
            PageStore[page].locks=PAGE_BUSY;

            mpi->flags |= MP_IMGFLAG_DIRECT;
            mpi->stride[0] = mode_stride;
            mpi->planes[0] = PageStore[page].vbase +
                             y_pos*mode_stride + (x_pos*mpi->bpp)/8;
            mpi->priv=(void *)page;
            mp_msg(MSGT_VO,MSGL_DBG3, "vo_svga: direct render allocated! page=%d\n",page);
            return VO_TRUE;
        }
    }

    return VO_FALSE;
}
