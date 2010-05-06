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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "libvo/osd.h"
#include "libvo/font_load.h"
#include "libvo/sub.h"
#include "osdep/keycodes.h"
#include "asxparser.h"
#include "stream/stream.h"
#include "input/input.h"

#include "libmpcodecs/img_format.h"
#include "libmpcodecs/mp_image.h"
#include "m_option.h"
#include "m_struct.h"
#include "menu.h"

extern menu_info_t menu_info_cmdlist;
extern menu_info_t menu_info_chapsel;
extern menu_info_t menu_info_pt;
extern menu_info_t menu_info_filesel;
extern menu_info_t menu_info_txt;
extern menu_info_t menu_info_console;
extern menu_info_t menu_info_pref;
extern menu_info_t menu_info_dvbsel;


menu_info_t* menu_info_list[] = {
  &menu_info_pt,
  &menu_info_cmdlist,
  &menu_info_chapsel,
  &menu_info_filesel,
  &menu_info_txt,
  &menu_info_console,
#ifdef CONFIG_DVBIN
  &menu_info_dvbsel,
#endif
  &menu_info_pref,
  NULL
};

typedef struct key_cmd_s {
  int key;
  char *cmd;
} key_cmd_t;

typedef struct menu_cmd_bindings_s {
  char *name;
  key_cmd_t *bindings;
  int binding_num;
  struct menu_cmd_bindings_s *parent;
} menu_cmd_bindings_t;

struct menu_def_st {
  char* name;
  menu_info_t* type;
  void* cfg;
  char* args;
};

double menu_mouse_x = -1.0;
double menu_mouse_y = -1.0;
int menu_mouse_pos_updated = 0;

static struct MPContext *menu_ctx = NULL;
static menu_def_t* menu_list = NULL;
static int menu_count = 0;
static menu_cmd_bindings_t *cmd_bindings = NULL;
static int cmd_bindings_num = 0;


static menu_cmd_bindings_t *get_cmd_bindings(const char *name)
{
  int i;
  for (i = 0; i < cmd_bindings_num; ++i)
    if (!strcasecmp(cmd_bindings[i].name, name))
      return &cmd_bindings[i];
  return NULL;
}

