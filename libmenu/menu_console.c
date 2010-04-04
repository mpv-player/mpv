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
#include "mp_msg.h"
#include "help_mp.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>
#ifndef __MINGW32__
#include <sys/wait.h>
#endif
#include <unistd.h>
#include <errno.h>

#include "libmpcodecs/img_format.h"
#include "libmpcodecs/mp_image.h"

#include "m_struct.h"
#include "m_option.h"
#include "menu.h"

#include "libvo/font_load.h"
#include "osdep/keycodes.h"
#include "input/input.h"
#include "osdep/timer.h"

typedef struct history_st history_t;

struct history_st {
  char* buffer;
  int size;
  history_t* next;
  history_t* prev;
};

struct menu_priv_s {
  char** lines; // Our buffer
  int last_line;
  int num_lines;
  int add_line;
  unsigned int hide_ts;
  unsigned int show_ts;
  pid_t child; // Child process if we are running a shell cmd
  int child_fd[3]; // The 3 default fd
  char* prompt;
  //int max_lines; // Max number of lines with the last mpi
  history_t* history;
  history_t* cur_history;
  int history_size;

  char* mp_prompt;
  char* child_prompt;
  int buf_lines; // Buffer size (in line)
  int height; // Display size in %
  int minb;
  int vspace;
  int bg,bg_alpha;
  unsigned int hide_time;
  unsigned int show_time;
  int history_max;
  int raw_child;
};

static struct menu_priv_s cfg_dflt = {
  NULL,
  0,
  0,
  1,
  0,
  0,
  0,
  { 0 , 0, 0 },
  NULL,
  NULL,
  NULL,
  0,

  "# ",
  "$ ",
  50, // lines
  33, // %
  3,
  3,
  0x80,0x40,
  500,
  500,
  10,
  0
};

#define ST_OFF(m) M_ST_OFF(struct menu_priv_s,m)

static const m_option_t cfg_fields[] = {
  { "prompt", ST_OFF(mp_prompt), CONF_TYPE_STRING, M_OPT_MIN, 1, 0, NULL },
  { "child-prompt", ST_OFF(child_prompt), CONF_TYPE_STRING, M_OPT_MIN, 1, 0, NULL },
  { "buffer-lines", ST_OFF(buf_lines), CONF_TYPE_INT, M_OPT_MIN, 5, 0, NULL },
  { "height", ST_OFF(height), CONF_TYPE_INT, M_OPT_RANGE, 1, 100, NULL },
  { "minbor", ST_OFF(minb), CONF_TYPE_INT, M_OPT_MIN, 0, 0, NULL },
  { "vspace", ST_OFF(vspace), CONF_TYPE_INT, M_OPT_MIN, 0, 0, NULL },
  { "bg", ST_OFF(bg), CONF_TYPE_INT, M_OPT_RANGE, -1, 255, NULL },
  { "bg-alpha", ST_OFF(bg_alpha), CONF_TYPE_INT, M_OPT_RANGE, 0, 255, NULL },
  { "show-time",ST_OFF(show_time), CONF_TYPE_INT, M_OPT_MIN, 0, 0, NULL },
  { "hide-time",ST_OFF(hide_time), CONF_TYPE_INT, M_OPT_MIN, 0, 0, NULL },
  { "history-size",ST_OFF(history_max), CONF_TYPE_INT, M_OPT_MIN, 1, 0, NULL },
  { "raw-child", ST_OFF(raw_child), CONF_TYPE_FLAG, 0, 0, 1, NULL },
  { NULL, NULL, NULL, 0,0,0,NULL }
};

#define mpriv (menu->priv)

static void check_child(menu_t* menu);

static void add_line(struct menu_priv_s* priv, char* l) {
  char* eol = strchr(l,'\n');

  if(eol) {
    if(eol != l) {
      eol[0] = '\0';
      add_line(priv,l);
    }
    if(eol[1]) add_line(priv,eol+1);
    return;
  }

  if(priv->num_lines >= priv->buf_lines && priv->lines[priv->last_line])
    free(priv->lines[priv->last_line]);
  else
    priv->num_lines++;

  priv->lines[priv->last_line] = strdup(l);
  priv->last_line = (priv->last_line + 1) % priv->buf_lines;
  priv->add_line = 1;
}

