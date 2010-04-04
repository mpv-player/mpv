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
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>


#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "m_struct.h"
#include "m_option.h"

#include "libmpcodecs/img_format.h"
#include "libmpcodecs/mp_image.h"

#include "menu.h"
#include "menu_list.h"
#include "input/input.h"
#include "osdep/keycodes.h"

#define MENU_KEEP_PATH "/tmp/mp_current_path"

int menu_keepdir = 0;
char *menu_chroot = NULL;
extern char *filename;

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
  char** actions;
  char* filter;
};

static struct menu_priv_s cfg_dflt = {
  MENU_LIST_PRIV_DFLT,
  NULL,

  NULL,
  "Select a file: %p",
  "loadfile '%p'",
  NULL,
  NULL,
  NULL
};

#define ST_OFF(m) M_ST_OFF(struct menu_priv_s,m)

static const m_option_t cfg_fields[] = {
  MENU_LIST_PRIV_FIELDS,
  { "path", ST_OFF(path),  CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "title", ST_OFF(title),  CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "file-action", ST_OFF(file_action),  CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "dir-action", ST_OFF(dir_action),  CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "actions", ST_OFF(actions), CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
  { "filter", ST_OFF(filter), CONF_TYPE_STRING, 0, 0, 0, NULL},
  { NULL, NULL, NULL, 0,0,0,NULL }
};

#define mpriv (menu->priv)

static void free_entry(list_entry_t* entry) {
  free(entry->p.txt);
  free(entry);
}

static char* replace_path(char* title , char* dir , int escape) {
  char *p = strstr(title,"%p");
  if(p) {
    int tl = strlen(title);
    int dl = strlen(dir);
    int t1l = p-title;
    int l = tl - 2 + dl;
    char *r, *n, *d = dir;

    if (escape) {
    do {
      if (*d == '\\')
        l++;
      else if (*d == '\'') /* ' -> \'\\\'\' */
        l+=7;
    } while (*d++);
    }
    r = malloc(l + 1);
    n = r + t1l;
    memcpy(r,title,t1l);
    do {
      if (escape) {
      if (*dir == '\\')
        *n++ = '\\';
      else if (*dir == '\'') { /* ' -> \'\\\'\' */
        *n++ = '\\'; *n++ = '\'';
        *n++ = '\\'; *n++ = '\\';
        *n++ = '\\'; *n++ = '\'';
        *n++ = '\\';
      }
      }
    } while ((*n++ = *dir++));
    if(tl - t1l - 2 > 0)
      strcpy(n-1,p+2);
    return r;
  } else
    return title;
}

typedef int (*kill_warn)(const void*, const void*);

static int mylstat(char *dir, char *file,struct stat* st) {
  int l = strlen(dir) + strlen(file);
  char s[l+2];
  if (!strcmp("..", file)) {
    char *slash;
    l -= 3;
    strcpy(s, dir);
#if HAVE_DOS_PATHS
    if (s[l] == '/' || s[l] == '\\')
#else
    if (s[l] == '/')
#endif
      s[l] = '\0';
    slash = strrchr(s, '/');
#if HAVE_DOS_PATHS
    if (!slash)
      slash = strrchr(s,'\\');
#endif
    if (!slash)
      return stat(dir,st);
    slash[1] = '\0';
    return stat(s,st);
  }
  sprintf(s,"%s/%s",dir,file);
  return stat(s,st);
}

static int compare(char **a, char **b){
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

static char **get_extensions(menu_t *menu){
  char **extensions, ext[32];
  FILE *fp;
  int n = 1;

  if (!mpriv->filter)
    return NULL;

  fp = fopen(mpriv->filter, "r");
  if(!fp)
    return NULL;

  extensions = malloc(sizeof(*extensions));
  *extensions = NULL;

  while(fgets(ext,sizeof(ext),fp)) {
    char **l, *e;
    int s = strlen (ext);

    if(ext[s-1] == '\n') {
      ext[s-1] = '\0';
      s--;
    }
    e = malloc(s+1);
    extensions = realloc(extensions, ++n * sizeof(*extensions));
    extensions = realloc(extensions, ++n * sizeof(*extensions));
    strcpy (e, ext);
    for (l=extensions; *l; l++);
    *l++ = e;
    *l = NULL;
  }

  fclose (fp);
  return extensions;
}

static void free_extensions(char **extensions){
  if (extensions) {
    char **l = extensions;
    while (*l)
      free (*l++);
    free (extensions);
  }
}

static int open_dir(menu_t* menu,char* args) {
  char **namelist, **tp;
  struct dirent *dp;
  struct stat st;
  int n;
  int path_fp;
  char* p = NULL;
  list_entry_t* e;
  DIR* dirp;
  extern int file_filter;
  char **extensions, **elem, *ext;

  menu_list_init(menu);

  if(mpriv->dir)
    free(mpriv->dir);
  mpriv->dir = strdup(args);
  if(mpriv->p.title && mpriv->p.title != mpriv->title && mpriv->p.title != cfg_dflt.p.title)
    free(mpriv->p.title);
  p = strstr(mpriv->title,"%p");

  mpriv->p.title = replace_path(mpriv->title,mpriv->dir,0);

  if ((dirp = opendir (mpriv->dir)) == NULL){
    mp_msg(MSGT_GLOBAL,MSGL_ERR,MSGTR_LIBMENU_OpendirError, strerror(errno));
    return 0;
  }

  if (menu_keepdir) {
    path_fp = open (MENU_KEEP_PATH, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (path_fp >= 0) {
      write (path_fp, mpriv->dir, strlen (mpriv->dir));
      close (path_fp);
    }
  }

  namelist = malloc(sizeof(char *));
  extensions = get_extensions(menu);

  n=0;
  while ((dp = readdir(dirp)) != NULL) {
    if(dp->d_name[0] == '.' && strcmp(dp->d_name,"..") != 0)
      continue;
    if (menu_chroot && !strcmp (dp->d_name,"..")) {
      size_t len = strlen (menu_chroot);
      if ((strlen (mpriv->dir) == len || strlen (mpriv->dir) == len + 1)
          && !strncmp (mpriv->dir, menu_chroot, len))
        continue;
    }
    if (mylstat(args,dp->d_name,&st))
      continue;
    if (file_filter && extensions && !S_ISDIR(st.st_mode)) {
      if((ext = strrchr(dp->d_name,'.')) == NULL)
        continue;
      ext++;
      elem = extensions;
      do {
        if (!strcasecmp(ext, *elem))
          break;
      } while (*++elem);
      if (*elem == NULL)
        continue;
    }
    if(n%20 == 0){ // Get some more mem
      if((tp = realloc(namelist, (n+20) * sizeof (char *)))
         == NULL) {
        mp_msg(MSGT_GLOBAL,MSGL_ERR,MSGTR_LIBMENU_ReallocError, strerror(errno));
	n--;
        goto bailout;
      }
      namelist=tp;
    }

    namelist[n] = malloc(strlen(dp->d_name) + 2);
    if(namelist[n] == NULL){
      mp_msg(MSGT_GLOBAL,MSGL_ERR,MSGTR_LIBMENU_MallocError, strerror(errno));
      n--;
      goto bailout;
    }

    strcpy(namelist[n], dp->d_name);
    if(S_ISDIR(st.st_mode))
      strcat(namelist[n], "/");
    n++;
  }

bailout:
  free_extensions (extensions);
  closedir(dirp);

  qsort(namelist, n, sizeof(char *), (kill_warn)compare);

  if (n < 0) {
    mp_msg(MSGT_GLOBAL,MSGL_ERR,MSGTR_LIBMENU_ReaddirError,strerror(errno));
    return 0;
  }
  while(n--) {
    if((e = calloc(1,sizeof(list_entry_t))) != NULL){
    e->p.next = NULL;
    e->p.txt = strdup(namelist[n]);
    if(strchr(namelist[n], '/') != NULL)
      e->d = 1;
    menu_list_add_entry(menu,e);
    }else{
      mp_msg(MSGT_GLOBAL,MSGL_ERR,MSGTR_LIBMENU_MallocError, strerror(errno));
    }
    free(namelist[n]);
  }
  free(namelist);

  return 1;
}

static char *action;

static void read_cmd(menu_t* menu,int cmd) {
  switch(cmd) {
  case MENU_CMD_LEFT:
    mpriv->p.current = mpriv->p.menu; // Hack : we consider that the first entry is ../
  case MENU_CMD_RIGHT:
  case MENU_CMD_OK: {
    // Directory
    if(mpriv->p.current->d && !mpriv->dir_action) {
        // Default action : open this dirctory ourself
	int l = strlen(mpriv->dir);
	char *slash =  NULL, *p = NULL;
	if(strcmp(mpriv->p.current->p.txt,"../") == 0) {
	  if(l <= 1) break;
	  mpriv->dir[l-1] = '\0';
	  slash = strrchr(mpriv->dir,'/');
#if HAVE_DOS_PATHS
	  if (!slash)
	    slash = strrchr(mpriv->dir,'\\');
#endif
	  if(!slash) break;
	  slash[1] = '\0';
	  p = strdup(mpriv->dir);
	} else {
	  p = malloc(l + strlen(mpriv->p.current->p.txt) + 1);
	  sprintf(p,"%s%s",mpriv->dir,mpriv->p.current->p.txt);
	}
	menu_list_uninit(menu,free_entry);
	if(!open_dir(menu,p)) {
	  mp_msg(MSGT_GLOBAL,MSGL_ERR,MSGTR_LIBMENU_CantOpenDirectory,p);
	  menu->cl = 1;
	}
	free(p);
    } else { // File and directory dealt with action string.
      int fname_len = strlen(mpriv->dir) + strlen(mpriv->p.current->p.txt) + 1;
      char filename[fname_len];
      char *str;
      char *action = mpriv->p.current->d ? mpriv->dir_action:mpriv->file_action;
      sprintf(filename,"%s%s",mpriv->dir,mpriv->p.current->p.txt);
      str = replace_path(action, filename,1);
      mp_input_parse_and_queue_cmds(str);
      if (str != action)
	free(str);
    }
  } break;
  case MENU_CMD_ACTION: {
    int fname_len = strlen(mpriv->dir) + strlen(mpriv->p.current->p.txt) + 1;
    char filename[fname_len];
    char *str;
    sprintf(filename,"%s%s",mpriv->dir,mpriv->p.current->p.txt);
    str = replace_path(action, filename,1);
    mp_input_parse_and_queue_cmds(str);
    if(str != action)
      free(str);
  } break;
  default:
    menu_list_read_cmd(menu,cmd);
  }
}

static int read_key(menu_t* menu,int c){
    char **str;
    for (str=mpriv->actions; str && *str; str++)
      if (c == (*str)[0]) {
        action = &(*str)[2];
        read_cmd(menu,MENU_CMD_ACTION);
        return 1;
      }
  if (menu_dflt_read_key(menu, c))
    return 1;
  return menu_list_jump_to_key(menu, c);
}

static void clos(menu_t* menu) {
  menu_list_uninit(menu,free_entry);
  free(mpriv->dir);
}

static int open_fs(menu_t* menu, char* args) {
  char *path = mpriv->path;
  int r = 0;
  char wd[PATH_MAX+1], b[PATH_MAX+1];
  args = NULL; // Warning kill

  menu->draw = menu_list_draw;
  menu->read_cmd = read_cmd;
  menu->read_key = read_key;
  menu->close = clos;

  if (menu_keepdir) {
    if (!path || path[0] == '\0') {
      struct stat st;
      int path_fp;

      path_fp = open (MENU_KEEP_PATH, O_RDONLY);
      if (path_fp >= 0) {
        if (!fstat (path_fp, &st) && (st.st_size > 0)) {
          path = malloc(st.st_size+1);
          path[st.st_size] = '\0';
          if (!((read(path_fp, path, st.st_size) == st.st_size) && path[0] == '/'
              && !stat(path, &st) && S_ISDIR(st.st_mode))) {
            free(path);
            path = NULL;
          }
        }
        close (path_fp);
      }
    }
  }

  getcwd(wd,PATH_MAX);
  if (!path || path[0] == '\0') {
#if 0
    char *slash = NULL;
    if (filename && !strstr(filename, "://") && (path=realpath(filename, b))) {
      slash = strrchr(path, '/');
#if HAVE_DOS_PATHS
      // FIXME: Do we need and can convert all '\\' in path to '/' on win32?
      if (!slash)
        slash = strrchr(path, '\\');
#endif
    }
    if (slash)
      slash[1] = '\0';
    else
#endif
      path = wd;
  }
  if (path[0] != '/') {
    if(path[strlen(path)-1] != '/')
      snprintf(b,sizeof(b),"%s/%s/",wd,path);
    else
      snprintf(b,sizeof(b),"%s/%s",wd,path);
    path = b;
  } else if (path[strlen(path)-1]!='/') {
    sprintf(b,"%s/",path);
    path = b;
  }
  if (menu_chroot && menu_chroot[0] == '/') {
    int l = strlen(menu_chroot);
    if (l > 0 && menu_chroot[l-1] == '/')
      --l;
    if (strncmp(menu_chroot, path, l) || (path[l] != '\0' && path[l] != '/')) {
      if (menu_chroot[l] == '/')
        path = menu_chroot;
      else {
        sprintf(b,"%s/",menu_chroot);
        path = b;
      }
    }
  }
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