static int menu_parse_config(char* buffer) {
  char *element,*body, **attribs, *name;
  menu_info_t* minfo = NULL;
  int r,i;
  ASX_Parser_t* parser = asx_parser_new();

  while(1) {
    r = asx_get_element(parser,&buffer,&element,&body,&attribs);
    if(r < 0) {
      mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_SyntaxErrorAtLine,parser->line);
      asx_parser_free(parser);
      return 0;
    } else if(r == 0) {
      asx_parser_free(parser);
      return 1;
    }
    // Has it a name ?
    name = asx_get_attrib("name",attribs);
    if(!name) {
      mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_MenuDefinitionsNeedANameAttrib,parser->line);
      free(element);
      if(body) free(body);
      asx_free_attribs(attribs);
      continue;
    }

    if (!strcasecmp(element, "keybindings")) {
      menu_cmd_bindings_t *bindings = cmd_bindings;
      char *parent_bindings;
      cmd_bindings = realloc(cmd_bindings,
                             (cmd_bindings_num+1)*sizeof(menu_cmd_bindings_t));
      for (i = 0; i < cmd_bindings_num; ++i)
        if (cmd_bindings[i].parent)
          cmd_bindings[i].parent = cmd_bindings[i].parent-bindings+cmd_bindings;
      bindings = &cmd_bindings[cmd_bindings_num];
      memset(bindings, 0, sizeof(menu_cmd_bindings_t));
      bindings->name = name;
      parent_bindings = asx_get_attrib("parent",attribs);
      if (parent_bindings) {
        bindings->parent = get_cmd_bindings(parent_bindings);
        free(parent_bindings);
      }
      free(element);
      asx_free_attribs(attribs);
      if (body) {
        char *bd = body;
        char *b, *key, *cmd;
        int keycode;
        for(;;) {
          r = asx_get_element(parser,&bd,&element,&b,&attribs);
          if(r < 0) {
            mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_SyntaxErrorAtLine,
                   parser->line);
            free(body);
            asx_parser_free(parser);
            return 0;
          }
          if(r == 0)
            break;
          if (!strcasecmp(element, "binding")) {
            key = asx_get_attrib("key",attribs);
            cmd = asx_get_attrib("cmd",attribs);
            if (key && (keycode = mp_input_get_key_from_name(key)) >= 0) {
              keycode &= ~MP_NO_REPEAT_KEY;
              mp_msg(MSGT_GLOBAL,MSGL_V,
                     "[libmenu] got keybinding element %d %s=>[%s].\n",
                     keycode, key, cmd ? cmd : "");
              bindings->bindings = realloc(bindings->bindings,
                                   (bindings->binding_num+1)*sizeof(key_cmd_t));
              bindings->bindings[bindings->binding_num].key = keycode;
              bindings->bindings[bindings->binding_num].cmd = cmd;
              ++bindings->binding_num;
            }
            else
              free(cmd);
            free(key);
          }
          free(element);
          asx_free_attribs(attribs);
          free(b);
        }
        free(body);
      }
      ++cmd_bindings_num;
      continue;
    }
    // Try to find this menu type in our list
    for(i = 0, minfo = NULL ; menu_info_list[i] ; i++) {
      if(strcasecmp(element,menu_info_list[i]->name) == 0) {
	minfo = menu_info_list[i];
	break;
      }
    }
    // Got it : add this to our list
    if(minfo) {
      menu_list = realloc(menu_list,(menu_count+2)*sizeof(menu_def_t));
      menu_list[menu_count].name = name;
      menu_list[menu_count].type = minfo;
      menu_list[menu_count].cfg = m_struct_alloc(&minfo->priv_st);
      menu_list[menu_count].args = body;
      // Setup the attribs
      for(i = 0 ; attribs[2*i] ; i++) {
	if(strcasecmp(attribs[2*i],"name") == 0) continue;
	if(!m_struct_set(&minfo->priv_st,menu_list[menu_count].cfg,attribs[2*i], attribs[2*i+1]))
	  mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_BadAttrib,attribs[2*i],attribs[2*i+1],
		 name,parser->line);
      }
      menu_count++;
      memset(&menu_list[menu_count],0,sizeof(menu_def_t));
    } else {
      mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_UnknownMenuType,element,parser->line);
      free(name);
      if(body) free(body);
    }

    free(element);
    asx_free_attribs(attribs);
  }

}


/// This will build the menu_defs list from the cfg file
#define BUF_STEP 1024
#define BUF_MIN 128
#define BUF_MAX BUF_STEP*1024
int menu_init(struct MPContext *mpctx, char* cfg_file) {
  char* buffer = NULL;
  int bl = BUF_STEP, br = 0;
  int f, fd;
#ifndef CONFIG_FREETYPE
  if(vo_font == NULL)
    return 0;
#endif
  fd = open(cfg_file, O_RDONLY);
  if(fd < 0) {
    mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_CantOpenConfigFile,cfg_file);
    return 0;
  }
  buffer = malloc(bl);
  while(1) {
    int r;
    if(bl - br < BUF_MIN) {
      if(bl >= BUF_MAX) {
	mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_ConfigFileIsTooBig,BUF_MAX/1024);
	close(fd);
	free(buffer);
	return 0;
      }
      bl += BUF_STEP;
      buffer = realloc(buffer,bl);
    }
    r = read(fd,buffer+br,bl-br);
    if(r == 0) break;
    br += r;
  }
  if(!br) {
    mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_ConfigFileIsEmpty);
    return 0;
  }
  buffer[br-1] = '\0';

  close(fd);

  menu_ctx = mpctx;
  f = menu_parse_config(buffer);
  free(buffer);
  return f;
}

// Destroy all this stuff
void menu_uninit(void) {
  int i;
  for(i = 0 ; menu_list && menu_list[i].name ; i++) {
    free(menu_list[i].name);
    m_struct_free(&menu_list[i].type->priv_st,menu_list[i].cfg);
    if(menu_list[i].args) free(menu_list[i].args);
  }
  free(menu_list);
  menu_count = 0;
  for (i = 0; i < cmd_bindings_num; ++i) {
    free(cmd_bindings[i].name);
    while(cmd_bindings[i].binding_num > 0)
      free(cmd_bindings[i].bindings[--cmd_bindings[i].binding_num].cmd);
    free(cmd_bindings[i].bindings);
  }
  free(cmd_bindings);
}

