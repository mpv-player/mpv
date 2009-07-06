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

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "config.h"

#include "libmpcodecs/img_format.h"
#include "libmpcodecs/mp_image.h"

#include "m_struct.h"
#include "menu.h"

#include "libvo/font_load.h"
#include "osdep/keycodes.h"

#define IMPL 1
#include "menu_list.h"

extern double menu_mouse_x;
extern double menu_mouse_y;
extern int menu_mouse_pos_updated;
static int mouse_x;
static int mouse_y;
static int selection_x;
static int selection_y;
static int selection_w;
static int selection_h;

#define mpriv (menu->priv)

void menu_list_draw(menu_t* menu,mp_image_t* mpi) {
  int x = mpriv->x;
  int y = mpriv->y;
  int i;
  int h = mpriv->h;
  int w = mpriv->w;
  int dh = 0,dw =  0;
  int bx, dx, dy = 0;
  int need_h = 0,need_w = 0,ptr_l,sidx = 0;
  int th,count = 0;
  int bg_w;
  int line_h;
  list_entry_t* m;

  if(mpriv->count < 1)
    return;

  if(h <= 0) h = mpi->height;
  if(w <= 0) w = mpi->width;
  dh = h - 2*mpriv->minb;
  dw = w - 2*mpriv->minb;
  ptr_l = mpriv->ptr ? menu_text_length(mpriv->ptr) : 0;
  // mpi is too small
  if(h - vo_font->height <= 0 || w - ptr_l <= 0 || dw <= 0 || dh <= 0)
    return;

  line_h = mpriv->vspace + vo_font->height;
  th = menu_text_num_lines(mpriv->title,dw) * line_h + mpriv->vspace;

  // the selected item is hidden, find a visible one
  if(mpriv->current->hide) {
    // try the next
    for(m = mpriv->current->next ; m ; m = m->next)
      if(!m->hide) break;
    if(!m) // or the previous
      for(m = mpriv->current->prev ; m ; m = m->prev)
        if(!m->hide) break;
    if(m) mpriv->current = m;
    else ptr_l = 0;
  }

  for(i = 0, m = mpriv->menu ; m ; m = m->next, i++) {
    int ll;
    if(m->hide) continue;
    ll = menu_text_length(m->txt);
    if(ptr_l + ll > need_w) need_w = ptr_l + ll;
    if(m == mpriv->current) sidx = i;
    count++;
  }
  if(need_w > dw) need_w = dw;
  if(x >= 0)
    x += mpriv->minb;
  if(y > 0)
    y += mpriv->minb;
  else
    y = mpriv->minb;

  need_h = count * line_h - mpriv->vspace;
  if( need_h + th > dh) {
    int start,end;
    mpriv->disp_lines = (dh + mpriv->vspace - th) / line_h;
    if(mpriv->disp_lines < 4) {
      th = 0;
      mpriv->disp_lines = (dh + mpriv->vspace) / line_h;
    }
    // Too smoll
    if(mpriv->disp_lines < 1) return;
    need_h = mpriv->disp_lines * line_h - mpriv->vspace;

    start = sidx - (mpriv->disp_lines/2);
    if(start < 0) start = 0;
    end = start + mpriv->disp_lines;
    if(end > count) {
      end = count;
      if(end - start < mpriv->disp_lines)
	start = end - mpriv->disp_lines < 0 ? 0 : end - mpriv->disp_lines;
    }
    m = mpriv->menu;
    for(i = 0 ; m->next && i < start ; ) {
      if(!m->hide) i++;
      m = m->next;
    }
  } else {
    m = mpriv->menu;
    mpriv->disp_lines = count;
  }

  bg_w = need_w+2*mpriv->minb;
  if(th > 0) {
    int tw,th2;
    menu_text_size(mpriv->title,dw,mpriv->vspace,1,&tw,&th2);
    if(mpriv->title_bg >= 0) {
      if(tw+2*mpriv->minb > bg_w) bg_w = tw+2*mpriv->minb;
      menu_draw_box(mpi,mpriv->title_bg,mpriv->title_bg_alpha,
                    x < 0 ? (mpi->w-bg_w)/2 : x-mpriv->minb,dy+y-mpriv->vspace/2,bg_w,th);
    }
    menu_draw_text_full(mpi,mpriv->title,
			x < 0 ? mpi->w / 2 : x,
			dy+y, x < 0 ? dw : (tw > need_w ? tw : need_w), 0,
			mpriv->vspace,1,
			MENU_TEXT_TOP|MENU_TEXT_HCENTER,
			MENU_TEXT_TOP|(x < 0 ? MENU_TEXT_HCENTER :MENU_TEXT_LEFT));
    dy += th;
  }

  dx = x < 0 ? (mpi->w - need_w) / 2 : x;
  bx = x < 0 ? (mpi->w - bg_w) / 2 : x - mpriv->minb;

  // If mouse moved, try to update selected menu item by the mouse position.
  if (menu_mouse_pos_updated) {
    mouse_x = menu_mouse_x * mpi->width;
    mouse_y = menu_mouse_y * mpi->height;
    if (mouse_x >= bx && mouse_x < bx + bg_w) {
      int by = dy + y - mpriv->vspace / 2;
      int max_by = dh + y + mpriv->vspace / 2;
      if (mouse_y >= by && mouse_y < max_by) {
        int cur_no = (mouse_y - by) / line_h;
        list_entry_t* e = m;
        for (i = 0; e != NULL; e = e->next) {
          if (e->hide) continue;
          if (i == cur_no) {
            mpriv->current = e;
            break;
          }
          ++i;
        }
      }
    }
    menu_mouse_pos_updated = 0;
  }

  for( ; m != NULL && dy + vo_font->height < dh ; m = m->next ) {
    if(m->hide) continue;
    if(m == mpriv->current) {
      // Record rectangle of current selection box.
      selection_x = bx;
      selection_y = dy + y - mpriv->vspace / 2;
      selection_w = bg_w;
      selection_h = line_h;

      if(mpriv->ptr_bg >= 0)
        menu_draw_box(mpi,mpriv->ptr_bg,mpriv->ptr_bg_alpha,
                      bx, dy + y - mpriv->vspace / 2,
                      bg_w, line_h);
      if(ptr_l > 0)
        menu_draw_text_full(mpi,mpriv->ptr,
                            dx,
                            dy+y,dw,dh - dy,
                            mpriv->vspace,0,
                            MENU_TEXT_TOP|MENU_TEXT_LEFT,
                            MENU_TEXT_TOP|MENU_TEXT_LEFT);
    } else if(mpriv->item_bg >= 0)
      menu_draw_box(mpi,mpriv->item_bg,mpriv->item_bg_alpha,
                    bx, dy + y - mpriv->vspace / 2,
                    bg_w, line_h);
    menu_draw_text_full(mpi,m->txt,
			dx + ptr_l,
			dy+y,dw-ptr_l,dh - dy,
			mpriv->vspace,0,
			MENU_TEXT_TOP|MENU_TEXT_LEFT,
			MENU_TEXT_TOP|MENU_TEXT_LEFT);
    dy += line_h;
  }

}

