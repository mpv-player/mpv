#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <X11/extensions/XvMClib.h>

#include "config.h"
#include "mp_msg.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "fastmemcpy.h"

#include "x11_common.h"
#include "xvmc_render.h"

#include "sub.h"
#include "aspect.h"

#ifdef HAVE_NEW_GUI
#include "../Gui/interface.h"
#endif

#undef HAVE_XINERAMA

#undef NDEBUG 
#include <assert.h>

//no chanse xinerama to be suported in near future

#define UNUSED(x) ((void)(x))


extern int vo_directrendering;
extern int vo_verbose;

static void xvmc_free(void);

static int image_width,image_height;
static uint32_t  drwX,drwY;

static XvPortID xv_port;

//0-auto;1-backgound always keycolor;2-autopaint(by X);3-manual fill 
static int keycolor_handling;
static unsigned long keycolor;

static XvMCSurfaceInfo surface_info;
static XvMCContext ctx;
static XvMCBlockArray data_blocks;
static XvMCMacroBlockArray mv_blocks;

#define MAX_SURFACES 8
static int number_of_surfaces=0;
static XvMCSurface surface_array[MAX_SURFACES];
static xvmc_render_state_t * surface_render;

static xvmc_render_state_t * p_render_surface_to_show=NULL;
static xvmc_render_state_t * p_render_surface_visible=NULL;

static vo_info_t info = {
  "XVideo Motion Compensation",
  "xvmc",
  "Ivan Kalvachev <iive@users.sf.net>",
  ""
};

LIBVO_EXTERN(xvmc);

static void  init_keycolor(){
Atom xv_atom;
XvAttribute * attributes;
int colorkey;
int rez;
int attrib_count,i;

   keycolor=2110;

   if(keycolor_handling == 0){
   //XV_AUTOPING_COLORKEY doesn't work for XvMC yet(NVidia 43.63)
      attributes = XvQueryPortAttributes(mDisplay, xv_port, &attrib_count);
      if(attributes!=NULL) 
         for (i = 0; i < attrib_count; i++)
            if (!strcmp(attributes[i].name, "XV_AUTOPAINT_COLORKEY"))
            {
               xv_atom = XInternAtom(mDisplay, "XV_AUTOPAINT_COLORKEY", False);
               if(xv_atom!=None)
               {
                  rez=XvSetPortAttribute(mDisplay, xv_port, xv_atom, 1);
                  if(rez == Success) 
                     keycolor_handling = 2;//this is the way vo_xv works
               }
               break;
            }
         XFree(attributes);
   }

   xv_atom = XInternAtom(mDisplay, "XV_COLORKEY",False);
   if(xv_atom == None) return;
   rez=XvGetPortAttribute(mDisplay,xv_port, xv_atom, &colorkey);
   if(rez == Success){
      keycolor=colorkey;
      if(keycolor_handling == 0){
         keycolor_handling = 3;
      }
   }
}

//from vo_xmga
static void mDrawColorKey(uint32_t x,uint32_t  y, uint32_t w, uint32_t h)
{
   if( (keycolor_handling != 2) && (keycolor_handling != 3) ) 
      return ;//unknow method

   XSetBackground( mDisplay,vo_gc,0 );
   XClearWindow( mDisplay,vo_window );
 
   if(keycolor_handling == 3){
      XSetForeground( mDisplay,vo_gc,keycolor );
      XFillRectangle( mDisplay,vo_window,vo_gc,x,y,w,h);
   }
   XFlush( mDisplay );
}

// now it is ugly, but i need it working
static int xvmc_check_surface_format(uint32_t format, XvMCSurfaceInfo * surf_info){
   if ( format == IMGFMT_XVMC_IDCT_MPEG2 ){ 
      if( surf_info->mc_type != (XVMC_IDCT|XVMC_MPEG_2) ) return -1;
      if( surf_info->chroma_format != XVMC_CHROMA_FORMAT_420 ) return -1;
      return 0;
   }      
   if ( format == IMGFMT_XVMC_MOCO_MPEG2 ){ 
      if(surf_info->mc_type != XVMC_MPEG_2) return -1;
      if(surf_info->chroma_format != XVMC_CHROMA_FORMAT_420) return -1;
      return 0;
   }      
return -1;//fail
}

