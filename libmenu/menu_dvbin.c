
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
#include "../osdep/keycodes.h"

#include "../libmpdemux/dvbin.h"



struct list_entry_s {
  struct list_entry p;
  int num;		//the position of the chosen channel in the list
};

struct menu_priv_s {
  menu_list_priv_t p;
  char* title;
  char* file;
  int card;
};


#define ST_OFF(m) M_ST_OFF(struct menu_priv_s, m)
#define mpriv (menu->priv)

static m_option_t cfg_fields[] = {
  MENU_LIST_PRIV_FIELDS,
  { "title", ST_OFF(title), CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "file", ST_OFF(file),  CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "card", ST_OFF(card),  CONF_TYPE_INT, 0, 0, 0, NULL },
  { NULL, NULL, NULL, 0,0,0,NULL }
};


static struct menu_priv_s cfg_dflt = {
  MENU_LIST_PRIV_DFLT,
  "Select a channel: %p",
  "channels.conf",
  1,
  NULL,
};




static void free_entry(list_entry_t* entry)
{
  free(entry->p.txt);
  free(entry);
}


static int fill_menu(menu_t* menu)
{
	int n;
	list_entry_t* elem;
	char *name;
	extern dvb_channels_list  *dvb_list_ptr;
	dvb_channel_t *channel;

	menu_list_init(menu);
	
	mpriv->p.title = mpriv->title;
	
	if(dvb_list_ptr == NULL)
    {
    	mp_msg(MSGT_DEMUX, MSGL_ERR, "dvb_set_channel: LIST NULL PTR, quit\n");
		n = 1;
		if((elem = malloc(sizeof(list_entry_t))) != NULL)
		{
			name = malloc(80);
			sprintf(name, "Empty channel list from file %s; \nrun mplayer dvb:// to load the list", mpriv->file);
			elem->p.next 	= NULL;
			elem->p.txt 	= name;

			menu_list_add_entry(menu, elem);
		}
    }
	else
	{
		n = dvb_list_ptr->NUM_CHANNELS;
		for(n = 0; n < dvb_list_ptr->NUM_CHANNELS; n++)
		{
			channel = &(dvb_list_ptr->channels[n]);
			if((elem = malloc(sizeof(list_entry_t))) != NULL)
			{
		  		name = malloc(80);
				strncpy(name, channel->name, 79);
				name[79] = 0;
				elem->p.next 	= NULL;
				elem->p.txt 	= name;
				elem->num 	= n;
				
				
				menu_list_add_entry(menu, elem);
			}
			else
			{
				mp_msg(MSGT_DEMUX, MSGL_ERR, "dvb_menu: fill_menu: couldn't malloc %d bytes for menu item: %s, exit\n", 
						sizeof(list_entry_t), strerror(errno));
			
			  if(n)
				return 1;
			
			  return 0;
			}
		}  
	}
	
	return 1;
}


static void read_cmd(menu_t* menu, int cmd)
{
  list_entry_t *p;
  mp_cmd_t* c;
  char *cmd_name;
  switch(cmd)
  {
	case MENU_CMD_OK:
	{
		p = mpriv->p.current;
		mp_msg(MSGT_DEMUX, MSGL_V, "CHOSEN DVB CHANNEL %d\n\n", p->num);
		
		cmd_name = malloc(30);
		sprintf(cmd_name, "dvb_set_channel %d", p->num);
		c = mp_input_parse_cmd(cmd_name);
    	if(c)
		  mp_input_queue_cmd(c);
  	}
  	break;

  	default:
    	menu_list_read_cmd(menu, cmd);
  }
}


static void close_menu(menu_t* menu)
{
	menu_list_uninit(menu, free_entry);
	//free(mpriv->dir);
}


static int open_dvb_sel(menu_t* menu, char* args)
{
	menu->draw 		= menu_list_draw;
	menu->read_cmd 	= read_cmd;
	//menu->read_key 	= read_key;
	menu->close 	= close_menu;

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
    sizeof(dvb_channels_list),	//size
    &cfg_dflt,					//defaults
    cfg_fields					//settable fields
  },
  open_dvb_sel					//open function
};
