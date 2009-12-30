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
#ifdef CONFIG_GUI
#include "gui/interface.h"
#endif
#include "fastmemcpy.h"
#include "libass/ass_mp.h"

static const vo_info_t info =
{
  "X11 (OpenGL)",
  "gl",
  "Arpad Gereoffy <arpi@esp-team.scene.hu>",
  ""
};

const LIBVO_EXTERN(gl)

#ifdef CONFIG_GL_X11
static int                  wsGLXAttrib[] = { GLX_RGBA,
                                       GLX_RED_SIZE,1,
                                       GLX_GREEN_SIZE,1,
                                       GLX_BLUE_SIZE,1,
                                       GLX_DOUBLEBUFFER,
                                       None };
#endif
static MPGLContext glctx;

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
#define LARGE_EOSD_TEX_SIZE 512
#define TINYTEX_SIZE 16
#define TINYTEX_COLS (LARGE_EOSD_TEX_SIZE/TINYTEX_SIZE)
#define TINYTEX_MAX (TINYTEX_COLS*TINYTEX_COLS)
#define SMALLTEX_SIZE 32
#define SMALLTEX_COLS (LARGE_EOSD_TEX_SIZE/SMALLTEX_SIZE)
#define SMALLTEX_MAX (SMALLTEX_COLS*SMALLTEX_COLS)
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
static int use_ycbcr;
#define MASK_ALL_YUV (~(1 << YUV_CONVERSION_NONE))
#define MASK_NOT_COMBINERS (~((1 << YUV_CONVERSION_NONE) | (1 << YUV_CONVERSION_COMBINERS) | (1 << YUV_CONVERSION_COMBINERS_ATI)))
#define MASK_GAMMA_SUPPORT (MASK_NOT_COMBINERS & ~(1 << YUV_CONVERSION_FRAGMENT))
static int use_yuv;
static int is_yuv;
static int lscale;
static int cscale;
static float filter_strength;
static int yuvconvtype;
static int use_rectangle;
static int err_shown;
static uint32_t image_width;
static uint32_t image_height;
static uint32_t image_format;
static int many_fmts;
static int ati_hack;
static int force_pbo;
static int mesa_buffer;
static int use_glFinish;
static int swap_interval;
static GLenum gl_target;
static GLint gl_texfmt;
static GLenum gl_format;
static GLenum gl_type;
static GLuint gl_buffer;
static GLuint gl_buffer_uv[2];
static int gl_buffersize;
static int gl_buffersize_uv;
static void *gl_bufferptr;
static void *gl_bufferptr_uv[2];
static int mesa_buffersize;
static void *mesa_bufferptr;
static GLuint fragprog;
static GLuint default_texs[22];
static char *custom_prog;
static char *custom_tex;
static int custom_tlin;
static int custom_trect;
static int mipmap_gen;

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

static void redraw(void);

static void resize(int x,int y){
  mp_msg(MSGT_VO, MSGL_V, "[gl] Resize: %dx%d\n",x,y);
  if (WinID >= 0) {
    int top = 0, left = 0, w = x, h = y;
    geometry(&top, &left, &w, &h, vo_screenwidth, vo_screenheight);
    Viewport(top, left, w, h);
  } else
  Viewport( 0, 0, x, y );

  MatrixMode(GL_PROJECTION);
  LoadIdentity();
  ass_border_x = ass_border_y = 0;
  if (aspect_scaling() && use_aspect) {
    int new_w, new_h;
    GLdouble scale_x, scale_y;
    aspect(&new_w, &new_h, A_WINZOOM);
    panscan_calc_windowed();
    new_w += vo_panscan_x;
    new_h += vo_panscan_y;
    scale_x = (GLdouble)new_w / (GLdouble)x;
    scale_y = (GLdouble)new_h / (GLdouble)y;
    Scaled(scale_x, scale_y, 1);
    ass_border_x = (vo_dwidth - new_w) / 2;
    ass_border_y = (vo_dheight - new_h) / 2;
  }
  Ortho(0, image_width, image_height, 0, -1,1);

  MatrixMode(GL_MODELVIEW);
  LoadIdentity();

  if (!scaled_osd) {
#ifdef CONFIG_FREETYPE
  // adjust font size to display size
  force_load_font = 1;
#endif
  vo_osd_changed(OSDTYPE_OSD);
  }
  Clear(GL_COLOR_BUFFER_BIT);
  redraw();
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
  if (mesa_buffer) *texw = (*texw + 63) & ~63;
  else if (ati_hack) *texw = (*texw + 511) & ~511;
}

//! maximum size of custom fragment program
#define MAX_CUSTOM_PROG_SIZE (1024 * 1024)
static void update_yuvconv(void) {
  int xs, ys;
  float bri = eq_bri / 100.0;
  float cont = (eq_cont + 100) / 100.0;
  float hue = eq_hue / 100.0 * 3.1415927;
  float sat = (eq_sat + 100) / 100.0;
  float rgamma = exp(log(8.0) * eq_rgamma / 100.0);
  float ggamma = exp(log(8.0) * eq_ggamma / 100.0);
  float bgamma = exp(log(8.0) * eq_bgamma / 100.0);
  gl_conversion_params_t params = {gl_target, yuvconvtype,
      bri, cont, hue, sat, rgamma, ggamma, bgamma,
      texture_width, texture_height, 0, 0, filter_strength};
  mp_get_chroma_shift(image_format, &xs, &ys);
  params.chrom_texw = params.texw >> xs;
  params.chrom_texh = params.texh >> ys;
  glSetupYUVConversion(&params);
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
  DeleteTextures(osdtexCnt, osdtex);
#ifndef FAST_OSD
  DeleteTextures(osdtexCnt, osdatex);
  for (i = 0; i < osdtexCnt; i++)
    DeleteLists(osdaDispList[i], 1);
#endif
  for (i = 0; i < osdtexCnt; i++)
    DeleteLists(osdDispList[i], 1);
  osdtexCnt = 0;
}

