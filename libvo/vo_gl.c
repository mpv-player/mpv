#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "config.h"
#include "mp_msg.h"
#include "subopt-helper.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "font_load.h"
#include "sub.h"

#include "gl_common.h"
#include "aspect.h"
#ifdef HAVE_NEW_GUI
#include "Gui/interface.h"
#endif

static vo_info_t info = 
{
	"X11 (OpenGL)",
	"gl",
	"Arpad Gereoffy <arpi@esp-team.scene.hu>",
	""
};

LIBVO_EXTERN(gl)

#ifdef GL_WIN32
static int gl_vinfo = 0;
static HGLRC gl_context = 0;
#else
static XVisualInfo *gl_vinfo = NULL;
static GLXContext gl_context = 0;
static int                  wsGLXAttrib[] = { GLX_RGBA,
                                       GLX_RED_SIZE,1,
                                       GLX_GREEN_SIZE,1,
                                       GLX_BLUE_SIZE,1,
                                       GLX_DOUBLEBUFFER,
                                       None };
#endif

static int use_osd;
static int scaled_osd;
//! How many parts the OSD may consist of at most
#define MAX_OSD_PARTS 20
//! Textures for OSD
static GLuint osdtex[MAX_OSD_PARTS];
#ifndef FAST_OSD
//! Alpha textures for OSD
static GLuint osdatex[MAX_OSD_PARTS];
#endif
//! Display lists that draw the OSD parts
static GLuint osdDispList[MAX_OSD_PARTS];
//! How many parts the OSD currently consists of
static int osdtexCnt;

static int use_aspect;
static int use_yuv;
static int use_rectangle;
static int err_shown;
static uint32_t image_width;
static uint32_t image_height;
static uint32_t image_format;
static int many_fmts;
static int use_glFinish;
static int swap_interval;
static GLenum gl_target;
static GLint gl_texfmt;
static GLenum gl_format;
static GLenum gl_type;
static GLuint gl_buffer;
static int gl_buffersize;
static GLuint fragprog;
static GLuint uvtexs[2];
static GLuint lookupTex;
static char *custom_prog;
static char *custom_tex;
static int custom_tlin;

static int int_pause;
static int eq_bri = 0;
static int eq_cont = 0;
static int eq_sat = 0;
static int eq_hue = 0;
static int eq_rgamma = 0;
static int eq_ggamma = 0;
static int eq_bgamma = 0;

static uint32_t texture_width;
static uint32_t texture_height;

static unsigned int slice_height = 1;

static void resize(int x,int y){
  mp_msg(MSGT_VO, MSGL_V, "[gl] Resize: %dx%d\n",x,y);
  if (WinID >= 0) {
    int top = 0, left = 0, w = x, h = y;
    geometry(&top, &left, &w, &h, vo_screenwidth, vo_screenheight);
    glViewport(top, left, w, h);
  } else
  glViewport( 0, 0, x, y );

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  if (vo_fs && use_aspect) {
    int new_w, new_h;
    GLdouble scale_x, scale_y;
    aspect(&new_w, &new_h, A_ZOOM);
    panscan_calc();
    new_w += vo_panscan_x;
    new_h += vo_panscan_y;
    scale_x = (GLdouble) new_w / (GLdouble) x;
    scale_y = (GLdouble) new_h / (GLdouble) y;
    glScaled(scale_x, scale_y, 1);
  }
  glOrtho(0, image_width, image_height, 0, -1,1);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  if (!scaled_osd) {
#ifdef HAVE_FREETYPE
  // adjust font size to display size
  force_load_font = 1;
#endif
  vo_osd_changed(OSDTYPE_OSD);
  }
  if (vo_fs && use_aspect && !vo_doublebuffering)
    glClear(GL_COLOR_BUFFER_BIT);
}

static void texSize(int w, int h, int *texw, int *texh) {
  if (use_rectangle) {
    *texw = w; *texh = h;
  } else {
    *texw = 32;
    while (*texw < w)
      *texw *= 2;
    *texh = 32;
    while (*texh < h)
      *texh *= 2;
  }
}

