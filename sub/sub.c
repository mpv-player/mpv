/*
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
#include <string.h>

#include <libavutil/mem.h>
#include <libavutil/common.h>

#include "config.h"

#include "stream/stream.h"

#include "osdep/timer.h"

#include "talloc.h"
#include "options.h"
#include "mplayer.h"
#include "mp_msg.h"
#include "libvo/video_out.h"
#include "sub.h"
#include "sub/ass_mp.h"
#include "spudec.h"


char * const sub_osd_names[]={
    _("Seekbar"),
    _("Play"),
    _("Pause"),
    _("Stop"),
    _("Rewind"),
    _("Forward"),
    _("Clock"),
    _("Contrast"),
    _("Saturation"),
    _("Volume"),
    _("Brightness"),
    _("Hue"),
    _("Balance")
};
char * const sub_osd_names_short[] ={ "", "|>", "||", "[]", "<<" , ">>", "", "", "", "", "", "", "" };

int sub_unicode=0;
int sub_utf8=0;
int sub_pos=100;
int sub_width_p=100;
int sub_visibility=1;
int sub_bg_color=0; /* subtitles background color */
int sub_bg_alpha=0;
int sub_justify=0;

int vo_osd_progbar_type=-1;
int vo_osd_progbar_value=100;   // 0..256
subtitle* vo_sub=NULL;
char *subtitle_font_encoding = NULL;
float text_font_scale_factor = 3.5;
float osd_font_scale_factor = 4.0;
float subtitle_font_radius = 2.0;
float subtitle_font_thickness = 2.0;
// 0 = no autoscale
// 1 = video height
// 2 = video width
// 3 = diagonal
int subtitle_autoscale = 3;

char *font_name = NULL;
char *sub_font_name = NULL;
float font_factor = 0.75;
float sub_delay = 0;
float sub_fps = 0;

// allocates/enlarges the alpha/bitmap buffer
void osd_alloc_buf(mp_osd_obj_t* obj)
{
    int len;
    if (obj->bbox.x2 < obj->bbox.x1) obj->bbox.x2 = obj->bbox.x1;
    if (obj->bbox.y2 < obj->bbox.y1) obj->bbox.y2 = obj->bbox.y1;
    obj->stride = ((obj->bbox.x2-obj->bbox.x1)+7)&(~7);
    len = obj->stride*(obj->bbox.y2-obj->bbox.y1);
    if (obj->allocated<len) {
	obj->allocated = len;
	av_free(obj->bitmap_buffer);
	av_free(obj->alpha_buffer);
	obj->bitmap_buffer = av_malloc(len);
	obj->alpha_buffer  = av_malloc(len);
    }
    memset(obj->bitmap_buffer, sub_bg_color, len);
    memset(obj->alpha_buffer, sub_bg_alpha, len);
}

// renders the buffer
void vo_draw_text_from_buffer(mp_osd_obj_t* obj,void (*draw_alpha)(void *ctx, int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride), void *ctx)
{
    if (obj->allocated > 0) {
	draw_alpha(ctx,
		   obj->bbox.x1,obj->bbox.y1,
		   obj->bbox.x2-obj->bbox.x1,
		   obj->bbox.y2-obj->bbox.y1,
		   obj->bitmap_buffer,
		   obj->alpha_buffer,
		   obj->stride);
    }
}

inline static void vo_update_spudec_sub(struct osd_state *osd, mp_osd_obj_t* obj)
{
  unsigned int bbox[4];
  spudec_calc_bbox(vo_spudec, osd->w, osd->h, bbox);
  obj->bbox.x1 = bbox[0];
  obj->bbox.x2 = bbox[1];
  obj->bbox.y1 = bbox[2];
  obj->bbox.y2 = bbox[3];
  obj->flags |= OSDFLAG_BBOX;
}

inline static void vo_draw_spudec_sub(mp_osd_obj_t* obj, void (*draw_alpha)(void *ctx, int x0, int y0, int w, int h, unsigned char* src, unsigned char* srca, int stride), void *ctx)
{
    spudec_draw_scaled(vo_spudec, obj->dxs, obj->dys, draw_alpha, ctx);
}

void *vo_spudec=NULL;
void *vo_vobsub=NULL;

mp_osd_obj_t* vo_osd_list=NULL;

