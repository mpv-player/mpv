
#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "libmpcodecs/img_format.h"
#include "libmpcodecs/mp_image.h"

#include "m_option.h"
#include "m_struct.h"
#include "asxparser.h"
#include "menu.h"
#include "menu_list.h"

#include "libvo/font_load.h"

#include "input/input.h"
#include "version.h"



struct list_entry_s {
  struct list_entry p;

  char* ok;
  char* cancel;
  char* left;
  char* right;
};

struct menu_priv_s {
  menu_list_priv_t p;
  int auto_close;
};

#define ST_OFF(m) M_ST_OFF(struct menu_priv_s, m)

static struct menu_priv_s cfg_dflt = {
  MENU_LIST_PRIV_DFLT,
  0,
};

static m_option_t cfg_fields[] = {
  MENU_LIST_PRIV_FIELDS,
  { "title",M_ST_OFF(struct menu_priv_s,p.title), CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "auto-close", ST_OFF(auto_close), CONF_TYPE_FLAG, 0, 0, 1, NULL },
  { NULL, NULL, NULL, 0,0,0,NULL }
};

#define mpriv (menu->priv)

static void read_cmd(menu_t* menu,int cmd) {
  switch(cmd) {
  case MENU_CMD_RIGHT:
    if(mpriv->p.current->right) {
      mp_cmd_t* c = mp_input_parse_cmd(mpriv->p.current->right);
      if(c) mp_input_queue_cmd(c);
      break;
    } // fallback on ok if right is not defined
  case MENU_CMD_OK: {
    if(mpriv->p.current->ok) {
      mp_cmd_t* c = mp_input_parse_cmd(mpriv->p.current->ok);
      if(c)
        {
          if (mpriv->auto_close)
              mp_input_queue_cmd (mp_input_parse_cmd ("menu hide"));
	mp_input_queue_cmd(c);
        }
    }
   } break;
  case MENU_CMD_LEFT:
    if(mpriv->p.current->left) {
      mp_cmd_t* c = mp_input_parse_cmd(mpriv->p.current->left);
      if(c) mp_input_queue_cmd(c);
      break;
    } // fallback on cancel if left is not defined
  case MENU_CMD_CANCEL:
    if(mpriv->p.current->cancel) {
      mp_cmd_t* c = mp_input_parse_cmd(mpriv->p.current->cancel);
      if(c)
	mp_input_queue_cmd(c);
      break;
    }
  default:
    menu_list_read_cmd(menu,cmd);
  }
}

static void read_key(menu_t* menu,int c){
  menu_list_read_key(menu,c,0);
}

static void free_entry(list_entry_t* entry) {
  if(entry->ok)
    free(entry->ok);
  if(entry->cancel)
    free(entry->cancel);
  free(entry->p.txt);
  free(entry);
}

static void close_menu(menu_t* menu) {
  menu_list_uninit(menu,free_entry);
}

static int parse_args(menu_t* menu,char* args) {
  char *element,*body, **attribs, *name;
  list_entry_t* m = NULL;
  int r;
  ASX_Parser_t* parser = asx_parser_new();

  while(1) {
    r = asx_get_element(parser,&args,&element,&body,&attribs);
    if(r < 0) {
      mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_SyntaxErrorAtLine,parser->line);
      asx_parser_free(parser);
      return -1;
    } else if(r == 0) {      
      asx_parser_free(parser);
      if(!m)
	mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_NoEntryFoundInTheMenuDefinition);
      return m ? 1 : 0;
    }
    // Has it a name ?
    name = asx_get_attrib("name",attribs);
    if(!name) {
      mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_ListMenuEntryDefinitionsNeedAName,parser->line);
      free(element);
      if(body) free(body);
      asx_free_attribs(attribs);
      continue;
    }
    m = calloc(1,sizeof(struct list_entry_s));
    m->p.txt = name;
    m->ok = asx_get_attrib("ok",attribs);
    m->cancel = asx_get_attrib("cancel",attribs);
    m->left = asx_get_attrib("left",attribs);
    m->right = asx_get_attrib("right",attribs);
    menu_list_add_entry(menu,m);

    free(element);
    if(body) free(body);
    asx_free_attribs(attribs);
  }
}

static int open(menu_t* menu, char* args) {
  menu->draw = menu_list_draw;
  menu->read_cmd = read_cmd;
  menu->read_key = read_key;
  menu->close = close_menu;

  if(!args) {
    mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_LIBMENU_ListMenuNeedsAnArgument);
    return 0;
  }
 
  menu_list_init(menu);
  if(!parse_args(menu,args))
    return 0;
  return 1;
}

const menu_info_t menu_info_cmdlist = {
  "Command list menu",
  "cmdlist",
  "Albeu",
  "",
  {
    "cmdlist_cfg",
    sizeof(struct menu_priv_s),
    &cfg_dflt,
    cfg_fields
  },
  open
};
