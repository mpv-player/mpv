
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

#define mpriv (menu->priv)

void menu_list_draw(menu_t* menu,mp_image_t* mpi) {
  int x = mpriv->x;
  int y = mpriv->y;
  int i;
  int h = mpriv->h;
  int w = mpriv->w;
  int dh = 0,dw =  0;
  int dy = 0;
  int need_h = 0,need_w = 0,ptr_l,sidx = 0;
  int th,count = 0;
  int bg_w;
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

  th = menu_text_num_lines(mpriv->title,dw) * (mpriv->vspace + vo_font->height) + mpriv->vspace;

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
  if(x > 0)
    x += mpriv->minb;
  if(y > 0)
    y += mpriv->minb;
  else 
    y = mpriv->minb;

  need_h = count * (mpriv->vspace + vo_font->height) - mpriv->vspace;
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
    if(end > count) {
      end = count;
      if(end - start < maxl)
	start = end - maxl < 0 ? 0 : end - maxl;
    }
    m = mpriv->menu;
    for(i = 0 ; m->next && i < start ; ) {
      if(!m->hide) i++;
      m = m->next;
    }
  } else
    m = mpriv->menu;

  bg_w = need_w+2*mpriv->minb;
  if(th > 0) {
    if(mpriv->title_bg >= 0) {
      int tw,th2;
      menu_text_size(mpriv->title,dw,mpriv->vspace,1,&tw,&th2);
      if(tw+2*mpriv->minb > bg_w) bg_w = tw+2*mpriv->minb;
      menu_draw_box(mpi,mpriv->title_bg,mpriv->title_bg_alpha,
                    x < 0 ? (mpi->w-bg_w)/2 : x-mpriv->minb,dy+y-mpriv->vspace/2,bg_w,th);
    }
    menu_draw_text_full(mpi,mpriv->title,
			x < 0 ? mpi->w / 2 : x,
			dy+y,dw,0,
			mpriv->vspace,1,
			MENU_TEXT_TOP|MENU_TEXT_HCENTER,
			MENU_TEXT_TOP|(x < 0 ? MENU_TEXT_HCENTER :MENU_TEXT_LEFT));
    dy += th;
  }
  
  for( ; m != NULL && dy + vo_font->height < dh ; m = m->next ) {
    if(m->hide) continue;
    if(m == mpriv->current) {
      if(mpriv->ptr_bg >= 0)
        menu_draw_box(mpi,mpriv->ptr_bg,mpriv->ptr_bg_alpha,
                      x < 0 ? (mpi->w-bg_w)/2 : x-mpriv->minb,dy+y-mpriv->vspace/2,
                      bg_w,vo_font->height + mpriv->vspace);
      if(ptr_l > 0)
        menu_draw_text_full(mpi,mpriv->ptr,
                            x < 0 ? (mpi->w - need_w) / 2 + ptr_l : x,
                            dy+y,dw,dh - dy,
                            mpriv->vspace,0,
                            MENU_TEXT_TOP|(x < 0 ? MENU_TEXT_RIGHT :MENU_TEXT_LEFT) ,
                            MENU_TEXT_TOP|(x < 0 ? MENU_TEXT_RIGHT :MENU_TEXT_LEFT));
    } else if(mpriv->item_bg >= 0)
      menu_draw_box(mpi,mpriv->item_bg,mpriv->item_bg_alpha,
                    x < 0 ? (mpi->w-bg_w)/2 : x-mpriv->minb,dy+y-mpriv->vspace/2,
                    bg_w,vo_font->height + mpriv->vspace);
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
  case MENU_CMD_LEFT:
  case MENU_CMD_CANCEL:
    menu->show = 0;
    menu->cl = 1;
    break;
  }    
}

void menu_list_jump_to_key(menu_t* menu,int c) {
  if(c < 256 && isalnum(c)) {
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