/**
 * \brief remove textures, display list and free memory used by EOSD
 */
static void clearEOSD(void) {
  if (eosdDispList)
    DeleteLists(eosdDispList, 1);
  eosdDispList = 0;
  if (eosdtexCnt)
    DeleteTextures(eosdtexCnt, eosdtex);
  eosdtexCnt = 0;
  free(eosdtex);
  eosdtex = NULL;
}

static inline int is_tinytex(ass_image_t *i, int tinytexcur) {
  return i->w < TINYTEX_SIZE && i->h < TINYTEX_SIZE && tinytexcur < TINYTEX_MAX;
}

static inline int is_smalltex(ass_image_t *i, int smalltexcur) {
  return i->w < SMALLTEX_SIZE && i->h < SMALLTEX_SIZE && smalltexcur < SMALLTEX_MAX;
}

static inline void tinytex_pos(int tinytexcur, int *x, int *y) {
  *x = (tinytexcur % TINYTEX_COLS) * TINYTEX_SIZE;
  *y = (tinytexcur / TINYTEX_COLS) * TINYTEX_SIZE;
}

static inline void smalltex_pos(int smalltexcur, int *x, int *y) {
  *x = (smalltexcur % SMALLTEX_COLS) * SMALLTEX_SIZE;
  *y = (smalltexcur / SMALLTEX_COLS) * SMALLTEX_SIZE;
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
  GLint scale_type = scaled_osd ? GL_LINEAR : GL_NEAREST;
  ass_image_t *img = imgs->imgs;
  ass_image_t *i;

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
    glCreateClearTex(gl_target, GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE, scale_type, LARGE_EOSD_TEX_SIZE, LARGE_EOSD_TEX_SIZE, 0);
    BindTexture(gl_target, largeeosdtex[1]);
    glCreateClearTex(gl_target, GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE, scale_type, LARGE_EOSD_TEX_SIZE, LARGE_EOSD_TEX_SIZE, 0);
  }
  for (i = img; i; i = i->next)
  {
    if (i->w <= 0 || i->h <= 0 || i->stride < i->w)
      continue;
    if (is_tinytex(i, tinytexcur))
      tinytexcur++;
    else if (is_smalltex(i, smalltexcur))
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
    if (is_tinytex(i, tinytexcur)) {
      tinytex_pos(tinytexcur, &x, &y);
      BindTexture(gl_target, largeeosdtex[0]);
      tinytexcur++;
    } else if (is_smalltex(i, smalltexcur)) {
      smalltex_pos(smalltexcur, &x, &y);
      BindTexture(gl_target, largeeosdtex[1]);
      smalltexcur++;
    } else {
      texSize(i->w, i->h, &sx, &sy);
      BindTexture(gl_target, *curtex++);
      glCreateClearTex(gl_target, GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE, scale_type, sx, sy, 0);
    }
    glUploadTex(gl_target, GL_ALPHA, GL_UNSIGNED_BYTE, i->bitmap, i->stride,
                x, y, i->w, i->h, 0);
  }
  eosdDispList = GenLists(1);
skip_upload:
  NewList(eosdDispList, GL_COMPILE);
  tinytexcur = smalltexcur = 0;
  for (i = img, curtex = eosdtex; i; i = i->next) {
    int x = 0, y = 0;
    if (i->w <= 0 || i->h <= 0 || i->stride < i->w)
      continue;
    Color4ub(i->color >> 24, (i->color >> 16) & 0xff, (i->color >> 8) & 0xff, 255 - (i->color & 0xff));
    if (is_tinytex(i, tinytexcur)) {
      tinytex_pos(tinytexcur, &x, &y);
      sx = sy = LARGE_EOSD_TEX_SIZE;
      BindTexture(gl_target, largeeosdtex[0]);
      tinytexcur++;
    } else if (is_smalltex(i, smalltexcur)) {
      smalltex_pos(smalltexcur, &x, &y);
      sx = sy = LARGE_EOSD_TEX_SIZE;
      BindTexture(gl_target, largeeosdtex[1]);
      smalltexcur++;
    } else {
      texSize(i->w, i->h, &sx, &sy);
      BindTexture(gl_target, *curtex++);
    }
    glDrawTex(i->dst_x, i->dst_y, i->w, i->h, x, y, i->w, i->h, sx, sy, use_rectangle == 1, 0, 0);
  }
  EndList();
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
    DeleteTextures(i, default_texs);
  default_texs[0] = 0;
  clearOSD();
  clearEOSD();
  if (largeeosdtex[0])
    DeleteTextures(2, largeeosdtex);
  largeeosdtex[0] = 0;
  if (DeleteBuffers && gl_buffer)
    DeleteBuffers(1, &gl_buffer);
  gl_buffer = 0; gl_buffersize = 0;
  gl_bufferptr = NULL;
  if (DeleteBuffers && gl_buffer_uv[0])
    DeleteBuffers(2, gl_buffer_uv);
  gl_buffer_uv[0] = gl_buffer_uv[1] = 0; gl_buffersize_uv = 0;
  gl_bufferptr_uv[0] = gl_bufferptr_uv[1] = 0;
