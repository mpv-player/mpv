
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>


#include "../config.h"

#include "../m_struct.h"
#include "../m_option.h"
#include "../m_config.h"
#include "../asxparser.h"

#include "img_format.h"
#include "mp_image.h"

#include "menu.h"
#include "menu_list.h"
#include "../input/input.h"
#include "../linux/keycodes.h"

struct list_entry_s {
  struct list_entry p;
  m_option_t* opt;
};

struct menu_priv_s {
  menu_list_priv_t p;
  char* edit;
  int edit_len;
  /// Cfg fields
};

static struct menu_priv_s cfg_dflt = {
  MENU_LIST_PRIV_DFLT,
  NULL,
  0
};

static m_option_t cfg_fields[] = {
  MENU_LIST_PRIV_FIELDS,
  { "title", M_ST_OFF(menu_list_priv_t,title),  CONF_TYPE_STRING, 0, 0, 0, NULL },
  { NULL, NULL, NULL, 0,0,0,NULL }
};

#define mpriv (menu->priv)

extern m_config_t* mconfig;

static int parse_args(menu_t* menu,char* args) {
  char *element,*body, **attribs, *name, *ok, *cancel;
  list_entry_t* m = NULL;
  int r;
  m_option_t* opt;
  ASX_Parser_t* parser = asx_parser_new();
  

  while(1) {
    r = asx_get_element(parser,&args,&element,&body,&attribs);
    if(r < 0) {
      printf("Syntax error at line %d\n",parser->line);
      asx_parser_free(parser);
      return -1;
    } else if(r == 0) {      
      asx_parser_free(parser);
      if(!m)
	printf("No entry found in the menu definition\n");
      return m ? 1 : 0;
    }
    // Has it a name ?
    name = asx_get_attrib("name",attribs);
    opt = name ? m_config_get_option(mconfig,name) : NULL;
    if(!opt) {
      printf("Pref menu entry definitions need a valid name attribut (line %d)\n",parser->line);
      free(element);
      if(name) free(name);
      if(body) free(body);
      asx_free_attribs(attribs);
      continue;
    }
    m = calloc(1,sizeof(struct list_entry_s));
    m->p.txt = name;
    m->opt = opt;
    menu_list_add_entry(menu,m);

    free(element);
    if(body) free(body);
    asx_free_attribs(attribs);
  }
}

static void read_key(menu_t* menu,int c) {
  menu_list_read_key(menu,c,0);
}

static void free_entry(list_entry_t* entry) {
  free(entry->p.txt);
  free(entry);
}

static void close(menu_t* menu) {
  menu_list_uninit(menu,free_entry);
  if(mpriv->edit)
    free(mpriv->edit);
}

static int open(menu_t* menu, char* args) {
  list_entry_t* e;

  menu->draw = menu_list_draw;
  menu->read_cmd = menu_list_read_cmd;
  menu->read_key = read_key;
  menu->close = close;


  if(!args) {
    printf("Pref menu need an argument\n");
    return 0;
  }
 
  menu_list_init(menu);
  if(!parse_args(menu,args))
    return 0;

  for(e = mpriv->p.menu ; e ; e = e->p.next) {
    int l;
    char* val = m_option_print(e->opt,e->opt->p);
    if((int)val == -1) {
      printf("Can't get value of option %s\n",e->opt->name);
      continue;
    } else if(!val)
      val = strdup("NULL");
    l = strlen(e->opt->name) + 2 + strlen(val) + 1;
    e->p.txt = malloc(l);
    sprintf(e->p.txt,"%s: %s",e->opt->name,val);
    free(val);
  }    

  return 1;
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
  open
};
