/*
 * Linux Apple IR Remote input interface
 *
 * Copyright (C) 2008 Benjamin Zores <ben at geexbox dot org>
 *
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

#include "ar.h"
#include "input.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <linux/types.h>
#include <linux/input.h>

#include "mp_msg.h"
#include "help_mp.h"

#define EVDEV_MAX_EVENTS 32

/* ripped from AppleIR driver */
#define USB_VENDOR_APPLE          0x05ac
#define USB_DEV_APPLE_IR          0x8240
#define USB_DEV_APPLE_IR_2        0x8242

/* Apple IR Remote evdev mapping */
#define APPLE_IR_MINUS            KEY_VOLUMEDOWN
#define APPLE_IR_PLUS             KEY_VOLUMEUP
#define APPLE_IR_MENU             KEY_MENU
#define APPLE_IR_FORWARD          KEY_NEXTSONG
#define APPLE_IR_PLAY             KEY_PLAYPAUSE
#define APPLE_IR_BACKWARD         KEY_PREVIOUSSONG

static const struct {
  int linux_keycode;
  int value;
  int mp_keycode;
} apple_ir_mapping[] = {
  { APPLE_IR_PLAY,              1,   AR_PLAY      },
  { APPLE_IR_PLAY,              2,   AR_PLAY_HOLD },
  { APPLE_IR_FORWARD,           1,   AR_NEXT      },
  { APPLE_IR_FORWARD,           2,   AR_NEXT_HOLD },
  { APPLE_IR_BACKWARD,          1,   AR_PREV      },
  { APPLE_IR_BACKWARD,          2,   AR_PREV_HOLD },
  { APPLE_IR_MENU,              1,   AR_MENU      },
  { APPLE_IR_MENU,              2,   AR_MENU_HOLD },
  { APPLE_IR_PLUS,              1,   AR_VUP       },
  { APPLE_IR_MINUS,             1,   AR_VDOWN     },
  { -1,                        -1,   -1           }
};

int mp_input_appleir_init (char *dev)
{
  int i, fd;

  if (dev)
  {
    mp_msg (MSGT_INPUT, MSGL_V, MSGTR_INPUT_APPLE_IR_Init, dev);
    fd = open (dev, O_RDONLY | O_NONBLOCK);
    if (fd < 0)
    {
      mp_msg (MSGT_INPUT, MSGL_ERR,
              MSGTR_INPUT_APPLE_IR_CantOpen, strerror (errno));
      return -1;
    }

    return fd;
  }
  else
  {
    /* look for a valid AppleIR device on system */
    for (i = 0; i < EVDEV_MAX_EVENTS; i++)
    {
      struct input_id id;
      char file[64];

      sprintf (file, "/dev/input/event%d", i);
      fd = open (file, O_RDONLY | O_NONBLOCK);
      if (fd < 0)
        continue;

      ioctl (fd, EVIOCGID, &id);
      if (id.bustype == BUS_USB &&
          id.vendor  == USB_VENDOR_APPLE &&
          (id.product == USB_DEV_APPLE_IR ||id.product == USB_DEV_APPLE_IR_2))
      {
        mp_msg (MSGT_INPUT, MSGL_V, MSGTR_INPUT_APPLE_IR_Detect, file);
        return fd;
      }
      close (fd);
    }

    mp_msg (MSGT_INPUT, MSGL_ERR,
            MSGTR_INPUT_APPLE_IR_CantOpen, strerror (errno));
  }

  return -1;
}

int mp_input_appleir_read (int fd)
{
  struct input_event ev;
  int i, r;

  r = read (fd, &ev, sizeof (struct input_event));
  if (r <= 0 || r < sizeof (struct input_event))
    return MP_INPUT_NOTHING;

  /* check for key press only */
  if (ev.type != EV_KEY)
    return MP_INPUT_NOTHING;

  /* EvDev Key values:
   *  0: key release
   *  1: key press
   *  2: key auto-repeat
   */
  if (ev.value == 0)
    return MP_INPUT_NOTHING;

  /* find Linux evdev -> MPlayer keycode mapping */
  for (i = 0; apple_ir_mapping[i].linux_keycode != -1; i++)
    if (apple_ir_mapping[i].linux_keycode == ev.code &&
        apple_ir_mapping[i].value == ev.value)
      return apple_ir_mapping[i].mp_keycode;

  return MP_INPUT_NOTHING;
}