#ifdef CONFIG_GL_X11
  if (mesa_bufferptr)
    FreeMemoryMESA(mDisplay, mScreen, mesa_bufferptr);
#endif
  mesa_bufferptr = NULL;
  err_shown = 0;
}

static void autodetectGlExtensions(void) {
  const char *extensions = GetString(GL_EXTENSIONS);
  const char *vendor     = GetString(GL_VENDOR);
  const char *version    = GetString(GL_VERSION);
  int is_ati = strstr(vendor, "ATI") != NULL;
  int ati_broken_pbo = 0;
  if (is_ati && strncmp(version, "2.1.", 4) == 0) {
    int ver = atoi(version + 4);
    mp_msg(MSGT_VO, MSGL_V, "[gl] Detected ATI driver version: %i\n", ver);
    ati_broken_pbo = ver && ver < 8395;
  }
  if (ati_hack      == -1) ati_hack      = ati_broken_pbo;
  if (force_pbo     == -1) force_pbo     = strstr(extensions, "_pixel_buffer_object")      ? is_ati : 0;
  if (use_rectangle == -1) use_rectangle = strstr(extensions, "_texture_non_power_of_two") ?      0 : 0;
  if (is_ati && (lscale == 1 || lscale == 2 || cscale == 1 || cscale == 2))
    mp_msg(MSGT_VO, MSGL_WARN, "[gl] Selected scaling mode may be broken on ATI cards.\n"
             "Tell _them_ to fix GL_REPEAT if you have issues.\n");
  mp_msg(MSGT_VO, MSGL_V, "[gl] Settings after autodetection: ati-hack = %i, force-pbo = %i, rectangle = %i\n",
         ati_hack, force_pbo, use_rectangle);
}

/**
 * \brief Initialize a (new or reused) OpenGL context.
 * set global gl-related variables to their default values
 */
static int initGl(uint32_t d_width, uint32_t d_height) {
  int scale_type = mipmap_gen ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR;
  autodetectGlExtensions();
  texSize(image_width, image_height, &texture_width, &texture_height);

  Disable(GL_BLEND);
  Disable(GL_DEPTH_TEST);
  DepthMask(GL_FALSE);
  Disable(GL_CULL_FACE);
  Enable(gl_target);
  DrawBuffer(vo_doublebuffering?GL_BACK:GL_FRONT);
  TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  mp_msg(MSGT_VO, MSGL_V, "[gl] Creating %dx%d texture...\n",
          texture_width, texture_height);

  if (is_yuv) {
    int i;
    int xs, ys;
    mp_get_chroma_shift(image_format, &xs, &ys);
    GenTextures(21, default_texs);
    default_texs[21] = 0;
    for (i = 0; i < 7; i++) {
      ActiveTexture(GL_TEXTURE1 + i);
      BindTexture(GL_TEXTURE_2D, default_texs[i]);
      BindTexture(GL_TEXTURE_RECTANGLE, default_texs[i + 7]);
      BindTexture(GL_TEXTURE_3D, default_texs[i + 14]);
    }
    ActiveTexture(GL_TEXTURE1);
    glCreateClearTex(gl_target, gl_texfmt, gl_format, gl_type, scale_type,
                     texture_width >> xs, texture_height >> ys, 128);
    if (mipmap_gen)
      TexParameteri(gl_target, GL_GENERATE_MIPMAP, GL_TRUE);
    ActiveTexture(GL_TEXTURE2);
    glCreateClearTex(gl_target, gl_texfmt, gl_format, gl_type, scale_type,
                     texture_width >> xs, texture_height >> ys, 128);
    if (mipmap_gen)
      TexParameteri(gl_target, GL_GENERATE_MIPMAP, GL_TRUE);
    ActiveTexture(GL_TEXTURE0);
    BindTexture(gl_target, 0);
  }
  if (is_yuv || custom_prog)
  {
    if ((MASK_NOT_COMBINERS & (1 << use_yuv)) || custom_prog) {
      if (!GenPrograms || !BindProgram) {
        mp_msg(MSGT_VO, MSGL_ERR, "[gl] fragment program functions missing!\n");
      } else {
        GenPrograms(1, &fragprog);
        BindProgram(GL_FRAGMENT_PROGRAM, fragprog);
      }
    }
    update_yuvconv();
  }
  glCreateClearTex(gl_target, gl_texfmt, gl_format, gl_type, scale_type,
                   texture_width, texture_height, 0);
  if (mipmap_gen)
    TexParameteri(gl_target, GL_GENERATE_MIPMAP, GL_TRUE);

  resize(d_width, d_height);

  ClearColor( 0.0f,0.0f,0.0f,0.0f );
  Clear( GL_COLOR_BUFFER_BIT );
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
  int xs, ys;
  image_height = height;
  image_width = width;
  image_format = format;
  is_yuv = mp_get_chroma_shift(image_format, &xs, &ys) > 0;
  is_yuv |= (xs << 8) | (ys << 16);
  glFindFormat(format, NULL, &gl_texfmt, &gl_format, &gl_type);

  int_pause = 0;
  vo_flipped = !!(flags & VOFLAG_FLIPPING);

#ifdef CONFIG_GUI
  if (use_gui) {
    // GUI creates and manages window for us
    guiGetEvent(guiSetShVideo, 0);
    goto glconfig;
  }
#endif
#ifdef CONFIG_GL_WIN32
  if (glctx.type == GLTYPE_W32 && !vo_w32_config(d_width, d_height, flags))
    return -1;
#endif
#ifdef CONFIG_GL_X11
  if (glctx.type == GLTYPE_X11) {
    XVisualInfo *vinfo=glXChooseVisual( mDisplay,mScreen,wsGLXAttrib );
    if (vinfo == NULL)
    {
      mp_msg(MSGT_VO, MSGL_ERR, "[gl] no GLX support present\n");
      return -1;
    }
    mp_msg(MSGT_VO, MSGL_V, "[gl] GLX chose visual with ID 0x%x\n", (int)vinfo->visualid);

    vo_x11_create_vo_window(vinfo, vo_dx, vo_dy, d_width, d_height, flags,
            XCreateColormap(mDisplay, mRootWin, vinfo->visual, AllocNone),
            "gl", title);
  }
#endif

glconfig:
  if (vo_config_count)
    uninitGl();
  if (glctx.setGlWindow(&glctx) == SET_WINDOW_FAILED)
    return -1;
  if (mesa_buffer && !AllocateMemoryMESA) {
    mp_msg(MSGT_VO, MSGL_ERR, "Can not enable mesa-buffer because AllocateMemoryMESA was not found\n");
    mesa_buffer = 0;
  }
  initGl(vo_dwidth, vo_dheight);

  return 0;
}