//! maximum size of custom fragment program
#define MAX_CUSTOM_PROG_SIZE (1024 * 1024)
static void update_yuvconv() {
  float bri = eq_bri / 100.0;
  float cont = (eq_cont + 100) / 100.0;
  float hue = eq_hue / 100.0 * 3.1415927;
  float sat = (eq_sat + 100) / 100.0;
  float rgamma = exp(log(8.0) * eq_rgamma / 100.0);
  float ggamma = exp(log(8.0) * eq_ggamma / 100.0);
  float bgamma = exp(log(8.0) * eq_bgamma / 100.0);
  glSetupYUVConversion(gl_target, use_yuv, bri, cont, hue, sat,
                       rgamma, ggamma, bgamma);
  if (custom_prog) {
    FILE *f = fopen(custom_prog, "r");
    if (!f)
      mp_msg(MSGT_VO, MSGL_WARN,
             "[gl] Could not read customprog %s\n", custom_prog);
    else {
      int i;
      char *prog = calloc(1, MAX_CUSTOM_PROG_SIZE + 1);
      fread(prog, 1, MAX_CUSTOM_PROG_SIZE, f);
      fclose(f);
      ProgramString(GL_FRAGMENT_PROGRAM, GL_PROGRAM_FORMAT_ASCII,
                   strlen(prog), prog);
      glGetIntegerv(GL_PROGRAM_ERROR_POSITION, &i);
      if (i != -1)
        mp_msg(MSGT_VO, MSGL_ERR,
          "[gl] Error in custom program at pos %i (%.20s)\n", i, &prog[i]);
      free(prog);
    }
    ProgramEnvParameter4f(GL_FRAGMENT_PROGRAM, 0,
               1.0 / texture_width, 1.0 / texture_height, 0, 0);
  }
  if (custom_tex) {
    FILE *f = fopen(custom_tex, "r");
    if (!f)
      mp_msg(MSGT_VO, MSGL_WARN,
             "[gl] Could not read customtex %s\n", custom_tex);
    else {
      int width, height, maxval;
      ActiveTexture(GL_TEXTURE3);
      if (glCreatePPMTex(GL_TEXTURE_2D, 3,
                     custom_tlin?GL_LINEAR:GL_NEAREST,
                     f, &width, &height, &maxval))
        ProgramEnvParameter4f(GL_FRAGMENT_PROGRAM, 1,
                   1.0 / width, 1.0 / height, 1.0 / maxval, 0);
      else
        mp_msg(MSGT_VO, MSGL_WARN,
               "[gl] Error parsing customtex %s\n", custom_tex);
      fclose(f);
      ActiveTexture(GL_TEXTURE0);
    }
  }
}

/**
 * \brief remove all OSD textures and display-lists, thus clearing it.
 */
static void clearOSD() {
  int i;
  glDeleteTextures(osdtexCnt, osdtex);
#ifndef FAST_OSD
  glDeleteTextures(osdtexCnt, osdatex);
#endif
  for (i = 0; i < osdtexCnt; i++)
    glDeleteLists(osdDispList[i], 1);
  osdtexCnt = 0;
}

/**
 * \brief uninitialize OpenGL context, freeing textures, buffers etc.
 */
static void uninitGl() {
  if (DeletePrograms && fragprog)
    DeletePrograms(1, &fragprog);
  fragprog = 0;
  if (uvtexs[0] || uvtexs[1])
    glDeleteTextures(2, uvtexs);
  uvtexs[0] = uvtexs[1] = 0;
  if (lookupTex)
    glDeleteTextures(1, &lookupTex);
  lookupTex = 0;
  clearOSD();
  if (DeleteBuffers && gl_buffer)
    DeleteBuffers(1, &gl_buffer);
  gl_buffer = 0; gl_buffersize = 0;
  err_shown = 0;
}

/**
 * \brief Initialize a (new or reused) OpenGL context.
 * set global gl-related variables to their default values
 */
