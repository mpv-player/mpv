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

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "mplayer.h"
#include "mp_msg.h"

#include "libmpcodecs/img_format.h"
#include "libmpcodecs/mp_image.h"
#include "libmpcodecs/vf.h"

#include "libvo/fastmemcpy.h"
#include "libvo/video_out.h"
#include "libvo/font_load.h"
#include "libvo/sub.h"
#include "input/input.h"
#include "m_struct.h"
#include "menu.h"
#include "access_mpcontext.h"


static struct vf_priv_s* st_priv = NULL;

static mp_image_t* pause_mpi = NULL;
static int go2pause = 0;
/// if nonzero display menu at startup
int menu_startup = 0;

struct vf_priv_s {
  menu_t* root;
  menu_t* current;
  int passthrough;
};

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts);

void vf_menu_pause_update(struct vf_instance_s* vf) {
  const vo_functions_t *video_out = mpctx_get_video_out(vf->priv->current->ctx);
  if(pause_mpi) {
    put_image(vf,pause_mpi, MP_NOPTS_VALUE);
    // Don't draw the osd atm
    //vf->control(vf,VFCTRL_DRAW_OSD,NULL);
    video_out->flip_page();
  }
}

static int cmd_filter(mp_cmd_t* cmd, int paused, struct vf_priv_s * priv) {

  switch(cmd->id) {
  case MP_CMD_MENU : {  // Convert txt cmd from the users into libmenu stuff
    char* arg = cmd->args[0].v.s;
    
    if (!priv->current->show && strcmp(arg,"hide"))
      priv->current->show = 1;
    else if(strcmp(arg,"up") == 0)
      menu_read_cmd(priv->current,MENU_CMD_UP);
    else if(strcmp(arg,"down") == 0)
      menu_read_cmd(priv->current,MENU_CMD_DOWN);
    else if(strcmp(arg,"left") == 0)
      menu_read_cmd(priv->current,MENU_CMD_LEFT);
    else if(strcmp(arg,"right") == 0)
      menu_read_cmd(priv->current,MENU_CMD_RIGHT);
    else if(strcmp(arg,"ok") == 0)
      menu_read_cmd(priv->current,MENU_CMD_OK);
    else if(strcmp(arg,"cancel") == 0)
      menu_read_cmd(priv->current,MENU_CMD_CANCEL);
    else if(strcmp(arg,"home") == 0)
      menu_read_cmd(priv->current,MENU_CMD_HOME);
    else if(strcmp(arg,"end") == 0)
      menu_read_cmd(priv->current,MENU_CMD_END);
    else if(strcmp(arg,"pageup") == 0)
      menu_read_cmd(priv->current,MENU_CMD_PAGE_UP);
    else if(strcmp(arg,"pagedown") == 0)
      menu_read_cmd(priv->current,MENU_CMD_PAGE_DOWN);
    else if(strcmp(arg,"click") == 0)
      menu_read_cmd(priv->current,MENU_CMD_CLICK);
    else if(strcmp(arg,"hide") == 0 || strcmp(arg,"toggle") == 0)
      priv->current->show = 0;
    else
      mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_UnknownMenuCommand,arg);
    return 1;
  }
  case MP_CMD_SET_MENU : {
    char* menu = cmd->args[0].v.s;
    menu_t* l = priv->current;
    priv->current = menu_open(menu);
    if(!priv->current) {
      mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_FailedToOpenMenu,menu);
      priv->current = l;
      priv->current->show = 0;
    } else {
      priv->current->show = 1;
      priv->current->parent = l;
    }
    return 1;
  }
  }
  return 0;
}

static void get_image(struct vf_instance_s* vf, mp_image_t *mpi){
  mp_image_t *dmpi;

  if(mpi->type == MP_IMGTYPE_TEMP && (!(mpi->flags&MP_IMGFLAG_PRESERVE)) ) {
    dmpi = vf_get_image(vf->next,mpi->imgfmt,mpi->type, mpi->flags, mpi->w, mpi->h);
    memcpy(mpi->planes,dmpi->planes,MP_MAX_PLANES*sizeof(unsigned char*));
    memcpy(mpi->stride,dmpi->stride,MP_MAX_PLANES*sizeof(unsigned int));
    mpi->flags|=MP_IMGFLAG_DIRECT;
    mpi->priv=(void*)dmpi;
    return;
  }
}
  