static void check_events(void)
{
    int e=glctx.check_events();
    if(e&VO_EVENT_RESIZE) resize(vo_dwidth,vo_dheight);
    if(e&VO_EVENT_EXPOSE && int_pause) redraw();
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
  GLint scale_type = scaled_osd ? GL_LINEAR : GL_NEAREST;

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
  GenTextures(1, &osdtex[osdtexCnt]);
  BindTexture(gl_target, osdtex[osdtexCnt]);
  glCreateClearTex(gl_target, GL_LUMINANCE, GL_LUMINANCE, GL_UNSIGNED_BYTE, scale_type, sx, sy, 0);
  glUploadTex(gl_target, GL_LUMINANCE, GL_UNSIGNED_BYTE, src, stride,
              0, 0, w, h, 0);

#ifndef FAST_OSD
  GenTextures(1, &osdatex[osdtexCnt]);
  BindTexture(gl_target, osdatex[osdtexCnt]);
  glCreateClearTex(gl_target, GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE, scale_type, sx, sy, 0);
  {
  int i;
  char *tmp = malloc(stride * h);
  // convert alpha from weird MPlayer scale.
  // in-place is not possible since it is reused for future OSDs
  for (i = h * stride - 1; i >= 0; i--)
    tmp[i] = -srca[i];
  glUploadTex(gl_target, GL_ALPHA, GL_UNSIGNED_BYTE, tmp, stride,
              0, 0, w, h, 0);
  free(tmp);
  }
#endif

  BindTexture(gl_target, 0);

  // Create a list for rendering this OSD part
#ifndef FAST_OSD
  osdaDispList[osdtexCnt] = GenLists(1);
  NewList(osdaDispList[osdtexCnt], GL_COMPILE);
  // render alpha
  BindTexture(gl_target, osdatex[osdtexCnt]);
  glDrawTex(x0, y0, w, h, 0, 0, w, h, sx, sy, use_rectangle == 1, 0, 0);
  EndList();
#endif
  osdDispList[osdtexCnt] = GenLists(1);
  NewList(osdDispList[osdtexCnt], GL_COMPILE);
  // render OSD
  BindTexture(gl_target, osdtex[osdtexCnt]);
  glDrawTex(x0, y0, w, h, 0, 0, w, h, sx, sy, use_rectangle == 1, 0, 0);
  EndList();

  osdtexCnt++;
}

/**
 * \param type bit 0: render OSD, bit 1: render EOSD
 */
static void do_render_osd(int type) {
  if (((type & 1) && osdtexCnt > 0) || ((type & 2) && eosdDispList)) {
    // set special rendering parameters
    if (!scaled_osd) {
      MatrixMode(GL_PROJECTION);
      PushMatrix();
      LoadIdentity();
      Ortho(0, vo_dwidth, vo_dheight, 0, -1, 1);
    }
    Enable(GL_BLEND);
    if ((type & 2) && eosdDispList) {
      BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      CallList(eosdDispList);
    }
    if ((type & 1) && osdtexCnt > 0) {
      Color4ub((osd_color >> 16) & 0xff, (osd_color >> 8) & 0xff, osd_color & 0xff, 0xff - (osd_color >> 24));
      // draw OSD
#ifndef FAST_OSD
      BlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
      CallLists(osdtexCnt, GL_UNSIGNED_INT, osdaDispList);
#endif
      BlendFunc(GL_SRC_ALPHA, GL_ONE);
      CallLists(osdtexCnt, GL_UNSIGNED_INT, osdDispList);
    }
    // set rendering parameters back to defaults
    Disable(GL_BLEND);
    if (!scaled_osd)
      PopMatrix();
    BindTexture(gl_target, 0);
  }
}

static void draw_osd(void)
{
  if (!use_osd) return;
  if (vo_osd_changed(0)) {
    int osd_h, osd_w;
    clearOSD();
    osd_w = scaled_osd ? image_width : vo_dwidth;
    osd_h = scaled_osd ? image_height : vo_dheight;
    vo_draw_text_ext(osd_w, osd_h, ass_border_x, ass_border_y, ass_border_x, ass_border_y,
                     image_width, image_height, create_osd_texture);
  }
  if (vo_doublebuffering) do_render_osd(1);
}

