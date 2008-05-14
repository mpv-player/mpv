/*
 * Support chapter list and selection.
 *
 * Copyright (C) 2006-2007 Benjamin Zores <ben A geexbox P org>
 * Copyright (C) 2007 Ulion <ulion A gmail P com>
 *
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

#include "config.h"

#include "m_struct.h"
#include "m_option.h"
#include "input/input.h"

#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "access_mpcontext.h"

#include "libmpcodecs/mp_image.h"

#include "menu.h"
#include "menu_list.h"

struct list_entry_s {
  struct list_entry p;
  int cid;
};

struct menu_priv_s {
  menu_list_priv_t p;
  char* title;
  int auto_close;
  char* fmt_with_time;
};

static struct menu_priv_s cfg_dflt = {
  MENU_LIST_PRIV_DFLT,
  "Select chapter",
  0,
  "${chapter_name}  [${start}]"
};

#define ST_OFF(m) M_ST_OFF(struct menu_priv_s,m)

static m_option_t cfg_fields[] = {
  MENU_LIST_PRIV_FIELDS,
  { "title", ST_OFF (title),  CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "auto-close", ST_OFF (auto_close), CONF_TYPE_FLAG, 0, 0, 1, NULL },
  { "fmt-with-time", ST_OFF (fmt_with_time),  CONF_TYPE_STRING, 0, 0, 0, NULL },
  { NULL, NULL, NULL, 0, 0, 0, NULL }
};

static char *fmt_replace(const char *fmt, const char *chapter_name,
                         const char *start) {
    static const char ctag[] = "${chapter_name}"; 
    static const char stag[] = "${start}"; 
    int l = strlen(fmt);
    int cl = strlen(chapter_name);
    int sl = strlen(start);
    char *str = malloc(l + cl + sl);
    char *p;
    strcpy(str, fmt);
    p = strstr(str, ctag);
    if (p) {
        memmove(p+cl, p+sizeof(ctag)-1, str+l+1 - (p+sizeof(ctag)-1));
        memcpy(p, chapter_name, cl);
        l -= sizeof(ctag) + 1;
        l += cl;
    }
    p = strstr(str, stag);
    if (p) {
        memmove(p+sl, p+sizeof(stag)-1, str+l+1 - (p+sizeof(stag)-1));
        memcpy(p, start, sl);
        l -= sizeof(stag) + 1;
        l += sl;
    }
    return str;
}

static int fill_menu (menu_t* menu)
{
  list_entry_t* e;
  int cid, chapter_num = 0;
  int start_time;
  demuxer_t* demuxer = mpctx_get_demuxer(menu->ctx);

  if (demuxer)
    chapter_num = demuxer_chapter_count(demuxer);
  if (chapter_num > 0) {
    menu_list_init (menu);
    for (cid = 0; cid < chapter_num; ++cid)
      if ((e = calloc (1, sizeof (list_entry_t))) != NULL) {
        e->cid = cid + 1;
        e->p.next = NULL;
        e->p.txt = demuxer_chapter_display_name(demuxer, cid);
        start_time = demuxer_chapter_time(demuxer, cid, NULL);
        if (start_time >= 0) {
            char timestr[13];
            char *tmp;
            int hour = start_time / 3600;
            int minute = (start_time / 60) % 60;
            int seconds = start_time % 60;
            sprintf(timestr,"%02d:%02d:%02d", hour, minute, seconds);

            tmp = fmt_replace(menu->priv->fmt_with_time, e->p.txt, timestr);
            free(e->p.txt);
            e->p.txt = tmp;
        }
        menu_list_add_entry(menu, e);
      }
  }
  else
    menu_list_read_cmd(menu, MENU_CMD_CANCEL);

  return 1;
}

static void read_cmd (menu_t* menu, int cmd)
{
  switch (cmd) {
    case MENU_CMD_RIGHT:
    case MENU_CMD_OK: {
      char cmdbuf[26];
      sprintf(cmdbuf, "seek_chapter %d 1", menu->priv->p.current->cid);
      mp_input_queue_cmd(mp_input_parse_cmd(cmdbuf));
      if (menu->priv->auto_close)
        mp_input_queue_cmd(mp_input_parse_cmd("menu hide"));
      break;
    }
    default:
      menu_list_read_cmd (menu, cmd);
  }
}

static void close_cs (menu_t* menu)
{
  menu_list_uninit (menu, NULL);
}

static int open_cs (menu_t* menu, char* args)
{
  args = NULL;

  menu->draw = menu_list_draw;
  menu->read_cmd = read_cmd;
  menu->close = close_cs;
  menu->priv->p.title = menu->priv->title;

  return fill_menu (menu);
}

const menu_info_t menu_info_chapsel = {
  "Chapter selector menu",
  "chapsel",
  "Benjamin Zores & Ulion",
  "",
  {
    "chapsel_cfg",
    sizeof(struct menu_priv_s),
    &cfg_dflt,
    cfg_fields
  },
  open_cs
};
