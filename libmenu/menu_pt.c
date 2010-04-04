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
#include <string.h>
//#include <libgen.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "libmpcodecs/img_format.h"
#include "libmpcodecs/mp_image.h"

#include "m_struct.h"
#include "m_option.h"
#include "menu.h"
#include "menu_list.h"


#include "playtree.h"
#include "input/input.h"
#include "access_mpcontext.h"

#define mp_basename(s) (strrchr((s),'/')==NULL?(char*)(s):(strrchr((s),'/')+1))

struct list_entry_s {
  struct list_entry p;
  play_tree_t* pt;
};


struct menu_priv_s {
  menu_list_priv_t p;
  char* title;
  int auto_close;
};

static struct menu_priv_s cfg_dflt = {
  MENU_LIST_PRIV_DFLT,
  "Jump to",
  0
};

#define ST_OFF(m) M_ST_OFF(struct menu_priv_s,m)

static const m_option_t cfg_fields[] = {
  MENU_LIST_PRIV_FIELDS,
  { "title", ST_OFF(title),  CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "auto-close", ST_OFF(auto_close), CONF_TYPE_FLAG, 0, 0, 1, NULL },
  { NULL, NULL, NULL, 0,0,0,NULL }
};

#define mpriv (menu->priv)

static void read_cmd(menu_t* menu,int cmd) {
  switch(cmd) {
  case MENU_CMD_RIGHT:
  case MENU_CMD_OK: {
    int d = 1;
    char str[15];
    play_tree_t* i;
    mp_cmd_t* c;
    play_tree_iter_t* playtree_iter = mpctx_get_playtree_iter(menu->ctx);

    if(playtree_iter->tree == mpriv->p.current->pt)
      break;

    if(playtree_iter->tree->parent && mpriv->p.current->pt == playtree_iter->tree->parent)
      snprintf(str,15,"pt_up_step 1");
    else {
      for(i = playtree_iter->tree->next; i != NULL ; i = i->next) {
	if(i == mpriv->p.current->pt)
	  break;
	d++;
      }
      if(i == NULL) {
	d = -1;
	for(i = playtree_iter->tree->prev; i != NULL ; i = i->prev) {
	  if(i == mpriv->p.current->pt)
	    break;
	  d--;
	}
	if(i == NULL) {
	  mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_CantfindTheTargetItem);
	  break;
	}
      }
      snprintf(str,15,"pt_step %d",d);
    }
    c = mp_input_parse_cmd(str);
    if(c) {
      if(mpriv->auto_close)
        mp_input_queue_cmd(mp_input_parse_cmd("menu hide"));
      mp_input_queue_cmd(c);
    }
    else
      mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_FailedToBuildCommand,str);
  } break;
  default:
    menu_list_read_cmd(menu,cmd);
  }
}

static int read_key(menu_t* menu,int c){
  if (menu_dflt_read_key(menu, c))
    return 1;
  return menu_list_jump_to_key(menu, c);
}

static void close_menu(menu_t* menu) {
  menu_list_uninit(menu,NULL);
}

static int op(menu_t* menu, char* args) {
  play_tree_t* i;
  list_entry_t* e;
  play_tree_iter_t* playtree_iter = mpctx_get_playtree_iter(menu->ctx);

  args = NULL; // Warning kill

  menu->draw = menu_list_draw;
  menu->read_cmd = read_cmd;
  menu->read_key = read_key;
  menu->close = close_menu;

  menu_list_init(menu);

  mpriv->p.title = mpriv->title;

  if(playtree_iter->tree->parent != playtree_iter->root) {
    e = calloc(1,sizeof(list_entry_t));
    e->p.txt = "..";
    e->pt = playtree_iter->tree->parent;
    menu_list_add_entry(menu,e);
  }

  for(i = playtree_iter->tree ; i->prev != NULL ; i = i->prev)
    /* NOP */;
  for( ; i != NULL ; i = i->next ) {
    e = calloc(1,sizeof(list_entry_t));
    if(i->files)
      e->p.txt = mp_basename(i->files[0]);
    else
      e->p.txt = "Group ...";
    e->pt = i;
    menu_list_add_entry(menu,e);
  }

  return 1;
}

const menu_info_t menu_info_pt = {
  "Playtree menu",
  "pt",
  "Albeu",
  "",
  {
    "pt_cfg",
    sizeof(struct menu_priv_s),
    &cfg_dflt,
    cfg_fields
  },
  op
};