static void do_render(void) {
//  Enable(GL_TEXTURE_2D);
//  BindTexture(GL_TEXTURE_2D, texture_id);

  Color3f(1,1,1);
  if (is_yuv || custom_prog)
    glEnableYUVConversion(gl_target, yuvconvtype);
  glDrawTex(0, 0, image_width, image_height,
            0, 0, image_width, image_height,
            texture_width, texture_height,
            use_rectangle == 1, is_yuv,
            mpi_flipped ^ vo_flipped);
  if (is_yuv || custom_prog)
    glDisableYUVConversion(gl_target, yuvconvtype);
}

static void flip_page(void) {
  if (vo_doublebuffering) {
    if (use_glFinish) Finish();
    glctx.swapGlBuffers(&glctx);
    if (aspect_scaling() && use_aspect)
      Clear(GL_COLOR_BUFFER_BIT);
  } else {
    do_render();
    do_render_osd(3);
    if (use_glFinish) Finish();
    else Flush();
  }
}

static void redraw(void) {
  if (vo_doublebuffering) { do_render(); do_render_osd(3); }
  flip_page();
}

//static inline uint32_t draw_slice_x11(uint8_t *src[], uint32_t slice_num)
static int draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y)
{
  mpi_flipped = stride[0] < 0;
  glUploadTex(gl_target, gl_format, gl_type, src[0], stride[0],
              x, y, w, h, slice_height);
  if (is_yuv) {
    int xs, ys;
    mp_get_chroma_shift(image_format, &xs, &ys);
    ActiveTexture(GL_TEXTURE1);
    glUploadTex(gl_target, gl_format, gl_type, src[1], stride[1],
                x >> xs, y >> ys, w >> xs, h >> ys, slice_height);
    ActiveTexture(GL_TEXTURE2);
    glUploadTex(gl_target, gl_format, gl_type, src[2], stride[2],
                x >> xs, y >> ys, w >> xs, h >> ys, slice_height);
    ActiveTexture(GL_TEXTURE0);
  }
  return 0;
}

static uint32_t get_image(mp_image_t *mpi) {
  int needed_size;
  if (!GenBuffers || !BindBuffer || !BufferData || !MapBuffer) {
    if (!err_shown)
      mp_msg(MSGT_VO, MSGL_ERR, "[gl] extensions missing for dr\n"
                                "Expect a _major_ speed penalty\n");
    err_shown = 1;
    return VO_FALSE;
  }
  if (mpi->flags & MP_IMGFLAG_READABLE) return VO_FALSE;
  if (mpi->type != MP_IMGTYPE_STATIC && mpi->type != MP_IMGTYPE_TEMP &&
      (mpi->type != MP_IMGTYPE_NUMBERED || mpi->number))
    return VO_FALSE;
  if (mesa_buffer) mpi->width = texture_width;
  else if (ati_hack) {
    mpi->width = texture_width;
    mpi->height = texture_height;
  }
  mpi->stride[0] = mpi->width * mpi->bpp / 8;
  needed_size = mpi->stride[0] * mpi->height;
  if (mesa_buffer) {
#ifdef CONFIG_GL_X11
    if (mesa_bufferptr && needed_size > mesa_buffersize) {
      FreeMemoryMESA(mDisplay, mScreen, mesa_bufferptr);
      mesa_bufferptr = NULL;
    }
    if (!mesa_bufferptr)
      mesa_bufferptr = AllocateMemoryMESA(mDisplay, mScreen, needed_size, 0, 1.0, 1.0);
    mesa_buffersize = needed_size;
#endif
    mpi->planes[0] = mesa_bufferptr;
  } else {
    if (!gl_buffer)
      GenBuffers(1, &gl_buffer);
    BindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_buffer);
    if (needed_size > gl_buffersize) {
      gl_buffersize = needed_size;
      BufferData(GL_PIXEL_UNPACK_BUFFER, gl_buffersize,
                 NULL, GL_DYNAMIC_DRAW);
    }
    if (!gl_bufferptr)
      gl_bufferptr = MapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    mpi->planes[0] = gl_bufferptr;
    BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  }
  if (!mpi->planes[0]) {
    if (!err_shown)
      mp_msg(MSGT_VO, MSGL_ERR, "[gl] could not acquire buffer for dr\n"
                                "Expect a _major_ speed penalty\n");
    err_shown = 1;
    return VO_FALSE;
  }
  if (is_yuv) {
    // planar YUV
    int xs, ys;
    mp_get_chroma_shift(image_format, &xs, &ys);
    mpi->flags |= MP_IMGFLAG_COMMON_STRIDE | MP_IMGFLAG_COMMON_PLANE;
    mpi->stride[0] = mpi->width;
    mpi->planes[1] = mpi->planes[0] + mpi->stride[0] * mpi->height;
    mpi->stride[1] = mpi->width >> xs;
    mpi->planes[2] = mpi->planes[1] + mpi->stride[1] * (mpi->height >> ys);
    mpi->stride[2] = mpi->width >> xs;
    if (ati_hack && !mesa_buffer) {
      mpi->flags &= ~MP_IMGFLAG_COMMON_PLANE;
      if (!gl_buffer_uv[0]) GenBuffers(2, gl_buffer_uv);
      if (mpi->stride[1] * mpi->height > gl_buffersize_uv) {
        BindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_buffer_uv[0]);
        BufferData(GL_PIXEL_UNPACK_BUFFER, mpi->stride[1] * mpi->height,
                   NULL, GL_DYNAMIC_DRAW);
        BindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_buffer_uv[1]);
        BufferData(GL_PIXEL_UNPACK_BUFFER, mpi->stride[1] * mpi->height,
                   NULL, GL_DYNAMIC_DRAW);
        gl_buffersize_uv = mpi->stride[1] * mpi->height;
      }
      if (!gl_bufferptr_uv[0]) {
        BindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_buffer_uv[0]);
        gl_bufferptr_uv[0] = MapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
        BindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_buffer_uv[1]);
        gl_bufferptr_uv[1] = MapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
      }
      mpi->planes[1] = gl_bufferptr_uv[0];
      mpi->planes[2] = gl_bufferptr_uv[1];
    }
  }
  mpi->flags |= MP_IMGFLAG_DIRECT;
  return VO_TRUE;
}