/// Default read_key function
int menu_dflt_read_key(menu_t* menu,int cmd) {
  int i;
  menu_cmd_bindings_t *bindings = get_cmd_bindings(menu->type->name);
  if (!bindings)
    bindings = get_cmd_bindings(menu->type->type->name);
  if (!bindings)
    bindings = get_cmd_bindings("default");
  while (bindings) {
    for (i = 0; i < bindings->binding_num; ++i) {
      if (bindings->bindings[i].key == cmd) {
        if (bindings->bindings[i].cmd)
          mp_input_parse_and_queue_cmds(bindings->bindings[i].cmd);
        return 1;
      }
    }
    bindings = bindings->parent;
  }
  return 0;
}

menu_t* menu_open(char *name) {
  menu_t* m;
  int i;

  for(i = 0 ; menu_list[i].name != NULL ; i++) {
    if(strcmp(name,menu_list[i].name) == 0)
      break;
  }
  if(menu_list[i].name == NULL) {
    mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_MenuNotFound,name);
    return NULL;
  }
  m = calloc(1,sizeof(menu_t));
  m->priv_st = &(menu_list[i].type->priv_st);
  m->priv = m_struct_copy(m->priv_st,menu_list[i].cfg);
  m->ctx = menu_ctx;
  m->type = &menu_list[i];
  if(menu_list[i].type->open(m,menu_list[i].args))
    return m;
  if(m->priv)
    m_struct_free(m->priv_st,m->priv);
  free(m);
  mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_MenuInitFailed,name);
  return NULL;
}

void menu_draw(menu_t* menu,mp_image_t* mpi) {
  if(menu->show && menu->draw)
    menu->draw(menu,mpi);
}

void menu_update_mouse_pos(double x, double y) {
  menu_mouse_x = x;
  menu_mouse_y = y;
  menu_mouse_pos_updated = 1;
}

void menu_read_cmd(menu_t* menu,int cmd) {
  if(menu->read_cmd)
    menu->read_cmd(menu,cmd);
}

void menu_close(menu_t* menu) {
  if(menu->close)
    menu->close(menu);
  if(menu->priv)
    m_struct_free(menu->priv_st,menu->priv);
  free(menu);
}

int menu_read_key(menu_t* menu,int cmd) {
  if(menu->read_key)
    return menu->read_key(menu,cmd);
  else
    return menu_dflt_read_key(menu,cmd);
}

///////////////////////////// Helpers ////////////////////////////////////

typedef void (*draw_alpha_f)(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride);

inline static draw_alpha_f get_draw_alpha(uint32_t fmt) {
  switch(fmt) {
  case IMGFMT_BGR12:
  case IMGFMT_RGB12:
    return vo_draw_alpha_rgb12;
  case IMGFMT_BGR15:
  case IMGFMT_RGB15:
    return vo_draw_alpha_rgb15;
  case IMGFMT_BGR16:
  case IMGFMT_RGB16:
    return vo_draw_alpha_rgb16;
  case IMGFMT_BGR24:
  case IMGFMT_RGB24:
    return vo_draw_alpha_rgb24;
  case IMGFMT_BGR32:
  case IMGFMT_RGB32:
    return vo_draw_alpha_rgb32;
  case IMGFMT_YV12:
  case IMGFMT_I420:
  case IMGFMT_IYUV:
  case IMGFMT_YVU9:
  case IMGFMT_IF09:
  case IMGFMT_Y800:
  case IMGFMT_Y8:
    return vo_draw_alpha_yv12;
  case IMGFMT_YUY2:
    return vo_draw_alpha_yuy2;
  case IMGFMT_UYVY:
    return vo_draw_alpha_uyvy;
  }

  return NULL;
}

