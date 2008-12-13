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

#ifndef MPLAYER_MENU_H
#define MPLAYER_MENU_H

#include "m_struct.h"
#include "libmpcodecs/mp_image.h"

struct menu_priv_s;
typedef struct  menu_s menu_t;

typedef struct menu_def_st menu_def_t;

struct m_struct_st;

struct  menu_s {
  struct MPContext *ctx;
  void (*draw)(menu_t* menu,mp_image_t* mpi);
  void (*read_cmd)(menu_t* menu,int cmd);
  int (*read_key)(menu_t* menu,int cmd);
  void (*close)(menu_t* menu);
  struct m_struct_st* priv_st;
  struct menu_priv_s* priv;
  int show; // Draw it ?
  int cl; // Close request (user sent a close cmd or
  menu_t* parent;
  menu_def_t *type;
};

typedef struct menu_info_s {
  const char *info;
  const char *name;
  const char *author;
  const char *comment;
  struct m_struct_st priv_st; // Config struct definition
  // cfg is a config struct as defined in cfg_st, it may be used as a priv struct
  // cfg is filled from the attributs found in the cfg file
  // the args param hold the content of the balise in the cfg file (if any)
  int (*open)(menu_t* menu, char* args);
} menu_info_t;


#define MENU_CMD_UP 0
#define MENU_CMD_DOWN 1
#define MENU_CMD_OK 2
#define MENU_CMD_CANCEL 3
#define MENU_CMD_LEFT 4
#define MENU_CMD_RIGHT 5 
#define MENU_CMD_ACTION 6
#define MENU_CMD_HOME 7
#define MENU_CMD_END 8
#define MENU_CMD_PAGE_UP 9
#define MENU_CMD_PAGE_DOWN 10
#define MENU_CMD_CLICK 11

/// Global init/uninit
int menu_init(struct MPContext *mpctx, char* cfg_file);
void menu_uninit(void);

/// Open a menu defined in the config file
menu_t* menu_open(char *name);

void menu_draw(menu_t* menu,mp_image_t* mpi);
void menu_read_cmd(menu_t* menu,int cmd);
void menu_close(menu_t* menu);
int menu_read_key(menu_t* menu,int cmd);

//// Default implementation
int menu_dflt_read_key(menu_t* menu,int cmd);

/// Receive mouse position events.
void menu_update_mouse_pos(double x, double y);

/////////// Helpers

#define MENU_TEXT_TOP	(1<<0)
#define MENU_TEXT_VCENTER	(1<<1)
#define MENU_TEXT_BOT	(1<<2)
#define MENU_TEXT_VMASK	(MENU_TEXT_TOP|MENU_TEXT_VCENTER|MENU_TEXT_BOT)
#define MENU_TEXT_LEFT	(1<<3)
#define MENU_TEXT_HCENTER	(1<<4)
#define MENU_TEXT_RIGHT	(1<<5)
#define MENU_TEXT_HMASK	(MENU_TEXT_LEFT|MENU_TEXT_HCENTER|MENU_TEXT_RIGHT)
#define MENU_TEXT_CENTER	(MENU_TEXT_VCENTER|MENU_TEXT_HCENTER)

void menu_draw_text(mp_image_t* mpi, char* txt, int x, int y);
int menu_text_length(char* txt);
int menu_text_num_lines(char* txt, int max_width);

void menu_text_size(char* txt,int max_width, 
		    int vspace, int warp,
		    int* _w, int* _h);

void menu_draw_text_full(mp_image_t* mpi,char* txt,
			 int x, int y,int w, int h,
			 int vspace, int warp, int align, int anchor);

void menu_draw_box(mp_image_t* mpi, unsigned char grey, unsigned char alpha, int x, int y, int w, int h);

#endif /* MPLAYER_MENU_H */
