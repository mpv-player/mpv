#include <stdlib.h>
#include "osdep/timer.h"
#include "input/input.h"
#include "input/mouse.h"


int key_fifo_size = 7;
static int *key_fifo_data = NULL;
static int key_fifo_read=0;
static int key_fifo_write=0;

static void mplayer_put_key_internal(int code){
  int fifo_free = key_fifo_read - key_fifo_write - 1;
  if (fifo_free < 0) fifo_free += key_fifo_size;
//  printf("mplayer_put_key(%d)\n",code);
  if (key_fifo_data == NULL)
    key_fifo_data = malloc(key_fifo_size * sizeof(int));
  if(!fifo_free) return; // FIFO FULL!!
  // reserve some space for key release events to avoid stuck keys
  if((code & MP_KEY_DOWN) && fifo_free < (key_fifo_size >> 1))
    return;
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


unsigned doubleclick_time = 300;

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