// return the real height of a char:
static inline int get_height(int c,int h){
    int font;
    if ((font=vo_font->font[c])>=0)
	if(h<vo_font->pic_a[font]->h) h=vo_font->pic_a[font]->h;
    return h;
}

static void render_txt(char *txt)
{
  while (*txt) {
    int c = utf8_get_char((const char**)&txt);
    render_one_glyph(vo_font, c);
  }
}

#ifdef CONFIG_FRIBIDI
#include <fribidi/fribidi.h>
#include "libavutil/common.h"
char *menu_fribidi_charset = NULL;
int menu_flip_hebrew = 0;
int menu_fribidi_flip_commas = 0;

static char *menu_fribidi(char *txt)
{
  static int char_set_num = -1;
  static FriBidiChar *logical, *visual;
  static size_t buffer_size = 1024;
  static char *outputstr;

  FriBidiCharType base;
  fribidi_boolean log2vis;
  size_t len;

  if (menu_flip_hebrew) {
    len = strlen(txt);
    if (char_set_num == -1) {
      fribidi_set_mirroring (1);
      fribidi_set_reorder_nsm (0);
      char_set_num = fribidi_parse_charset("UTF-8");
      buffer_size = FFMAX(1024,len+1);
      logical = malloc(buffer_size);
      visual = malloc(buffer_size);
      outputstr = malloc(buffer_size);
    } else if (len+1 > buffer_size) {
      buffer_size = len+1;
      logical = realloc(logical, buffer_size);
      visual = realloc(visual, buffer_size);
      outputstr = realloc(outputstr, buffer_size);
    }
    len = fribidi_charset_to_unicode (char_set_num, txt, len, logical);
    base = menu_fribidi_flip_commas?FRIBIDI_TYPE_ON:FRIBIDI_TYPE_L;
    log2vis = fribidi_log2vis (logical, len, &base, visual, NULL, NULL, NULL);
    if (log2vis) {
      len = fribidi_remove_bidi_marks (visual, len, NULL, NULL, NULL);
      fribidi_unicode_to_charset (char_set_num, visual, len, outputstr);
      return outputstr;
    }
  }
  return txt;
}
#endif

void menu_draw_text(mp_image_t* mpi,char* txt, int x, int y) {
  draw_alpha_f draw_alpha = get_draw_alpha(mpi->imgfmt);
  int font;

  if(!draw_alpha) {
    mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_UnsupportedOutformat);
    return;
  }

#ifdef CONFIG_FRIBIDI
  txt = menu_fribidi(txt);
#endif
  render_txt(txt);

  while (*txt) {
    int c=utf8_get_char((const char**)&txt);
    if ((font=vo_font->font[c])>=0 && (x + vo_font->width[c] <= mpi->w) && (y + vo_font->pic_a[font]->h <= mpi->h))
      draw_alpha(vo_font->width[c], vo_font->pic_a[font]->h,
		 vo_font->pic_b[font]->bmp+vo_font->start[c],
		 vo_font->pic_a[font]->bmp+vo_font->start[c],
		 vo_font->pic_a[font]->w,
		 mpi->planes[0] + y * mpi->stride[0] + x * (mpi->bpp>>3),
		 mpi->stride[0]);
    x+=vo_font->width[c]+vo_font->charspace;
  }

}

void menu_draw_text_full(mp_image_t* mpi,char* txt,
			 int x, int y,int w, int h,
			 int vspace, int warp, int align, int anchor) {
  int need_w,need_h;
  int sy, ymin, ymax;
  int sx, xmin, xmax, xmid, xrmin;
  int ll = 0;
  int font;
  draw_alpha_f draw_alpha = get_draw_alpha(mpi->imgfmt);

  if(!draw_alpha) {
    mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_UnsupportedOutformat);
    return;
  }

#ifdef CONFIG_FRIBIDI
  txt = menu_fribidi(txt);
