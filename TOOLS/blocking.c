#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUFFSIZE (32*65536)

unsigned char *buffer[1];

int main(int argc,char* argv[]){

           fd_set rfds;
           struct timeval tv;
           int retval;
	   int in_fd=0; // stdin

	   buffer[0]=malloc(BUFFSIZE);
	   
	   if(argc>1) in_fd=open(argv[1],O_RDONLY|O_NONBLOCK);

while(1){
           FD_ZERO(&rfds); FD_SET(in_fd, &rfds);
           tv.tv_sec = 1;
           tv.tv_usec = 0;
           retval = select(in_fd+1, &rfds, NULL, NULL, &tv);

           if (retval){
	       if(FD_ISSET(in_fd, &rfds)){
		       // we can read input.
	               int len;
	               fprintf(stderr,"r");fflush(stderr);
		       len=read(in_fd,buffer[0],BUFFSIZE);
	               fprintf(stderr,"(%d)",len);fflush(stderr);
		}
	   } else {
	           fprintf(stderr,".");fflush(stderr);
	   }

           fprintf(stderr,"\n");fflush(stderr);
}

return 0;
}