static void add_string(struct menu_priv_s* priv, char* l) {
  char* eol = strchr(l,'\n');
  int ll =  priv->last_line > 0 ? priv->last_line - 1 : priv->buf_lines-1;

  if(priv->num_lines <= 0 || priv->add_line || eol == l) {
    add_line(priv,l);
    priv->add_line = 0;
    return;
  }

  if(eol) {
    eol[0] = '\0';
    add_string(priv,l);
    if(eol[1]) {
      add_line(priv,eol+1);
      priv->add_line = 0;
    } else
      priv->add_line = 1;
    return;
  }
  priv->lines[ll] = realloc(priv->lines[ll],strlen(priv->lines[ll]) + strlen(l) + 1);
  if ( priv->lines[ll] != NULL )
  {
    strcat(priv->lines[ll],l);
  }
}

static void draw(menu_t* menu, mp_image_t* mpi) {
  int h = mpi->h*mpriv->height/100;
  int w = mpi->w - 2* mpriv->minb;
  int x = mpriv->minb, y;
  int lw,lh,i, ll;

  if(mpriv->child) check_child(menu);

  ll = mpriv->last_line - 1;

  if(mpriv->hide_ts) {
    unsigned int t = GetTimerMS() - mpriv->hide_ts;
    if(t >= mpriv->hide_time) {
      mpriv->hide_ts = 0;
      menu->show = 0;
      return;
    }
    h = mpi->h*(mpriv->height - (mpriv->height * t /mpriv->hide_time))/100;
  } else if(mpriv->show_time && mpriv->show_ts == 0) {
    mpriv->show_ts = GetTimerMS();
    return;
  } else if(mpriv->show_ts > 0) {
    unsigned int t = GetTimerMS() - mpriv->show_ts;
    if(t > mpriv->show_time)
      mpriv->show_ts = -1;
    else
      h = mpi->h*(mpriv->height * t /mpriv->hide_time)/100;
  }

  y = h -  mpriv->vspace;

  if(x < 0 || y < 0 || w <= 0 || h <= 0 )
    return;

  if(mpriv->bg >= 0)
    menu_draw_box(mpi,mpriv->bg,mpriv->bg_alpha,0,0,mpi->w,h);

  if(!mpriv->child || !mpriv->raw_child){
    char input[strlen(mpriv->cur_history->buffer) + strlen(mpriv->prompt) + 1];
    sprintf(input,"%s%s",mpriv->prompt,mpriv->cur_history->buffer);
    menu_text_size(input,w,mpriv->vspace,1,&lw,&lh);
    menu_draw_text_full(mpi,input,x,y,w,h,mpriv->vspace,1,
			MENU_TEXT_BOT|MENU_TEXT_LEFT,
			MENU_TEXT_BOT|MENU_TEXT_LEFT);
    y -= lh + mpriv->vspace;
  }


  for( i = 0 ; y > mpriv->minb && i < mpriv->num_lines ; i++){
    int c = (ll - i) >= 0 ? ll - i : mpriv->buf_lines + ll - i;
    menu_text_size(mpriv->lines[c],w,mpriv->vspace,1,&lw,&lh);
    menu_draw_text_full(mpi,mpriv->lines[c],x,y,w,h,mpriv->vspace,1,
			MENU_TEXT_BOT|MENU_TEXT_LEFT,
			MENU_TEXT_BOT|MENU_TEXT_LEFT);
    y -= lh + mpriv->vspace;
  }
  return;
}

