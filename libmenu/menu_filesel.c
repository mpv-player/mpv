
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>


#include "../config.h"

#include "../m_struct.h"
#include "../m_option.h"

#include "img_format.h"
#include "mp_image.h"

#include "menu.h"
#include "menu_list.h"
#include "../input/input.h"
#include "../linux/keycodes.h"

struct list_entry_s {
  struct list_entry p;
  int d;
};

struct menu_priv_s {
  menu_list_priv_t p;
  char* dir; // current dir
  /// Cfg fields
  char* path;
  char* title;
  char* file_action;
  char* dir_action;
  int auto_close;
};

static struct menu_priv_s cfg_dflt = {
  MENU_LIST_PRIV_DFLT,
  NULL,

  NULL,
  "Select a file: %p",
  "loadfile '%p'",
  NULL,
  0
};

#define ST_OFF(m) M_ST_OFF(struct menu_priv_s,m)

static m_option_t cfg_fields[] = {
  MENU_LIST_PRIV_FIELDS,
  { "path", ST_OFF(path),  CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "title", ST_OFF(title),  CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "file-action", ST_OFF(file_action),  CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "dir-action", ST_OFF(dir_action),  CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "auto-close", ST_OFF(auto_close), CONF_TYPE_FLAG, 0, 0, 1, NULL },
  { NULL, NULL, NULL, 0,0,0,NULL }
};

#define mpriv (menu->priv)

static void free_entry(list_entry_t* entry) {
  free(entry->p.txt);
  free(entry);
}

static char* replace_path(char* title , char* dir) {
  char *p = strstr(title,"%p");
  if(p) {
    int tl = strlen(title);
    int dl = strlen(dir);
    int t1l = p-title; 
    int l = tl - 2 + dl;
    char*r = malloc(l + 1);
    memcpy(r,title,t1l);
    memcpy(r+t1l,dir,dl);
    if(tl - t1l - 2 > 0)
      memcpy(r+t1l+dl,p+2,tl - t1l - 2);
    r[l] = '\0';
    return r;
  } else
    return title;
}

typedef int (*kill_warn)(const void*, const void*);

static int mylstat(char *dir, char *file,struct stat* st) {
  int l = strlen(dir) + strlen(file);
  char s[l+1];
  sprintf(s,"%s/%s",dir,file);
  return lstat(s,st);
}

static int compare(char **a, char **b){
  int la,lb;
  la = strlen(*a);
  lb = strlen(*b);
  if((*a)[strlen(*a) - 1] == '/') {
    if((*b)[strlen(*b) - 1] == '/')
      return strcmp(*b, *a) ;
    else
      return 1;
  } else {
    if((*b)[strlen(*b) - 1] == '/')
      return -1;
    else
      return strcmp(*b, *a);
  }
}

static int open_dir(menu_t* menu,char* args) {
  char **namelist, **tp;
  struct dirent *dp;
  struct stat st;
  int n;
  char* p = NULL;
  list_entry_t* e;
  DIR* dirp;

  menu_list_init(menu);

  if(mpriv->dir)
    free(mpriv->dir);
  mpriv->dir = strdup(args);
  if(mpriv->p.title && mpriv->p.title != mpriv->title && mpriv->p.title != cfg_dflt.p.title)
    free(mpriv->p.title);
  p = strstr(mpriv->title,"%p");

  mpriv->p.title = replace_path(mpriv->title,mpriv->dir);

  if ((dirp = opendir (mpriv->dir)) == NULL){
    printf("opendir error: %s", strerror(errno));
    return 0;
  }

  namelist = (char **) malloc(sizeof(char *));

  n=0;
  while ((dp = readdir(dirp)) != NULL) {
    if(dp->d_name[0] == '.' && strcmp(dp->d_name,"..") != 0)
      continue;
    if(n%20 == 0){ // Get some more mem
      if((tp = (char **) realloc(namelist, (n+20) * sizeof (char *)))
         == NULL) {
        printf("realloc error: %s", strerror(errno));
        goto bailout;
      } 
      namelist=tp;
    }

    namelist[n] = (char *) malloc(strlen(dp->d_name) + 2);
    if(namelist[n] == NULL){
      printf("malloc error: %s", strerror(errno));
      goto bailout;
    }
     
    strcpy(namelist[n], dp->d_name);
    mylstat(args,namelist[n],&st); 
    if(S_ISDIR(st.st_mode))
      strcat(namelist[n], "/");
    n++;
  }
  qsort(namelist, n, sizeof(char *), (kill_warn)compare);

bailout:
  if (n < 0) {
    printf("scandir error: %s\n",strerror(errno));
    return 0;
  }
  while(n--) {
    e = calloc(1,sizeof(list_entry_t));
    e->p.txt = strdup(namelist[n]);
    if(strchr(namelist[n], '/') != NULL)
      e->d = 1;
    menu_list_add_entry(menu,e);
    free(namelist[n]);
  }
  free(namelist);

  return 1;
}
    

