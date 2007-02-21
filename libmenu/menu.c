
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
#include "osdep/keycodes.h"
#include "asxparser.h"
#include "stream/stream.h"

#include "libmpcodecs/img_format.h"
#include "libmpcodecs/mp_image.h"
#include "m_option.h"
#include "m_struct.h"
#include "menu.h"

extern menu_info_t menu_info_cmdlist;
extern menu_info_t menu_info_pt;
extern menu_info_t menu_info_filesel;
extern menu_info_t menu_info_txt;
extern menu_info_t menu_info_console;
extern menu_info_t menu_info_pref;
#ifdef HAS_DVBIN_SUPPORT
extern menu_info_t menu_info_dvbsel;
#endif


menu_info_t* menu_info_list[] = {
  &menu_info_pt,
  &menu_info_cmdlist,
  &menu_info_filesel,
  &menu_info_txt,
  &menu_info_console,
#ifdef HAS_DVBIN_SUPPORT
  &menu_info_dvbsel,
#endif  
  &menu_info_pref,
  NULL
};

typedef struct menu_def_st {
  char* name;
  menu_info_t* type;
  void* cfg;
  char* args;
} menu_def_t;

static struct MPContext *menu_ctx = NULL;
static menu_def_t* menu_list = NULL;
static int menu_count = 0;


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
#ifndef HAVE_FREETYPE
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
void menu_unint(void) {
  int i;
  for(i = 0 ; menu_list && menu_list[i].name ; i++) {
    free(menu_list[i].name);
    m_struct_free(&menu_list[i].type->priv_st,menu_list[i].cfg);
    if(menu_list[i].args) free(menu_list[i].args);
  }
  free(menu_list);
  menu_count = 0;
}

/// Default read_key function
void menu_dflt_read_key(menu_t* menu,int cmd) {
  switch(cmd) {
  case KEY_UP:
    menu->read_cmd(menu,MENU_CMD_UP);
    break;
  case KEY_DOWN:
    menu->read_cmd(menu,MENU_CMD_DOWN);
    break;
  case KEY_LEFT:
    menu->read_cmd(menu,MENU_CMD_LEFT);
    break;
  case KEY_ESC:
    menu->read_cmd(menu,MENU_CMD_CANCEL);
    break;
  case KEY_RIGHT:
    menu->read_cmd(menu,MENU_CMD_RIGHT);
    break;
  case KEY_ENTER:
    menu->read_cmd(menu,MENU_CMD_OK);
    break;
  }
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

void menu_read_key(menu_t* menu,int cmd) {
  if(menu->read_key)
    menu->read_key(menu,cmd);
  else
    menu_dflt_read_key(menu,cmd);
}

///////////////////////////// Helpers ////////////////////////////////////

typedef void (*draw_alpha_f)(int w,int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride);

inline static draw_alpha_f get_draw_alpha(uint32_t fmt) {
  switch(fmt) {
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

#ifdef HAVE_FREETYPE
#define render_txt(t)  { unsigned char* p = t;  while(*p) render_one_glyph(vo_font,*p++); }
#else
#define render_txt(t)
#endif
    

void menu_draw_text(mp_image_t* mpi,char* txt, int x, int y) {
  draw_alpha_f draw_alpha = get_draw_alpha(mpi->imgfmt);
  int font;

  if(!draw_alpha) {
    mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_UnsupportedOutformat);
    return;
  }

  render_txt(txt);

  while (*txt) {
    unsigned char c=*txt++;
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
    unsigned char c=*txt++;
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
      unsigned char c = *txt++;
      font = vo_font->font[c];
      if(font >= 0) {
 	int cs = (vo_font->pic_a[font]->h - vo_font->height) / 2;
	if ((sx + vo_font->width[c] < xmax)  &&  (sy + vo_font->height < ymax) )
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
    unsigned char c=*txt++;
    l += vo_font->width[c]+vo_font->charspace;
  }
  return l - vo_font->charspace;
}

void menu_text_size(char* txt,int max_width, int vspace, int warp, int* _w, int* _h) {
  int l = 1, i = 0;
  int w = 0;

  render_txt(txt);
  while (*txt) {
    unsigned char c=*txt++;
    if(c == '\n' || (warp && i + vo_font->width[c] >= max_width)) {
      if(*txt)
	l++;
      i = 0;
      if(c == '\n') continue;
    }
    i += vo_font->width[c]+vo_font->charspace;
    if(i > w) w = i;
  }
  
  *_w = w;
  *_h = (l-1) * (vo_font->height + vspace) + vo_font->height;
}


int menu_text_num_lines(char* txt, int max_width) {
  int l = 1, i = 0;
  render_txt(txt);
  while (*txt) {
    unsigned char c=*txt++;
    if(c == '\n' || i + vo_font->width[c] > max_width) {
      l++;
      i = 0;
      if(c == '\n') continue;
    }
    i += vo_font->width[c]+vo_font->charspace;
  }
  return l;
}
  
char* menu_text_get_next_line(char* txt, int max_width) {
  int i = 0;
  render_txt(txt);
  while (*txt) {
    unsigned char c=*txt;
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
