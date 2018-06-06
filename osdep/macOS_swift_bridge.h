/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

// including IOKit here again doesn't make sense, but otherwise the swift
// compiler doesn't include the needed header in our generated header file
#import <IOKit/pwr_mgt/IOPMLib.h>

#include "player/client.h"
#include "video/out/libmpv.h"
#include "libmpv/render_gl.h"

#include "options/m_config.h"
#include "player/core.h"
#include "input/input.h"
#include "video/out/win_state.h"

#include "osdep/macosx_application_objc.h"
#include "osdep/macosx_events_objc.h"


// complex macros won't get imported to Swift so we have to reassign them
static int SWIFT_MBTN_LEFT       = MP_MBTN_LEFT;
static int SWIFT_MBTN_MID        = MP_MBTN_MID;
static int SWIFT_MBTN_RIGHT      = MP_MBTN_RIGHT;
static int SWIFT_WHEEL_UP        = MP_WHEEL_UP;
static int SWIFT_WHEEL_DOWN      = MP_WHEEL_DOWN;
static int SWIFT_WHEEL_LEFT      = MP_WHEEL_LEFT;
static int SWIFT_WHEEL_RIGHT     = MP_WHEEL_RIGHT;
static int SWIFT_MBTN_BACK       = MP_MBTN_BACK;
static int SWIFT_MBTN_FORWARD    = MP_MBTN_FORWARD;
static int SWIFT_MBTN9           = MP_MBTN9;

static int SWIFT_KEY_CLOSE_WIN   = MP_KEY_CLOSE_WIN;
static int SWIFT_KEY_MOUSE_LEAVE = MP_KEY_MOUSE_LEAVE;
static int SWIFT_KEY_MOUSE_ENTER = MP_KEY_MOUSE_ENTER;
static int SWIFT_KEY_STATE_DOWN  = MP_KEY_STATE_DOWN;
static int SWIFT_KEY_STATE_UP    = MP_KEY_STATE_UP;