// WARNING This function may changes xv_port and surface_info!
static int xvmc_find_surface_by_format(int format,int width,int height, 
                       XvMCSurfaceInfo * surf_info,int query){
int rez;
XvAdaptorInfo * ai;
int num_adaptors,i;
unsigned long p;
int s,mc_surf_num;
XvMCSurfaceInfo * mc_surf_list;
   
   rez = XvQueryAdaptors(mDisplay,DefaultRootWindow(mDisplay),&num_adaptors,&ai);
   if( rez != Success ) return -1;
   if( verbose > 2 ) printf("vo_xvmc: Querying %d adaptors\n",num_adaptors);
   for(i=0; i<num_adaptors; i++)
   {
      if( verbose > 2) printf("vo_xvmc: Quering adaptor #%d\n",i);
      if( ai[i].type == 0 ) continue;// we need at least dummy type!
//probing ports
      for(p=ai[i].base_id; p<ai[i].base_id+ai[i].num_ports; p++)
      {
         if( verbose > 2) printf("vo_xvmc: probing port #%ld\n",p);
	 mc_surf_list = XvMCListSurfaceTypes(mDisplay,p,&mc_surf_num);
	 if( mc_surf_list == NULL || mc_surf_num == 0){
	    if( verbose > 2) printf("vo_xvmc: No XvMC supported. \n");
	    continue;
	 }
	 if( verbose > 2) printf("vo_xvmc: XvMC list have %d surfaces\n",mc_surf_num);
//we have XvMC list!
         for(s=0; s<mc_surf_num; s++)
         {
	    if( width > mc_surf_list[s].max_width ) continue;
	    if( height > mc_surf_list[s].max_height ) continue;
            if( xvmc_check_surface_format(format,&mc_surf_list[s])<0 ) continue;
//we have match!
	    
	    if(!query){
	       rez = XvGrabPort(mDisplay,p,CurrentTime);
	       if(rez != Success){
	          if (verbose > 2) printf("vo_xvmc: Fail to grab port %ld\n",p);
	          continue;
	       }
	       printf("vo_xvmc: Port %ld grabed\n",p);
	       xv_port = p;
	    }
	    goto surface_found;
	 }//for mc surf
	 XFree(mc_surf_list);//if mc_surf_num==0 is list==NULL ?
      }//for ports
   }//for adaptors
   
   if(!query) printf("vo_xvmc: Could not find free matching surface. Sorry.\n");
   return 0;

// somebody know cleaner way to escape from 3 internal loops?
surface_found:

   memcpy(surf_info,&mc_surf_list[s],sizeof(XvMCSurfaceInfo));
   if( verbose > 2 || !query) 
      printf("vo_xvmc: Found matching surface with id=%X on %ld port at %d adapter\n",
             mc_surf_list[s].surface_type_id,p,i);
   return mc_surf_list[s].surface_type_id;
}

static uint32_t xvmc_draw_image(mp_image_t *mpi){
xvmc_render_state_t * rndr;

   assert(mpi!=NULL);
   assert(mpi->flags &MP_IMGFLAG_DIRECT);
//   assert(mpi->flags &MP_IMGFLAGS_DRAWBACK);
   
   rndr = (xvmc_render_state_t*)mpi->priv;//there is copy in plane[2]
   assert( rndr != NULL );
   assert( rndr->magic == MP_XVMC_RENDER_MAGIC );
   if( verbose > 3 ) 
       printf("vo_xvmc: draw_image(show rndr=%p)\n",rndr);
// the surface have passed vf system without been skiped, it will be displayed
   rndr->state |= MP_XVMC_STATE_DISPLAY_PENDING;
   p_render_surface_to_show = rndr;
   return VO_TRUE;
}

static uint32_t preinit(const char *arg){
int xv_version,xv_release,xv_request_base,xv_event_base,xv_error_base;
int mc_eventBase,mc_errorBase;
int mc_ver,mc_rev;

//Obtain display handler
   if (!vo_init()) return -1;//vo_xv

  //XvMC is subdivision of XVideo
   if (Success != XvQueryExtension(mDisplay,&xv_version,&xv_release,&xv_request_base,
			 &xv_event_base,&xv_error_base) ){
      mp_msg(MSGT_VO,MSGL_ERR,"Sorry, Xv(MC) not supported by this X11 version/driver\n");
      mp_msg(MSGT_VO,MSGL_ERR,"********** Try with  -vo x11  or  -vo sdl  ***********\n");
      return -1;
   }
   printf("vo_xvmc: X-Video extension %d.%d\n",xv_version,xv_release);
   
   if( True != XvMCQueryExtension(mDisplay,&mc_eventBase,&mc_errorBase) ){	
      printf("vo_xvmc: No X-Video MotionCompensation Extension on %s\n",
	      XDisplayName(NULL));
      return -1;
   }
    
   if(Success == XvMCQueryVersion(mDisplay, &mc_ver, &mc_rev) ){
      printf("vo_xvmc: X-Video MotionCompensation Extension version %i.%i\n",
		mc_ver,mc_rev);
   }
   else{
      printf("vo_xvmc: Error querying version info!\n");
      return -1;
   }
   xv_port = 0;
   number_of_surfaces = 0;
   keycolor_handling = 3;//!!fixme
   surface_render=NULL;
   
   return 0;
}

