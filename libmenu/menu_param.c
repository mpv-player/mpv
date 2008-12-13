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

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>

#include "mp_msg.h"
#include "help_mp.h"

#include "m_struct.h"
#include "m_option.h"
#include "m_property.h"
#include "asxparser.h"

#include "libmpcodecs/mp_image.h"

#include "menu.h"
#include "menu_list.h"
#include "input/input.h"
#include "command.h"

struct list_entry_s {
  struct list_entry p;
  char* name;
  char* txt;
  char* prop;
  m_option_t* opt;
  char* menu;
  int auto_update;
};

struct menu_priv_s {
  menu_list_priv_t p;
  char* ptr;
  int edit;
  /// Cfg fields
  char* na;
  int hide_na;
};
 
static struct menu_priv_s cfg_dflt = {
  MENU_LIST_PRIV_DFLT,
  NULL,
  0,
  "N/A",
  1
};

static m_option_t cfg_fields[] = {
  MENU_LIST_PRIV_FIELDS,
  { "title", M_ST_OFF(menu_list_priv_t,title),  CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "na", M_ST_OFF(struct menu_priv_s,na), CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "hide-na", M_ST_OFF(struct menu_priv_s,hide_na), CONF_TYPE_FLAG, CONF_RANGE, 0, 1, NULL },
  { NULL, NULL, NULL, 0,0,0,NULL }
};

#define mpriv (menu->priv)

static void entry_set_text(menu_t* menu, list_entry_t* e) {
  char* val = e->txt ? property_expand_string(menu->ctx, e->txt) :
    mp_property_print(e->prop, menu->ctx);
  int l,edit = (mpriv->edit && e == mpriv->p.current);
  if(!val || !val[0]) {
    if(val) free(val);
    if(mpriv->hide_na) {
      e->p.hide = 1;
      return;
    }
    val = strdup(mpriv->na);
  } else if(mpriv->hide_na)
      e->p.hide = 0;
  l = strlen(e->name) + 2 + strlen(val) + (edit ? 4 : 0) + 1;
  if(e->p.txt) free(e->p.txt);
  e->p.txt = malloc(l);
  sprintf(e->p.txt,"%s: %s%s%s",e->name,edit ? "> " : "",val,edit ? " <" : "");
  free(val);
}

static void update_entries(menu_t* menu, int auto_update) {
  list_entry_t* e;
  for(e = mpriv->p.menu ; e ; e = e->p.next)
    if ((e->txt || e->prop) && (!auto_update || e->auto_update))
      entry_set_text(menu, e);
}

static int parse_args(menu_t* menu,char* args) {
  char *element,*body, **attribs, *name, *txt, *auto_update;
  list_entry_t* m = NULL;
  int r;
  m_option_t* opt;
  ASX_Parser_t* parser = asx_parser_new();
  

  while(1) {
    r = asx_get_element(parser,&args,&element,&body,&attribs);
    if(r < 0) {
      mp_msg(MSGT_OSD_MENU,MSGL_ERR,MSGTR_LIBMENU_SyntaxErrorAtLine,parser->line);
      asx_parser_free(parser);
      return -1;
    } else if(r == 0) {      
      asx_parser_free(parser);
      if(!m)
        mp_msg(MSGT_OSD_MENU,MSGL_WARN,MSGTR_LIBMENU_NoEntryFoundInTheMenuDefinition);
      m = calloc(1,sizeof(struct list_entry_s));
      m->p.txt = strdup("Back");
      menu_list_add_entry(menu,m);
      return 1;
    }
    if(!strcmp(element,"menu")) {
      name = asx_get_attrib("menu",attribs);
      if(!name) {
        mp_msg(MSGT_OSD_MENU,MSGL_WARN,MSGTR_LIBMENU_SubmenuDefinitionNeedAMenuAttribut);
        goto next_element;
      }
      m = calloc(1,sizeof(struct list_entry_s));
      m->menu = name;
      name = NULL; // we want to keep it
      m->p.txt = asx_get_attrib("name",attribs);
      if(!m->p.txt) m->p.txt = strdup(m->menu);
      menu_list_add_entry(menu,m);
      goto next_element;
    }

    name = asx_get_attrib("property",attribs);
    opt = NULL;
    if(name && mp_property_do(name,M_PROPERTY_GET_TYPE,&opt,menu->ctx) <= 0) {
      mp_msg(MSGT_OSD_MENU,MSGL_WARN,MSGTR_LIBMENU_InvalidProperty,
             name,parser->line);
      goto next_element;
    }
    txt = asx_get_attrib("txt",attribs);
    if(!(name || txt)) {
      mp_msg(MSGT_OSD_MENU,MSGL_WARN,MSGTR_LIBMENU_PrefMenuEntryDefinitionsNeed,parser->line);
      if(txt) free(txt), txt = NULL;
      goto next_element;
    }
    m = calloc(1,sizeof(struct list_entry_s));
    m->opt = opt;
    m->txt = txt; txt = NULL;
    m->prop = name; name = NULL;
    m->name = asx_get_attrib("name",attribs);
    if(!m->name) m->name = strdup(opt ? opt->name : "-");
    auto_update = asx_get_attrib("auto-update", attribs);
    if (auto_update) {
      if (!strcmp(auto_update, "1") ||
          !strcasecmp(auto_update, "on") ||
          !strcasecmp(auto_update, "yes") ||
          !strcasecmp(auto_update, "true"))
        m->auto_update = 1;
      free(auto_update);
    }
    entry_set_text(menu,m);
    menu_list_add_entry(menu,m);

  next_element:
    free(element);
    if(body) free(body);
    if(name) free(name);
    asx_free_attribs(attribs);
  }
}

