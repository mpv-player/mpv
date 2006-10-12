
#include "config.h"

#include <lirc/lirc_client.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>

#include "mp_msg.h"
#include "help_mp.h"
#include "input.h"

static struct lirc_config *lirc_config;
char *lirc_configfile;

static char* cmd_buf = NULL;

int 
mp_input_lirc_init(void) {
  int lirc_sock;

  mp_msg(MSGT_LIRC,MSGL_V,MSGTR_SettingUpLIRC);
  if((lirc_sock=lirc_init("mplayer",1))==-1){
    mp_msg(MSGT_LIRC,MSGL_ERR,MSGTR_LIRCopenfailed);
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
  fd_set fds;
  struct timeval tv;
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
      
  // Nothing in the buffer, pool the lirc fd
  FD_ZERO(&fds);
  FD_SET(fd,&fds);
  memset(&tv,0,sizeof(tv));
  while((r = select(fd+1,&fds,NULL,NULL,&tv)) <= 0) {
    if(r < 0) {
      if(errno == EINTR)
	continue;
      mp_msg(MSGT_INPUT,MSGL_ERR,"Select error : %s\n",strerror(errno));
      return MP_INPUT_ERROR;
    } else
      return MP_INPUT_NOTHING;
  }
  
  // There's something to read
  if(lirc_nextcode(&code) != 0) {
    mp_msg(MSGT_INPUT,MSGL_ERR,"Lirc error :(\n");
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
