
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

static int buttons = 0;
static int* axis;

int mp_input_joystick_init(char* dev) {
  int fd,l=0;
  int inited = 0;
  struct js_event ev;
  char n_axis;
  
  printf("Opening joystick device %s\n",dev ? dev : JS_DEV);

  fd = open( dev ? dev : JS_DEV , O_RDONLY | O_NONBLOCK );
  if(fd < 0) {
    printf("Can't open joystick device %s : %s\n",dev ? dev : JS_DEV,strerror(errno));
    return -1;
  }

  if(ioctl(fd, JSIOCGAXES, &n_axis) < 0) {
    printf("Joystick : can't get number of axis, %s\n",strerror(errno));
    close(fd);
    return -1;
  }

  axis = (int*)malloc(n_axis*sizeof(int));
  
  
  while(! inited) {
    l = 0;
    while((unsigned int)l < sizeof(struct js_event)) {
      int r = read(fd,&ev+l,sizeof(struct js_event)-l);
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
      return fd;
    }
    if(ev.type & JS_EVENT_INIT) {
      ev.type &= ~JS_EVENT_INIT;
      if(ev.type & JS_EVENT_BUTTON)
	buttons |= (ev.value << ev.number);
      else if(ev.type & JS_EVENT_AXIS)
	axis[ev.number] = ev.value;
    } else
      printf("Joystick : Warning non-init event during init :-o\n");
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
    printf("Joystick : warning reinit (Can happend more than on time)\n");
     ev.type &= ~JS_EVENT_INIT;
     if(ev.type & JS_EVENT_BUTTON)
       buttons |= (ev.value << ev.number);
     else if(ev.type & JS_EVENT_AXIS)
       axis[ev.number] = ev.value;
     else
       printf("Joystick warning unknow event type %d\n",ev.type);
     return mp_input_joystick_read(fd);
  }

  ev.type &= ~JS_EVENT_INIT;
  
  if(ev.type & JS_EVENT_BUTTON) {
    if(ev.value != ( 1 & (buttons >> ev.number)))
      return JOY_BTN0+ev.number;      
  } else if(ev.type & JS_EVENT_AXIS) {
    if(ev.value - axis[ev.number] > JOY_AXIS_DELTA)
      return JOY_AXIS0_MINUS+(2*ev.number);
    else if(axis[ev.number]  - ev.value > JOY_AXIS_DELTA)
      return JOY_AXIS0_PLUS+(2*ev.number);
  } else {
    printf("Joystick warning unknow event type %d\n",ev.type);
    return MP_INPUT_ERROR;
  }

  return MP_INPUT_NOTHING;
}

void
mp_input_joystick_close(int fd) {
  struct js_event ev;
  int l=0;
  fd_set fds;
  struct timeval tv;
  
  // Wait to see if there is any stale relaease event in the queue (when we quit we the joystick)
  FD_SET(fd,&fds);
  tv.tv_sec = 0;
  tv.tv_usec = 2000000;
  if(select(fd+1,&fds,NULL,NULL,&tv) > 0) {
    while((unsigned int)l < sizeof(struct js_event)) {
      int r = read(fd,&ev+l,sizeof(struct js_event)-l);
      if(r <= 0) {
	if(errno == EINTR)
	  continue;
	else
	  break;
      } 	
      l += r;
    }
  }
  close(fd);
}

#else

// dummy function

int mp_input_joystick_init(char* dev) {
  return -1;
}

int mp_input_joystick_read(int fd) {
  
  return MP_INPUT_NOTHING;
}

void
mp_input_joystick_close(int fd) {

}

#endif

#endif