void menu_list_read_cmd(menu_t* menu,int cmd) {
  list_entry_t* m;
  int i;
  switch(cmd) {
  case MENU_CMD_UP:
    while(mpriv->current->prev) {
      mpriv->current = mpriv->current->prev;
      if(!mpriv->current->hide) return;
    }
    for( ; mpriv->current->next != NULL ; mpriv->current = mpriv->current->next)
      /* NOTHING */;
    if(!mpriv->current->hide) return;
    while(mpriv->current->prev) {
      mpriv->current = mpriv->current->prev;
      if(!mpriv->current->hide) return;
    }
    break;
  case MENU_CMD_DOWN:
    while(mpriv->current->next) {
      mpriv->current = mpriv->current->next;
      if(!mpriv->current->hide) return;
    }
    mpriv->current = mpriv->menu;
    if(!mpriv->current->hide) return;
    while(mpriv->current->next) {
      mpriv->current = mpriv->current->next;
      if(!mpriv->current->hide) return;
    }
    break;
  case MENU_CMD_HOME:
    mpriv->current = mpriv->menu;
    break;
  case MENU_CMD_END:
    for(m = mpriv->current ; m && m->next ; m = m->next)
      /**/;
    if(m)
      mpriv->current = m;
    break;
  case MENU_CMD_PAGE_UP:
    for(i = 0, m = mpriv->current ; m && m->prev && i < mpriv->disp_lines ; m = m->prev, i++)
      /**/;
    if(m)
      mpriv->current = m;
    break;
  case MENU_CMD_PAGE_DOWN:
    for(i = 0, m = mpriv->current ; m && m->next && i < mpriv->disp_lines ; m = m->next, i++)
      /**/;
    if(m)
      mpriv->current = m;
    break;
  case MENU_CMD_LEFT:
  case MENU_CMD_CANCEL:
    menu->show = 0;
    menu->cl = 1;
    break;
  case MENU_CMD_CLICK:
    if (mouse_x >= selection_x && mouse_x < selection_x + selection_w &&
        mouse_y >= selection_y && mouse_y < selection_y + selection_h)
      menu_read_cmd(menu, MENU_CMD_OK);
    break;
  }
}

int menu_list_jump_to_key(menu_t* menu,int c) {
  if(c < 256 && isalnum(c)) {
    list_entry_t* e = mpriv->current;
    if(e->txt[0] == c) e = e->next;
    for(  ; e ; e = e->next) {
	if(e->txt[0] == c) {
	  mpriv->current = e;
	  return 1;
	}
    }
    for(e = mpriv->menu ; e ;  e = e->next) {
	if(e->txt[0] == c) {
	  mpriv->current = e;
	  return 1;
	}
    }
    return 1;
  }
  return 0;
}

void menu_list_add_entry(menu_t* menu,list_entry_t* entry) {
  list_entry_t* l;
  mpriv->count++;

  if(mpriv->menu == NULL) {
    mpriv->menu = mpriv->current = entry;
    return;
  }

  for(l = mpriv->menu ; l->next != NULL ; l = l->next)
    /* NOP */;
  l->next = entry;
  entry->prev = l;
}

void menu_list_init(menu_t* menu) {
  if(!mpriv)
    mpriv = calloc(1,sizeof(struct menu_priv_s));

}

void menu_list_uninit(menu_t* menu,free_entry_t free_func) {
  list_entry_t *i,*j;

  if(!free_func) free_func = (free_entry_t)free;

  for(i = mpriv->menu ; i != NULL ; ) {
    j = i->next;
    free_func(i);
    i = j;
  }

  mpriv->menu = mpriv->current = NULL;

}
