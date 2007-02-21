
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

#include "libmpcodecs/img_format.h"
#include "libmpcodecs/mp_image.h"

#include "menu.h"
#include "menu_list.h"
#include "input/input.h"
#include "osdep/keycodes.h"
#include "metadata.h"

struct list_entry_s {
  struct list_entry p;
  char* name;
  m_option_t* opt;
  char* menu;
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

#define OPT_NAME "name"
#define OPT_VCODEC "vcodec"
#define OPT_VBITRATE "vbitrate"
#define OPT_RESOLUTION "resolution"
#define OPT_ACODEC "acodec"
#define OPT_ABITRATE "abitrate"
#define OPT_SAMPLES "asamples"
#define OPT_INFO_TITLE "title"
#define OPT_INFO_ARTIST "artist"
#define OPT_INFO_ALBUM "album"
#define OPT_INFO_YEAR "year"
#define OPT_INFO_COMMENT "comment"
#define OPT_INFO_TRACK "track"
#define OPT_INFO_GENRE "genre"

m_option_t*  mp_property_find(const char* name);

static void entry_set_text(menu_t* menu, list_entry_t* e) {
  char* val = m_property_print(e->opt, menu->ctx);
  int l,edit = (mpriv->edit && e == mpriv->p.current);
  if(!val) {
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

static void update_entries(menu_t* menu) {
  list_entry_t* e;
  for(e = mpriv->p.menu ; e ; e = e->p.next)
    if(e->opt) entry_set_text(menu,e);
}

static int parse_args(menu_t* menu,char* args) {
  char *element,*body, **attribs, *name, *meta, *val;
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

    meta = asx_get_attrib("meta",attribs);
    val = NULL;
    if(meta) {
      if (!strcmp (meta, OPT_NAME))
        val = get_metadata (META_NAME);
      else if (!strcmp (meta, OPT_VCODEC))
        val = get_metadata (META_VIDEO_CODEC);
      else if (!strcmp(meta, OPT_VBITRATE))
        val = get_metadata (META_VIDEO_BITRATE);
      else if(!strcmp(meta, OPT_RESOLUTION))
      val = get_metadata (META_VIDEO_RESOLUTION);
      else if (!strcmp(meta, OPT_ACODEC))
        val = get_metadata (META_AUDIO_CODEC);
      else if(!strcmp(meta, OPT_ABITRATE))
        val = get_metadata (META_AUDIO_BITRATE);
      else if(!strcmp(meta, OPT_SAMPLES))
        val = get_metadata (META_AUDIO_SAMPLES);
      else if (!strcmp (meta, OPT_INFO_TITLE))
        val = get_metadata (META_INFO_TITLE);
      else if (!strcmp (meta, OPT_INFO_ARTIST))
        val = get_metadata (META_INFO_ARTIST);
      else if (!strcmp (meta, OPT_INFO_ALBUM))
        val = get_metadata (META_INFO_ALBUM);
      else if (!strcmp (meta, OPT_INFO_YEAR))
        val = get_metadata (META_INFO_YEAR);
      else if (!strcmp (meta, OPT_INFO_COMMENT))
        val = get_metadata (META_INFO_COMMENT);
      else if (!strcmp (meta, OPT_INFO_TRACK))
        val = get_metadata (META_INFO_TRACK);
      else if (!strcmp (meta, OPT_INFO_GENRE))
      val = get_metadata (META_INFO_GENRE);
    if (val) {
      char *item = asx_get_attrib("name",attribs);
      int l;

      if (!item)
        item = strdup (meta);
      l = strlen(item) + 2 + strlen(val) + 1;
      m = calloc(1,sizeof(struct list_entry_s));
      m->p.txt = malloc(l);
      sprintf(m->p.txt,"%s: %s",item,val);
      free(val);
      free(item);
      menu_list_add_entry(menu,m);
    }
    free (meta);
    if (element)
      free(element);
    if(body)
      free(body);
    asx_free_attribs(attribs);
    continue;
    }
    
    name = asx_get_attrib("property",attribs);
    opt = name ? mp_property_find(name) : NULL;
    if(!opt) {
      mp_msg(MSGT_OSD_MENU,MSGL_WARN,MSGTR_LIBMENU_PrefMenuEntryDefinitionsNeed,parser->line);
      goto next_element;
    }
    m = calloc(1,sizeof(struct list_entry_s));
    m->opt = opt;
    m->name = asx_get_attrib("name",attribs);
    if(!m->name) m->name = strdup(opt->name);
    entry_set_text(menu,m);
    menu_list_add_entry(menu,m);

  next_element:
    free(element);
    if(body) free(body);
    if(name) free(name);
    asx_free_attribs(attribs);
  }
}

static void read_key(menu_t* menu,int c) {
  menu_list_read_key(menu,c,0);
}

static void read_cmd(menu_t* menu,int cmd) {
  list_entry_t* e = mpriv->p.current;

  if(e->opt) {
    switch(cmd) {
    case MENU_CMD_UP:
      if(!mpriv->edit) break;
    case MENU_CMD_RIGHT:
      if(m_property_do(e->opt,M_PROPERTY_STEP_UP,NULL,menu->ctx) > 0)
        update_entries(menu);
      return;
    case MENU_CMD_DOWN:
      if(!mpriv->edit) break;
    case MENU_CMD_LEFT:
      if(m_property_do(e->opt,M_PROPERTY_STEP_DOWN,NULL,menu->ctx) > 0)
        update_entries(menu);
      return;
      
    case MENU_CMD_OK:
      // check that the property is writable
      if(m_property_do(e->opt,M_PROPERTY_SET,NULL,menu->ctx) < 0) return;
      // shortcut for flags
      if(e->opt->type == CONF_TYPE_FLAG) {
	if(m_property_do(e->opt,M_PROPERTY_STEP_UP,NULL,menu->ctx) > 0)
          update_entries(menu);
        return;
      }
      // switch
      mpriv->edit = !mpriv->edit;
      // update the menu
      update_entries(menu);
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
      update_entries(menu);
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
  if(entry->menu) free(entry->menu);
  free(entry);
}

static void closeMenu(menu_t* menu) {
  menu_list_uninit(menu,free_entry);
}

static int openMenu(menu_t* menu, char* args) {

  menu->draw = menu_list_draw;
  menu->read_cmd = read_cmd;
  menu->read_key = read_key;
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
