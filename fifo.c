
#ifndef HAVE_NO_POSIX_SELECT

// keyboard:
static int keyb_fifo_put=-1;
static int keyb_fifo_get=-1;

static void make_pipe(int* pr,int* pw){
  int temp[2];
  if(pipe(temp)!=0) printf("Cannot make PIPE!\n");
  *pr=temp[0];
  *pw=temp[1];
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
           if(select(keyb_fifo_put+1, NULL, &rfds, NULL, &tv)>0){
             write(keyb_fifo_put,&code,4);
//             printf("*** key event %d sent ***\n",code);
           } else {
//             printf("*** key event dropped (FIFO is full) ***\n");
           }
}

#else

#define KEY_FIFO_SIZE 1024
static int key_fifo_data[KEY_FIFO_SIZE];
static int key_fifo_read=0;
static int key_fifo_write=0;

void mplayer_put_key(int code){
//  printf("mplayer_put_key(%d)\n",code);
  if(((key_fifo_write+1)%KEY_FIFO_SIZE)==key_fifo_read) return; // FIFO FULL!!
  key_fifo_data[key_fifo_write]=code;
  key_fifo_write=(key_fifo_write+1)%KEY_FIFO_SIZE;
}

int mplayer_get_key(int fd){
  int key;
//  printf("mplayer_get_key(%d)\n",fd);
  if(key_fifo_write==key_fifo_read) return MP_INPUT_NOTHING;
  key=key_fifo_data[key_fifo_read];
  key_fifo_read=(key_fifo_read+1)%KEY_FIFO_SIZE;
//  printf("mplayer_get_key => %d\n",key);
  return key;
}

#endif

