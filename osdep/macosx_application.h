/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPV_MACOSX_APPLICATION
#define MPV_MACOSX_APPLICATION

struct input_ctx;
struct mp_fifo;

typedef int (*mpv_main_fn)(int, char**);

// Menu Keys identifing menu items
typedef enum {
    MPM_H_SIZE,
    MPM_N_SIZE,
    MPM_D_SIZE,
    MPM_MINIMIZE,
    MPM_ZOOM,
} MPMenuKey;

// multithreaded wrapper for mpv_main
int cocoa_main(mpv_main_fn mpv_main, int argc, char *argv[]);

void cocoa_register_menu_item_action(MPMenuKey key, void* action);

// initializes Cocoa application
void init_cocoa_application(void);
void terminate_cocoa_application(void);
void cocoa_autorelease_pool_alloc(void);
void cocoa_autorelease_pool_drain(void);

// Runs the Cocoa Main Event Loop
void cocoa_run_runloop(void);
void cocoa_stop_runloop(void);
void cocoa_post_fake_event(void);

void cocoa_set_state(struct input_ctx *input_context, struct mp_fifo *key_fifo);

void macosx_finder_args_preinit(int *argc, char ***argv);

#endif /* MPV_MACOSX_APPLICATION */
