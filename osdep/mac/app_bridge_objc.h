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

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include "player/client.h"
#include "video/out/libmpv.h"
#include "libmpv/render_gl.h"

#include "options/m_config.h"
#include "player/core.h"
#include "input/input.h"
#include "input/event.h"
#include "input/keycodes.h"
#include "video/out/win_state.h"

#include "osdep/main-fn.h"
#include "osdep/mac/app_bridge.h"

// complex macros won't get imported to swift so we have to reassign them
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

static int SWIFT_KEY_MOUSE_LEAVE = MP_KEY_MOUSE_LEAVE;
static int SWIFT_KEY_MOUSE_ENTER = MP_KEY_MOUSE_ENTER;

static const char *swift_mpv_version = mpv_version;
static const char *swift_mpv_copyright = mpv_copyright;

NSData *app_bridge_icon(void);
void app_bridge_tarray_append(void *t, char ***a, int *i, char *s);
const struct m_sub_options *app_bridge_mac_conf(void);
const struct m_sub_options *app_bridge_vo_conf(void);