static uint32_t config(uint32_t width, uint32_t height,
		       uint32_t d_width, uint32_t d_height,
		       uint32_t flags, char *title, uint32_t format){
int i,mode_id,rez;
int numblocks,blocks_per_macroblock;//bpmb we have 6,8,12

//from vo_xv
char *hello = (title == NULL) ? "XvMC render" : title;
XSizeHints hint;
XVisualInfo vinfo;
XGCValues xgcv;
XSetWindowAttributes xswa;
XWindowAttributes attribs;
unsigned long xswamask;
int depth;
#ifdef HAVE_XF86VM
int vm=0;
unsigned int modeline_width, modeline_height;
static uint32_t vm_width;
static uint32_t vm_height;
#endif
//end of vo_xv

   if( !IMGFMT_IS_XVMC(format) )
   {
      assert(0);//should never happen, abort on debug or
      return 1;//return error on relese
   }

// Find free port that supports MC, by querying adaptors
   if( xv_port != 0 || number_of_surfaces != 0 ){
      xvmc_free();
   };
   numblocks=((width+15)/16)*((height+15)/16);
// Find Supported Surface Type
   mode_id = xvmc_find_surface_by_format(format,width,height,&surface_info,0);//false=1 to grab port, not query
   
   rez = XvMCCreateContext(mDisplay, xv_port,mode_id,width,height,XVMC_DIRECT,&ctx);
   if( rez != Success ) return -1;
   if( ctx.flags & XVMC_DIRECT ){
      printf("vo_xvmc: Allocated Direct Context\n");
   }else{
      printf("vo_xvmc: Allocated Indirect Context!\n");
   }

   blocks_per_macroblock = 6;
   if(surface_info.chroma_format == XVMC_CHROMA_FORMAT_422)
      blocks_per_macroblock = 8;
   if(surface_info.chroma_format ==  XVMC_CHROMA_FORMAT_444)
      blocks_per_macroblock = 12;

   rez = XvMCCreateBlocks(mDisplay,&ctx,numblocks*blocks_per_macroblock,&data_blocks);
   if( rez != Success ){
      XvMCDestroyContext(mDisplay,&ctx);
      return -1;
   }
   printf("vo_xvmc: data_blocks allocated\n");

   rez = XvMCCreateMacroBlocks(mDisplay,&ctx,numblocks,&mv_blocks);
   if( rez != Success ){
      XvMCDestroyBlocks(mDisplay,&data_blocks);
      XvMCDestroyContext(mDisplay,&ctx);
      return -1;
   }
   printf("vo_xvmc: mv_blocks allocated\n");

   if(surface_render==NULL)
      surface_render=malloc(MAX_SURFACES*sizeof(xvmc_render_state_t));//easy mem debug
   
   for(i=0; i<MAX_SURFACES; i++){
      rez=XvMCCreateSurface(mDisplay,&ctx,&surface_array[i]);
      if( rez != Success )
	 break;
      memset(&surface_render[i],0,sizeof(xvmc_render_state_t));
      surface_render[i].magic = MP_XVMC_RENDER_MAGIC;
      surface_render[i].data_blocks = data_blocks.blocks;
      surface_render[i].mv_blocks = mv_blocks.macro_blocks;
      surface_render[i].total_number_of_mv_blocks = numblocks;
      surface_render[i].total_number_of_data_blocks = numblocks*blocks_per_macroblock;;
      surface_render[i].mc_type = surface_info.mc_type & (~XVMC_IDCT);
      surface_render[i].idct = (surface_info.mc_type & XVMC_IDCT) == XVMC_IDCT;
      surface_render[i].chroma_format = surface_info.chroma_format;
      surface_render[i].unsigned_intra = (surface_info.flags & XVMC_INTRA_UNSIGNED) == XVMC_INTRA_UNSIGNED;
      surface_render[i].p_surface = &surface_array[i];
      if( verbose > 3 )
          printf("vo_xvmc: surface[%d] = %p .rndr=%p\n",i,&surface_array[i], &surface_render[i]);
   }
   number_of_surfaces = i;
   if( number_of_surfaces < 4 ){// +2 I or P and +2 for B (to avoid visible motion drawing)
      printf("vo_xvmc: Unable to allocate at least 4 Surfaces\n");
      uninit();
      return -1;
   }
   printf("vo_xvmc: Motion Compensation context allocated - %d surfaces\n",
          number_of_surfaces);

  //debug
   printf("vo_xvmc: idct=%d unsigned_intra=%d\n",
           (surface_info.mc_type & XVMC_IDCT) == XVMC_IDCT,
	   (surface_info.flags & XVMC_INTRA_UNSIGNED) == XVMC_INTRA_UNSIGNED);

   init_keycolor();// take keycolor value and choose method for handling it

//taken from vo_xv
   panscan_init();

   aspect_save_orig(width,height);
   aspect_save_prescale(d_width,d_height);

   image_height = height;
   image_width = width;
 
   vo_mouse_autohide = 1;

   vo_dx=( vo_screenwidth - d_width ) / 2; vo_dy=( vo_screenheight - d_height ) / 2;
   geometry(&vo_dx, &vo_dy, &d_width, &d_height, vo_screenwidth, vo_screenheight);
   vo_dwidth=d_width; vo_dheight=d_height;
     
#ifdef HAVE_XF86VM
   if( flags&0x02 ) vm = 1;
#endif

   aspect_save_screenres(vo_screenwidth,vo_screenheight);

#ifdef HAVE_NEW_GUI
   if(use_gui)
      guiGetEvent( guiSetShVideo,0 ); // let the GUI to setup/resize our window
   else
#endif
   {
      hint.x = vo_dx;
      hint.y = vo_dy;
      aspect(&d_width,&d_height,A_NOZOOM);
      hint.width = d_width;
      hint.height = d_height;
#ifdef HAVE_XF86VM
      if ( vm )
      {
	 if ((d_width==0) && (d_height==0))
	    { vm_width=image_width; vm_height=image_height; }
	 else
	    { vm_width=d_width; vm_height=d_height; }
	 vo_vm_switch(vm_width, vm_height,&modeline_width, &modeline_height);
	 hint.x=(vo_screenwidth-modeline_width)/2;
	 hint.y=(vo_screenheight-modeline_height)/2;
	 hint.width=modeline_width;
	 hint.height=modeline_height;
	 aspect_save_screenres(modeline_width,modeline_height);
      }
      else
#endif
      if ( vo_fs )
      {
#ifdef X11_FULLSCREEN
     /* this code replaces X11_FULLSCREEN hack in mplayer.c
      * aspect() is available through aspect.h for all vos.
      * besides zooming should only be done with -zoom,
      * but I leave the old -fs behaviour so users don't get
      * irritated for now (and send lots o' mails ;) ::atmos
      */

         aspect(&d_width,&d_height,A_ZOOM);
#endif

      }
   vo_dwidth=d_width; vo_dheight=d_height;
   hint.flags = PPosition | PSize /* | PBaseSize */;
   hint.base_width = hint.width; hint.base_height = hint.height;
   XGetWindowAttributes(mDisplay, DefaultRootWindow(mDisplay), &attribs);
   depth=attribs.depth;
   if (depth != 15 && depth != 16 && depth != 24 && depth != 32) depth = 24;
   XMatchVisualInfo(mDisplay, mScreen, depth, TrueColor, &vinfo);

   xswa.background_pixel = 0;
   if (keycolor_handling == 1)
      xswa.background_pixel = keycolor;// 2110;
   xswa.border_pixel     = 0;
   xswamask = CWBackPixel | CWBorderPixel;

   if ( WinID>=0 ){
      vo_window = WinID ? ((Window)WinID) : mRootWin;
      if ( WinID ) 
      {
         XUnmapWindow( mDisplay,vo_window );
         XChangeWindowAttributes( mDisplay,vo_window,xswamask,&xswa );
	 vo_x11_selectinput_witherr( mDisplay,vo_window,StructureNotifyMask | KeyPressMask | PropertyChangeMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask | ExposureMask );
         XMapWindow( mDisplay,vo_window );
      } else { drwX=vo_dx; drwY=vo_dy; }
   } else 
      if ( vo_window == None ){
         vo_window = XCreateWindow(mDisplay, mRootWin,
              hint.x, hint.y, hint.width, hint.height,
              0, depth,CopyFromParent,vinfo.visual,xswamask,&xswa);

         vo_x11_classhint( mDisplay,vo_window,"xvmc" );
         vo_hidecursor(mDisplay,vo_window);

         vo_x11_selectinput_witherr(mDisplay, vo_window, StructureNotifyMask | KeyPressMask | PropertyChangeMask | ExposureMask |
	      ((WinID==0) ? 0 : (PointerMotionMask
		| ButtonPressMask | ButtonReleaseMask)) );
         XSetStandardProperties(mDisplay, vo_window, hello, hello, None, NULL, 0, &hint);
         XSetWMNormalHints( mDisplay,vo_window,&hint );
	 XMapWindow(mDisplay, vo_window);
	 if ( flags&1 ) vo_x11_fullscreen();
	 else {
#ifdef HAVE_XINERAMA
	    vo_x11_xinerama_move(mDisplay,vo_window);
#endif
	    vo_x11_sizehint( hint.x, hint.y, hint.width, hint.height,0 );
	 }
      } else {
	// vo_fs set means we were already at fullscreen
	 vo_x11_sizehint( hint.x, hint.y, hint.width, hint.height,0 );
	 if ( !vo_fs ) XMoveResizeWindow( mDisplay,vo_window,hint.x,hint.y,hint.width,hint.height );
	 if ( flags&1 && !vo_fs ) vo_x11_fullscreen(); // handle -fs on non-first file
      }
    
//    vo_x11_sizehint( hint.x, hint.y, hint.width, hint.height,0 );   
    
      if ( vo_gc != None ) XFreeGC( mDisplay,vo_gc );
      vo_gc = XCreateGC(mDisplay, vo_window, GCForeground, &xgcv);
      XFlush(mDisplay);
      XSync(mDisplay, False);
#ifdef HAVE_XF86VM
      if ( vm )
      {
      /* Grab the mouse pointer in our window */
         if(vo_grabpointer)
         XGrabPointer(mDisplay, vo_window, True, 0,
                      GrabModeAsync, GrabModeAsync,
                      vo_window, None, CurrentTime );
         XSetInputFocus(mDisplay, vo_window, RevertToNone, CurrentTime);
      }
#endif
   }

   aspect(&vo_dwidth,&vo_dheight,A_NOZOOM);
   if ( (( flags&1 )&&( WinID <= 0 )) || vo_fs )
   {
      aspect(&vo_dwidth,&vo_dheight,A_ZOOM);
      drwX=( vo_screenwidth - (vo_dwidth > vo_screenwidth?vo_screenwidth:vo_dwidth) ) / 2;
      drwY=( vo_screenheight - (vo_dheight > vo_screenheight?vo_screenheight:vo_dheight) ) / 2;
      vo_dwidth=(vo_dwidth > vo_screenwidth?vo_screenwidth:vo_dwidth);
      vo_dheight=(vo_dheight > vo_screenheight?vo_screenheight:vo_dheight);
      mp_msg(MSGT_VO,MSGL_V, "[xvmc-fs] dx: %d dy: %d dw: %d dh: %d\n",drwX,drwY,vo_dwidth,vo_dheight );
   }

   panscan_calc();

   mp_msg(MSGT_VO,MSGL_V, "[xvmc] dx: %d dy: %d dw: %d dh: %d\n",drwX,drwY,vo_dwidth,vo_dheight );

   saver_off(mDisplay);  // turning off screen saver
//end vo_xv
   
   /* store image dimesions for displaying */
   p_render_surface_visible = NULL;
   p_render_surface_to_show = NULL;
 
   vo_directrendering = 1;//ugly hack, coz xvmc works only with direct rendering
   return 0;		
}

