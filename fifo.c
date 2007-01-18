#include "input/mouse.h"

#if 0

// keyboard:
static int keyb_fifo_put=-1;
static int keyb_fifo_get=-1;

static void set_nonblock_flag(int fd) {
  int oldflags;

  oldflags = fcntl(fd, F_GETFL, 0);
  if (oldflags != -1) {
    if (fcntl(keyb_fifo_put, F_SETFL, oldflags | O_NONBLOCK) != -1) {
       return;
    }
  }
  mp_msg(MSGT_INPUT,MSGL_ERR,"Cannot set nonblocking mode for fd %d!\n", fd);
}

static void make_pipe(int* pr,int* pw){
  int temp[2];
  if(pipe(temp)!=0) mp_msg(MSGT_FIXME, MSGL_FIXME, MSGTR_CannotMakePipe);
  *pr=temp[0];
  *pw=temp[1];
  set_nonblock_flag(temp[1]);
}

static void mplayer_put_key_internal(int code){

    if( write(keyb_fifo_put,&code,4) != 4 ){
        mp_msg(MSGT_INPUT,MSGL_ERR,"*** key event dropped (FIFO is full) ***\n");
    }
}

#else

int key_fifo_size = 10;
static int *key_fifo_data = NULL;
static int key_fifo_read=0;
static int key_fifo_write=0;

static void mplayer_put_key_internal(int code){
//  printf("mplayer_put_key(%d)\n",code);
  if (key_fifo_data == NULL)
    key_fifo_data = malloc(key_fifo_size * sizeof(int));
  if(((key_fifo_write+1)%key_fifo_size)==key_fifo_read) return; // FIFO FULL!!
  key_fifo_data[key_fifo_write]=code;
  key_fifo_write=(key_fifo_write+1)%key_fifo_size;
}

int mplayer_get_key(int fd){
  int key;
//  printf("mplayer_get_key(%d)\n",fd);
  if (key_fifo_data == NULL)
    return MP_INPUT_NOTHING;
  if(key_fifo_write==key_fifo_read) return MP_INPUT_NOTHING;
  key=key_fifo_data[key_fifo_read];
  key_fifo_read=(key_fifo_read+1)%key_fifo_size;
//  printf("mplayer_get_key => %d\n",key);
  return key;
}

#endif

static unsigned doubleclick_time = 300;

static void put_double(int code) {
  if (code >= MOUSE_BTN0 && code <= MOUSE_BTN9)
    mplayer_put_key_internal(code - MOUSE_BTN0 + MOUSE_BTN0_DBL);
}

void mplayer_put_key(int code) {
  static unsigned last_key_time[2];
  static int last_key[2];
  unsigned now = GetTimerMS();
  // ignore system-doubleclick if we generate these events ourselves
  if (doubleclick_time &&
      (code & ~MP_KEY_DOWN) >= MOUSE_BTN0_DBL &&
      (code & ~MP_KEY_DOWN) <= MOUSE_BTN9_DBL)
    return;
  mplayer_put_key_internal(code);
  if (code & MP_KEY_DOWN) {
    code &= ~MP_KEY_DOWN;
    last_key[1] = last_key[0];
    last_key[0] = code;
    last_key_time[1] = last_key_time[0];
    last_key_time[0] = now;
    if (last_key[1] == code &&
        now - last_key_time[1] < doubleclick_time)
      put_double(code);
    return;
  }
  if (last_key[0] == code && last_key[1] == code &&
      now - last_key_time[1] < doubleclick_time)
    put_double(code);
}