#endif
  render_txt(txt);

  if(x > mpi->w || y > mpi->h)
    return;

  if(anchor & MENU_TEXT_VCENTER) {
    if(h <= 0) h = mpi->h;
    ymin = y - h/2;
    ymax = y + h/2;
  }  else if(anchor & MENU_TEXT_BOT) {
    if(h <= 0) h = mpi->h - y;
    ymin = y - h;
    ymax = y;
  } else {
    if(h <= 0) h = mpi->h - y;
    ymin = y;
    ymax = y + h;
  }

  if(anchor & MENU_TEXT_HCENTER) {
    if(w <= 0) w = mpi->w;
    xmin = x - w/2;
    xmax = x + w/2;
  }  else if(anchor & MENU_TEXT_RIGHT) {
    if(w <= 0) w = mpi->w -x;
    xmin = x - w;
    xmax = x;
  } else {
    if(w <= 0) w = mpi->w -x;
    xmin = x;
    xmax = x + w;
  }

  // How many space do we need to draw this ?
  menu_text_size(txt,w,vspace,warp,&need_w,&need_h);

  // Find the first line
  if(align & MENU_TEXT_VCENTER)
    sy = ymin + ((h - need_h)/2);
  else if(align & MENU_TEXT_BOT)
    sy = ymax - need_h - 1;
  else
    sy = y;

#if 0
  // Find the first col
  if(align & MENU_TEXT_HCENTER)
    sx = xmin + ((w - need_w)/2);
  else if(align & MENU_TEXT_RIGHT)
    sx = xmax - need_w;
#endif

  xmid = xmin + (xmax - xmin) / 2;
  xrmin = xmin;
  // Clamp the bb to the mpi size
  if(ymin < 0) ymin = 0;
  if(xmin < 0) xmin = 0;
  if(ymax > mpi->h) ymax = mpi->h;
  if(xmax > mpi->w) xmax = mpi->w;

  // Jump some the beginnig text if needed
  while(sy < ymin && *txt) {
    int c=utf8_get_char((const char**)&txt);
    if(c == '\n' || (warp && ll + vo_font->width[c] > w)) {
      ll = 0;
      sy += vo_font->height + vspace;
      if(c == '\n') continue;
    }
    ll += vo_font->width[c]+vo_font->charspace;
  }
  if(*txt == '\0') // Nothing left to draw
      return;

  while(sy < ymax && *txt) {
    char* line_end = NULL;
    int n;

    if(txt[0] == '\n') { // New line
      sy += vo_font->height + vspace;
      txt++;
      continue;
    }

    // Get the length and end of this line
    for(n = 0, ll = 0 ; txt[n] != '\0' && txt[n] != '\n'  ; n++) {
      unsigned char c = txt[n];
      if(warp && ll + vo_font->width[c]  > w)  break;
      ll += vo_font->width[c]+vo_font->charspace;
    }
    line_end = &txt[n];
    ll -= vo_font->charspace;


    if(align & (MENU_TEXT_HCENTER|MENU_TEXT_RIGHT)) {
      // Too long line
      if(ll > xmax-xmin) {
	if(align & MENU_TEXT_HCENTER) {
	  int mid = ll/2;
	  // Find the middle point
	  for(n--, ll = 0 ; n <= 0 ; n--) {
	    ll += vo_font->width[(int)txt[n]]+vo_font->charspace;
	    if(ll - vo_font->charspace > mid) break;
	  }
	  ll -= vo_font->charspace;
	  sx = xmid + mid - ll;
	} else// MENU_TEXT_RIGHT)
	  sx = xmax + vo_font->charspace;

	// We are after the start point -> go back
	if(sx > xmin) {
	  for(n-- ; n <= 0 ; n--) {
	    unsigned char c = txt[n];
	    if(sx - vo_font->width[c] - vo_font->charspace < xmin) break;
	    sx -= vo_font->width[c]+vo_font->charspace;
	  }
	} else { // We are before the start point -> go forward
	  for( ; sx < xmin && (&txt[n]) != line_end ; n++) {
	    unsigned char c = txt[n];
	    sx += vo_font->width[c]+vo_font->charspace;
	  }
	}
	txt = &txt[n]; // Jump to the new start char
      } else {
	if(align & MENU_TEXT_HCENTER)
	  sx = xmid - ll/2;
	else
	  sx = xmax - 1 - ll;
      }
    } else {
      for(sx = xrmin ;  sx < xmin && txt != line_end ; txt++) {
	unsigned char c = txt[n];
	sx += vo_font->width[c]+vo_font->charspace;
      }
    }

    while(sx < xmax && txt != line_end) {
      int c=utf8_get_char((const char**)&txt);
      font = vo_font->font[c];
      if(font >= 0) {
 	int cs = (vo_font->pic_a[font]->h - vo_font->height) / 2;
	if ((sx + vo_font->width[c] <= xmax) && (sy + vo_font->height <= ymax) )
	  draw_alpha(vo_font->width[c], vo_font->height,
		     vo_font->pic_b[font]->bmp+vo_font->start[c] +
		     cs * vo_font->pic_a[font]->w,
		     vo_font->pic_a[font]->bmp+vo_font->start[c] +
		     cs * vo_font->pic_a[font]->w,
		     vo_font->pic_a[font]->w,
		     mpi->planes[0] + sy * mpi->stride[0] + sx * (mpi->bpp>>3),
		     mpi->stride[0]);
	//	else
	//printf("Can't draw '%c'\n",c);
      }
      sx+=vo_font->width[c]+vo_font->charspace;
    }
    txt = line_end;
    if(txt[0] == '\0') break;
    sy += vo_font->height + vspace;
  }
}

