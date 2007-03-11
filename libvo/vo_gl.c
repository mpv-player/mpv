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
#include "libass/ass.h"
#include "libass/ass_mp.h"

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
#define update_xinerama_info w32_update_xinerama_info
#define vo_init vo_w32_init
#define vo_window vo_w32_window
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
static GLuint *eosdtex;
static GLuint largeeosdtex[2];
//! Display lists that draw the OSD parts
static GLuint osdDispList[MAX_OSD_PARTS];
#ifndef FAST_OSD
static GLuint osdaDispList[MAX_OSD_PARTS];
#endif
static GLuint eosdDispList;
//! How many parts the OSD currently consists of
static int osdtexCnt;
static int eosdtexCnt;
static int osd_color;

static int use_aspect;
static int use_yuv;
static int lscale;
static int cscale;
static int yuvconvtype;
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
static GLuint default_texs[22];
static char *custom_prog;
static char *custom_tex;
static int custom_tlin;
static int custom_trect;

static int int_pause;
static int eq_bri = 0;
static int eq_cont = 0;
static int eq_sat = 0;
static int eq_hue = 0;
static int eq_rgamma = 0;
static int eq_ggamma = 0;
static int eq_bgamma = 0;

static int texture_width;
static int texture_height;
static int mpi_flipped;
static int vo_flipped;
static int ass_border_x, ass_border_y;

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
    ass_border_x = (vo_screenwidth - new_w) / 2;
    ass_border_y = (vo_screenheight - new_h) / 2;
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
static void update_yuvconv(void) {
  float bri = eq_bri / 100.0;
  float cont = (eq_cont + 100) / 100.0;
  float hue = eq_hue / 100.0 * 3.1415927;
  float sat = (eq_sat + 100) / 100.0;
  float rgamma = exp(log(8.0) * eq_rgamma / 100.0);
  float ggamma = exp(log(8.0) * eq_ggamma / 100.0);
  float bgamma = exp(log(8.0) * eq_bgamma / 100.0);
  glSetupYUVConversion(gl_target, yuvconvtype, bri, cont, hue, sat,
                       rgamma, ggamma, bgamma,
                       texture_width, texture_height);
  if (custom_prog) {
    FILE *f = fopen(custom_prog, "r");
    if (!f)
      mp_msg(MSGT_VO, MSGL_WARN,
             "[gl] Could not read customprog %s\n", custom_prog);
    else {
      char *prog = calloc(1, MAX_CUSTOM_PROG_SIZE + 1);
      fread(prog, 1, MAX_CUSTOM_PROG_SIZE, f);
      fclose(f);
      loadGPUProgram(GL_FRAGMENT_PROGRAM, prog);
      free(prog);
    }
    ProgramEnvParameter4f(GL_FRAGMENT_PROGRAM, 0,
               1.0 / texture_width, 1.0 / texture_height,
               texture_width, texture_height);
  }
  if (custom_tex) {
    FILE *f = fopen(custom_tex, "r");
    if (!f)
      mp_msg(MSGT_VO, MSGL_WARN,
             "[gl] Could not read customtex %s\n", custom_tex);
    else {
      int width, height, maxval;
      ActiveTexture(GL_TEXTURE3);
      if (glCreatePPMTex(custom_trect?GL_TEXTURE_RECTANGLE:GL_TEXTURE_2D, 0,
                     custom_tlin?GL_LINEAR:GL_NEAREST,
                     f, &width, &height, &maxval))
        ProgramEnvParameter4f(GL_FRAGMENT_PROGRAM, 1,
                   1.0 / width, 1.0 / height, width, height);
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
static void clearOSD(void) {
  int i;
  if (!osdtexCnt)
    return;
  glDeleteTextures(osdtexCnt, osdtex);
#ifndef FAST_OSD
  glDeleteTextures(osdtexCnt, osdatex);
  for (i = 0; i < osdtexCnt; i++)
    glDeleteLists(osdaDispList[i], 1);
#endif
  for (i = 0; i < osdtexCnt; i++)
    glDeleteLists(osdDispList[i], 1);
  osdtexCnt = 0;
}

/**
 * \brief remove textures, display list and free memory used by EOSD
 */
static void clearEOSD(void) {
  if (eosdDispList)
    glDeleteLists(eosdDispList, 1);
  eosdDispList = 0;
  if (eosdtexCnt)
    glDeleteTextures(eosdtexCnt, eosdtex);
  eosdtexCnt = 0;
  free(eosdtex);
  eosdtex = NULL;
}

/**
 * \brief construct display list from ass image list
 * \param img image list to create OSD from.
 *            A value of NULL has the same effect as clearEOSD()
 */
static void genEOSD(mp_eosd_images_t *imgs) {
  int sx, sy;
  int tinytexcur = 0;
  int smalltexcur = 0;
  GLuint *curtex;
  GLint scale_type = (scaled_osd) ? GL_LINEAR : GL_NEAREST;
  ass_image_t *img = imgs->imgs;
  ass_image_t *i;
  int cnt;

  if (imgs->changed == 0) // there are elements, but they are unchanged
      return;
  if (img && imgs->changed == 1) // there are elements, but they just moved
      goto skip_upload;

  clearEOSD();
  if (!img)
    return;
  if (!largeeosdtex[0]) {
    glGenTextures(2, largeeosdtex);
    BindTexture(gl_target, largeeosdtex[0]);
    glCreateClearTex(gl_target, GL_ALPHA, scale_type, 512, 512, 0);
    BindTexture(gl_target, largeeosdtex[1]);
    glCreateClearTex(gl_target, GL_ALPHA, scale_type, 512, 512, 0);
  }
  for (i = img; i; i = i->next)
  {
    if (i->w <= 0 || i->h <= 0 || i->stride < i->w)
      continue;
    if (i->w < 16 && i->h < 16 && tinytexcur < 1024)
      tinytexcur++;
    else if (i->w < 32 && i->h < 32 && smalltexcur < 256)
      smalltexcur++;
    else
      eosdtexCnt++;
  }
  mp_msg(MSGT_VO, MSGL_DBG2, "EOSD counts (tiny, small, all): %i, %i, %i\n",
         tinytexcur, smalltexcur, eosdtexCnt);
  if (eosdtexCnt) {
    eosdtex = calloc(eosdtexCnt, sizeof(GLuint));
    glGenTextures(eosdtexCnt, eosdtex);
  }
  tinytexcur = smalltexcur = 0;
  for (i = img, curtex = eosdtex; i; i = i->next) {
    int x = 0, y = 0;
    if (i->w <= 0 || i->h <= 0 || i->stride < i->w) {
      mp_msg(MSGT_VO, MSGL_V, "Invalid dimensions OSD for part!\n");
      continue;
    }
    if (i->w < 16 && i->h < 16 && tinytexcur < 1024) {
      x = (tinytexcur & 31) << 4;
      y = (tinytexcur >> 5) << 4;
      BindTexture(gl_target, largeeosdtex[0]);
      tinytexcur++;
    } else if (i->w < 32 && i->h < 32 && smalltexcur < 256) {
      x = (smalltexcur & 15) << 5;
      y = (smalltexcur >> 4) << 5;
      BindTexture(gl_target, largeeosdtex[1]);
      smalltexcur++;
    } else {
      texSize(i->w, i->h, &sx, &sy);
      BindTexture(gl_target, *curtex++);
      glCreateClearTex(gl_target, GL_ALPHA, scale_type, sx, sy, 0);
    }
    glUploadTex(gl_target, GL_ALPHA, GL_UNSIGNED_BYTE, i->bitmap, i->stride,
                x, y, i->w, i->h, 0);
  }
  eosdDispList = glGenLists(1);
skip_upload:
  glNewList(eosdDispList, GL_COMPILE);
  tinytexcur = smalltexcur = 0;
  for (i = img, curtex = eosdtex; i; i = i->next) {
    int x = 0, y = 0;
    if (i->w <= 0 || i->h <= 0 || i->stride < i->w)
      continue;
    glColor4ub(i->color >> 24, (i->color >> 16) & 0xff, (i->color >> 8) & 0xff, 255 - (i->color & 0xff));
    if (i->w < 16 && i->h < 16 && tinytexcur < 1024) {
      x = (tinytexcur & 31) << 4;
      y = (tinytexcur >> 5) << 4;
      sx = sy = 512;
      BindTexture(gl_target, largeeosdtex[0]);
      tinytexcur++;
    } else if (i->w < 32 && i->h < 32 && smalltexcur < 256) {
      x = (smalltexcur & 15) << 5;
      y = (smalltexcur >> 4) << 5;
      sx = sy = 512;
      BindTexture(gl_target, largeeosdtex[1]);
      smalltexcur++;
    } else {
      texSize(i->w, i->h, &sx, &sy);
      BindTexture(gl_target, *curtex++);
    }
    glDrawTex(i->dst_x, i->dst_y, i->w, i->h, x, y, i->w, i->h, sx, sy, use_rectangle == 1, 0, 0);
  }
  glEndList();
  BindTexture(gl_target, 0);
}

/**
 * \brief uninitialize OpenGL context, freeing textures, buffers etc.
 */
static void uninitGl(void) {
  int i = 0;
  if (DeletePrograms && fragprog)
    DeletePrograms(1, &fragprog);
  fragprog = 0;
  while (default_texs[i] != 0)
    i++;
  if (i)
    glDeleteTextures(i, default_texs);
  default_texs[0] = 0;
  clearOSD();
  clearEOSD();
  if (largeeosdtex[0])
    glDeleteTextures(2, largeeosdtex);
  largeeosdtex[0] = 0;
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
  texSize(image_width, image_height, &texture_width, &texture_height);

  glDisable(GL_BLEND); 
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glEnable(gl_target);
  glDrawBuffer(vo_doublebuffering?GL_BACK:GL_FRONT);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  mp_msg(MSGT_VO, MSGL_V, "[gl] Creating %dx%d texture...\n",
          texture_width, texture_height);

  if (image_format == IMGFMT_YV12) {
    int i;
    glGenTextures(21, default_texs);
    default_texs[21] = 0;
    for (i = 0; i < 7; i++) {
      ActiveTexture(GL_TEXTURE1 + i);
      BindTexture(GL_TEXTURE_2D, default_texs[i]);
      BindTexture(GL_TEXTURE_RECTANGLE, default_texs[i + 7]);
      BindTexture(GL_TEXTURE_3D, default_texs[i + 14]);
    }
    ActiveTexture(GL_TEXTURE1);
    glCreateClearTex(gl_target, gl_texfmt, GL_LINEAR,
                     texture_width / 2, texture_height / 2, 128);
    ActiveTexture(GL_TEXTURE2);
    glCreateClearTex(gl_target, gl_texfmt, GL_LINEAR,
                     texture_width / 2, texture_height / 2, 128);
    switch (use_yuv) {
      case YUV_CONVERSION_FRAGMENT_LOOKUP:
      case YUV_CONVERSION_FRAGMENT_POW:
      case YUV_CONVERSION_FRAGMENT:
        if (!GenPrograms || !BindProgram) {
          mp_msg(MSGT_VO, MSGL_ERR, "[gl] fragment program functions missing!\n");
          break;
        }
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
  vo_flipped = !!(flags & VOFLAG_FLIPPING);

#ifdef HAVE_NEW_GUI
  if (use_gui) {
    // GUI creates and manages window for us
    guiGetEvent(guiSetShVideo, 0);
#ifndef GL_WIN32
    goto glconfig;
#endif
  }
#endif
#ifdef GL_WIN32
  if (!vo_w32_config(d_width, d_height, flags))
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

    vo_window = vo_x11_create_smooth_window(mDisplay, mRootWin, vinfo->visual,
                  hint.x, hint.y, hint.width, hint.height, vinfo->depth,
                  XCreateColormap(mDisplay, mRootWin, vinfo->visual, AllocNone));

    vo_x11_classhint( mDisplay,vo_window,"gl" );
    vo_hidecursor(mDisplay,vo_window);

    XSelectInput(mDisplay, vo_window, StructureNotifyMask);
    /* Tell other applications about this window */
    XSetStandardProperties(mDisplay, vo_window, title, title, None, NULL, 0, &hint);
    /* Map window. */
    XMapWindow(mDisplay, vo_window);

    /* Wait for map. */
    do {
      XNextEvent(mDisplay, &xev);
    } while (xev.type != MapNotify || xev.xmap.event != vo_window);

    XSelectInput(mDisplay, vo_window, NoEventMask);

    XSync(mDisplay, False);

    vo_x11_selectinput_witherr(mDisplay, vo_window,
        StructureNotifyMask | KeyPressMask | PointerMotionMask |
        ButtonPressMask | ButtonReleaseMask | ExposureMask);
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
    int e=vo_check_events();
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

  if (w <= 0 || h <= 0 || stride < w) {
    mp_msg(MSGT_VO, MSGL_V, "Invalid dimensions OSD for part!\n");
    return;
  }
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
  char *tmp = malloc(stride * h);
  // convert alpha from weird MPlayer scale.
  // in-place is not possible since it is reused for future OSDs
  for (i = h * stride - 1; i >= 0; i--)
    tmp[i] = srca[i] - 1;
  glUploadTex(gl_target, GL_ALPHA, GL_UNSIGNED_BYTE, tmp, stride,
              0, 0, w, h, 0);
  free(tmp);
  }
#endif

  BindTexture(gl_target, 0);

  // Create a list for rendering this OSD part
#ifndef FAST_OSD
  osdaDispList[osdtexCnt] = glGenLists(1);
  glNewList(osdaDispList[osdtexCnt], GL_COMPILE);
  // render alpha
  BindTexture(gl_target, osdatex[osdtexCnt]);
  glDrawTex(x0, y0, w, h, 0, 0, w, h, sx, sy, use_rectangle == 1, 0, 0);
  glEndList();
#endif
  osdDispList[osdtexCnt] = glGenLists(1);
  glNewList(osdDispList[osdtexCnt], GL_COMPILE);
  // render OSD
  BindTexture(gl_target, osdtex[osdtexCnt]);
  glDrawTex(x0, y0, w, h, 0, 0, w, h, sx, sy, use_rectangle == 1, 0, 0);
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
    glEnableYUVConversion(gl_target, yuvconvtype);
  glDrawTex(0, 0, image_width, image_height,
            0, 0, image_width, image_height,
            texture_width, texture_height,
            use_rectangle == 1, image_format == IMGFMT_YV12,
            mpi_flipped ^ vo_flipped);
  if (image_format == IMGFMT_YV12)
    glDisableYUVConversion(gl_target, yuvconvtype);

  if (osdtexCnt > 0 || eosdDispList) {
    // set special rendering parameters
    if (!scaled_osd) {
      glMatrixMode(GL_PROJECTION);
      glPushMatrix();
      glLoadIdentity();
      glOrtho(0, vo_dwidth, vo_dheight, 0, -1, 1);
    }
    glEnable(GL_BLEND);
    if (eosdDispList) {
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glCallList(eosdDispList);
    }
    if (osdtexCnt > 0) {
      glColor4ub((osd_color >> 16) & 0xff, (osd_color >> 8) & 0xff, osd_color & 0xff, 0xff);
      // draw OSD
#ifndef FAST_OSD
      glBlendFunc(GL_ZERO, GL_SRC_ALPHA);
      glCallLists(osdtexCnt, GL_UNSIGNED_INT, osdaDispList);
#endif
      glBlendFunc(GL_ONE, GL_ONE);
      glCallLists(osdtexCnt, GL_UNSIGNED_INT, osdDispList);
    }
    // set rendering parameters back to defaults
    glDisable (GL_BLEND);
    if (!scaled_osd)
      glPopMatrix();
    BindTexture(gl_target, 0);
  }

  if (use_glFinish)
  glFinish();
  if (vo_doublebuffering)
    swapGlBuffers();
  else if (!use_glFinish)
    glFlush();
 
  if (vo_fs && use_aspect && vo_doublebuffering)
    glClear(GL_COLOR_BUFFER_BIT);
}

//static inline uint32_t draw_slice_x11(uint8_t *src[], uint32_t slice_num)
static int draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y)
{
  mpi_flipped = (stride[0] < 0);
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
  int slice = slice_height;
  int stride[3] = {mpi->stride[0], mpi->stride[1], mpi->stride[2]};
  unsigned char *planes[3] = {mpi->planes[0], mpi->planes[1], mpi->planes[2]};
  if (mpi->flags & MP_IMGFLAG_DRAW_CALLBACK)
    return VO_TRUE;
  mpi_flipped = (stride[0] < 0);
  if (mpi->flags & MP_IMGFLAG_DIRECT) {
    intptr_t base = (intptr_t)planes[0];
    if (mpi_flipped)
      base += (mpi->h - 1) * stride[0];
    planes[0] -= base;
    planes[1] -= base;
    planes[2] -= base;
    BindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_buffer);
    UnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    slice = 0; // always "upload" full texture
  }
  glUploadTex(gl_target, gl_format, gl_type, planes[0], stride[0],
              mpi->x, mpi->y, mpi->w, mpi->h, slice);
  if (mpi->imgfmt == IMGFMT_YV12) {
    ActiveTexture(GL_TEXTURE1);
    glUploadTex(gl_target, gl_format, gl_type, planes[1], stride[1],
                mpi->x / 2, mpi->y / 2, mpi->w / 2, mpi->h / 2, slice);
    ActiveTexture(GL_TEXTURE2);
    glUploadTex(gl_target, gl_format, gl_type, planes[2], stride[2],
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
               VFCAP_FLIP |
               VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN | VFCAP_ACCEPT_STRIDE;
    if (use_osd)
      caps |= VFCAP_OSD | VFCAP_EOSD;
    if ((format == IMGFMT_RGB24) || (format == IMGFMT_RGBA))
        return caps;
    if (use_yuv && format == IMGFMT_YV12)
        return caps;
    // HACK, otherwise we get only b&w with some filters (e.g. -vf eq)
    // ideally MPlayer should be fixed instead not to use Y800 when it has the choice
    if (!use_yuv && (format == IMGFMT_Y8 || format == IMGFMT_Y800))
        return 0;
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
  vo_uninit();
}

static opt_t subopts[] = {
  {"manyfmts",     OPT_ARG_BOOL, &many_fmts,    NULL},
  {"osd",          OPT_ARG_BOOL, &use_osd,      NULL},
  {"scaled-osd",   OPT_ARG_BOOL, &scaled_osd,   NULL},
  {"aspect",       OPT_ARG_BOOL, &use_aspect,   NULL},
  {"slice-height", OPT_ARG_INT,  &slice_height, (opt_test_f)int_non_neg},
  {"rectangle",    OPT_ARG_INT,  &use_rectangle,(opt_test_f)int_non_neg},
  {"yuv",          OPT_ARG_INT,  &use_yuv,      (opt_test_f)int_non_neg},
  {"lscale",       OPT_ARG_INT,  &lscale,       (opt_test_f)int_non_neg},
  {"cscale",       OPT_ARG_INT,  &cscale,       (opt_test_f)int_non_neg},
  {"glfinish",     OPT_ARG_BOOL, &use_glFinish, NULL},
  {"swapinterval", OPT_ARG_INT,  &swap_interval,NULL},
  {"customprog",   OPT_ARG_MSTRZ,&custom_prog,  NULL},
  {"customtex",    OPT_ARG_MSTRZ,&custom_tex,   NULL},
  {"customtlin",   OPT_ARG_BOOL, &custom_tlin,  NULL},
  {"customtrect",  OPT_ARG_BOOL, &custom_trect, NULL},
  {"osdcolor",     OPT_ARG_INT,  &osd_color,    NULL},
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
    lscale = 0;
    cscale = 0;
    use_rectangle = 0;
    use_glFinish = 0;
    swap_interval = 1;
    slice_height = -1;
    custom_prog = NULL;
    custom_tex = NULL;
    custom_tlin = 1;
    custom_trect = 0;
    osd_color = 0xffffff;
    if (subopt_parse(arg, subopts) != 0) {
      mp_msg(MSGT_VO, MSGL_FATAL,
              "\n-vo gl command line help:\n"
              "Example: mplayer -vo gl:slice-height=4\n"
              "\nOptions:\n"
              "  nomanyfmts\n"
              "    Disable extended color formats for OpenGL 1.2 and later\n"
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
              "    6: use lookup via 3D texture.\n"
              "  lscale=<n>\n"
              "    0: use standard bilinear scaling for luma.\n"
              "    1: use improved bicubic scaling for luma.\n"
              "    2: use cubic in X, linear in Y direction scaling for luma.\n"
              "  cscale=<n>\n"
              "    as lscale but for chroma (2x slower with little visible effect).\n"
              "  customprog=<filename>\n"
              "    use a custom YUV conversion program\n"
              "  customtex=<filename>\n"
              "    use a custom YUV conversion lookup texture\n"
              "  nocustomtlin\n"
              "    use GL_NEAREST scaling for customtex texture\n"
              "  customtrect\n"
              "    use texture_rectangle for customtex texture\n"
              "  osdcolor=<0xRRGGBB>\n"
              "    use the given color for the OSD\n"
              "\n" );
      return -1;
    }
    if (use_rectangle == 1)
      gl_target = GL_TEXTURE_RECTANGLE;
    else
      gl_target = GL_TEXTURE_2D;
    if (slice_height == -1)
      slice_height = use_yuv ? 16 : 4;
    yuvconvtype = use_yuv | lscale << YUV_LUM_SCALER_SHIFT | cscale << YUV_CHROM_SCALER_SHIFT;
    if (many_fmts)
      mp_msg (MSGT_VO, MSGL_INFO, "[gl] using extended formats. "
               "Use -vo gl:nomanyfmts if playback fails.\n");
    mp_msg (MSGT_VO, MSGL_V, "[gl] Using %d as slice height "
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
  case VOCTRL_DRAW_EOSD:
    if (!data)
      return VO_FALSE;
    genEOSD(data);
    return VO_TRUE;
  case VOCTRL_GET_EOSD_RES:
    {
      mp_eosd_res_t *r = data;
      r->mt = r->mb = r->ml = r->mr = 0;
      if (scaled_osd) {r->w = image_width; r->h = image_height;}
      else if (vo_fs) {
        r->w = vo_screenwidth; r->h = vo_screenheight;
        r->ml = r->mr = ass_border_x > 0 ? ass_border_x : 0;
        r->mt = r->mb = ass_border_y > 0 ? ass_border_y : 0;
      } else {
        r->w = vo_dwidth; r->h = vo_dheight;
      }
    }
    return VO_TRUE;
  case VOCTRL_GUISUPPORT:
    return VO_TRUE;
  case VOCTRL_ONTOP:
    vo_ontop();
    return VO_TRUE;
  case VOCTRL_FULLSCREEN:
    vo_fullscreen();
    resize(vo_dwidth, vo_dheight);
    return VO_TRUE;
#ifdef GL_WIN32
  case VOCTRL_BORDER:
    vo_w32_border();
    return VO_TRUE;
#endif
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
  case VOCTRL_UPDATE_SCREENINFO:
    update_xinerama_info();
    return VO_TRUE;
  }
  return VO_NOTIMPL;
}
