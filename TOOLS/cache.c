
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

//int open(const char *pathname, int flags);


#define BUFFSIZE (4*65536)
#define NUM_BUFS (16)

unsigned char *buffer[NUM_BUFS];

unsigned int buf_read=0;
unsigned int buf_write=0;
unsigned int buf_read_pos=0;
unsigned int buf_write_pos=0;
int full_buffers=0;

int main(int argc,char* argv[]){

           fd_set rfds;
           fd_set wfds;
           struct timeval tv;
           int retval;
	   int i;
//	   int empty=1;
	   int can_read=1;
	   int eof=0;
	   int in_fd=0; // stdin
	   
	   if(argc>1) in_fd=open(argv[1],O_RDONLY|O_NDELAY);
	   
	   for(i=0;i<NUM_BUFS;i++) buffer[i]=malloc(BUFFSIZE);

while(1){
           /* Watch stdin (fd 0) to see when it has input. */
           FD_ZERO(&rfds); if(can_read){ FD_SET(in_fd, &rfds);}
           FD_ZERO(&wfds); FD_SET(1, &wfds);
           /* Wait up to five seconds. */
           tv.tv_sec = 1;
           tv.tv_usec = 0;
           retval = select((in_fd<1?1:in_fd)+1, &rfds, &wfds, NULL, &tv);
           /* Don't rely on the value of tv now! */

           if (retval){
	       if(FD_ISSET(in_fd, &rfds) || !full_buffers){
	               fprintf(stderr,"\n%d  r",full_buffers);fflush(stderr);
		   if(full_buffers==NUM_BUFS){
		       // buffer is full!
		       can_read=0;
	               fprintf(stderr,"\n%d  full!\n",full_buffers);fflush(stderr);
		   } else {
		       // we can read input.
		       int len=BUFFSIZE-buf_read_pos;
	               fprintf(stderr,"R");fflush(stderr);
		       len=read(in_fd,buffer[buf_read]+buf_read_pos,len);
	               fprintf(stderr,"(%d)\n",len);fflush(stderr);
		       if(len>0){
		           buf_read_pos+=len;
			   if(buf_read_pos>=BUFFSIZE){
			       // block is full, find next!
			       buf_read=(buf_read+1)%NUM_BUFS;
			       ++full_buffers;
			       buf_read_pos=0;
	                       fprintf(stderr,"+");fflush(stderr);
			   }
		       } else {
		           eof=1;
		       }
		   }
	       }
	       if(FD_ISSET(1, &wfds)){
	               fprintf(stderr,"\n%d  w",full_buffers);fflush(stderr);
		   if(full_buffers==0){
		       if(eof){
		           // flush buffer!
			   int pos=0;
			   int len;
	                   fprintf(stderr,"\nf");fflush(stderr);
			   while((len=buf_read_pos-pos)>0){
			       len=write(1,buffer[buf_write]+pos,len);
	               fprintf(stderr,"(%d)",len);fflush(stderr);
			       if(len<=0) break;
			       pos+=len;
			   }
		           exit(1);
		       }
		       fprintf(stderr," empty");fflush(stderr);
		       //empty=1; // we must fill buffers!
		   } else {
		       // yeah, we can read from the buffer!
		       int len=BUFFSIZE-buf_write_pos;
	               fprintf(stderr,"W");fflush(stderr);
		       len=write(1,buffer[buf_write]+buf_write_pos,len);
	               fprintf(stderr,"(%d)",len);fflush(stderr);
		       if(len>0){
		           buf_write_pos+=len;
			   if(buf_write_pos>=BUFFSIZE){
			       // block is empty, find next!
			       buf_write=(buf_write+1)%NUM_BUFS;
			       --full_buffers;
			       buf_write_pos=0;
	                       fprintf(stderr,"-");fflush(stderr);
			       can_read=1;
			   }
		       }
		   }
	       }
	   } else {
	           fprintf(stderr,".");fflush(stderr);
	   }
}

return 0;
}

