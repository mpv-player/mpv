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

#include "options/options.h"
#include "input/input.h"

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

// Initialization that comes after mpv's initialization.
// Note that inputCtx needs to remain valid up until terminate_cocoa_application()
// is called, but the reference to *opts is not kept.
void init_cocoa_application(const struct MPOpts *opts, struct input_ctx *inputCtx);

// Ends the application (if its running)
// * App will no longer use the inputCtx provided with init_cocoa_application()
// * mpv should exit via cocoa_exit soon afterwards.
void terminate_cocoa_application(void);

// mpv should use this instead of exit() when running wrapped by cocoa_main()
void MP_NORETURN cocoa_exit(int);

void cocoa_register_menu_item_action(MPMenuKey key, void* action);

#endif /* MPV_MACOSX_APPLICATION */