static int initGl(uint32_t d_width, uint32_t d_height) {
  osdtexCnt = 0; gl_buffer = 0; gl_buffersize = 0; err_shown = 0;
  fragprog = 0; uvtexs[0] = 0; uvtexs[1] = 0; lookupTex = 0;
  texSize(image_width, image_height, &texture_width, &texture_height);

  glDisable(GL_BLEND); 
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glEnable(gl_target);
  glDrawBuffer(vo_doublebuffering?GL_BACK:GL_FRONT);

  mp_msg(MSGT_VO, MSGL_V, "[gl] Creating %dx%d texture...\n",
          texture_width, texture_height);

  if (image_format == IMGFMT_YV12) {
    glGenTextures(2, uvtexs);
    ActiveTexture(GL_TEXTURE1);
    BindTexture(gl_target, uvtexs[0]);
    glCreateClearTex(gl_target, gl_texfmt, GL_LINEAR,
                     texture_width / 2, texture_height / 2, 128);
    ActiveTexture(GL_TEXTURE2);
    BindTexture(gl_target, uvtexs[1]);
    glCreateClearTex(gl_target, gl_texfmt, GL_LINEAR,
                     texture_width / 2, texture_height / 2, 128);
    switch (use_yuv) {
      case YUV_CONVERSION_FRAGMENT_LOOKUP:
        glGenTextures(1, &lookupTex);
        ActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, lookupTex);
      case YUV_CONVERSION_FRAGMENT_POW:
      case YUV_CONVERSION_FRAGMENT:
        GenPrograms(1, &fragprog);
        BindProgram(GL_FRAGMENT_PROGRAM, fragprog);
        break;
    }
    ActiveTexture(GL_TEXTURE0);
    BindTexture(gl_target, 0);
    update_yuvconv();
  }
  glCreateClearTex(gl_target, gl_texfmt, GL_LINEAR,
                   texture_width, texture_height, 0);

  resize(d_width, d_height);

  glClearColor( 0.0f,0.0f,0.0f,0.0f );
  glClear( GL_COLOR_BUFFER_BIT );
  if (SwapInterval && swap_interval >= 0)
    SwapInterval(swap_interval);
  return 1;
}

/* connect to server, create and map window,
 * allocate colors and (shared) memory
 */
static int 
config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
	image_height = height;
	image_width = width;
	image_format = format;
	glFindFormat(format, NULL, &gl_texfmt, &gl_format, &gl_type);

	int_pause = 0;

	panscan_init();
	aspect_save_orig(width,height);
	aspect_save_prescale(d_width,d_height);
	aspect_save_screenres(vo_screenwidth,vo_screenheight);

	aspect(&d_width,&d_height,A_NOZOOM);
	vo_dx = (vo_screenwidth - d_width) / 2;
	vo_dy = (vo_screenheight - d_height) / 2;
	geometry(&vo_dx, &vo_dy, &d_width, &d_height,
	          vo_screenwidth, vo_screenheight);
#ifdef X11_FULLSCREEN
//        if( flags&VOFLAG_FULLSCREEN ){ // (-fs)
//          aspect(&d_width,&d_height,A_ZOOM);
//        }
#endif
#ifdef HAVE_NEW_GUI
  if (use_gui) {
    // GUI creates and manages window for us
    vo_dwidth = d_width;
    vo_dheight= d_height;
    guiGetEvent(guiSetShVideo, 0);
    goto glconfig;
  }
#endif
#ifdef GL_WIN32
  o_dwidth = d_width;
  o_dheight = d_height;
  vo_fs = flags & VOFLAG_FULLSCREEN;
  vo_vm = flags & VOFLAG_MODESWITCHING;
  vo_dwidth = d_width;
  vo_dheight = d_height;
  if (!createRenderingContext())
    return -1;