static mp_osd_obj_t* new_osd_obj(int type){
    mp_osd_obj_t* osd=malloc(sizeof(mp_osd_obj_t));
    memset(osd,0,sizeof(mp_osd_obj_t));
    osd->next=vo_osd_list;
    vo_osd_list=osd;
    osd->type=type;
    osd->alpha_buffer = NULL;
    osd->bitmap_buffer = NULL;
    osd->allocated = -1;
    return osd;
}

void osd_free(struct osd_state *osd)
{
    osd_destroy_backend(osd);
    mp_osd_obj_t* obj=vo_osd_list;
    while(obj){
	mp_osd_obj_t* next=obj->next;
	av_free(obj->alpha_buffer);
	av_free(obj->bitmap_buffer);
	free(obj);
	obj=next;
    }
    vo_osd_list=NULL;
    talloc_free(osd);
}

static int osd_update_ext(struct osd_state *osd, int dxs, int dys,
                          int left_border, int top_border, int right_border,
                          int bottom_border, int orig_w, int orig_h)
{
    struct MPOpts *opts = osd->opts;
    mp_osd_obj_t* obj=vo_osd_list;
    int chg=0;

    osd->w = dxs;
    osd->h = dys;

    osd_font_load(osd);

    while(obj){
      if(dxs!=obj->dxs || dys!=obj->dys || obj->flags&OSDFLAG_FORCE_UPDATE){
        int vis=obj->flags&OSDFLAG_VISIBLE;
	obj->flags&=~OSDFLAG_BBOX;
	switch(obj->type){
	case OSDTYPE_SUBTITLE:
	    vo_update_text_sub(osd, obj);
	    break;
	case OSDTYPE_PROGBAR:
	    vo_update_text_progbar(osd, obj);
	    break;
	case OSDTYPE_SPU:
	    if (opts->sub_visibility && vo_spudec && spudec_visible(vo_spudec)){
	        vo_update_spudec_sub(osd, obj);
		obj->flags|=OSDFLAG_VISIBLE|OSDFLAG_CHANGED;
	    }
	    else
		obj->flags&=~OSDFLAG_VISIBLE;
	    break;
	case OSDTYPE_OSD:
	    if(osd->osd_text[0]){
		vo_update_text_osd(osd, obj);
		obj->flags|=OSDFLAG_VISIBLE|OSDFLAG_CHANGED;
	    } else
		obj->flags&=~OSDFLAG_VISIBLE;
	    break;
	}
	// check bbox:
	if(!(obj->flags&OSDFLAG_BBOX)){
	    // we don't know, so assume the whole screen changed :(
	    obj->bbox.x1=obj->bbox.y1=0;
	    obj->bbox.x2=dxs;
	    obj->bbox.y2=dys;
	    obj->flags|=OSDFLAG_BBOX;
	} else {
	    // check bbox, reduce it if it's out of bounds (corners):
	    if(obj->bbox.x1<0) obj->bbox.x1=0;
	    if(obj->bbox.y1<0) obj->bbox.y1=0;
	    if(obj->bbox.x2>dxs) obj->bbox.x2=dxs;
	    if(obj->bbox.y2>dys) obj->bbox.y2=dys;
	    if(obj->flags&OSDFLAG_VISIBLE)
	    // debug:
	    mp_msg(MSGT_OSD,MSGL_DBG2,"OSD update: %d;%d %dx%d  \n",
		obj->bbox.x1,obj->bbox.y1,obj->bbox.x2-obj->bbox.x1,
		obj->bbox.y2-obj->bbox.y1);
	}
	// check if visibility changed:
	if(vis != (obj->flags&OSDFLAG_VISIBLE) ) obj->flags|=OSDFLAG_CHANGED;
	// remove the cause of automatic update:
	obj->dxs=dxs; obj->dys=dys;
	obj->flags&=~OSDFLAG_FORCE_UPDATE;
      }
      if(obj->flags&OSDFLAG_CHANGED){
        chg|=1<<obj->type;
	mp_msg(MSGT_OSD,MSGL_DBG2,"OSD chg: %d  V: %s  pb:%d  \n",obj->type,(obj->flags&OSDFLAG_VISIBLE)?"yes":"no",vo_osd_progbar_type);
      }
      obj=obj->next;
    }
    return chg;
}

int osd_update(struct osd_state *osd, int dxs, int dys)
{
    return osd_update_ext(osd, dxs, dys, 0, 0, 0, 0, dxs, dys);
}