static void clear_border(uint8_t *dst, int start, int stride, int height, int full_height, int value) {
  int right_border = stride - start;
  int bottom_border = full_height - height;
  while (height > 0) {
    memset(dst + start, value, right_border);
    dst += stride;
    height--;
  }
  if (bottom_border > 0)
    memset(dst, value, stride * bottom_border);
}

static uint32_t draw_image(mp_image_t *mpi) {
  int slice = slice_height;
  int stride[3];
  unsigned char *planes[3];
  mp_image_t mpi2 = *mpi;
  int w = mpi->w, h = mpi->h;
  if (mpi->flags & MP_IMGFLAG_DRAW_CALLBACK)
    goto skip_upload;
  mpi2.flags = 0; mpi2.type = MP_IMGTYPE_TEMP;
  mpi2.width = mpi2.w; mpi2.height = mpi2.h;
  if (force_pbo && !(mpi->flags & MP_IMGFLAG_DIRECT) && !gl_bufferptr && get_image(&mpi2) == VO_TRUE) {
    int bpp = is_yuv ? 8 : mpi->bpp;
    int xs, ys;
    mp_get_chroma_shift(image_format, &xs, &ys);
    memcpy_pic(mpi2.planes[0], mpi->planes[0], mpi->w * bpp / 8, mpi->h, mpi2.stride[0], mpi->stride[0]);
    if (is_yuv) {
      memcpy_pic(mpi2.planes[1], mpi->planes[1], mpi->w >> xs, mpi->h >> ys, mpi2.stride[1], mpi->stride[1]);
      memcpy_pic(mpi2.planes[2], mpi->planes[2], mpi->w >> xs, mpi->h >> ys, mpi2.stride[2], mpi->stride[2]);
    }
    if (ati_hack) { // since we have to do a full upload we need to clear the borders
      clear_border(mpi2.planes[0], mpi->w * bpp / 8, mpi2.stride[0], mpi->h, mpi2.height, 0);
      if (is_yuv) {
        clear_border(mpi2.planes[1], mpi->w >> xs, mpi2.stride[1], mpi->h >> ys, mpi2.height >> ys, 128);
        clear_border(mpi2.planes[2], mpi->w >> xs, mpi2.stride[2], mpi->h >> ys, mpi2.height >> ys, 128);
      }
    }
    mpi = &mpi2;
  }
  stride[0] = mpi->stride[0]; stride[1] = mpi->stride[1]; stride[2] = mpi->stride[2];
  planes[0] = mpi->planes[0]; planes[1] = mpi->planes[1]; planes[2] = mpi->planes[2];
  mpi_flipped = stride[0] < 0;
  if (mpi->flags & MP_IMGFLAG_DIRECT) {
    if (mesa_buffer) {
      PixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, 1);
      w = texture_width;
    } else {
      intptr_t base = (intptr_t)planes[0];
      if (ati_hack) { w = texture_width; h = texture_height; }
      if (mpi_flipped)
        base += (mpi->h - 1) * stride[0];
      planes[0] -= base;
      planes[1] -= base;
      planes[2] -= base;
      BindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_buffer);
      UnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
      gl_bufferptr = NULL;
      if (!(mpi->flags & MP_IMGFLAG_COMMON_PLANE))
        planes[0] = planes[1] = planes[2] = NULL;
    }
    slice = 0; // always "upload" full texture
  }
  glUploadTex(gl_target, gl_format, gl_type, planes[0], stride[0],
              mpi->x, mpi->y, w, h, slice);
  if (is_yuv) {
    int xs, ys;
    mp_get_chroma_shift(image_format, &xs, &ys);
    if ((mpi->flags & MP_IMGFLAG_DIRECT) && !(mpi->flags & MP_IMGFLAG_COMMON_PLANE)) {
      BindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_buffer_uv[0]);
      UnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
      gl_bufferptr_uv[0] = NULL;
    }
    ActiveTexture(GL_TEXTURE1);
    glUploadTex(gl_target, gl_format, gl_type, planes[1], stride[1],
                mpi->x >> xs, mpi->y >> ys, w >> xs, h >> ys, slice);
    if ((mpi->flags & MP_IMGFLAG_DIRECT) && !(mpi->flags & MP_IMGFLAG_COMMON_PLANE)) {
      BindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_buffer_uv[1]);
      UnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
      gl_bufferptr_uv[1] = NULL;
    }
    ActiveTexture(GL_TEXTURE2);
    glUploadTex(gl_target, gl_format, gl_type, planes[2], stride[2],
                mpi->x >> xs, mpi->y >> ys, w >> xs, h >> ys, slice);
    ActiveTexture(GL_TEXTURE0);
  }
  if (mpi->flags & MP_IMGFLAG_DIRECT) {
    if (mesa_buffer) PixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, 0);
    else BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  }