#else
  if (WinID >= 0) {
    vo_window = WinID ? (Window)WinID : mRootWin;
    vo_x11_selectinput_witherr(mDisplay, vo_window,
             StructureNotifyMask | KeyPressMask | PointerMotionMask |
             ButtonPressMask | ButtonReleaseMask | ExposureMask);
    goto glconfig;
  }
  if ( vo_window == None ) {
	unsigned int fg, bg;
	XSizeHints hint;
	XVisualInfo *vinfo;
	XEvent xev;

	vo_fs = VO_FALSE;

	hint.x = vo_dx;
	hint.y = vo_dy;
	hint.width = d_width;
	hint.height = d_height;
	hint.flags = PPosition | PSize;

	/* Get some colors */

	bg = WhitePixel(mDisplay, mScreen);
	fg = BlackPixel(mDisplay, mScreen);

	/* Make the window */

  vinfo=glXChooseVisual( mDisplay,mScreen,wsGLXAttrib );
  if (vinfo == NULL)
  {
    mp_msg(MSGT_VO, MSGL_ERR, "[gl] no GLX support present\n");
    return -1;
  }



          vo_window = vo_x11_create_smooth_window(mDisplay, mRootWin, vinfo->visual, hint.x, hint.y, hint.width, hint.height,
			                          vinfo->depth, XCreateColormap(mDisplay, mRootWin, vinfo->visual, AllocNone));

      vo_x11_classhint( mDisplay,vo_window,"gl" );
      vo_hidecursor(mDisplay,vo_window);

//      if ( flags&VOFLAG_FULLSCREEN ) vo_x11_decoration( mDisplay,vo_window,0 );
	  XSelectInput(mDisplay, vo_window, StructureNotifyMask);
	  /* Tell other applications about this window */
	  XSetStandardProperties(mDisplay, vo_window, title, title, None, NULL, 0, &hint);
	  /* Map window. */
	  XMapWindow(mDisplay, vo_window);
#ifdef HAVE_XINERAMA
	  vo_x11_xinerama_move(mDisplay,vo_window);
#endif

	  /* Wait for map. */
	  do 
	  {
		XNextEvent(mDisplay, &xev);
	  }
	  while (xev.type != MapNotify || xev.xmap.event != vo_window);

	  XSelectInput(mDisplay, vo_window, NoEventMask);

	XSync(mDisplay, False);

	vo_x11_selectinput_witherr(mDisplay, vo_window, StructureNotifyMask | KeyPressMask | PointerMotionMask
		     | ButtonPressMask | ButtonReleaseMask | ExposureMask
        );
  }
      if (vo_ontop) vo_x11_setlayer(mDisplay, vo_window, vo_ontop);

  vo_x11_nofs_sizepos(vo_dx, vo_dy, d_width, d_height);
  if (vo_fs ^ (flags & VOFLAG_FULLSCREEN))
    vo_x11_fullscreen();
#endif

glconfig:
  if (vo_config_count)
    uninitGl();
  setGlWindow(&gl_vinfo, &gl_context, vo_window);
  initGl(vo_dwidth, vo_dheight);

	return 0;
}

static void check_events(void)
{
#ifdef GL_WIN32
    int e=vo_w32_check_events();
#else
    int e=vo_x11_check_events(mDisplay);
#endif
    if(e&VO_EVENT_RESIZE) resize(vo_dwidth,vo_dheight);
    if(e&VO_EVENT_EXPOSE && int_pause) flip_page();
}

/**
 * Creates the textures and the display list needed for displaying
 * an OSD part.
 * Callback function for vo_draw_text().
 */
static void create_osd_texture(int x0, int y0, int w, int h,
                                 unsigned char *src, unsigned char *srca,
                                 int stride)
{
  // initialize to 8 to avoid special-casing on alignment
  int sx = 8, sy = 8;
  GLint scale_type = (scaled_osd) ? GL_LINEAR : GL_NEAREST;
  texSize(w, h, &sx, &sy);

  if (osdtexCnt >= MAX_OSD_PARTS) {
    mp_msg(MSGT_VO, MSGL_ERR, "Too many OSD parts, contact the developers!\n");
    return;
  }

  // create Textures for OSD part
  glGenTextures(1, &osdtex[osdtexCnt]);
  BindTexture(gl_target, osdtex[osdtexCnt]);
  glCreateClearTex(gl_target, GL_LUMINANCE, scale_type, sx, sy, 0);
  glUploadTex(gl_target, GL_LUMINANCE, GL_UNSIGNED_BYTE, src, stride,
              0, 0, w, h, 0);

#ifndef FAST_OSD
  glGenTextures(1, &osdatex[osdtexCnt]);
  BindTexture(gl_target, osdatex[osdtexCnt]);
  glCreateClearTex(gl_target, GL_ALPHA, scale_type, sx, sy, 255);
  {
  int i;
  char *tmp = (char *)malloc(stride * h);
  // convert alpha from weird MPlayer scale.
  // in-place is not possible since it is reused for future OSDs
  for (i = h * stride - 1; i; i--)
    tmp[i] = srca[i] - 1;
  glUploadTex(gl_target, GL_ALPHA, GL_UNSIGNED_BYTE, tmp, stride,
              0, 0, w, h, 0);
  free(tmp);
  }
#endif

  BindTexture(gl_target, 0);

  // Create a list for rendering this OSD part
  osdDispList[osdtexCnt] = glGenLists(1);
  glNewList(osdDispList[osdtexCnt], GL_COMPILE);
#ifndef FAST_OSD
  // render alpha
  glBlendFunc(GL_ZERO, GL_SRC_ALPHA);
  BindTexture(gl_target, osdatex[osdtexCnt]);
  glDrawTex(x0, y0, w, h, 0, 0, w, h, sx, sy, use_rectangle == 1, 0);
#endif
  // render OSD
  glBlendFunc (GL_ONE, GL_ONE);
  BindTexture(gl_target, osdtex[osdtexCnt]);
  glDrawTex(x0, y0, w, h, 0, 0, w, h, sx, sy, use_rectangle == 1, 0);
  glEndList();

  osdtexCnt++;
}