static uint32_t draw_frame(uint8_t *srcp[]){
assert(0 && srcp==NULL);//silense unused srcp warning
}

static void draw_osd(void){
}

static void xvmc_sync_surface(XvMCSurface * srf){
int status,rez;
   rez = XvMCGetSurfaceStatus(mDisplay,srf,&status);
   assert(rez==Success);
   if( status & XVMC_RENDERING )
      XvMCSyncSurface(mDisplay, srf);
/*
   rez = XvMCFlushSurface(mDisplay, srf);
   assert(rez==Success);

   do {
   usleep(1);
   printf("waiting...\n");
   XvMCGetSurfaceStatus(mDisplay,srf,&status);
   } while (status & XVMC_RENDERING);       
*/
}

static void flip_page(void){
int rez;
int clipX,clipY,clipW,clipH;

   clipX = drwX-(vo_panscan_x>>1);
   clipY = drwY-(vo_panscan_y>>1); 
   clipW = vo_dwidth+vo_panscan_x;
   clipH = vo_dheight+vo_panscan_y;//
   
   if( verbose > 3 ) 
      printf("vo_xvmc: flip_page  show(rndr=%p)\n\n",p_render_surface_to_show);

   if(p_render_surface_to_show == NULL) return;
   assert( p_render_surface_to_show->magic == MP_XVMC_RENDER_MAGIC );
//fixme   assert( p_render_surface_to_show != p_render_surface_visible);
   
// make sure the rendering is done
   xvmc_sync_surface(p_render_surface_to_show->p_surface);

//the visible surface won't be displayed anymore, mark it as free
   if( p_render_surface_visible!=NULL ) 
      p_render_surface_visible->state &= ~MP_XVMC_STATE_DISPLAY_PENDING;

//!!fixme   assert(p_render_surface_to_show->state & MP_XVMC_STATE_DISPLAY_PENDING);

// show it
// if(benchmark)
   rez=XvMCPutSurface(mDisplay, p_render_surface_to_show->p_surface, vo_window,
                      0, 0, image_width, image_height,
                      clipX, clipY, clipW, clipH,
                      3);//p_render_surface_to_show->display_flags);

   assert(rez==Success);
   
   p_render_surface_visible = p_render_surface_to_show;
   p_render_surface_to_show = NULL;
}

