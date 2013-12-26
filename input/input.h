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

#ifndef MPLAYER_INPUT_H
#define MPLAYER_INPUT_H

#include <stdbool.h>
#include "bstr/bstr.h"

#include "cmd_list.h"
#include "cmd_parse.h"

// Error codes for the drivers

// An error occurred but we can continue
#define MP_INPUT_ERROR -1
// A fatal error occurred, this driver should be removed
#define MP_INPUT_DEAD -2
// No input was available
#define MP_INPUT_NOTHING -3
//! Input will be available if you try again
#define MP_INPUT_RETRY -4
// Key FIFO was full - release events may be lost, zero button-down status
#define MP_INPUT_RELEASE_ALL -5

enum mp_cmd_flags {
    MP_ON_OSD_NO = 0,           // prefer not using OSD
    MP_ON_OSD_AUTO = 1,         // use default behavior of the specific command
    MP_ON_OSD_BAR = 2,          // force a bar, if applicable
    MP_ON_OSD_MSG = 4,          // force a message, if applicable
    MP_EXPAND_PROPERTIES = 8,   // expand strings as properties
    MP_PAUSING = 16,            // pause after running command
    MP_PAUSING_TOGGLE = 32,     // toggle pause after running command

    MP_ON_OSD_FLAGS = MP_ON_OSD_NO | MP_ON_OSD_AUTO |
                      MP_ON_OSD_BAR | MP_ON_OSD_MSG,
    MP_PAUSING_FLAGS = MP_PAUSING | MP_PAUSING_TOGGLE,
};

enum mp_input_section_flags {
    // If a key binding is not defined in the current section, do not search the
    // other sections for it (like the default section). Instead, an unbound
    // key warning will be printed.
    MP_INPUT_EXCLUSIVE = 1,
    // Let mp_input_test_dragging() return true, even if inside the mouse area.
    MP_INPUT_ALLOW_VO_DRAGGING = 2,
    // Don't force mouse pointer visible, even if inside the mouse area.
    MP_INPUT_ALLOW_HIDE_CURSOR = 4,
};

struct input_ctx;
struct mp_log;

struct mp_cmd_arg {
    const struct m_option *type;
    union {
        int i;
        float f;
        double d;
        char *s;
        void *p;
    } v;
};

typedef struct mp_cmd {
    int id;
    char *name;
    struct mp_cmd_arg args[MP_CMD_MAX_ARGS];
    int nargs;
    int flags; // mp_cmd_flags bitfield
    bstr original;
    char *input_section;
    bool key_up_follows;
    bool repeated;
    bool mouse_move;
    int mouse_x, mouse_y;
    struct mp_cmd *queue_next;
    double scale;               // for scaling numeric arguments
    const struct mp_cmd_def *def;
} mp_cmd_t;

/* Add a new command input source.
 * "fd" is a file descriptor (use -1 if you don't use any fd)
 * "select" tells whether to use select() on the fd to determine when to
 * try reading.
 * "read_cmd_func" is optional. It must return either text data or one of the
 * MP_INPUT error codes above. For return values >= 0, it behaves like UNIX
 * read() and returns the number of bytes copied to the dest buffer.
 * "read_key_func" is optional. It returns either key codes (ASCII, keycodes.h),
 * or MP_INPUT error codes.
 * "close_func" will be called when closing. Can be NULL. Its return value
 * is ignored (it's only there to allow using standard close() as the func).
 * "ctx" is for free use, and is passed to the callbacks.
 */
int mp_input_add_fd(struct input_ctx *ictx, int fd, int select,
                    int read_cmd_func(void *ctx, int fd, char *dest, int size),
                    int read_key_func(void *ctx, int fd),
                    int close_func(void *ctx, int fd), void *ctx);

/* Can be passed as read_func for above function in order to read() from the FD.
 */
int input_default_read_cmd(void *ctx, int fd, char *buf, int l);

// Process keyboard input. code is a key code from keycodes.h, possibly
// with modifiers applied. MP_INPUT_RELEASE_ALL is also a valid value.
void mp_input_put_key(struct input_ctx *ictx, int code);