static void check_child(menu_t* menu) {
#ifndef __MINGW32__
  fd_set rfd;
  struct timeval tv;
  int max_fd = mpriv->child_fd[2] > mpriv->child_fd[1] ? mpriv->child_fd[2] :
    mpriv->child_fd[1];
  int i,r,child_status,w;
  char buffer[256];

  if(!mpriv->child) return;

  memset(&tv,0,sizeof(struct timeval));
  FD_ZERO(&rfd);
  FD_SET(mpriv->child_fd[1],&rfd);
  FD_SET(mpriv->child_fd[2],&rfd);

  r = select(max_fd+1,&rfd, NULL, NULL, &tv);
  if(r == 0) {
    r = waitpid(mpriv->child,&child_status,WNOHANG);
    if(r < 0){
      if(errno==ECHILD){  ///exiting children get handled in mplayer.c
        for(i = 0 ; i < 3 ; i++)
          close(mpriv->child_fd[i]);
        mpriv->child = 0;
        mpriv->prompt = mpriv->mp_prompt;
        //add_line(mpriv,"Child process exited");
      }
      else mp_msg(MSGT_GLOBAL,MSGL_ERR,MSGTR_LIBMENU_WaitPidError,strerror(errno));
    }
  } else if(r < 0) {
    mp_msg(MSGT_GLOBAL,MSGL_ERR,MSGTR_LIBMENU_SelectError);
    return;
  }

  w = 0;
  for(i = 1 ; i < 3 ; i++) {
    if(FD_ISSET(mpriv->child_fd[i],&rfd)){
      if(w) mpriv->add_line = 1;
      r = read(mpriv->child_fd[i],buffer,255);
      if(r < 0)
	mp_msg(MSGT_GLOBAL,MSGL_ERR,MSGTR_LIBMENU_ReadErrorOnChildFD, i == 1 ? "stdout":"stderr");
      else if(r>0) {
	buffer[r] = '\0';
	add_string(mpriv,buffer);
      }
      w = 1;
    }
  }
#endif

}

#define close_pipe(pipe) close(pipe[0]); close(pipe[1])

static int run_shell_cmd(menu_t* menu, char* cmd) {
#ifndef __MINGW32__
  int in[2],out[2],err[2];

  mp_msg(MSGT_GLOBAL,MSGL_INFO,MSGTR_LIBMENU_ConsoleRun,cmd);
  if(mpriv->child) {
    mp_msg(MSGT_GLOBAL,MSGL_ERR,MSGTR_LIBMENU_AChildIsAlreadyRunning);
    return 0;
  }

  pipe(in);
  pipe(out);
  pipe(err);

  mpriv->child = fork();
  if(mpriv->child < 0) {
    mp_msg(MSGT_GLOBAL,MSGL_ERR,MSGTR_LIBMENU_ForkFailed);
    close_pipe(in);
    close_pipe(out);
    close_pipe(err);
    return 0;
  }
  if(!mpriv->child) { // Chlid process
    int err_fd = dup(2);
    FILE* errf = fdopen(err_fd,"w");
    // Bind the std fd to our pipes
    dup2(in[0],0);
    dup2(out[1],1);
    dup2(err[1],2);
    execl("/bin/sh","sh","-c",cmd,(void*)NULL);
    fprintf(errf,"exec failed : %s\n",strerror(errno));
    exit(1);
  }
  // MPlayer
  mpriv->child_fd[0] = in[1];
  mpriv->child_fd[1] = out[0];
  mpriv->child_fd[2] = err[0];
  mpriv->prompt = mpriv->child_prompt;
  //add_line(mpriv,"Child process started");
#endif
  return 1;
}

static void enter_cmd(menu_t* menu) {
  history_t* h;
  char input[strlen(mpriv->cur_history->buffer) + strlen(mpriv->prompt) + 1];

  sprintf(input,"%s%s",mpriv->prompt,mpriv->cur_history->buffer);
  add_line(mpriv,input);

  if(mpriv->history == mpriv->cur_history) {
    if(mpriv->history_size >= mpriv->history_max) {
      history_t* i;
      for(i = mpriv->history ; i->prev ; i = i->prev)
	/**/;
      i->next->prev = NULL;
      free(i->buffer);
      free(i);
    } else
      mpriv->history_size++;
    h = calloc(1,sizeof(history_t));
    h->size = 255;
    h->buffer = calloc(h->size,1);
    h->prev = mpriv->history;
    mpriv->history->next = h;
    mpriv->history = h;
  } else
    mpriv->history->buffer[0] = '\0';

  mpriv->cur_history = mpriv->history;
  //mpriv->input = mpriv->cur_history->buffer;
}

