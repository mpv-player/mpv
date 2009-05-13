/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

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

#include "mp_msg.h"
#include "help_mp.h"

#ifndef JOY_AXIS_DELTA
#define JOY_AXIS_DELTA 500
#endif

#ifndef JS_DEV
#define JS_DEV "/dev/input/js0"
#endif

#include <linux/joystick.h>

int axis[256];
int btns = 0;

int mp_input_joystick_init(char* dev) {
  int fd,l=0;
  int initialized = 0;
  struct js_event ev;

  mp_msg(MSGT_INPUT,MSGL_V,MSGTR_INPUT_JOYSTICK_Opening,dev ? dev : JS_DEV);

  fd = open( dev ? dev : JS_DEV , O_RDONLY | O_NONBLOCK );
  if(fd < 0) {
    mp_msg(MSGT_INPUT,MSGL_ERR,MSGTR_INPUT_JOYSTICK_CantOpen,dev ? dev : JS_DEV,strerror(errno));
    return -1;
  }

  while(! initialized) {
    l = 0;
    while((unsigned int)l < sizeof(struct js_event)) {
      int r = read(fd,((char*)&ev)+l,sizeof(struct js_event)-l);
      if(r < 0) {
	if(errno == EINTR)
	  continue;
	else if(errno == EAGAIN) {
	  initialized = 1;
	  break;
	}
	mp_msg(MSGT_INPUT,MSGL_ERR,MSGTR_INPUT_JOYSTICK_ErrReading,strerror(errno));
	close(fd);
	return -1;
      }
      l += r;
    }
    if((unsigned int)l < sizeof(struct js_event)) {
      if(l > 0)
	mp_msg(MSGT_INPUT,MSGL_WARN,MSGTR_INPUT_JOYSTICK_LoosingBytes,l);
      break;
    }
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
	mp_msg(MSGT_INPUT,MSGL_ERR,MSGTR_INPUT_JOYSTICK_ErrReading,strerror(errno));
      else
	mp_msg(MSGT_INPUT,MSGL_ERR,MSGTR_INPUT_JOYSTICK_ErrReading,"EOF");
      return MP_INPUT_DEAD;
    }
    l += r;
  }

  if((unsigned int)l < sizeof(struct js_event)) {
    if(l > 0)
      mp_msg(MSGT_INPUT,MSGL_WARN,MSGTR_INPUT_JOYSTICK_LoosingBytes,l);
    return MP_INPUT_NOTHING;
  }

  if(ev.type & JS_EVENT_INIT) {
    mp_msg(MSGT_INPUT,MSGL_WARN,MSGTR_INPUT_JOYSTICK_WarnLostSync);
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
      return (JOY_BTN0 + ev.number) | MP_KEY_DOWN;
    else
      return JOY_BTN0 + ev.number;
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
    mp_msg(MSGT_INPUT,MSGL_WARN,MSGTR_INPUT_JOYSTICK_WarnUnknownEvent,ev.type);
    return MP_INPUT_ERROR;
  }

  return MP_INPUT_NOTHING;
}
