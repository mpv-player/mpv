
#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "mp_msg.h"

#include "libmpcodecs/img_format.h"
#include "libmpcodecs/mp_image.h"
#include "libmpcodecs/vf.h"

#include "libvo/fastmemcpy.h"
#include "libvo/video_out.h"
#include "libvo/font_load.h"
#include "input/input.h"
#include "m_struct.h"
#include "menu.h"
#include "access_mpcontext.h"


static struct vf_priv_s* st_priv = NULL;

static mp_image_t* pause_mpi = NULL;
static int go2pause = 0;
/// if nonzero display menu at startup
int attribute_used menu_startup = 0;

struct vf_priv_s {
  menu_t* root;
  menu_t* current;
  int passthrough;
};

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts);

static mp_image_t* alloc_mpi(int w, int h, uint32_t fmt) {
  mp_image_t* mpi = new_mp_image(w,h);

  mp_image_setfmt(mpi,fmt);
  // IF09 - allocate space for 4. plane delta info - unused
  if (mpi->imgfmt == IMGFMT_IF09)
    {
      mpi->planes[0]=memalign(64, mpi->bpp*mpi->width*(mpi->height+2)/8+
			      mpi->chroma_width*mpi->chroma_height);
      /* delta table, just for fun ;) */
      mpi->planes[3]=mpi->planes[0]+2*(mpi->chroma_width*mpi->chroma_height);
    }
  else
    mpi->planes[0]=memalign(64, mpi->bpp*mpi->width*(mpi->height+2)/8);
  if(mpi->flags&MP_IMGFLAG_PLANAR){
    // YV12/I420/YVU9/IF09. feel free to add other planar formats here...
    if(!mpi->stride[0]) mpi->stride[0]=mpi->width;
    if(!mpi->stride[1]) mpi->stride[1]=mpi->stride[2]=mpi->chroma_width;
    if(mpi->flags&MP_IMGFLAG_SWAPPED){
      // I420/IYUV  (Y,U,V)
      mpi->planes[1]=mpi->planes[0]+mpi->width*mpi->height;
      mpi->planes[2]=mpi->planes[1]+mpi->chroma_width*mpi->chroma_height;
    } else {
      // YV12,YVU9,IF09  (Y,V,U)
      mpi->planes[2]=mpi->planes[0]+mpi->width*mpi->height;
      mpi->planes[1]=mpi->planes[2]+mpi->chroma_width*mpi->chroma_height;
    }
  } else {
    if(!mpi->stride[0]) mpi->stride[0]=mpi->width*mpi->bpp/8;
  }
  mpi->flags|=MP_IMGFLAG_ALLOCATED;
  
  return mpi;
}

void vf_menu_pause_update(struct vf_instance_s* vf) {
  vo_functions_t *video_out = mpctx_get_video_out(vf->priv->current->ctx);
  if(pause_mpi) {
    put_image(vf,pause_mpi, MP_NOPTS_VALUE);
    // Don't draw the osd atm
    //vf->control(vf,VFCTRL_DRAW_OSD,NULL);
    video_out->flip_page();
  }
}

static int cmd_filter(mp_cmd_t* cmd, int paused, struct vf_priv_s * priv) {

  switch(cmd->id) {
  case MP_CMD_PAUSE :
#if 0 // http://lists.mplayerhq.hu/pipermail/mplayer-dev-eng/2003-March/017286.html
    if(!paused && !go2pause) { // Initial pause cmd -> wait the next put_image
      go2pause = 1;
      return 1;
    }
#endif
    if(go2pause == 2) // Msg resent by put_image after saving the image
      go2pause = 0;
    break;
  case MP_CMD_MENU : {  // Convert txt cmd from the users into libmenu stuff
    char* arg = cmd->args[0].v.s;
    
    if(!priv->current->show && !(strcmp(arg,"hide") == 0) )
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
  
static void key_cb(int code) {
  menu_read_key(st_priv->current,code);
}



inline static void copy_mpi(mp_image_t *dmpi, mp_image_t *mpi) {
  if(mpi->flags&MP_IMGFLAG_PLANAR){
    memcpy_pic(dmpi->planes[0],mpi->planes[0], mpi->w, mpi->h,
	       dmpi->stride[0],mpi->stride[0]);
    memcpy_pic(dmpi->planes[1],mpi->planes[1], mpi->chroma_width, mpi->chroma_height,
	       dmpi->stride[1],mpi->stride[1]);
    memcpy_pic(dmpi->planes[2], mpi->planes[2], mpi->chroma_width, mpi->chroma_height,
	       dmpi->stride[2],mpi->stride[2]);
  } else {
    memcpy_pic(dmpi->planes[0],mpi->planes[0], 
	       mpi->w*(dmpi->bpp/8), mpi->h,
	       dmpi->stride[0],mpi->stride[0]);
  }
}



static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts){
  mp_image_t *dmpi = NULL;

  if (vf->priv->passthrough) {
    dmpi=vf_get_image(vf->next, IMGFMT_MPEGPES, MP_IMGTYPE_EXPORT,
                      0, mpi->w, mpi->h);
    dmpi->planes[0]=mpi->planes[0];
    return vf_next_put_image(vf,dmpi, pts);
  }

  if(vf->priv->current->show 
  || (vf->priv->current->parent && vf->priv->current->parent->show)) {
  // Close all menu who requested it
  while(vf->priv->current->cl && vf->priv->current != vf->priv->root) {
    menu_t* m = vf->priv->current;
    vf->priv->current = m->parent ? m->parent :  vf->priv->root;
    menu_close(m);
  }

  // Step 1 : save the picture
  while(go2pause == 1) {
    static char delay = 0; // Hack : wait the 2 frame to be sure to show the right picture
    delay ^= 1; // after a seek
    if(!delay) break;

    if(pause_mpi && (mpi->w != pause_mpi->w || mpi->h != pause_mpi->h ||
		     mpi->imgfmt != pause_mpi->imgfmt)) {
      free_mp_image(pause_mpi);
      pause_mpi = NULL;
    }
    if(!pause_mpi)
      pause_mpi = alloc_mpi(mpi->w,mpi->h,mpi->imgfmt);
    copy_mpi(pause_mpi,mpi);
    mp_input_queue_cmd(mp_input_parse_cmd("pause"));
    go2pause = 2;
    break;
  }

  // Grab // Ungrab the keys
  if(!mp_input_key_cb && vf->priv->current->show)
    mp_input_key_cb = key_cb;
  if(mp_input_key_cb && !vf->priv->current->show)
    mp_input_key_cb = NULL;

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
#ifdef HAVE_FREETYPE    
  // here is the right place to get screen dimensions
  if (force_load_font) {
    force_load_font = 0;
    load_font_ft(width,height);
  }
#endif
  if(outfmt == IMGFMT_MPEGPES)
    vf->priv->passthrough = 1;
  return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
  return (vf_next_query_format(vf,fmt));
}

static int open(vf_instance_t *vf, char* args){
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
  open,
  NULL
};