// Like mp_input_put_key(), but process all UTF-8 characters in the given
// string as key events.
void mp_input_put_key_utf8(struct input_ctx *ictx, int mods, struct bstr t);

// Process scrolling input. Support for precise scrolling. Scales the given
// scroll amount add multiplies it with the command (seeking, sub-delay, etc)
void mp_input_put_axis(struct input_ctx *ictx, int direction, double value);

// Update mouse position (in window coordinates).
void mp_input_set_mouse_pos(struct input_ctx *ictx, int x, int y);

void mp_input_get_mouse_pos(struct input_ctx *ictx, int *x, int *y);

// As for the cmd one you usually don't need this function.
void mp_input_rm_key_fd(struct input_ctx *ictx, int fd);

// Add a command to the command queue.
int mp_input_queue_cmd(struct input_ctx *ictx, struct mp_cmd *cmd);

/* Return next available command, or sleep up to "time" ms if none is
 * available. If "peek_only" is true return a reference to the command
 * but leave it queued.
 */
struct mp_cmd *mp_input_get_cmd(struct input_ctx *ictx, int time,
                                int peek_only);

// Parse text and return corresponding struct mp_cmd.
// The location parameter is for error messages.
struct mp_cmd *mp_input_parse_cmd(struct input_ctx *ictx, bstr str,
                                  const char *location);

// Set current input section. The section is appended on top of the list of
// active sections, so its bindings are considered first. If the section was
// already active, it's moved to the top as well.
// name==NULL will behave as if name=="default"
// flags is a bitfield of enum mp_input_section_flags values
void mp_input_enable_section(struct input_ctx *ictx, char *name, int flags);

// Undo mp_input_enable_section().
// name==NULL will behave as if name=="default"
void mp_input_disable_section(struct input_ctx *ictx, char *name);

// Like mp_input_set_section(ictx, ..., 0) for all sections.
void mp_input_disable_all_sections(struct input_ctx *ictx);

// Set the contents of an input section.
//  name: name of the section, for mp_input_set_section() etc.
//  location: location string (like filename) for error reporting
//  contents: list of keybindings, like input.conf
//            a value of NULL deletes the section
//  builtin: create as builtin section; this means if the user defines bindings
//           using "{name}", they won't be ignored or overwritten - instead,
//           they are preferred to the bindings defined with this call
// If the section already exists, its bindings are removed and replaced.
void mp_input_define_section(struct input_ctx *ictx, char *name, char *location,
                             char *contents, bool builtin);

// Define where on the screen the named input section should receive.
// Setting a rectangle of size 0 unsets the mouse area.
// A rectangle with negative size disables mouse input for this section.
void mp_input_set_section_mouse_area(struct input_ctx *ictx, char *name,
                                     int x0, int y0, int x1, int y1);

// Used to detect mouse movement.
unsigned int mp_input_get_mouse_event_counter(struct input_ctx *ictx);

// Test whether there is any input section which wants to receive events.
// Note that the mouse event is always delivered, even if this returns false.
bool mp_input_test_mouse_active(struct input_ctx *ictx, int x, int y);

// Whether input.c wants mouse drag events at this mouse position. If this
// returns false, some VOs will initiate window dragging.
bool mp_input_test_dragging(struct input_ctx *ictx, int x, int y);

// Initialize the input system
struct mpv_global;
struct input_ctx *mp_input_init(struct mpv_global *global);

void mp_input_uninit(struct input_ctx *ictx);

// Wake up sleeping input loop from another thread.
void mp_input_wakeup(struct input_ctx *ictx);

// Interruptible usleep:  (used by demux)
int mp_input_check_interrupt(struct input_ctx *ictx, int time);

// If this returns true, use Right Alt key as Alt Gr to produce special
// characters. If false, count Right Alt as the modifier Alt key.
bool mp_input_use_alt_gr(struct input_ctx *ictx);

extern int async_quit_request;

#endif /* MPLAYER_INPUT_H */
