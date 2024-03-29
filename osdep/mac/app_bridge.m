/*
 * Cocoa Application Event Handling
 *
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

#include "config.h"

#include "osdep/mac/app_bridge.h"
#if HAVE_SWIFT
#include "osdep/mac/swift.h"
#endif

void cocoa_init_media_keys(void)
{
    [[AppHub shared] startRemote];
}

void cocoa_uninit_media_keys(void)
{
    [[AppHub shared] stopRemote];
}

void cocoa_set_input_context(struct input_ctx *input_context)
{
    [[AppHub shared] initInput:input_context];
}

void cocoa_set_mpv_handle(struct mpv_handle *ctx)
{
    [[AppHub shared] initMpv:ctx];
}

void cocoa_init_cocoa_cb(void)
{
    [[AppHub shared] initCocoaCb];
}
