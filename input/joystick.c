
#include "../config.h"

#ifdef HAVE_JOYSTICK

#include "joystick.h"
#include "input.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#ifndef JOY_AXIS_DELTA
#define JOY_AXIS_DELTA 500
#endif

#ifndef JS_DEV
#define JS_DEV "/dev/input/js0"
#endif

#ifdef TARGET_LINUX

#include <linux/joystick.h>

int axis[10];
int btns = 0;

int mp_input_joystick_init(char* dev) {
  int fd,l=0;
  int inited = 0;
  struct js_event ev;
  
  printf("Opening joystick device %s\n",dev ? dev : JS_DEV);

  fd = open( dev ? dev : JS_DEV , O_RDONLY | O_NONBLOCK );
  if(fd < 0) {
    printf("Can't open joystick device %s : %s\n",dev ? dev : JS_DEV,strerror(errno));
    return -1;
  }
  
  while(! inited) {
    l = 0;
    while((unsigned int)l < sizeof(struct js_event)) {
      int r = read(fd,((char*)&ev)+l,sizeof(struct js_event)-l);
      if(r < 0) {
	if(errno == EINTR)
	  continue;
	else if(errno == EAGAIN) {
	  inited = 1;
	  break;
	}
	printf("Error while reading joystick device : %s\n",strerror(errno));
	close(fd);
	return -1;
      }	
      l += r;
    }
    if((unsigned int)l < sizeof(struct js_event)) {
      if(l > 0)
	printf("Joystick : we loose %d bytes of data\n",l);
      break;
    }
    ev.type &= ~JS_EVENT_INIT;
    if(ev.type == JS_EVENT_BUTTON)
      btns |= (ev.value << ev.number);
    if(ev.type == JS_EVENT_AXIS)
      axis[ev.number] = ev.value;
  }
	
  return fd;
}

int mp_input_joystick_read(int fd) {
  struct js_event ev;
  int l=0;

  while((unsigned int)l < sizeof(struct js_event)) {
    int r = read(fd,&ev+l,sizeof(struct js_event)-l);
    if(r <= 0) {
      if(errno == EINTR)
	continue;
      else if(errno == EAGAIN)
	return MP_INPUT_NOTHING;
      if( r < 0)
	printf("Joystick error while reading joystick device : %s\n",strerror(errno));
      else
	printf("Joystick error while reading joystick device : EOF\n");
      return MP_INPUT_DEAD;
    } 	
    l += r;
  }

  if((unsigned int)l < sizeof(struct js_event)) {
    if(l > 0)
      printf("Joystick : we loose %d bytes of data\n",l);
    return MP_INPUT_NOTHING;
  }

  if(ev.type & JS_EVENT_INIT) {
    printf("Joystick : warning init event, we have lost sync with driver\n");
    ev.type &= ~JS_EVENT_INIT;
    if(ev.type == JS_EVENT_BUTTON) {
      int s = (btns >> ev.number) & 1;
      if(s == ev.value) // State is the same : ignore
	return MP_INPUT_NOTHING;
    }
    if(ev.type == JS_EVENT_AXIS) {
      if( ( axis[ev.number] == 1 && ev.value > JOY_AXIS_DELTA) ||
	  (axis[ev.number] == -1 && ev.value < -JOY_AXIS_DELTA) ||
	  (axis[ev.number] == 0 && ev.value >= -JOY_AXIS_DELTA && ev.value <= JOY_AXIS_DELTA)
	  ) // State is the same : ignore
	return MP_INPUT_NOTHING;
    }	
  }
  
  if(ev.type & JS_EVENT_BUTTON) {
    btns &= ~(1 << ev.number);
    btns |= (ev.value << ev.number);
    if(ev.value == 1)
      return ((JOY_BTN0+ev.number) | MP_KEY_DOWN);
    else
      return (JOY_BTN0+ev.number); 
  } else if(ev.type & JS_EVENT_AXIS) {
    if(ev.value < -JOY_AXIS_DELTA && axis[ev.number] != -1) {
      axis[ev.number] = -1;
      return (JOY_AXIS0_MINUS+(2*ev.number)) | MP_KEY_DOWN;
    } else if(ev.value > JOY_AXIS_DELTA && axis[ev.number] != 1) {
      axis[ev.number] = 1;
      return (JOY_AXIS0_PLUS+(2*ev.number)) | MP_KEY_DOWN;
    } else if(ev.value <= JOY_AXIS_DELTA && ev.value >= -JOY_AXIS_DELTA && axis[ev.number] != 0) {
      int r = axis[ev.number] == 1 ? JOY_AXIS0_PLUS+(2*ev.number) : JOY_AXIS0_MINUS+(2*ev.number);
      axis[ev.number] = 0;
      return r;
    } else
      return MP_INPUT_NOTHING;
  } else {
    printf("Joystick warning unknow event type %d\n",ev.type);
    return MP_INPUT_ERROR;
  }

  return MP_INPUT_NOTHING;
}

#else

// dummy function

int mp_input_joystick_init(char* dev) {
  return -1;
}

int mp_input_joystick_read(int fd) {
  
  return MP_INPUT_NOTHING;
}

#endif

#endif