static void check_events(void){
int dwidth,dheight;
Window mRoot;
uint32_t drwBorderWidth,drwDepth;

int e=vo_x11_check_events(mDisplay);
   if(e&VO_EVENT_RESIZE)
   {
      e |= VO_EVENT_EXPOSE;
      
      XGetGeometry( mDisplay,vo_window,&mRoot,&drwX,&drwY,&vo_dwidth,&vo_dheight,
                   &drwBorderWidth,&drwDepth );
      drwX = drwY = 0;
      mp_msg(MSGT_VO,MSGL_V, "[xvmc] dx: %d dy: %d dw: %d dh: %d\n",drwX,drwY,
              vo_dwidth,vo_dheight );

      aspect(&dwidth,&dheight,A_NOZOOM);
      if ( vo_fs )
      {
         aspect(&dwidth,&dheight,A_ZOOM);
         drwX=( vo_screenwidth - (dwidth > vo_screenwidth?vo_screenwidth:dwidth) ) / 2;
         drwY=( vo_screenheight - (dheight > vo_screenheight?vo_screenheight:dheight) ) / 2;
         vo_dwidth=(dwidth > vo_screenwidth?vo_screenwidth:dwidth);
         vo_dheight=(dheight > vo_screenheight?vo_screenheight:dheight);
         mp_msg(MSGT_VO,MSGL_V, "[xvmc-fs] dx: %d dy: %d dw: %d dh: %d\n",drwX,drwY,vo_dwidth,vo_dheight );
      }
   }
   if ( e & VO_EVENT_EXPOSE )
   {
      mDrawColorKey(drwX,drwY,vo_dwidth,vo_dheight);     
     if(p_render_surface_visible != NULL)
      XvMCPutSurface(mDisplay, p_render_surface_visible->p_surface,vo_window,
                     0, 0, image_width, image_height,
                     drwX,drwY,vo_dwidth,vo_dheight,
                     3);//,p_render_surface_visible->display_flags);!!
   }
}