static void read_cmd(menu_t* menu,int cmd) {
  mp_cmd_t* c = NULL;
  switch(cmd) {
  case MENU_CMD_OK: {
    // Directory
    if(mpriv->p.current->d) {
      if(mpriv->dir_action) {
	int fname_len = strlen(mpriv->dir) + strlen(mpriv->p.current->p.txt) + 1;
	char filename[fname_len];
	char* str;
	sprintf(filename,"%s%s",mpriv->dir,mpriv->p.current->p.txt);
	str = replace_path(mpriv->dir_action,filename);
	c = mp_input_parse_cmd(str);
	if(str != mpriv->dir_action)
	  free(str);
      } else { // Default action : open this dirctory ourself
	int l = strlen(mpriv->dir);
	char *slash =  NULL, *p = NULL;
	if(strcmp(mpriv->p.current->p.txt,"../") == 0) {
	  if(l <= 1) break;
	  mpriv->dir[l-1] = '\0';
	  slash = strrchr(mpriv->dir,'/');
	  if(!slash) break;
	  slash[1] = '\0';
	  p = strdup(mpriv->dir);
	} else {
	  p = malloc(l + strlen(mpriv->p.current->p.txt) + 1);
	  sprintf(p,"%s%s",mpriv->dir,mpriv->p.current->p.txt);
	}
	menu_list_uninit(menu,free_entry);
	if(!open_dir(menu,p)) {
	  printf("Can't open directory %s\n",p);
	  menu->cl = 1;
	}
	free(p);
      }
    } else { // Files
      int fname_len = strlen(mpriv->dir) + strlen(mpriv->p.current->p.txt) + 1;
      char filename[fname_len];
      char *str;
      sprintf(filename,"%s%s",mpriv->dir,mpriv->p.current->p.txt);
      str = replace_path(mpriv->file_action,filename);
      c = mp_input_parse_cmd(str);
      if(str != mpriv->file_action)
	free(str);
    }	  
    if(c) {
      mp_input_queue_cmd(c);
      if(mpriv->auto_close)
	menu->cl = 1;
    }
  } break;
  default:
    menu_list_read_cmd(menu,cmd);
  }
}

static void read_key(menu_t* menu,int c){
  if(c == KEY_BS) {
    mpriv->p.current = mpriv->p.menu; // Hack : we consider that the first entry is ../
    read_cmd(menu,MENU_CMD_OK);
  } else
    menu_list_read_key(menu,c,1);
}

static void clos(menu_t* menu) {
  menu_list_uninit(menu,free_entry);
  free(mpriv->dir);
}

static int open_fs(menu_t* menu, char* args) {
  char *path = mpriv->path;
  int r = 0;
  char wd[PATH_MAX+1];
  args = NULL; // Warning kill

  menu->draw = menu_list_draw;
  menu->read_cmd = read_cmd;
  menu->read_key = read_key;
  menu->close = clos;

  getcwd(wd,PATH_MAX);
  if(!path || path[0] == '\0') {
    int l = strlen(wd) + 2;
    char b[l];
    sprintf(b,"%s/",wd);
    r = open_dir(menu,b);
  } else if(path[0] != '/') {
    int al = strlen(path);
    int l = strlen(wd) + al + 3;
    char b[l];
    if(b[al-1] != '/')
      sprintf(b,"%s/%s/",wd,path);
    else
      sprintf(b,"%s/%s",wd,path);
    r = open_dir(menu,b);
  } else
    r = open_dir(menu,path);

  return r;
}
  
const menu_info_t menu_info_filesel = {
  "File seletor menu",
  "filesel",
  "Albeu",
  "",
  {
    "fs_cfg",
    sizeof(struct menu_priv_s),
    &cfg_dflt,
    cfg_fields
  },
  open_fs
};