static void draw_osd(void)
{
  if (!use_osd) return;
  if (vo_osd_changed(0)) {
    int osd_h, osd_w;
    clearOSD();
    osd_w = (scaled_osd) ? image_width : vo_dwidth;
    osd_h = (scaled_osd) ? image_height : vo_dheight;
    vo_draw_text(osd_w, osd_h, create_osd_texture);
  }
}

static void
flip_page(void)
{
//  glEnable(GL_TEXTURE_2D);
//  glBindTexture(GL_TEXTURE_2D, texture_id);

  glColor3f(1,1,1);
  if (image_format == IMGFMT_YV12)
    glEnableYUVConversion(gl_target, use_yuv);
  glDrawTex(0, 0, image_width, image_height,
            0, 0, image_width, image_height,
            texture_width, texture_height,
            use_rectangle == 1, image_format == IMGFMT_YV12);
  if (image_format == IMGFMT_YV12)
    glDisableYUVConversion(gl_target, use_yuv);

  if (osdtexCnt > 0) {
    // set special rendering parameters
    if (!scaled_osd) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, vo_dwidth, vo_dheight, 0, -1, 1);
    }
    glEnable(GL_BLEND);
    // draw OSD
    glCallLists(osdtexCnt, GL_UNSIGNED_INT, osdDispList);
    // set rendering parameters back to defaults
    glDisable (GL_BLEND);
    if (!scaled_osd)
    glPopMatrix();
    BindTexture(gl_target, 0);
  }

//  glFlush();
  if (use_glFinish)
  glFinish();
  if (vo_doublebuffering)
#ifdef GL_WIN32
  SwapBuffers(vo_hdc);
#else
  glXSwapBuffers( mDisplay,vo_window );
#endif
 
  if (vo_fs && use_aspect && vo_doublebuffering)
    glClear(GL_COLOR_BUFFER_BIT);
}

//static inline uint32_t draw_slice_x11(uint8_t *src[], uint32_t slice_num)
static int draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y)
{
  glUploadTex(gl_target, gl_format, gl_type, src[0], stride[0],
              x, y, w, h, slice_height);
  if (image_format == IMGFMT_YV12) {
    ActiveTexture(GL_TEXTURE1);
    glUploadTex(gl_target, gl_format, gl_type, src[1], stride[1],
                x / 2, y / 2, w / 2, h / 2, slice_height);
    ActiveTexture(GL_TEXTURE2);
    glUploadTex(gl_target, gl_format, gl_type, src[2], stride[2],
                x / 2, y / 2, w / 2, h / 2, slice_height);
    ActiveTexture(GL_TEXTURE0);
  }
	return 0;
}