struct osd_state *osd_create(struct MPOpts *opts, struct ass_library *asslib)
{
    struct osd_state *osd = talloc_zero(NULL, struct osd_state);
    *osd = (struct osd_state){
        .opts = opts,
        .ass_library = asslib,
    };
    // temp hack, should be moved to mplayer later
    new_osd_obj(OSDTYPE_OSD);
    new_osd_obj(OSDTYPE_SUBTITLE);
    new_osd_obj(OSDTYPE_PROGBAR);
    new_osd_obj(OSDTYPE_SPU);
    osd_font_invalidate();
    osd->osd_text = talloc_strdup(osd, "");
    osd_init_backend(osd);
    return osd;
}

void osd_set_text(struct osd_state *osd, const char *text)
{
    if (!text)
        text = "";
    if (strcmp(osd->osd_text, text) == 0)
        return;
    talloc_free(osd->osd_text);
    osd->osd_text = talloc_strdup(osd, text);
    vo_osd_changed(OSDTYPE_OSD);
}

void osd_draw_text_ext(struct osd_state *osd, int dxs, int dys,
                       int left_border, int top_border, int right_border,
                       int bottom_border, int orig_w, int orig_h,
                       void (*draw_alpha)(void *ctx, int x0, int y0, int w,
                                          int h, unsigned char* src,
                                          unsigned char *srca,
                                          int stride),
                   void *ctx)
{
    mp_osd_obj_t* obj=vo_osd_list;
    osd_update_ext(osd, dxs, dys, left_border, top_border, right_border,
                   bottom_border, orig_w, orig_h);
    while(obj){
      if(obj->flags&OSDFLAG_VISIBLE){
	switch(obj->type){
	case OSDTYPE_SPU:
            if (vo_spudec)
                vo_draw_spudec_sub(obj, draw_alpha, ctx); // FIXME
	    break;
	case OSDTYPE_OSD:
	case OSDTYPE_SUBTITLE:
	case OSDTYPE_PROGBAR:
	    vo_draw_text_from_buffer(obj, draw_alpha, ctx);
	    break;
	}
	obj->old_bbox=obj->bbox;
	obj->flags|=OSDFLAG_OLD_BBOX;
      }
      obj->flags&=~OSDFLAG_CHANGED;
      obj=obj->next;
    }
}

void osd_draw_text(struct osd_state *osd, int dxs, int dys,
                   void (*draw_alpha)(void *ctx, int x0, int y0, int w, int h,
                                      unsigned char* src, unsigned char *srca,
                                      int stride),
                   void *ctx)
{
    osd_draw_text_ext(osd, dxs, dys, 0, 0, 0, 0, dxs, dys, draw_alpha, ctx);
}

void vo_osd_changed(int new_value)
{
    mp_osd_obj_t* obj=vo_osd_list;

    while(obj){
	if(obj->type==new_value) obj->flags|=OSDFLAG_FORCE_UPDATE;
	obj=obj->next;
    }
}

void vo_osd_reset_changed(void)
{
    mp_osd_obj_t* obj = vo_osd_list;
    while (obj) {
        obj->flags = obj->flags & ~OSDFLAG_FORCE_UPDATE;
        obj = obj->next;
    }
}

bool vo_osd_has_changed(struct osd_state *osd)
{
    mp_osd_obj_t* obj = vo_osd_list;
    while (obj) {
        if (obj->flags & OSDFLAG_FORCE_UPDATE)
            return true;
        obj = obj->next;
    }
    return false;
}

void vo_osd_resized()
{
    // font needs to be adjusted
    osd_font_invalidate();
    // OSD needs to be drawn fresh for new size
    vo_osd_changed(OSDTYPE_OSD);
    vo_osd_changed(OSDTYPE_SUBTITLE);
}

// return TRUE if we have osd in the specified rectangular area:
int vo_osd_check_range_update(int x1,int y1,int x2,int y2){
    mp_osd_obj_t* obj=vo_osd_list;
    while(obj){
	if(obj->flags&OSDFLAG_VISIBLE){
	    if(	(obj->bbox.x1<=x2 && obj->bbox.x2>=x1) &&
		(obj->bbox.y1<=y2 && obj->bbox.y2>=y1) &&
		obj->bbox.y2 > obj->bbox.y1 && obj->bbox.x2 > obj->bbox.x1
		) return 1;
	}
	obj=obj->next;
    }
    return 0;
}