skip_upload:
  if (vo_doublebuffering) do_render();
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
      caps |= VFCAP_OSD | VFCAP_EOSD | (scaled_osd ? 0 : VFCAP_EOSD_UNSCALED);
    if (format == IMGFMT_RGB24 || format == IMGFMT_RGBA)
        return caps;
    if (use_yuv && mp_get_chroma_shift(format, NULL, NULL))
        return caps;
    // HACK, otherwise we get only b&w with some filters (e.g. -vf eq)
    // ideally MPlayer should be fixed instead not to use Y800 when it has the choice
    if (!use_yuv && (format == IMGFMT_Y8 || format == IMGFMT_Y800))
        return 0;
    if (!use_ycbcr && (format == IMGFMT_UYVY || format == IMGFMT_YUY2))
        return 0;
    if (many_fmts &&
         glFindFormat(format, NULL, NULL, NULL, NULL))
        return caps;
    return 0;
}


static void
uninit(void)
{
  if (!vo_config_count) return;
  uninitGl();
  if (custom_prog) free(custom_prog);
  custom_prog = NULL;
  if (custom_tex) free(custom_tex);
  custom_tex = NULL;
  uninit_mpglcontext(&glctx);
}

static const opt_t subopts[] = {
  {"manyfmts",     OPT_ARG_BOOL, &many_fmts,    NULL},
  {"osd",          OPT_ARG_BOOL, &use_osd,      NULL},
  {"scaled-osd",   OPT_ARG_BOOL, &scaled_osd,   NULL},
  {"aspect",       OPT_ARG_BOOL, &use_aspect,   NULL},
  {"ycbcr",        OPT_ARG_BOOL, &use_ycbcr,    NULL},
  {"slice-height", OPT_ARG_INT,  &slice_height, (opt_test_f)int_non_neg},
  {"rectangle",    OPT_ARG_INT,  &use_rectangle,(opt_test_f)int_non_neg},
  {"yuv",          OPT_ARG_INT,  &use_yuv,      (opt_test_f)int_non_neg},
  {"lscale",       OPT_ARG_INT,  &lscale,       (opt_test_f)int_non_neg},
  {"cscale",       OPT_ARG_INT,  &cscale,       (opt_test_f)int_non_neg},
  {"filter-strength", OPT_ARG_FLOAT, &filter_strength, NULL},
  {"ati-hack",     OPT_ARG_BOOL, &ati_hack,     NULL},
  {"force-pbo",    OPT_ARG_BOOL, &force_pbo,    NULL},
  {"mesa-buffer",  OPT_ARG_BOOL, &mesa_buffer,  NULL},
  {"glfinish",     OPT_ARG_BOOL, &use_glFinish, NULL},
  {"swapinterval", OPT_ARG_INT,  &swap_interval,NULL},
  {"customprog",   OPT_ARG_MSTRZ,&custom_prog,  NULL},
  {"customtex",    OPT_ARG_MSTRZ,&custom_tex,   NULL},
  {"customtlin",   OPT_ARG_BOOL, &custom_tlin,  NULL},
  {"customtrect",  OPT_ARG_BOOL, &custom_trect, NULL},
  {"mipmapgen",    OPT_ARG_BOOL, &mipmap_gen,   NULL},
  {"osdcolor",     OPT_ARG_INT,  &osd_color,    NULL},
  {NULL}
};

static int preinit(const char *arg)
{
    enum MPGLType gltype = GLTYPE_X11;
    // set defaults
#ifdef CONFIG_GL_WIN32
    gltype = GLTYPE_W32;
#endif
    many_fmts = 1;
    use_osd = 1;
    scaled_osd = 0;
    use_aspect = 1;
    use_ycbcr = 0;
    use_yuv = 0;
    lscale = 0;
    cscale = 0;
    filter_strength = 0.5;
    use_rectangle = -1;
    use_glFinish = 0;
    ati_hack = -1;
    force_pbo = -1;
    mesa_buffer = 0;
    swap_interval = 1;
    slice_height = 0;
    custom_prog = NULL;
    custom_tex = NULL;
    custom_tlin = 1;
    custom_trect = 0;
    mipmap_gen = 0;
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
              "  scaled-osd\n"
              "    Render OSD at movie resolution and scale it\n"
              "  noaspect\n"
              "    Do not do aspect scaling\n"
              "  rectangle=<0,1,2>\n"
              "    0: use power-of-two textures\n"
              "    1: use texture_rectangle\n"
              "    2: use texture_non_power_of_two\n"
              "  ati-hack\n"
              "    Workaround ATI bug with PBOs\n"
              "  force-pbo\n"
              "    Force use of PBO even if this involves an extra memcpy\n"
              "  glfinish\n"
              "    Call glFinish() before swapping buffers\n"
              "  swapinterval=<n>\n"
              "    Interval in displayed frames between to buffer swaps.\n"
              "    1 is equivalent to enable VSYNC, 0 to disable VSYNC.\n"
              "    Requires GLX_SGI_swap_control support to work.\n"
              "  ycbcr\n"
              "    also try to use the GL_MESA_ycbcr_texture extension\n"
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
              "    3: as 1 but without using a lookup texture.\n"
              "    4: experimental unsharp masking (sharpening).\n"
              "    5: experimental unsharp masking (sharpening) with larger radius.\n"
              "  cscale=<n>\n"
              "    as lscale but for chroma (2x slower with little visible effect).\n"
              "  filter-strength=<value>\n"
              "    set the effect strength for some lscale/cscale filters\n"
              "  customprog=<filename>\n"
              "    use a custom YUV conversion program\n"
              "  customtex=<filename>\n"
              "    use a custom YUV conversion lookup texture\n"
              "  nocustomtlin\n"
              "    use GL_NEAREST scaling for customtex texture\n"
              "  customtrect\n"
              "    use texture_rectangle for customtex texture\n"
              "  mipmapgen\n"
              "    generate mipmaps for the video image (use with TXB in customprog)\n"
              "  osdcolor=<0xAARRGGBB>\n"
              "    use the given color for the OSD\n"
              "\n" );
      return -1;
    }
    if (use_rectangle == 1)
      gl_target = GL_TEXTURE_RECTANGLE;
    else
      gl_target = GL_TEXTURE_2D;
    yuvconvtype = use_yuv | lscale << YUV_LUM_SCALER_SHIFT | cscale << YUV_CHROM_SCALER_SHIFT;
    if (many_fmts)
      mp_msg(MSGT_VO, MSGL_INFO, "[gl] using extended formats. "
               "Use -vo gl:nomanyfmts if playback fails.\n");
    mp_msg(MSGT_VO, MSGL_V, "[gl] Using %d as slice height "
             "(0 means image height).\n", slice_height);
    if (!init_mpglcontext(&glctx, gltype)) return -1;

    return 0;
}