static uint32_t get_image(mp_image_t *mpi) {
  if (!GenBuffers || !BindBuffer || !BufferData || !MapBuffer) {
    if (!err_shown)
      mp_msg(MSGT_VO, MSGL_ERR, "[gl] extensions missing for dr\n"
                                "Expect a _major_ speed penalty\n");
    err_shown = 1;
    return VO_FALSE;
  }
  if (mpi->flags & MP_IMGFLAG_READABLE) return VO_FALSE;
  if (mpi->type == MP_IMGTYPE_IP || mpi->type == MP_IMGTYPE_IPB)
    return VO_FALSE; // we can not provide readable buffers
  if (!gl_buffer)
    GenBuffers(1, &gl_buffer);
  BindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_buffer);
  mpi->stride[0] = mpi->width * mpi->bpp / 8;
  if (mpi->stride[0] * mpi->h > gl_buffersize) {
    BufferData(GL_PIXEL_UNPACK_BUFFER, mpi->stride[0] * mpi->h,
               NULL, GL_DYNAMIC_DRAW);
    gl_buffersize = mpi->stride[0] * mpi->h;
  }
  UnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // HACK, needed for some MPEG4 files??
  mpi->planes[0] = MapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
  BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  if (mpi->planes[0] == NULL) {
    if (!err_shown)
      mp_msg(MSGT_VO, MSGL_ERR, "[gl] could not aquire buffer for dr\n"
                                "Expect a _major_ speed penalty\n");
    err_shown = 1;
    return VO_FALSE;
  }
  if (mpi->imgfmt == IMGFMT_YV12) {
    // YV12
    mpi->flags |= MP_IMGFLAG_COMMON_STRIDE | MP_IMGFLAG_COMMON_PLANE;
    mpi->stride[0] = mpi->width;
    mpi->planes[1] = mpi->planes[0] + mpi->stride[0] * mpi->height;
    mpi->stride[1] = mpi->width >> 1;
    mpi->planes[2] = mpi->planes[1] + mpi->stride[1] * (mpi->height >> 1);
    mpi->stride[2] = mpi->width >> 1;
  }
  mpi->flags |= MP_IMGFLAG_DIRECT;
  return VO_TRUE;
}

static uint32_t draw_image(mp_image_t *mpi) {
  char *data = mpi->planes[0];
  int slice = slice_height;
  if (mpi->flags & MP_IMGFLAG_DRAW_CALLBACK)
    return VO_TRUE;
  if (mpi->flags & MP_IMGFLAG_DIRECT) {
    data = NULL;
    BindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_buffer);
    UnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    slice = 0; // always "upload" full texture
  }
  glUploadTex(gl_target, gl_format, gl_type, data, mpi->stride[0],
              mpi->x, mpi->y, mpi->w, mpi->h, slice);
  if (mpi->imgfmt == IMGFMT_YV12) {
    data += mpi->planes[1] - mpi->planes[0];
    ActiveTexture(GL_TEXTURE1);
    glUploadTex(gl_target, gl_format, gl_type, data, mpi->stride[1],
                mpi->x / 2, mpi->y / 2, mpi->w / 2, mpi->h / 2, slice);
    data += mpi->planes[2] - mpi->planes[1];
    ActiveTexture(GL_TEXTURE2);
    glUploadTex(gl_target, gl_format, gl_type, data, mpi->stride[2],
                mpi->x / 2, mpi->y / 2, mpi->w / 2, mpi->h / 2, slice);
    ActiveTexture(GL_TEXTURE0);
  }
  if (mpi->flags & MP_IMGFLAG_DIRECT)
    BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  return VO_TRUE;
}

static int
draw_frame(uint8_t *src[])
{
  return VO_ERROR; 
}

static int
query_format(uint32_t format)
{
    int caps = VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW |
               VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN | VFCAP_ACCEPT_STRIDE;
    if (use_osd)
      caps |= VFCAP_OSD;
    if ((format == IMGFMT_RGB24) || (format == IMGFMT_RGBA))
        return caps;
    if (use_yuv && format == IMGFMT_YV12)
        return caps;
    if (many_fmts &&
         glFindFormat(format, NULL, NULL, NULL, NULL))
        return caps;
    return 0;
}


static void
uninit(void)
{
  if ( !vo_config_count ) return;
  uninitGl();
  releaseGlContext(&gl_vinfo, &gl_context);
  if (custom_prog) free(custom_prog);
  custom_prog = NULL;
  if (custom_tex) free(custom_tex);
  custom_tex = NULL;
#ifdef GL_WIN32
  vo_w32_uninit();
#else
  vo_x11_uninit();
#endif
}

