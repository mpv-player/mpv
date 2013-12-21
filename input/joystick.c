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

#include "common/msg.h"
#include "keycodes.h"

#ifndef JOY_AXIS_DELTA
#define JOY_AXIS_DELTA 500
#endif

#ifndef JS_DEV
#define JS_DEV "/dev/input/js0"
#endif

#include <linux/joystick.h>

static int mp_input_joystick_read(void *ctx, int fd);

struct ctx {
    struct mp_log *log;
    int axis[256];
    int btns;
};

static int close_js(void *ctx, int fd)
{
  talloc_free(ctx);
  return 0;
}

int mp_input_joystick_init(struct input_ctx *ictx, struct mp_log *log, char *dev)
{
  int fd,l=0;
  int initialized = 0;
  struct js_event ev;

  mp_verbose(log, "Opening joystick device %s\n",dev ? dev : JS_DEV);

  fd = open( dev ? dev : JS_DEV , O_RDONLY | O_NONBLOCK );
  if(fd < 0) {
    mp_err(log, "Can't open joystick device %s: %s\n",dev ? dev : JS_DEV,strerror(errno));
    return -1;
  }

  struct ctx *ctx = talloc_ptrtype(NULL, ctx);
  *ctx = (struct ctx) {.log = log};

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
	MP_ERR(ctx, "Error while reading joystick device: %s\n",strerror(errno));
	close(fd);
        talloc_free(ctx);
	return -1;
      }
      l += r;
    }
    if((unsigned int)l < sizeof(struct js_event)) {
      if(l > 0)
	MP_WARN(ctx, "Joystick: We lose %d bytes of data\n",l);
      break;
    }
    if(ev.type == JS_EVENT_BUTTON)
      ctx->btns |= (ev.value << ev.number);
    if(ev.type == JS_EVENT_AXIS)
      ctx->axis[ev.number] = ev.value;
  }

  mp_input_add_fd(ictx, fd, 1, NULL, mp_input_joystick_read, close_js, ctx);
  return fd;
}

static int mp_input_joystick_read(void *pctx, int fd) {
  struct ctx *ctx = pctx;
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
	MP_ERR(ctx, "Error while reading joystick device: %s\n",strerror(errno));
      else
	MP_ERR(ctx, "Error while reading joystick device: %s\n","EOF");
      return MP_INPUT_DEAD;
    }
    l += r;
  }

  if((unsigned int)l < sizeof(struct js_event)) {
    if(l > 0)
      MP_WARN(ctx, "Joystick: We lose %d bytes of data\n",l);
    return MP_INPUT_NOTHING;
  }

  if(ev.type & JS_EVENT_INIT) {
    MP_WARN(ctx, "Joystick: warning init event, we have lost sync with driver.\n");
    ev.type &= ~JS_EVENT_INIT;
    if(ev.type == JS_EVENT_BUTTON) {
      int s = (ctx->btns >> ev.number) & 1;
      if(s == ev.value) // State is the same : ignore
	return MP_INPUT_NOTHING;
    }
    if(ev.type == JS_EVENT_AXIS) {
      if( ( ctx->axis[ev.number] == 1 && ev.value > JOY_AXIS_DELTA) ||
	  (ctx->axis[ev.number] == -1 && ev.value < -JOY_AXIS_DELTA) ||
	  (ctx->axis[ev.number] == 0 && ev.value >= -JOY_AXIS_DELTA && ev.value <= JOY_AXIS_DELTA)
	  ) // State is the same : ignore
	return MP_INPUT_NOTHING;
    }
  }

  if(ev.type & JS_EVENT_BUTTON) {
    ctx->btns &= ~(1 << ev.number);
    ctx->btns |= (ev.value << ev.number);
    if(ev.value == 1)
      return (MP_JOY_BTN0 + ev.number) | MP_KEY_STATE_DOWN;
    else
      return (MP_JOY_BTN0 + ev.number) | MP_KEY_STATE_UP;
  } else if(ev.type & JS_EVENT_AXIS) {
    if(ev.value < -JOY_AXIS_DELTA && ctx->axis[ev.number] != -1) {
      ctx->axis[ev.number] = -1;
      return (MP_JOY_AXIS0_MINUS+(2*ev.number)) | MP_KEY_STATE_DOWN;
    } else if(ev.value > JOY_AXIS_DELTA && ctx->axis[ev.number] != 1) {
      ctx->axis[ev.number] = 1;
      return (MP_JOY_AXIS0_PLUS+(2*ev.number)) | MP_KEY_STATE_DOWN;
    } else if(ev.value <= JOY_AXIS_DELTA && ev.value >= -JOY_AXIS_DELTA && ctx->axis[ev.number] != 0) {
      int r = ctx->axis[ev.number] == 1 ? MP_JOY_AXIS0_PLUS+(2*ev.number) : MP_JOY_AXIS0_MINUS+(2*ev.number);
      ctx->axis[ev.number] = 0;
      return r | MP_KEY_STATE_UP;
    } else
      return MP_INPUT_NOTHING;
  } else {
    MP_WARN(ctx, "Joystick warning unknown event type %d\n",ev.type);
    return MP_INPUT_ERROR;
  }

  return MP_INPUT_NOTHING;
}
