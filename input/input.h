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
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_INPUT_H
#define MPLAYER_INPUT_H

#include <stdbool.h>
#include "misc/bstr.h"

#include "cmd_list.h"
#include "cmd_parse.h"

// For mp_input_put_key(): release all keys that are down.
#define MP_INPUT_RELEASE_ALL -1

enum mp_cmd_flags {
    MP_ON_OSD_NO = 0,           // prefer not using OSD
    MP_ON_OSD_AUTO = 1,         // use default behavior of the specific command
    MP_ON_OSD_BAR = 2,          // force a bar, if applicable
    MP_ON_OSD_MSG = 4,          // force a message, if applicable
    MP_EXPAND_PROPERTIES = 8,   // expand strings as properties
    MP_ALLOW_REPEAT = 16,       // if used as keybinding, allow key repeat

    MP_ON_OSD_FLAGS = MP_ON_OSD_NO | MP_ON_OSD_AUTO |
                      MP_ON_OSD_BAR | MP_ON_OSD_MSG,
};

enum mp_input_section_flags {
    // If a key binding is not defined in the current section, do not search the
    // other sections for it (like the default section). Instead, an unbound
    // key warning will be printed.
    MP_INPUT_EXCLUSIVE = 1,
    // Prefer it to other sections.
    MP_INPUT_ON_TOP = 2,
    // Let mp_input_test_dragging() return true, even if inside the mouse area.
    MP_INPUT_ALLOW_VO_DRAGGING = 4,
    // Don't force mouse pointer visible, even if inside the mouse area.
    MP_INPUT_ALLOW_HIDE_CURSOR = 8,
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
        char **str_list;
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
    bool is_up_down : 1;
    bool is_up : 1;
    bool emit_on_up : 1;
    bool is_mouse_button : 1;
    bool repeated : 1;
    bool mouse_move : 1;
    int mouse_x, mouse_y;
    struct mp_cmd *queue_next;
    double scale;               // for scaling numeric arguments
    const struct mp_cmd_def *def;
    char *sender; // name of the client API user which sent this
} mp_cmd_t;

struct mp_input_src {
    struct mpv_global *global;
    struct mp_log *log;
    struct input_ctx *input_ctx;

    struct mp_input_src_internal *in;

    // If not-NULL: called before destroying the input_src. Should unblock the
    // reader loop, and make it exit. (Use with mp_input_add_thread_src().)
    void (*cancel)(struct mp_input_src *src);
    // Called after the reader thread returns, and cancel() won't be called
    // again. This should make sure that nothing after this call accesses src.
    void (*uninit)(struct mp_input_src *src);

    // For free use by the implementer.
    void *priv;
};

// Add a new input source. The input code can create a new thread, which feeds
// keys or commands to input_ctx. mp_input_src.uninit must be set.
// mp_input_src_kill() must not be called by anything after init.
struct mp_input_src *mp_input_add_src(struct input_ctx *ictx);

// Add an input source that runs on a thread. The source is automatically
// removed if the thread loop exits.
//  ctx: this is passed to loop_fn.
//  loop_fn: this is called once inside of a new thread, and should not return
//      until all input is read, or src->cancel is called by another thread.
//      You must call mp_input_src_init_done(src) early during init to signal
//      success (then src->cancel may be called at a later point); on failure,
//      return from loop_fn immediately.
// Returns >=0 on success, <0 on failure to allocate resources.
// Do not set src->cancel after mp_input_src_init_done() has been called.
int mp_input_add_thread_src(struct input_ctx *ictx, void *ctx,
    void (*loop_fn)(struct mp_input_src *src, void *ctx));

// Signal successful init.
// Must be called on the same thread as loop_fn (see mp_input_add_thread_src()).
// Set src->cancel and src->uninit (if needed) before calling this.
void mp_input_src_init_done(struct mp_input_src *src);

// Currently only with mp_input_add_thread_src().
int mp_input_src_get_wakeup_fd(struct mp_input_src *src);

// Remove and free the source. You can call this only while the input_ctx
// exists; otherwise there would be a race condition when another thread
// destroys input_ctx.
void mp_input_src_kill(struct mp_input_src *src);

// Feed text data, which will be split into lines of commands.
void mp_input_src_feed_cmd_text(struct mp_input_src *src, char *buf, size_t len);

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

// Return whether we want/accept mouse input.
bool mp_input_mouse_enabled(struct input_ctx *ictx);

bool mp_input_vo_keyboard_enabled(struct input_ctx *ictx);

/* Make mp_input_set_mouse_pos() mangle the mouse coordinates. Hack for certain
 * VOs. dst=NULL, src=NULL reset it. src can be NULL.
 */
struct mp_rect;
void mp_input_set_mouse_transform(struct input_ctx *ictx, struct mp_rect *dst,
                                  struct mp_rect *src);

// Add a command to the command queue.
int mp_input_queue_cmd(struct input_ctx *ictx, struct mp_cmd *cmd);

// Return next queued command, or NULL.
struct mp_cmd *mp_input_read_cmd(struct input_ctx *ictx);

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

// Load config, options, and devices.
void mp_input_load(struct input_ctx *ictx);

void mp_input_uninit(struct input_ctx *ictx);

// Sleep for the given amount of seconds, until mp_input_wakeup() is called,
// or new input arrives. seconds<=0 returns immediately.
void mp_input_wait(struct input_ctx *ictx, double seconds);

// Wake up sleeping input loop from another thread.
void mp_input_wakeup(struct input_ctx *ictx);

void mp_input_wakeup_nolock(struct input_ctx *ictx);

// Used to asynchronously abort playback. Needed because the core still can
// block on network in some situations.
struct mp_cancel;
void mp_input_set_cancel(struct input_ctx *ictx, struct mp_cancel *cancel);

// If this returns true, use Right Alt key as Alt Gr to produce special
// characters. If false, count Right Alt as the modifier Alt key.
bool mp_input_use_alt_gr(struct input_ctx *ictx);

// Like mp_input_parse_cmd_strv, but also run the command.
void mp_input_run_cmd(struct input_ctx *ictx, const char **cmd);

void mp_input_set_repeat_info(struct input_ctx *ictx, int rate, int delay);

void mp_input_pipe_add(struct input_ctx *ictx, const char *filename);

struct mp_ipc_ctx;
struct mp_client_api;
struct mp_ipc_ctx *mp_init_ipc(struct mp_client_api *client_api,
                               struct mpv_global *global);
void mp_uninit_ipc(struct mp_ipc_ctx *ctx);

#endif /* MPLAYER_INPUT_H */