static opt_t subopts[] = {
  {"manyfmts",     OPT_ARG_BOOL, &many_fmts,    NULL},
  {"osd",          OPT_ARG_BOOL, &use_osd,      NULL},
  {"scaled-osd",   OPT_ARG_BOOL, &scaled_osd,   NULL},
  {"aspect",       OPT_ARG_BOOL, &use_aspect,   NULL},
  {"slice-height", OPT_ARG_INT,  &slice_height, (opt_test_f)int_non_neg},
  {"rectangle",    OPT_ARG_INT,  &use_rectangle,(opt_test_f)int_non_neg},
  {"yuv",          OPT_ARG_INT,  &use_yuv,      (opt_test_f)int_non_neg},
  {"glfinish",     OPT_ARG_BOOL, &use_glFinish, NULL},
  {"swapinterval", OPT_ARG_INT,  &swap_interval,NULL},
  {"customprog",   OPT_ARG_MSTRZ,&custom_prog,  NULL},
  {"customtex",    OPT_ARG_MSTRZ,&custom_tex,   NULL},
  {"customtlin",   OPT_ARG_BOOL, &custom_tlin,  NULL},
  {NULL}
};

static int preinit(const char *arg)
{
    // set defaults
    many_fmts = 1;
    use_osd = 1;
    scaled_osd = 0;
    use_aspect = 1;
    use_yuv = 0;
    use_rectangle = 0;
    use_glFinish = 0;
    swap_interval = 1;
    slice_height = 4;
    custom_prog = NULL;
    custom_tex = NULL;
    custom_tlin = 1;
    if (subopt_parse(arg, subopts) != 0) {
      mp_msg(MSGT_VO, MSGL_FATAL,
              "\n-vo gl command line help:\n"
              "Example: mplayer -vo gl:slice-height=4\n"
              "\nOptions:\n"
              "  manyfmts\n"
              "    Enable extended color formats for OpenGL 1.2 and later\n"
              "  slice-height=<0-...>\n"
              "    Slice size for texture transfer, 0 for whole image\n"
              "  noosd\n"
              "    Do not use OpenGL OSD code\n"
              "  noaspect\n"
              "    Do not do aspect scaling\n"
              "  rectangle=<0,1,2>\n"
              "    0: use power-of-two textures\n"
              "    1: use texture_rectangle\n"
              "    2: use texture_non_power_of_two\n"
              "  glfinish\n"
              "    Call glFinish() before swapping buffers\n"
              "  swapinterval=<n>\n"
              "    Interval in displayed frames between to buffer swaps.\n"
              "    1 is equivalent to enable VSYNC, 0 to disable VSYNC.\n"
              "    Requires GLX_SGI_swap_control support to work.\n"
              "  yuv=<n>\n"
              "    0: use software YUV to RGB conversion.\n"
              "    1: use register combiners (nVidia only, for older cards).\n"
              "    2: use fragment program.\n"
              "    3: use fragment program with gamma correction.\n"
              "    4: use fragment program with gamma correction via lookup.\n"
              "    5: use ATI-specific method (for older cards).\n"
              "  customprog=<filename>\n"
              "    use a custom YUV conversion program\n"
              "  customtex=<filename>\n"
              "    use a custom YUV conversion lookup texture\n"
              "  nocustomtlin\n"
              "    use GL_NEAREST scaling for customtex texture\n"
              "\n" );
      return -1;
    }
    if (use_rectangle == 1)
      gl_target = GL_TEXTURE_RECTANGLE;
    else
      gl_target = GL_TEXTURE_2D;
    if (many_fmts)
      mp_msg (MSGT_VO, MSGL_WARN, "[gl] using extended formats.\n"
               "Make sure you have OpenGL >= 1.2 and used corresponding "
               "headers for compiling!\n");
    mp_msg (MSGT_VO, MSGL_INFO, "[gl] Using %d as slice height "
             "(0 means image height).\n", slice_height);
    if( !vo_init() ) return -1; // Can't open X11

    return 0;
}

static int control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_PAUSE: return (int_pause=1);
  case VOCTRL_RESUME: return (int_pause=0);
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  case VOCTRL_GET_IMAGE:
    return get_image(data);
  case VOCTRL_DRAW_IMAGE:
    return draw_image(data);
  case VOCTRL_GUISUPPORT:
    return VO_TRUE;
  case VOCTRL_ONTOP:
#ifdef GL_WIN32
    vo_w32_ontop();
#else
    vo_x11_ontop();
#endif
    return VO_TRUE;
  case VOCTRL_FULLSCREEN:
#ifdef GL_WIN32
    vo_w32_fullscreen();
    resize(vo_dwidth, vo_dheight);
#else
    vo_x11_fullscreen();
#endif
    return VO_TRUE;
  case VOCTRL_GET_PANSCAN:
    if (!use_aspect) return VO_NOTIMPL;
    return VO_TRUE;
  case VOCTRL_SET_PANSCAN:
    if (!use_aspect) return VO_NOTIMPL;
    resize (vo_dwidth, vo_dheight);
    return VO_TRUE;
  case VOCTRL_GET_EQUALIZER:
    if (image_format == IMGFMT_YV12) {
      va_list va;
      int *value;
      va_start(va, data);
      value = va_arg(va, int *);
      va_end(va);
      if (strcasecmp(data, "brightness") == 0) {
        *value = eq_bri;
        if (use_yuv == YUV_CONVERSION_COMBINERS) break; // not supported
      } else if (strcasecmp(data, "contrast") == 0) {
        *value = eq_cont;
        if (use_yuv == YUV_CONVERSION_COMBINERS) break; // not supported
      } else if (strcasecmp(data, "saturation") == 0) {
        *value = eq_sat;
      } else if (strcasecmp(data, "hue") == 0) {
        *value = eq_hue;
      } else if (strcasecmp(data, "gamma") ==  0) {
        *value = eq_rgamma;
        if (use_yuv == YUV_CONVERSION_COMBINERS ||
            use_yuv == YUV_CONVERSION_FRAGMENT) break; // not supported
      } else if (strcasecmp(data, "red_gamma") ==  0) {
        *value = eq_rgamma;
        if (use_yuv == YUV_CONVERSION_COMBINERS ||
            use_yuv == YUV_CONVERSION_FRAGMENT) break; // not supported
      } else if (strcasecmp(data, "green_gamma") ==  0) {
        *value = eq_ggamma;
        if (use_yuv == YUV_CONVERSION_COMBINERS ||
            use_yuv == YUV_CONVERSION_FRAGMENT) break; // not supported
      } else if (strcasecmp(data, "blue_gamma") ==  0) {
        *value = eq_bgamma;
        if (use_yuv == YUV_CONVERSION_COMBINERS ||
            use_yuv == YUV_CONVERSION_FRAGMENT) break; // not supported
      }
      return VO_TRUE;
    }
    break;
  case VOCTRL_SET_EQUALIZER:
    if (image_format == IMGFMT_YV12) {
      va_list va;
      int value;
      va_start(va, data);
      value = va_arg(va, int);
      va_end(va);
      if (strcasecmp(data, "brightness") == 0) {
        eq_bri = value;
        if (use_yuv == YUV_CONVERSION_COMBINERS) break; // not supported
      } else if (strcasecmp(data, "contrast") == 0) {
        eq_cont = value;
        if (use_yuv == YUV_CONVERSION_COMBINERS) break; // not supported
      } else if (strcasecmp(data, "saturation") == 0) {
        eq_sat = value;
      } else if (strcasecmp(data, "hue") == 0) {
        eq_hue = value;
      } else if (strcasecmp(data, "gamma") ==  0) {
        eq_rgamma = eq_ggamma = eq_bgamma = value;
        if (use_yuv == YUV_CONVERSION_COMBINERS ||
            use_yuv == YUV_CONVERSION_FRAGMENT) break; // not supported
      } else if (strcasecmp(data, "red_gamma") ==  0) {
        eq_rgamma = value;
        if (use_yuv == YUV_CONVERSION_COMBINERS ||
            use_yuv == YUV_CONVERSION_FRAGMENT) break; // not supported
      } else if (strcasecmp(data, "green_gamma") ==  0) {
        eq_ggamma = value;
        if (use_yuv == YUV_CONVERSION_COMBINERS ||
            use_yuv == YUV_CONVERSION_FRAGMENT) break; // not supported
      } else if (strcasecmp(data, "blue_gamma") ==  0) {
        eq_bgamma = value;
        if (use_yuv == YUV_CONVERSION_COMBINERS ||
            use_yuv == YUV_CONVERSION_FRAGMENT) break; // not supported
      }
      update_yuvconv();
      return VO_TRUE;
    }
    break;
  }
  return VO_NOTIMPL;
}