int menu_text_length(char* txt) {
  int l = 0;
  render_txt(txt);
  while (*txt) {
    int c=utf8_get_char((const char**)&txt);
    l += vo_font->width[c]+vo_font->charspace;
  }
  return l - vo_font->charspace;
}

void menu_text_size(char* txt,int max_width, int vspace, int warp, int* _w, int* _h) {
  int l = 1, i = 0;
  int w = 0;

  render_txt(txt);
  while (*txt) {
    int c=utf8_get_char((const char**)&txt);
    if(c == '\n' || (warp && i + vo_font->width[c] >= max_width)) {
      i -= vo_font->charspace;
      if (i > w) w = i;
      if(*txt)
	l++;
      i = 0;
      if(c == '\n') continue;
    }
    i += vo_font->width[c]+vo_font->charspace;
  }
  if (i > 0) {
    i -= vo_font->charspace;
    if (i > w) w = i;
  }

  *_w = w;
  *_h = (l-1) * (vo_font->height + vspace) + vo_font->height;
}


int menu_text_num_lines(char* txt, int max_width) {
  int l = 1, i = 0;
  render_txt(txt);
  while (*txt) {
    int c=utf8_get_char((const char**)&txt);
    if(c == '\n' || i + vo_font->width[c] > max_width) {
      l++;
      i = 0;
      if(c == '\n') continue;
    }
    i += vo_font->width[c]+vo_font->charspace;
  }
  return l;
}

static char* menu_text_get_next_line(char* txt, int max_width)
{
  int i = 0;
  render_txt(txt);
  while (*txt) {
    int c=utf8_get_char((const char**)&txt);
    if(c == '\n') {
      txt++;
      break;
    }
    i += vo_font->width[c];
    if(i >= max_width)
      break;
    i += vo_font->charspace;
  }
  return txt;
}


void menu_draw_box(mp_image_t* mpi,unsigned char grey,unsigned char alpha, int x, int y, int w, int h) {
  draw_alpha_f draw_alpha = get_draw_alpha(mpi->imgfmt);
  int g;

  if(!draw_alpha) {
    mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_UnsupportedOutformat);
    return;
  }

  if(x > mpi->w || y > mpi->h) return;

  if(x < 0) w += x, x = 0;
  if(x+w > mpi->w) w = mpi->w-x;
  if(y < 0) h += y, y = 0;
  if(y+h > mpi->h) h = mpi->h-y;

  g = ((256-alpha)*grey)>>8;
  if(g < 1) g = 1;

  {
    int stride = (w+7)&(~7); // round to 8
    char pic[stride*h],pic_alpha[stride*h];
    memset(pic,g,stride*h);
    memset(pic_alpha,alpha,stride*h);
    draw_alpha(w,h,pic,pic_alpha,stride,
               mpi->planes[0] + y * mpi->stride[0] + x * (mpi->bpp>>3),
               mpi->stride[0]);
  }

}