static int key_cb(int code) {
  return menu_read_key(st_priv->current,code);
}

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts){
  mp_image_t *dmpi = NULL;

  if (vf->priv->passthrough) {
    dmpi=vf_get_image(vf->next, IMGFMT_MPEGPES, MP_IMGTYPE_EXPORT,
                      0, mpi->w, mpi->h);
    dmpi->planes[0]=mpi->planes[0];
    return vf_next_put_image(vf,dmpi, pts);
  }

  // Close all menu who requested it
  while(vf->priv->current->cl && vf->priv->current != vf->priv->root) {
    menu_t* m = vf->priv->current;
    vf->priv->current = m->parent ? m->parent :  vf->priv->root;
    menu_close(m);
  }

    // Try to capture the last frame before pause, or fallback to use
    // last captured frame.
    if(pause_mpi && (mpi->w != pause_mpi->w || mpi->h != pause_mpi->h ||
		     mpi->imgfmt != pause_mpi->imgfmt)) {
      free_mp_image(pause_mpi);
      pause_mpi = NULL;
    }
  if (!pause_mpi) {
    pause_mpi = alloc_mpi(mpi->w,mpi->h,mpi->imgfmt);
    copy_mpi(pause_mpi,mpi);
  }
  else if (mpctx_get_osd_function(vf->priv->root->ctx) == OSD_PAUSE)
    copy_mpi(pause_mpi,mpi);

  if (vf->priv->current->show) {
    if (!mp_input_key_cb)
      mp_input_key_cb = key_cb;

  if(mpi->flags&MP_IMGFLAG_DIRECT)
    dmpi = mpi->priv;
  else {
    dmpi = vf_get_image(vf->next,mpi->imgfmt,
			MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
			mpi->w,mpi->h);
    copy_mpi(dmpi,mpi);
  }
  menu_draw(vf->priv->current,dmpi);

  } else {
    if(mp_input_key_cb)
      mp_input_key_cb = NULL;

    if(mpi->flags&MP_IMGFLAG_DIRECT)
      dmpi = mpi->priv;
    else {
      dmpi = vf_get_image(vf->next,mpi->imgfmt,
                          MP_IMGTYPE_EXPORT, MP_IMGFLAG_ACCEPT_STRIDE,
                          mpi->w,mpi->h);

      dmpi->stride[0] = mpi->stride[0];
      dmpi->stride[1] = mpi->stride[1];
      dmpi->stride[2] = mpi->stride[2];
      dmpi->planes[0] = mpi->planes[0];
      dmpi->planes[1] = mpi->planes[1];
      dmpi->planes[2] = mpi->planes[2];
      dmpi->priv      = mpi->priv;
    }
  }
  return vf_next_put_image(vf,dmpi, pts);
}

static void uninit(vf_instance_t *vf) {
     vf->priv=NULL;
     if(pause_mpi) {
       free_mp_image(pause_mpi);
       pause_mpi = NULL;
     }
}

static int config(struct vf_instance_s* vf, int width, int height, int d_width, int d_height,
		  unsigned int flags, unsigned int outfmt) { 
#ifdef CONFIG_FREETYPE
  // here is the right place to get screen dimensions
  if (force_load_font) {
    force_load_font = 0;
    load_font_ft(width,height,&vo_font,font_name,osd_font_scale_factor);
  }
#endif
  if(outfmt == IMGFMT_MPEGPES)
    vf->priv->passthrough = 1;
  return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
  return vf_next_query_format(vf,fmt);
}

static int open_vf(vf_instance_t *vf, char* args){
  if(!st_priv) {
    st_priv = calloc(1,sizeof(struct vf_priv_s));
    st_priv->root = st_priv->current = menu_open(args);
    if(!st_priv->current) {
      free(st_priv);
      st_priv = NULL;
      return 0;
    }
    st_priv->root->show = menu_startup;
    mp_input_add_cmd_filter((mp_input_cmd_filter)cmd_filter,st_priv);
  }

  vf->config = config;
  vf->query_format=query_format;
  vf->put_image = put_image;
  vf->get_image = get_image;
  vf->uninit=uninit;
  vf->priv=st_priv;
  go2pause=0;

  return 1;
}


vf_info_t vf_info_menu  = {
  "Internal filter for libmenu",
  "menu",
  "Albeu",
  "",
  open_vf,
  NULL
};



