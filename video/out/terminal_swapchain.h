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

#pragma once

#include "misc/bstr.h"

struct vo;
struct terminal_swapchain;

// Writes what terminal VOs present to the terminal from its own thread, in
// order, with up to --swapchain-depth frames in flight, so writing a frame
// overlaps with rendering the next one. A VO acquires a buffer, builds the
// frame in it and presents it. Everything a VO writes to the terminal goes
// through it, so frames and the sequences around them stay in order.
struct terminal_swapchain *terminal_swapchain_create(struct vo *vo);

// Returns the buffer to build the next frame in, emptied. Blocks while
// --swapchain-depth frames are in flight. Only one buffer can be acquired
// at a time, and it must be presented before the next one is acquired.
bstr *terminal_swapchain_acquire(struct terminal_swapchain *sc);

// Queues the acquired buffer for writing.
void terminal_swapchain_present(struct terminal_swapchain *sc, bstr *frame);

// The buffer whose contents go out at the start of the next acquired buffer,
// for output that belongs with the next frame rather than in a write of its
// own, such as clearing the screen after a reconfig.
bstr *terminal_swapchain_next(struct terminal_swapchain *sc);

// Blocks until everything queued so far has been written.
void terminal_swapchain_wait(struct terminal_swapchain *sc);

// For VOCTRL_CHECK_EVENTS, check if any event is pending.
void terminal_swapchain_check_events(struct terminal_swapchain *sc);

// Whether the terminal size has changed since the last call.
bool terminal_swapchain_size_changed(struct terminal_swapchain *sc);

// Writes what is queued, stops the thread and frees the swapchain.
void terminal_swapchain_destroy(struct terminal_swapchain *sc);