static void read_cmd(menu_t* menu,int cmd) {
  switch(cmd) {
  case MENU_CMD_CANCEL:
    if(mpriv->hide_time)
      mpriv->hide_ts = GetTimerMS();
    else
      menu->show = 0;
    mpriv->show_ts = 0;
    return;
  case MENU_CMD_OK: {
    mp_cmd_t* c;
    if(mpriv->child) {
      char *str = mpriv->cur_history->buffer;
      int l = strlen(str);
      while(l > 0) {
	int w = write(mpriv->child_fd[0],str,l);
	if(w < 0) {
	  mp_msg(MSGT_GLOBAL,MSGL_ERR,MSGTR_LIBMENU_WriteError);
	  break;
	}
	l -= w;
	str += w;
      }
      if(write(mpriv->child_fd[0],"\n",1) < 0)
	mp_msg(MSGT_GLOBAL,MSGL_ERR,MSGTR_LIBMENU_WriteError);
      enter_cmd(menu);
      return;
    }
    c = mp_input_parse_cmd(mpriv->cur_history->buffer);
    enter_cmd(menu);
    if(!c)
      add_line(mpriv,"Invalid command try help");
    else {
      switch(c->id) {
      case MP_CMD_CHELP:
	add_line(mpriv,"MPlayer console 0.01");
	add_line(mpriv,"TODO: meaningful help message ;)");
	add_line(mpriv,"Enter any slave command");
	add_line(mpriv,"exit close this console");
	break;
      case MP_CMD_CEXIT:
	menu->show = 0;
	menu->cl = 1;
	break;
      case MP_CMD_CHIDE:
	if(mpriv->hide_time)
	  mpriv->hide_ts = GetTimerMS();
	else
	  menu->show = 0;
	mpriv->show_ts = 0;
	break;
      case MP_CMD_RUN:
	run_shell_cmd(menu,c->args[0].v.s);
	break;
      default: // Send the other commands to mplayer
	mp_input_queue_cmd(c);
      }
    }
    return;
  }
  case MENU_CMD_UP:
    if(mpriv->cur_history->prev)
      mpriv->cur_history = mpriv->cur_history->prev;
    break;
  case MENU_CMD_DOWN:
    if(mpriv->cur_history->next)
      mpriv->cur_history = mpriv->cur_history->next;
    break;
  }
}

static int read_key(menu_t* menu,int c) {
  if(mpriv->child && mpriv->raw_child) {
    write(mpriv->child_fd[0],&c,sizeof(int));
    return 1;
  }

  if (c == KEY_DELETE || c == KEY_BS) {
    unsigned int i = strlen(mpriv->cur_history->buffer);
    if(i > 0)
      mpriv->cur_history->buffer[i-1] = '\0';
    return 1;
  }
  if (menu_dflt_read_key(menu, c))
    return 1;

  if(isascii(c)) {
    int l = strlen(mpriv->cur_history->buffer);
    if(l >= mpriv->cur_history->size) {
      mpriv->cur_history->size += 255;
      mpriv->cur_history->buffer = realloc(mpriv->cur_history,mpriv->cur_history->size);
    }
    mpriv->cur_history->buffer[l] = (char)c;
    mpriv->cur_history->buffer[l+1] = '\0';
    return 1;
  }
  return 0;
}


static int openMenu(menu_t* menu, char* args) {


  menu->draw = draw;
  menu->read_cmd = read_cmd;
  menu->read_key = read_key;

  mpriv->lines = calloc(mpriv->buf_lines,sizeof(char*));
  mpriv->prompt = mpriv->mp_prompt;
  mpriv->cur_history = mpriv->history = calloc(1,sizeof(history_t));
  mpriv->cur_history->buffer = calloc(255,1);
  mpriv->cur_history->size = 255;

  if(args)
    add_line(mpriv,args);

  return 1;
}

const menu_info_t menu_info_console = {
  "MPlayer console",
  "console",
  "Albeu",
  "",
  {
    "console_cfg",
    sizeof(struct menu_priv_s),
    &cfg_dflt,
    cfg_fields
  },
  openMenu,
};