static void read_cmd(menu_t* menu,int cmd) {
  list_entry_t* e = mpriv->p.current;

  if(e->opt) {
    switch(cmd) {
    case MENU_CMD_UP:
      if(!mpriv->edit) break;
    case MENU_CMD_RIGHT:
      if(mp_property_do(e->prop,M_PROPERTY_STEP_UP,NULL,menu->ctx) > 0)
        update_entries(menu, 0);
      return;
    case MENU_CMD_DOWN:
      if(!mpriv->edit) break;
    case MENU_CMD_LEFT:
      if(mp_property_do(e->prop,M_PROPERTY_STEP_DOWN,NULL,menu->ctx) > 0)
        update_entries(menu, 0);
      return;
      
    case MENU_CMD_OK:
      // check that the property is writable
      if(mp_property_do(e->prop,M_PROPERTY_SET,NULL,menu->ctx) < 0) return;
      // shortcut for flags
      if(e->opt->type == CONF_TYPE_FLAG) {
	if(mp_property_do(e->prop,M_PROPERTY_STEP_UP,NULL,menu->ctx) > 0)
          update_entries(menu, 0);
        return;
      }
      // switch
      mpriv->edit = !mpriv->edit;
      // update the menu
      update_entries(menu, 0);
      // switch the pointer
      if(mpriv->edit) {
        mpriv->ptr = mpriv->p.ptr;
        mpriv->p.ptr = NULL;
      } else
        mpriv->p.ptr = mpriv->ptr;
      return;
    case MENU_CMD_CANCEL:
      if(!mpriv->edit) break;
      mpriv->edit = 0;
      update_entries(menu, 0);
      mpriv->p.ptr = mpriv->ptr;
      return;
    }
  } else if(e->menu) {
    switch(cmd) {
    case MENU_CMD_RIGHT:
    case MENU_CMD_OK: {
      mp_cmd_t* c;
      char* txt = malloc(10 + strlen(e->menu) + 1);
      sprintf(txt,"set_menu %s",e->menu);
      c = mp_input_parse_cmd(txt);
      if(c) mp_input_queue_cmd(c);
      return;
    }
    }
  } else {
    switch(cmd) {
    case MENU_CMD_RIGHT:
    case MENU_CMD_OK:
      menu->show = 0;
      menu->cl = 1;
      return;
    }
  }
  menu_list_read_cmd(menu,cmd);
}

static void free_entry(list_entry_t* entry) {
  free(entry->p.txt);
  if(entry->name) free(entry->name);
  if(entry->txt)  free(entry->txt);
  if(entry->prop) free(entry->prop);
  if(entry->menu) free(entry->menu);
  free(entry);
}

static void closeMenu(menu_t* menu) {
  menu_list_uninit(menu,free_entry);
}

static void menu_pref_draw(menu_t* menu, mp_image_t* mpi) {
  update_entries(menu, 1);
  menu_list_draw(menu, mpi);
}

static int openMenu(menu_t* menu, char* args) {

  menu->draw = menu_pref_draw;
  menu->read_cmd = read_cmd;
  menu->close = closeMenu;


  if(!args) {
    mp_msg(MSGT_OSD_MENU,MSGL_ERR,MSGTR_LIBMENU_PrefMenuNeedsAnArgument);
    return 0;
  }
 
  menu_list_init(menu);
  return parse_args(menu,args);
}

const menu_info_t menu_info_pref = {
  "Preferences menu",
  "pref",
  "Albeu",
  "",
  {
    "pref_cfg",
    sizeof(struct menu_priv_s),
    &cfg_dflt,
    cfg_fields
  },
  openMenu
};
