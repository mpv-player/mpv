
#include "../config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "img_format.h"
#include "mp_image.h"

#include "../m_struct.h"
#include "../m_option.h"
#include "menu.h"

#include "../libvo/font_load.h"
#include "../linux/keycodes.h"
#include "../input/input.h"
#include "../linux/timer.h"

struct menu_priv_s {
  char** lines; // Our buffer
  int last_line;
  int num_lines;
  char* input; // input buffer
  int input_size; // size of the input buffer in lines
  unsigned int hide_ts;
  unsigned int show_ts;

  //int max_lines; // Max number of lines with the last mpi
  
  char* prompt;
  int buf_lines; // Buffer size (in line)
  int height; // Display size in %
  int minb;
  int vspace;
  unsigned int hide_time;
  unsigned int show_time;
};

static struct menu_priv_s cfg_dflt = {
  NULL,
  0,
  0,
  NULL,
  0,
  0,
  0,

  "# ",
  50, // lines
  33, // %
  3,
  3,
  500,
  500,
};

#define ST_OFF(m) M_ST_OFF(struct menu_priv_s,m)

static m_option_t cfg_fields[] = {
  { "prompt", ST_OFF(prompt), CONF_TYPE_STRING, M_OPT_MIN, 1, 0, NULL },
  { "buffer-lines", ST_OFF(buf_lines), CONF_TYPE_INT, M_OPT_MIN, 5, 0, NULL },
  { "height", ST_OFF(height), CONF_TYPE_INT, M_OPT_RANGE, 1, 100, NULL },
  { "minbor", ST_OFF(minb), CONF_TYPE_INT, M_OPT_MIN, 0, 0, NULL },
  { "vspace", ST_OFF(vspace), CONF_TYPE_INT, M_OPT_MIN, 0, 0, NULL },
  { "show-time",ST_OFF(show_time), CONF_TYPE_INT, M_OPT_MIN, 0, 0, NULL },
  { "hide-time",ST_OFF(hide_time), CONF_TYPE_INT, M_OPT_MIN, 0, 0, NULL },
  { NULL, NULL, NULL, 0,0,0,NULL }
};

#define mpriv (menu->priv)

static void add_line(struct menu_priv_s* priv, char* l) {

  if(priv->num_lines >= priv->buf_lines && priv->lines[priv->last_line])
    free(priv->lines[priv->last_line]);
  else
    priv->num_lines++;

  priv->lines[priv->last_line] = strdup(l);
  priv->last_line = (priv->last_line + 1) % priv->buf_lines;
}

static void draw(menu_t* menu, mp_image_t* mpi) {
  int h = mpi->h*mpriv->height/100;
  int w = mpi->w - 2* mpriv->minb;
  int x = mpriv->minb, y;
  int lw,lh,i, ll = mpriv->last_line - 1;

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

  menu_text_size(mpriv->input,w,mpriv->vspace,1,&lw,&lh);
  menu_draw_text_full(mpi,mpriv->input,x,y,w,h,mpriv->vspace,1,
		      MENU_TEXT_BOT|MENU_TEXT_LEFT,
		      MENU_TEXT_BOT|MENU_TEXT_LEFT);
  y -= lh + mpriv->vspace;

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

static void read_cmd(menu_t* menu,int cmd) {
  switch(cmd) {
  case MENU_CMD_UP:
    break;
  case MENU_CMD_DOWN:
  case MENU_CMD_OK:
    break;
  case MENU_CMD_CANCEL:
    menu->show = 0;
    menu->cl = 1;
    break;
  }
}

static void read_key(menu_t* menu,int c) {
  switch(c) {
  case KEY_ESC:
    if(mpriv->hide_time)
      mpriv->hide_ts = GetTimerMS();
    else
      menu->show = 0;
    mpriv->show_ts = 0;
    return;
  case KEY_ENTER: {
    mp_cmd_t* c = mp_input_parse_cmd(&mpriv->input[strlen(mpriv->prompt)]);
    add_line(mpriv,mpriv->input);
    if(!c)
      add_line(mpriv,"Invalid command try help");
    else {
      switch(c->id) {
      case MP_CMD_CHELP:
	add_line(mpriv,"Mplayer console 0.01");
	add_line(mpriv,"TODO: Write some mainful help msg ;)");
	add_line(mpriv,"Enter any mplayer command");
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
      default: // Send the other commands to mplayer
	mp_input_queue_cmd(c);
      }
    }
    mpriv->input[strlen(mpriv->prompt)] = '\0';
    return;
  }
  case KEY_DELETE:
  case KEY_BS: {
    unsigned int i = strlen(mpriv->input);
    if(i > strlen(mpriv->prompt))
      mpriv->input[i-1] = '\0';
    return;
  }
  }

  if(isascii(c)) {
    int l = strlen(mpriv->input);
    mpriv->input[l] = (char)c;
    mpriv->input[l+1] = '\0';
  }

}


static int open(menu_t* menu, char* args) {


  menu->draw = draw;
  menu->read_cmd = read_cmd;
  menu->read_key = read_key;

  mpriv->lines = calloc(mpriv->buf_lines,sizeof(char*));
  mpriv->input_size = 1024;
  mpriv->input = calloc(mpriv->input_size,sizeof(char));
  strcpy(mpriv->input,mpriv->prompt);
  
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
  open,
};
