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
#include <ctype.h>
#include <unistd.h>
#include <limits.h>


#include "config.h"

#include "m_struct.h"
#include "m_option.h"

#include "libmpcodecs/img_format.h"
#include "libmpcodecs/mp_image.h"

#include "menu.h"
#include "menu_list.h"
#include "input/input.h"
#include "osdep/keycodes.h"

#include "stream/dvbin.h"



struct list_entry_s {
  struct list_entry p;
  int num;		//the position of the chosen channel in the list
};

struct menu_priv_s {
  menu_list_priv_t p;
  char* title;
  char* file;
  int card;
  int level;
  int auto_close;
  dvb_config_t *config;
};


#define ST_OFF(m) M_ST_OFF(struct menu_priv_s, m)
#define mpriv (menu->priv)

static m_option_t cfg_fields[] = {
  MENU_LIST_PRIV_FIELDS,
  { "title", ST_OFF(title), CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "auto-close", ST_OFF(auto_close), CONF_TYPE_FLAG, 0, 0, 1, NULL },
  { NULL, NULL, NULL, 0,0,0,NULL },
};


static struct menu_priv_s cfg_dflt = {
  MENU_LIST_PRIV_DFLT,
  "Select a channel: ",
  "channels.conf",
  0,
  0,
  1,
  NULL,
};




static void free_entry(list_entry_t* entry)
{
  free(entry->p.txt);
  free(entry);
}


static int fill_channels_menu(menu_t *menu, dvb_channels_list  *dvb_list_ptr)
{
	int n;
	dvb_channel_t *channel;
	list_entry_t* elem;

	mpriv->level = 2;
	if(dvb_list_ptr == NULL)
	{
		mp_msg(MSGT_DEMUX, MSGL_ERR, "dvb_set_channel: LIST NULL PTR, quit\n");
		n = 1;
		if((elem = calloc(1, sizeof(list_entry_t))) != NULL)
		{
			elem->p.next 	= NULL;
			elem->p.txt 	= strdup("There are no channels for this DVB card!");

			menu_list_add_entry(menu, elem);
		}
		return 1;
	}
	for(n = 0; n < dvb_list_ptr->NUM_CHANNELS; n++)
	{
		channel = &(dvb_list_ptr->channels[n]);
		if((elem = calloc(1, sizeof(list_entry_t))) != NULL)
		{
			elem->p.next 	= NULL;
			elem->p.txt 	= strdup(channel->name);
			elem->num 	= n;
			
			menu_list_add_entry(menu, elem);
		}
		else
		{
			mp_msg(MSGT_DEMUX, MSGL_ERR, "dvb_menu: fill_menu: couldn't malloc %d bytes for menu item: %s, exit\n", 
					sizeof(list_entry_t), strerror(errno));
			break;
		}
	}  
	
	return n;
}


static int fill_cards_menu(menu_t *menu, dvb_config_t *conf)
{
	int n;
	list_entry_t* elem;

	for(n = 0; n < conf->count; n++)
	{
		if((elem = calloc(1, sizeof(list_entry_t))) != NULL)
		{
			elem->p.next 	= NULL;
			elem->p.txt	= strdup(conf->cards[n].name);
			elem->num 	= n;
				
			if(n == 0)
			    elem->p.prev = NULL;
				
			menu_list_add_entry(menu, elem);
		}
		else
		{
			fprintf(stderr, "dvb_menu: fill_menu: couldn't malloc %d bytes for menu item: %s, exit\n", 
				sizeof(list_entry_t), strerror(errno));
			if(n)
				return 1;

			return 0;
		}
	}  
	
	return n;
}


static int fill_menu(menu_t* menu)
{
	list_entry_t* elem;
	dvb_channels_list  *dvb_list_ptr;
		
	menu_list_init(menu);
	
	if(mpriv->config == NULL)
	{
		if((elem = calloc(1, sizeof(list_entry_t))) != NULL)
		{
			elem->p.prev = elem->p.next = NULL;
			elem->p.txt = strdup("NO DVB configuration present!");

			menu_list_add_entry(menu, elem);
			return 1;
		}
		return 0;
	}
	
	mpriv->p.title = mpriv->title;
	if(mpriv->level == 1 && mpriv->config->count > 1)
		return fill_cards_menu(menu, mpriv->config);
	else
	{
		dvb_list_ptr = mpriv->config->cards[mpriv->card].list;
		return fill_channels_menu(menu, dvb_list_ptr);
	}
}


static void read_cmd(menu_t* menu, int cmd)
{
  list_entry_t *elem;
  mp_cmd_t* c;
  char *cmd_name;
  switch(cmd)
  {
	case MENU_CMD_RIGHT:
	case MENU_CMD_OK:
	{
		elem = mpriv->p.current;

		if(mpriv->level == 1)
		{
			mpriv->card = mpriv->p.current->num;
			mpriv->level = 2;
			menu_list_uninit(menu, free_entry);
			fill_menu(menu); 
		}
		else
		{
			dvb_priv_t *dvbp = (dvb_priv_t*) mpriv->config->priv;
			cmd_name = malloc(25 + strlen(elem->p.txt));
			if(dvbp != NULL)
				sprintf(cmd_name, "dvb_set_channel %d %d", elem->num, mpriv->card);	
			else
				sprintf(cmd_name, "loadfile 'dvb://%d@%s'", mpriv->card+1, elem->p.txt);
		
			c = mp_input_parse_cmd(cmd_name);
			free(cmd_name);
			if(c)
			{
				if(mpriv->auto_close)
					mp_input_queue_cmd (mp_input_parse_cmd ("menu hide"));
				mp_input_queue_cmd(c);
			}
		}
  	}
  	break;

	case MENU_CMD_LEFT:
	case MENU_CMD_CANCEL:
	{
		elem = mpriv->p.current;
		
		menu_list_uninit(menu, free_entry);
		if(mpriv->config->count > 1)
			mpriv->level--;
		else
			mpriv->level = 0;

		if(mpriv->level > 0)
		{
			fill_menu(menu);
			break;
		}
	}

  	default:
    	menu_list_read_cmd(menu, cmd);
  }
}


static void close_menu(menu_t* menu)
{
	dvb_free_config(mpriv->config);
	menu_list_uninit(menu, free_entry);
}


static int open_dvb_sel(menu_t* menu, char* args)
{
	mpriv->config = dvb_get_config();
	if(mpriv->config == NULL)
		return 0;

	menu->draw 		= menu_list_draw;
	menu->read_cmd 	= read_cmd;
	menu->close 	= close_menu;

	mpriv->card = 0;
	mpriv->level = 1;
	return fill_menu(menu);
}

const menu_info_t menu_info_dvbsel =
{
  "DVB channels menu",	//descr
  "dvbsel",						//name
  "Nico",						//author
  "dvb_sel",
  {								//m_struct_t priv_st=
    "dvb_cfg",					//name
    sizeof(struct menu_priv_s),			//size
    &cfg_dflt,					//defaults
    cfg_fields					//settable fields
  },
  open_dvb_sel					//open function
};
