
// keyboard:
static int keyb_fifo_put=-1;
static int keyb_fifo_get=-1;

static void make_pipe(int* pr,int* pw){
  int temp[2];
  if(pipe(temp)!=0) printf("Cannot make PIPE!\n");
  *pr=temp[0];
  *pw=temp[1];
}

static inline int my_write(int fd,unsigned char* mem,int len){
  int total=0;
  int len2;
  while(len>0){
    len2=write(fd,mem+total,len); if(len2<=0) break;
    total+=len2;len-=len2;
//    printf("%d bytes received, %d left\n",len2,len);
  }
  return total;
}

static inline int my_read(int fd,unsigned char* mem,int len){
  int total=0;
  int len2;
  while(len>0){
    len2=read(fd,mem+total,len); if(len2<=0) break;
    total+=len2;len-=len2;
//    printf("%d bytes received, %d left\n",len2,len);
  }
  return total;
}


void send_cmd(int fd,int cmd){
  int fifo_cmd=cmd;
  write(fd,&fifo_cmd,4);
//  fflush(control_fifo);
}


void mplayer_put_key(int code){
           fd_set rfds;
           struct timeval tv;

           /* Watch stdin (fd 0) to see when it has input. */
           FD_ZERO(&rfds);
           FD_SET(keyb_fifo_put, &rfds);
           tv.tv_sec = 0;
           tv.tv_usec = 0;

           //retval = select(keyb_fifo_put+1, &rfds, NULL, NULL, &tv);
           if(select(keyb_fifo_put+1, NULL, &rfds, NULL, &tv)){
             write(keyb_fifo_put,&code,4);
//             printf("*** key event %d sent ***\n",code);
           } else {
//             printf("*** key event dropped (FIFO is full) ***\n");
           }
}

int mplayer_get_key(){
           fd_set rfds;
           struct timeval tv;
           int code=-1;

           /* Watch stdin (fd 0) to see when it has input. */
           FD_ZERO(&rfds);
           FD_SET(keyb_fifo_get, &rfds);
           tv.tv_sec = 0;
           tv.tv_usec = 0;

           //retval = select(keyb_fifo_put+1, &rfds, NULL, NULL, &tv);
           if(select(keyb_fifo_put+1, &rfds, NULL, NULL, &tv)){
             read(keyb_fifo_get,&code,4);
//             printf("*** key event %d read ***\n",code);
           }
           return code;
}