static void xvmc_free(void){
int i;

   if( number_of_surfaces ){
      XvMCDestroyMacroBlocks(mDisplay,&mv_blocks);
      XvMCDestroyBlocks(mDisplay,&data_blocks);
      for(i=0; i<number_of_surfaces; i++)
      {
         XvMCHideSurface(mDisplay,&surface_array[i]);//it doesn't hurt, I hope
         XvMCDestroySurface(mDisplay,&surface_array[i]);

         if( (surface_render[i].state != 0) && 
             (p_render_surface_visible != &surface_render[i]) )
            printf("vo_xvmc::uninit surface_render[%d].status=%d\n",i,
                    surface_render[i].state); 
      }
      
      free(surface_render);surface_render=NULL;

      XvMCDestroyContext(mDisplay,&ctx);
      if( verbose > 3) printf("vo_xvmc: Context sucessfuly freed\n");
      number_of_surfaces = 0;
   }
   if( xv_port !=0 ){
      XvUngrabPort(mDisplay,xv_port,CurrentTime);
      xv_port = 0;
      if( verbose > 3) printf("vo_xvmc: xv_port sucessfuly ungrabed\n");
   }
}

static void uninit(void){
   if( verbose > 3 ) printf("vo_xvmc: uninit called\n");
   xvmc_free();
 //from vo_xv
   saver_on(mDisplay);
   vo_vm_close(mDisplay);
   vo_x11_uninit();
}

