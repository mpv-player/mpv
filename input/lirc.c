
#include "../config.h"

#ifdef HAVE_LIRC

#include <lirc/lirc_client.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>


#include "../mp_msg.h"
#include "../help_mp.h"

static struct lirc_config *lirc_config;
char *lirc_configfile;

static int child_pid=0;

static void
mp_input_lirc_process_quit(int sig);

static void
mp_input_lirc_process(int mp_fd);


int 
mp_input_lirc_init(void) {
  int lirc_sock;
  int p[2];

  mp_msg(MSGT_LIRC,MSGL_INFO,MSGTR_SettingUpLIRC);
  if((lirc_sock=lirc_init("mplayer",1))==-1){
    mp_msg(MSGT_LIRC,MSGL_ERR,MSGTR_LIRCopenfailed MSGTR_LIRCdisabled);
    return -1;
  }

  if(lirc_readconfig( lirc_configfile,&lirc_config,NULL )!=0 ){
    mp_msg(MSGT_LIRC,MSGL_ERR,MSGTR_LIRCcfgerr MSGTR_LIRCdisabled,
		    lirc_configfile == NULL ? "~/.lircrc" : lirc_configfile);
    lirc_deinit();
    return -1;
  }

  if(pipe(p) != 0) {
    mp_msg(MSGT_LIRC,MSGL_ERR,"Can't create lirc pipe : %s\n",strerror(errno));
    lirc_deinit();
  }

  child_pid = fork();

  if(child_pid < 0) {
    mp_msg(MSGT_LIRC,MSGL_ERR,"Can't fork lirc subprocess : %s\n",strerror(errno));
    lirc_deinit();
    close(p[0]);
    close(p[1]);
    return -1;
  } else if(child_pid == 0) {// setup child process
    close(p[0]);
    // put some signal handlers
    signal(SIGINT,mp_input_lirc_process_quit);
    signal(SIGHUP,mp_input_lirc_process_quit);
    signal(SIGQUIT,mp_input_lirc_process_quit);
    // start the process
    mp_input_lirc_process(p[1]);
  }

  // free unuseful ressources in parent process
  lirc_freeconfig(lirc_config);
  close(p[1]);

  mp_msg(MSGT_LIRC,MSGL_V,"NEW LIRC init was successful.\n");

  return p[0];
}

static void
mp_input_lirc_process_quit(int sig) {
  lirc_freeconfig(lirc_config);
  lirc_deinit();
  exit(sig > 0 ? 0 : -1);
}

static void
mp_input_lirc_process(int mp_fd) {
  char *cmd,*code;
  int ret;

  while(lirc_nextcode(&code)==0) {
    if(code==NULL)
      continue;
    while((ret=lirc_code2char(lirc_config,code,&cmd))==0 && cmd!=NULL) {
      int len = strlen(cmd)+1;
      char buf[len];
      int w=0;
      strcpy(buf,cmd);
      buf[len-1] = '\n';
      while(w < len) {
	int r = write(mp_fd,buf+w,len-w);
	if(r < 0) {
	  if(errno == EINTR)
	    continue;
	  mp_msg(MSGT_LIRC,MSGL_ERR,"LIRC subprocess can't write in input pipe : %s\n",
		 strerror(errno));
	  mp_input_lirc_process_quit(-1);
	}
	w += r;
      }
    }
    free(code);
    if(ret==-1)
      break;
  }
  mp_input_lirc_process_quit(-1);
}

void
mp_input_lirc_uninit(void) {
  if(child_pid <= 0)
    return;
  if( kill(child_pid,SIGQUIT) != 0) {
    mp_msg(MSGT_LIRC,MSGL_ERR,"LIRC can't kill subprocess %d : %s\n",
	   child_pid,strerror(errno));
    return;
  }

  if(waitpid(child_pid,NULL,0) < 0)
    mp_msg(MSGT_LIRC,MSGL_ERR,"LIRC error while waiting subprocess %d : %s\n",
	   child_pid,strerror(errno));

}

#endif