static const struct {
  const char *name;
  int *value;
  int supportmask;
} eq_map[] = {
  {"brightness",  &eq_bri,    MASK_NOT_COMBINERS},
  {"contrast",    &eq_cont,   MASK_NOT_COMBINERS},
  {"saturation",  &eq_sat,    MASK_ALL_YUV      },
  {"hue",         &eq_hue,    MASK_ALL_YUV      },
  {"gamma",       &eq_rgamma, MASK_GAMMA_SUPPORT},
  {"red_gamma",   &eq_rgamma, MASK_GAMMA_SUPPORT},
  {"green_gamma", &eq_ggamma, MASK_GAMMA_SUPPORT},
  {"blue_gamma",  &eq_bgamma, MASK_GAMMA_SUPPORT},
  {NULL,          NULL,       0                 }
};

static int control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_PAUSE:
  case VOCTRL_RESUME:
    int_pause = (request == VOCTRL_PAUSE);
    return VO_TRUE;
  case VOCTRL_QUERY_FORMAT:
    return query_format(*(uint32_t*)data);
  case VOCTRL_GET_IMAGE:
    return get_image(data);
  case VOCTRL_DRAW_IMAGE:
    return draw_image(data);
  case VOCTRL_DRAW_EOSD:
    if (!data)
      return VO_FALSE;
    genEOSD(data);
    if (vo_doublebuffering) do_render_osd(2);
    return VO_TRUE;
  case VOCTRL_GET_EOSD_RES:
    {
      mp_eosd_res_t *r = data;
      r->w = vo_dwidth; r->h = vo_dheight;
      r->srcw = image_width; r->srch = image_height;
      r->mt = r->mb = r->ml = r->mr = 0;
      if (scaled_osd) {r->w = image_width; r->h = image_height;}
      else if (aspect_scaling()) {
        r->ml = r->mr = ass_border_x;
        r->mt = r->mb = ass_border_y;
      }
    }
    return VO_TRUE;
  case VOCTRL_GUISUPPORT:
    return VO_TRUE;
  case VOCTRL_ONTOP:
    glctx.ontop();
    return VO_TRUE;
  case VOCTRL_FULLSCREEN:
    glctx.fullscreen();
    resize(vo_dwidth, vo_dheight);
    return VO_TRUE;
  case VOCTRL_BORDER:
    glctx.border();
    resize(vo_dwidth, vo_dheight);
    return VO_TRUE;
  case VOCTRL_GET_PANSCAN:
    if (!use_aspect) return VO_NOTIMPL;
    return VO_TRUE;
  case VOCTRL_SET_PANSCAN:
    if (!use_aspect) return VO_NOTIMPL;
    resize(vo_dwidth, vo_dheight);
    return VO_TRUE;
  case VOCTRL_GET_EQUALIZER:
    if (is_yuv) {
      int i;
      va_list va;
      int *value;
      va_start(va, data);
      value = va_arg(va, int *);
      va_end(va);
      for (i = 0; eq_map[i].name; i++)
        if (strcmp(data, eq_map[i].name) == 0) break;
      if (!(eq_map[i].supportmask & (1 << use_yuv)))
        break;
      *value = *eq_map[i].value;
      return VO_TRUE;
    }
    break;
  case VOCTRL_SET_EQUALIZER:
    if (is_yuv) {
      int i;
      va_list va;
      int value;
      va_start(va, data);
      value = va_arg(va, int);
      va_end(va);
      for (i = 0; eq_map[i].name; i++)
        if (strcmp(data, eq_map[i].name) == 0) break;
      if (!(eq_map[i].supportmask & (1 << use_yuv)))
        break;
      *eq_map[i].value = value;
      update_yuvconv();
      return VO_TRUE;
    }
    break;
  case VOCTRL_UPDATE_SCREENINFO:
    glctx.update_xinerama_info();
    return VO_TRUE;
  }
  return VO_NOTIMPL;
}
