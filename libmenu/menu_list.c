
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "../config.h"

#include "img_format.h"
#include "mp_image.h"

#include "m_struct.h"
#include "menu.h"

#include "../libvo/font_load.h"
#include "../linux/keycodes.h"

#define IMPL 1
#include "menu_list.h"

#define mpriv (menu->priv)

void menu_list_draw(menu_t* menu,mp_image_t* mpi) {
  int x = mpriv->x;
  int y = mpriv->y;
  int i;
  int h = mpriv->h;
  int w = mpriv->w;
  int dh = 0,dw =  0;
  int dy = 0;
  int need_h = 0,need_w = 0,ptr_l = menu_text_length(mpriv->ptr) + 10,sidx = 0;
  int th;
  list_entry_t* m;

  if(mpriv->count < 1)
    return;

  if(h <= 0) h = mpi->height;
  if(w <= 0) w = mpi->width;
  dh = h - 2*mpriv->minb;
  dw = w - 2*mpriv->minb;
  ptr_l = menu_text_length(mpriv->ptr);
  // mpi is too small
  if(h - vo_font->height <= 0 || w - ptr_l <= 0 || dw <= 0 || dh <= 0)
    return;

  th = menu_text_num_lines(mpriv->title,dw) * (mpriv->vspace + vo_font->height) + mpriv->vspace;

  for(i = 0, m = mpriv->menu ; m ; m = m->next, i++) {
    int ll = menu_text_length(m->txt);
    if(ptr_l + ll > need_w) need_w = ptr_l + ll;
    if(m == mpriv->current) sidx = i;
  }
  if(need_w > dw) need_w = dw;
  if(x > 0)
    x += mpriv->minb;
  if(y > 0)
    y += mpriv->minb;
  else 
    y = mpriv->minb;

  need_h = mpriv->count * (mpriv->vspace + vo_font->height) - mpriv->vspace;
  if( need_h + th > dh) {
    int start,end;
    int maxl = (dh + mpriv->vspace - th) / (mpriv->vspace + vo_font->height);
    if(maxl < 4) {
      th = 0;
      maxl = (dh + mpriv->vspace) / ( vo_font->height + mpriv->vspace);
    }
    // Too smoll
    if(maxl < 1) return;
    need_h = maxl*(mpriv->vspace + vo_font->height) - mpriv->vspace;

    start = sidx - (maxl/2);
    if(start < 0) start = 0;
    end = start + maxl;
    if(end > mpriv->count) {
      end = mpriv->count;
      if(end - start < maxl)
	start = end - maxl < 0 ? 0 : end - maxl;
    }
    m = mpriv->menu;
    for(i = 0 ; m->next && i < start ; i++)
      m = m->next;
  } else
    m = mpriv->menu;

  if(th > 0) {
    menu_draw_text_full(mpi,mpriv->title,
			x < 0 ? mpi->w / 2 : x,
			dy+y,dw,0,
			mpriv->vspace,1,
			MENU_TEXT_TOP|MENU_TEXT_HCENTER,
			MENU_TEXT_TOP|(x < 0 ? MENU_TEXT_HCENTER :MENU_TEXT_LEFT));
    dy += th;
  }
  
  for( ; m != NULL && dy + vo_font->height < dh ; m = m->next ) {
    if(m == mpriv->current)
      menu_draw_text_full(mpi,mpriv->ptr,
			  x < 0 ? (mpi->w - need_w) / 2 + ptr_l : x,
			  dy+y,dw,dh - dy,
			  mpriv->vspace,0,
			  MENU_TEXT_TOP|(x < 0 ? MENU_TEXT_RIGHT :MENU_TEXT_LEFT) ,
			  MENU_TEXT_TOP|(x < 0 ? MENU_TEXT_RIGHT :MENU_TEXT_LEFT));
    menu_draw_text_full(mpi,m->txt,
			x < 0 ? (mpi->w - need_w) / 2  + ptr_l : x + ptr_l,
			dy+y,dw-ptr_l,dh - dy,
			mpriv->vspace,0,
			MENU_TEXT_TOP|MENU_TEXT_LEFT,
			MENU_TEXT_TOP|MENU_TEXT_LEFT);
    dy +=  vo_font->height + mpriv->vspace;
  }

}

void menu_list_read_cmd(menu_t* menu,int cmd) {
  switch(cmd) {
  case MENU_CMD_UP:
    if(mpriv->current->prev) {
      mpriv->current = mpriv->current->prev;
    } else {
      for( ; mpriv->current->next != NULL ; mpriv->current = mpriv->current->next)
	/* NOTHING */;
    } break;
  case MENU_CMD_DOWN:
    if(mpriv->current->next) {
      mpriv->current = mpriv->current->next;
   } else {
     mpriv->current = mpriv->menu;
   } break;
  case MENU_CMD_CANCEL:
    menu->show = 0;
    menu->cl = 1;
    break;
  }    
}

void menu_list_jump_to_key(menu_t* menu,int c) {
  if(isalnum(c)) {
    list_entry_t* e = mpriv->current;
    if(e->txt[0] == c) e = e->next;
    for(  ; e ; e = e->next) {
	if(e->txt[0] == c) {
	  mpriv->current = e;
	  return;
	}
    }
    for(e = mpriv->menu ; e ;  e = e->next) {
	if(e->txt[0] == c) {
	  mpriv->current = e;
	  return;
	}
    }
  } else
    menu_dflt_read_key(menu,c);
}

void menu_list_read_key(menu_t* menu,int c,int jump_to) {
  list_entry_t* m;
  int i;
  switch(c) {
  case KEY_HOME:
    mpriv->current = mpriv->menu;
    break;
  case KEY_END:
    for(m = mpriv->current ; m && m->next ; m = m->next)
      /**/;
    if(m)
      mpriv->current = m;
    break;
  case KEY_PAGE_UP:
    for(i = 0, m = mpriv->current ; m && m->prev && i < 10 ; m = m->prev, i++)
      /**/;
    if(m)
      mpriv->current = m;
    break;
  case KEY_PAGE_DOWN:
    for(i = 0, m = mpriv->current ; m && m->next && i < 10 ; m = m->next, i++)
      /**/;
    if(m)
      mpriv->current = m;
    break;
  default:
    if(jump_to)
      menu_list_jump_to_key(menu,c);
    else
      menu_dflt_read_key(menu,c);
  }    
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