static uint32_t query_format(uint32_t format){
uint32_t flags;
XvMCSurfaceInfo qsurface_info;
int mode_id;

   if(verbose > 3)
      printf("vo_xvmc: query_format=%X\n",format);

   if(!IMGFMT_IS_XVMC(format)) return 0;// no caps supported
   mode_id = xvmc_find_surface_by_format(format, 16, 16, &qsurface_info, 1);//true=1 - quering

   if( mode_id == 0 ) return 0;
   
   flags = VFCAP_CSP_SUPPORTED |
           VFCAP_CSP_SUPPORTED_BY_HW |
	   VFCAP_ACCEPT_STRIDE;
  
// if(surfce_info.subpicture)
//    flags|=VFCAP_OSD;
   return flags;
}


static uint32_t draw_slice(uint8_t *image[], int stride[],
			   int w, int h, int x, int y){
xvmc_render_state_t * rndr;
int rez;

   if(verbose > 3)
      printf("vo_xvmc: draw_slice y=%d\n",y);

   rndr = (xvmc_render_state_t*)image[2];//this is copy of priv-ate
   assert( rndr != NULL );
   assert( rndr->magic == MP_XVMC_RENDER_MAGIC );

   rez = XvMCRenderSurface(mDisplay,&ctx,rndr->picture_structure,
             		   rndr->p_surface,
                           rndr->p_past_surface,
                           rndr->p_future_surface,
                           rndr->flags,
                           rndr->filled_mv_blocks_num,rndr->start_mv_blocks_num,
                           &mv_blocks,&data_blocks);
#if 1
   if(rez!=Success)
   {
   int i;
      printf("vo_xvmc::slice: RenderSirface returned %d\n",rez);
   
      printf("vo_xvmc::slice: pict=%d,flags=%x,start_blocks=%d,num_blocks=%d\n",
             rndr->picture_structure,rndr->flags,rndr->start_mv_blocks_num,
             rndr->filled_mv_blocks_num);
      printf("vo_xvmc::slice: this_surf=%p, past_surf=%p, future_surf=%p\n",
             rndr->p_surface,rndr->p_past_surface,rndr->p_future_surface);

      for(i=0;i<rndr->filled_mv_blocks_num;i++){
       XvMCMacroBlock* testblock;
         testblock=&mv_blocks.macro_blocks[i];

	 printf("vo_xvmc::slice: mv_block - x=%d,y=%d,mb_type=0x%x,mv_type=0x%x,mv_field_select=%d\n",
	        testblock->x,testblock->y,testblock->macroblock_type,
	        testblock->motion_type,testblock->motion_vertical_field_select);
         printf("vo_xvmc::slice: dct_type=%d,data_index=0x%x,cbp=%d,pad0=%d\n",
	         testblock->dct_type,testblock->index,testblock->coded_block_pattern,
	         testblock->pad0);
         printf("vo_xvmc::slice: PMV[0][0][0/1]=(%d,%d)\n",
	         testblock->PMV[0][0][0],testblock->PMV[0][0][1]);

       }

   }
#endif
   assert(rez==Success);
   if(verbose > 3 ) printf("vo_xvmc: flush surface\n");
   rez = XvMCFlushSurface(mDisplay, rndr->p_surface);
   assert(rez==Success);

//   rndr->start_mv_blocks_num += rndr->filled_mv_blocks_num;
   rndr->start_mv_blocks_num = 0;
   rndr->filled_mv_blocks_num = 0;

   rndr->next_free_data_block_num = 0;

   return VO_TRUE;
}


