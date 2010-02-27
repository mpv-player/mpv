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

#include <lirc/lirc_client.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "mp_msg.h"
#include "help_mp.h"
#include "input.h"
#include "lirc.h"

static struct lirc_config *lirc_config;
char *lirc_configfile;

static char* cmd_buf = NULL;

int
mp_input_lirc_init(void) {
  int lirc_sock;
  int mode;

  mp_msg(MSGT_LIRC,MSGL_V,MSGTR_SettingUpLIRC);
  if((lirc_sock=lirc_init("mplayer",1))==-1){
    mp_msg(MSGT_LIRC,MSGL_ERR,MSGTR_LIRCopenfailed);
    return -1;
  }

  mode = fcntl(lirc_sock, F_GETFL);
  if (mode < 0 || fcntl(lirc_sock, F_SETFL, mode | O_NONBLOCK) < 0) {
    mp_msg(MSGT_LIRC, MSGL_ERR, "setting non-blocking mode failed: %s\n",
	strerror(errno));
    lirc_deinit();
    return -1;
  }

  if(lirc_readconfig( lirc_configfile,&lirc_config,NULL )!=0 ){
    mp_msg(MSGT_LIRC,MSGL_ERR,MSGTR_LIRCcfgerr,
		    lirc_configfile == NULL ? "~/.lircrc" : lirc_configfile);
    lirc_deinit();
    return -1;
  }

  return lirc_sock;
}

int mp_input_lirc_read(int fd,char* dest, int s) {
  int r,cl = 0;
  char *code = NULL,*c = NULL;

  // We have something in the buffer return it
  if(cmd_buf != NULL) {
    int l = strlen(cmd_buf), w = l > s ? s : l;
    memcpy(dest,cmd_buf,w);
    l -= w;
    if(l > 0)
      memmove(cmd_buf,&cmd_buf[w],l+1);
    else {
      free(cmd_buf);
      cmd_buf = NULL;
    }
    return w;
  }

  // Nothing in the buffer, poll the lirc fd
  if(lirc_nextcode(&code) != 0) {
    mp_msg(MSGT_LIRC,MSGL_ERR,"Lirc error :(\n");
    return MP_INPUT_DEAD;
  }

  if(!code) return MP_INPUT_NOTHING;

  // We put all cmds in a single buffer separated by \n
  while((r = lirc_code2char(lirc_config,code,&c))==0 && c!=NULL) {
    int l = strlen(c);
    if(l <= 0)
      continue;
    cmd_buf = realloc(cmd_buf,cl+l+2);
    memcpy(&cmd_buf[cl],c,l);
    cl += l+1;
    cmd_buf[cl-1] = '\n';
    cmd_buf[cl] = '\0';
  }

  free(code);

  if(r < 0)
    return MP_INPUT_DEAD;
  else if(cmd_buf) // return the first command in the buffer
    return mp_input_lirc_read(fd,dest,s);
  else
    return MP_INPUT_RETRY;

}

void
mp_input_lirc_close(int fd) {
  if(cmd_buf) {
    free(cmd_buf);
    cmd_buf = NULL;
  }
  lirc_freeconfig(lirc_config);
  lirc_deinit();
}
