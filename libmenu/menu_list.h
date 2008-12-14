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

#ifndef MPLAYER_MENU_LIST_H
#define MPLAYER_MENU_LIST_H

#include "menu.h"

typedef struct list_entry_s list_entry_t;


#ifdef IMPL
struct list_entry_s {
#else
struct list_entry {
#endif
  list_entry_t* prev;
  list_entry_t* next;

  char* txt;
  char hide;
};


#ifndef IMPL
typedef struct menu_list_priv_s {
#else
typedef struct menu_priv_s {
#endif
  list_entry_t* menu;
  list_entry_t* current;
  int count;

  char* title;
  int x,y;
  int w,h;
  int vspace, minb;
  int disp_lines;
  char* ptr;
  int title_bg,title_bg_alpha;
  int item_bg,item_bg_alpha;
  int ptr_bg,ptr_bg_alpha;
} menu_list_priv_t;

typedef void (*free_entry_t)(list_entry_t* entry);

void menu_list_read_cmd(menu_t* menu,int cmd);
void menu_list_draw(menu_t* menu,mp_image_t* mpi);
void menu_list_add_entry(menu_t* menu,list_entry_t* entry);
void menu_list_init(menu_t* menu);
void menu_list_uninit(menu_t* menu,free_entry_t free_func);
int menu_list_jump_to_key(menu_t* menu,int c);

extern const menu_list_priv_t menu_list_priv_dflt;

#define MENU_LIST_PRIV_DFLT { \
  NULL, \
  NULL, \
  0, \
\
  "MPlayer", \
  -1,-1, \
  0,0, \
  5, 3, \
  0, \
  NULL, \
  0xFF, 0xFF, \
  0xFF, 0xFF, \
  0xA4, 0x50 \
}
  

#define MENU_LIST_PRIV_FIELDS \
  { "minbor", M_ST_OFF(menu_list_priv_t,minb), CONF_TYPE_INT, M_OPT_MIN, 0, 0, NULL }, \
  { "vspace", M_ST_OFF(menu_list_priv_t,vspace), CONF_TYPE_INT, M_OPT_MIN, 0, 0, NULL }, \
  { "x", M_ST_OFF(menu_list_priv_t,x), CONF_TYPE_INT, M_OPT_MIN, 0, 0, NULL }, \
  { "y", M_ST_OFF(menu_list_priv_t,y), CONF_TYPE_INT, M_OPT_MIN, 0, 0, NULL }, \
  { "w", M_ST_OFF(menu_list_priv_t,w), CONF_TYPE_INT, M_OPT_MIN, 0, 0, NULL }, \
  { "h", M_ST_OFF(menu_list_priv_t,h), CONF_TYPE_INT, M_OPT_MIN, 0, 0, NULL }, \
  { "ptr", M_ST_OFF(menu_list_priv_t,ptr), CONF_TYPE_STRING, 0, 0, 0, NULL }, \
  { "title-bg", M_ST_OFF(menu_list_priv_t,title_bg), CONF_TYPE_INT, M_OPT_RANGE, -1, 255, NULL }, \
  { "title-bg-alpha", M_ST_OFF(menu_list_priv_t,title_bg_alpha), \
    CONF_TYPE_INT, M_OPT_RANGE, 0, 255, NULL }, \
  { "item-bg", M_ST_OFF(menu_list_priv_t,item_bg), CONF_TYPE_INT, M_OPT_RANGE, -1, 255, NULL }, \
  { "item-bg-alpha", M_ST_OFF(menu_list_priv_t,item_bg_alpha), \
    CONF_TYPE_INT, M_OPT_RANGE, 0, 255, NULL }, \
  { "ptr-bg", M_ST_OFF(menu_list_priv_t,ptr_bg), CONF_TYPE_INT, M_OPT_RANGE, -1, 255, NULL }, \
  { "ptr-bg-alpha", M_ST_OFF(menu_list_priv_t,ptr_bg_alpha), \
    CONF_TYPE_INT, M_OPT_RANGE, 0, 255, NULL } \

#endif /* MPLAYER_MENU_LIST_H */