static inline int find_free_surface(){
int i,j,t;
int stat;

   j=-1;
   for(i=0; i<number_of_surfaces; i++){
//      printf("vo_xvmc: surface[%d].state=%d ( surf=%p)\n",i,
//          surface_render[i].state, surface_render[i].p_surface);
      if( surface_render[i].state == 0){
         XvMCGetSurfaceStatus(mDisplay, surface_render[i].p_surface,&stat);
         if( (stat & XVMC_DISPLAYING) == 0 ) return i;
         j=i;
      }
   }
   if(j>=0){//all surfaces are busy, but there is one that will be free
   //on next monitor retrace, we just have to wait
       for(t=0;t<1000;t++){ 
//          usleep(10); //!!!
          printf("vo_xvmc: waiting retrace\n");
          XvMCGetSurfaceStatus(mDisplay, surface_render[j].p_surface,&stat);
          if( (stat & XVMC_DISPLAYING) == 0 ) return j;
       }
       assert(0);//10 seconds wait for surface to get free!
       exit(1);
   }
   return -1;
}

static uint32_t get_image(mp_image_t *mpi){
int getsrf;

   
   getsrf=find_free_surface();
   if(getsrf<0){
   int i;
      printf("vo_xvmc: no free surfaces, this should not happen in g1\n");
      for(i=0;i<number_of_surfaces;i++)
         printf("vo_xvmc: surface[%d].state=%d\n",i,surface_render[i].state);
      return VO_FALSE;
   }

assert(surface_render[getsrf].start_mv_blocks_num == 0);
assert(surface_render[getsrf].filled_mv_blocks_num == 0);
assert(surface_render[getsrf].next_free_data_block_num == 0);

   mpi->flags |= MP_IMGFLAG_DIRECT;
//keep strides 0 to avoid field manipulations
   mpi->stride[0] = 0;
   mpi->stride[1] = 0;
   mpi->stride[2] = 0;

// these are shared!! so watch out
// do call RenderSurface before overwriting
   mpi->planes[0] = (char*)data_blocks.blocks;   
   mpi->planes[1] = (char*)mv_blocks.macro_blocks;
   mpi->priv =
   mpi->planes[2] = (char*)&surface_render[getsrf];

   surface_render[getsrf].picture_structure = 0;
   surface_render[getsrf].flags = 0;
   surface_render[getsrf].state = 0;
   surface_render[getsrf].start_mv_blocks_num = 0;
   surface_render[getsrf].filled_mv_blocks_num = 0;
   surface_render[getsrf].next_free_data_block_num = 0;
   
   if( verbose > 3 ) 
      printf("vo_xvmc: get_image:     .rndr=%p surface[%d]=%p \n",  
              mpi->priv,getsrf,surface_render[getsrf].p_surface);
return VO_TRUE;   
}

static uint32_t control(uint32_t request, void *data, ... )
{
   switch (request){
      case VOCTRL_QUERY_FORMAT:
         return query_format(*((uint32_t*)data));
      case VOCTRL_DRAW_IMAGE:
         return xvmc_draw_image((mp_image_t *)data);
      case VOCTRL_GET_IMAGE:
	 return get_image((mp_image_t *)data);
      //vo_xv
      case VOCTRL_GUISUPPORT:
         return VO_TRUE;
      case VOCTRL_FULLSCREEN:
         vo_x11_fullscreen();
      case VOCTRL_GET_PANSCAN:
         if ( !vo_config_count || !vo_fs ) return VO_FALSE;
         return VO_TRUE;
      // indended, fallthrough to update panscan on fullscreen/windowed switch 
      case VOCTRL_SET_PANSCAN:
         if ( ( vo_fs && ( vo_panscan != vo_panscan_amount ) ) || ( !vo_fs && vo_panscan_amount ) )
         {
            int old_y = vo_panscan_y;
            panscan_calc();
      
            if(old_y != vo_panscan_y)
            {
               XClearWindow(mDisplay, vo_window);
               XFlush(mDisplay);
            }
         }
         return VO_TRUE;

      case VOCTRL_SET_EQUALIZER:
      {
      va_list ap;
      int value;
    
         va_start(ap, data);
         value = va_arg(ap, int);
         va_end(ap);
    
         return(vo_xv_set_eq(xv_port, data, value));
      }

      case VOCTRL_GET_EQUALIZER:
      {
      va_list ap;
      int *value;
    
         va_start(ap, data);
         value = va_arg(ap, int*);
         va_end(ap);
    
         return(vo_xv_get_eq(xv_port, data, value));
      }
   }
return VO_NOTIMPL;
}

