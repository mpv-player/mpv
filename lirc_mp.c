/*

 lirc support for MPLayer (see www.lirc.org)

 v0.1
 
 written 15/2/2001 by Andreas Ackermann (acki@acki-netz.de) 

 file comes without warranty and all 

*/

// hack, will be remove later when ./configure fixed...
#include "config.h"
#ifdef HAVE_LIRC

// start of LIRC support

#include <lirc/lirc_client.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <string.h>
#include <fcntl.h>
#include "linux/keycodes.h"

// global stuff ----------------------------------------------------

static struct lirc_config *lirc_config;
static int    lirc_is_setup = 0;
char *lirc_configfile = NULL;

// setup routine ---------------------------------------------------

void lirc_mp_setup(void){

  int lirc_flags;
  int lirc_sock;

  printf("Setting up lirc support...\n");
  if((lirc_sock=lirc_init("mplayer_lirc",1))==-1){
    printf("Failed opening lirc support!\n");
    printf("You won't be able to use your remote control\n");
    return;
  }

  fcntl(lirc_sock,F_SETOWN,getpid());
  lirc_flags=fcntl(lirc_sock,F_GETFL,0);
  if(lirc_flags!=-1)
  {
     fcntl(lirc_sock,F_SETFL,lirc_flags|O_NONBLOCK);
  }else{
    lirc_deinit();
    printf("Something's wrong with the lirc socket: %s\n", 
            strerror(errno));
    printf("You won't be able to use your remote control\n");
    return;
  }


  if(lirc_readconfig( lirc_configfile,&lirc_config,NULL )!=0 ){
    printf("Failed to read config file %s !\n", 
		    lirc_configfile == NULL ? "~/.lircrc" : lirc_configfile);
    printf("You won't be able to use your remote control\n");
    lirc_deinit();
    return;
  }
  printf("LIRC init was successful.\n");
  lirc_is_setup = 1;
}

// cleanup routine -------------------------------------------

void lirc_mp_cleanup(void){
  if(lirc_is_setup != 0){
    printf("Cleaning up lirc stuff.\n");
    lirc_mp_getinput(NULL);
    lirc_freeconfig(lirc_config);
    lirc_deinit();
    lirc_is_setup = 0;
  }
}

// get some events -------------------------------------------


struct lirc_cmd {
  unsigned char *lc_lirccmd;
  int mplayer_cmd;
};

int lirc_mp_getinput(){

  static struct lirc_cmd lirc_cmd[] = {
    {"QUIT", KEY_ESC},
    {"FWD" , KEY_RIGHT},
    {"FFWD" , KEY_UP},
    {"RWND" , KEY_LEFT},
    {"FRWND" , KEY_DOWN},
    {"PAUSE", 'p'},
    {"INCVOL", '*'},
    {"DECVOL", '/'},
    {"MASTER", 'm'},
    {"OSD", 'o'}
  };
       
  char *code;
  char *c;
  int ret;
  int i;
  int retval = 0;

  if( lirc_is_setup == 0)return 0;
  
  if(lirc_config == NULL ){
    // do some cleanupstuff like freeing memory or the like
    // (if we ever should do it the right way and loop over all
    // all strings delivered by lirc_code2char() )
  }else{

    if(lirc_nextcode(&code)==0){
      if(code!=NULL){
        // this should be a while loop 
        // but we would have to introduce state since we need to keep 
        // code
        if((ret=lirc_code2char(lirc_config,code,&c))==0 && c!=NULL){
          fprintf(stderr, "LIRC: Got string \"%s\"",c);
          for(i=0; i< (sizeof(lirc_cmd)/sizeof(struct lirc_cmd)); i++){
	    if(!(strcmp(lirc_cmd[i].lc_lirccmd, c))){
              retval = lirc_cmd[i].mplayer_cmd;
	      break;
	    }

          }
        }
        free(code);
        if(ret==-1){ 
           printf("LIRC: lirc_code2char() returned an error!\n");
        }
      }
    }
  }
  return retval;
}

// end lirc support

#endif // HAVE_LIRC

