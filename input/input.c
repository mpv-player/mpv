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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include <mpv/client.h>

#include "osdep/io.h"
#include "misc/rendezvous.h"

#include "input.h"
#include "keycodes.h"
#include "osdep/threads.h"
#include "osdep/timer.h"
#include "common/msg.h"
#include "common/global.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "options/path.h"
#include "mpv_talloc.h"
#include "options/options.h"
#include "misc/bstr.h"
#include "misc/node.h"
#include "stream/stream.h"
#include "common/common.h"

#if HAVE_COCOA
#include "osdep/mac/app_bridge.h"
#endif

#define input_lock(ictx)    mp_mutex_lock(&ictx->mutex)
#define input_unlock(ictx)  mp_mutex_unlock(&ictx->mutex)

#define MP_MAX_KEY_DOWN 16

struct cmd_bind {
    int keys[MP_MAX_KEY_DOWN];
    int num_keys;
    char *cmd;
    char *location;     // filename/line number of definition
    char *desc;         // human readable description
    bool is_builtin;
    struct cmd_bind_section *owner;
};

struct cmd_bind_section {
    char *owner;
    struct cmd_bind *binds;
    int num_binds;
    bstr section;
    struct mp_rect mouse_area;  // set at runtime, if at all
    bool mouse_area_set;        // mouse_area is valid and should be tested
};

#define MP_MAX_SOURCES 10

struct active_section {
    bstr name;
    int flags;
};

struct cmd_queue {
    struct mp_cmd *first;
};

struct wheel_state {
    double dead_zone_accum;
    double unit_accum;
};

struct touch_point {
    int id;
    int x, y;
};

struct input_ctx {
    mp_mutex mutex;
    struct mp_log *log;
    struct mpv_global *global;
    struct m_config_cache *opts_cache;
    struct input_opts *opts;

    bool using_ar;
    bool using_cocoa_media_keys;

    // Autorepeat stuff
    short ar_state;
    int64_t last_ar;

    // history of key downs - the newest is in position 0
    int key_history[MP_MAX_KEY_DOWN];
    // key code of the last key that triggered MP_KEY_STATE_DOWN
    int last_key_down;
    int64_t last_key_down_time;
    struct mp_cmd *current_down_cmd;

    int last_doubleclick_key_down;
    double last_doubleclick_time;

    // VO dragging state
    bool dragging_button_down;
    int mouse_drag_x, mouse_drag_y;
    // Raw mouse position before transform
    int mouse_raw_x, mouse_raw_y;

    // Mouse position on the consumer side (as command.c sees it)
    int mouse_x, mouse_y;
    int mouse_hover;  // updated on mouse-enter/leave
    bstr mouse_section; // last section to receive mouse event

    // Mouse position on the producer side (as the VO sees it)
    // Unlike mouse_x/y, this can be used to resolve mouse click bindings.
    int mouse_vo_x, mouse_vo_y;

    bool mouse_mangle, mouse_src_mangle;
    struct mp_rect mouse_src, mouse_dst;

    // Wheel state (MP_WHEEL_*)
    struct wheel_state wheel_state_y; // MP_WHEEL_UP/MP_WHEEL_DOWN
    struct wheel_state wheel_state_x; // MP_WHEEL_LEFT/MP_WHEEL_RIGHT
    struct wheel_state *wheel_current; // The direction currently being scrolled
    double last_wheel_time; // mp_time_sec() of the last wheel event

    // List of command binding sections
    struct cmd_bind_section **sections;
    int num_sections;

    // List currently active command sections
    struct active_section *active_sections;
    int num_active_sections;

    // List currently active touch points
    struct touch_point *touch_points;
    int num_touch_points;

    unsigned int mouse_event_counter;

    struct mp_input_src *sources[MP_MAX_SOURCES];
    int num_sources;

    struct cmd_queue cmd_queue;

    void (*wakeup_cb)(void *ctx);
    void *wakeup_ctx;
};

static int parse_config(struct input_ctx *ictx, bool builtin, bstr data,
                        const char *location, const bstr restrict_section);
static void close_input_sources(struct input_ctx *ictx);
static bool test_mouse(struct input_ctx *ictx, int x, int y, int rej_flags);

#define OPT_BASE_STRUCT struct input_opts
struct input_opts {
    char *config_file;
    int doubleclick_time;
    // Maximum number of queued commands from keypresses (limit to avoid
    // repeated slow commands piling up)
    int key_fifo_size;
    // Autorepeat config (be aware of mp_input_set_repeat_info())
    int ar_delay;
    int ar_rate;
    int dragging_deadzone;
    bool use_alt_gr;
    bool use_gamepad;
    bool use_media_keys;
    bool default_bindings;
    bool builtin_bindings;
    bool builtin_dragging;
    bool enable_mouse_movements;
    bool vo_key_input;
    bool test;
    bool allow_win_drag;
    bool preprocess_wheel;
    bool touch_emulate_mouse;
};

const struct m_sub_options input_config = {
    .opts = (const m_option_t[]) {
        {"input-conf", OPT_STRING(config_file), .flags = M_OPT_FILE},
        {"input-ar-delay", OPT_INT(ar_delay)},
        {"input-ar-rate", OPT_INT(ar_rate)},
        {"input-keylist", OPT_PRINT(mp_print_key_list)},
        {"input-cmdlist", OPT_PRINT(mp_print_cmd_list)},
        {"input-default-bindings", OPT_BOOL(default_bindings)},
        {"input-builtin-bindings", OPT_BOOL(builtin_bindings)},
        {"input-builtin-dragging", OPT_BOOL(builtin_dragging)},
        {"input-test", OPT_BOOL(test)},
        {"input-doubleclick-time", OPT_INT(doubleclick_time),
         M_RANGE(0, 1000)},
        {"input-right-alt-gr", OPT_BOOL(use_alt_gr)},
        {"input-key-fifo-size", OPT_INT(key_fifo_size), M_RANGE(2, 65000)},
        {"input-cursor", OPT_BOOL(enable_mouse_movements)},
        {"input-vo-keyboard", OPT_BOOL(vo_key_input)},
        {"input-media-keys", OPT_BOOL(use_media_keys)},
        {"input-preprocess-wheel", OPT_BOOL(preprocess_wheel)},
        {"input-touch-emulate-mouse", OPT_BOOL(touch_emulate_mouse)},
        {"input-dragging-deadzone", OPT_INT(dragging_deadzone)},
#if HAVE_SDL2_GAMEPAD
        {"input-gamepad", OPT_BOOL(use_gamepad)},
#endif
        {"window-dragging", OPT_BOOL(allow_win_drag)},
        {0}
    },
    .size = sizeof(struct input_opts),
    .defaults = &(const struct input_opts){
        .key_fifo_size = 7,
        .doubleclick_time = 300,
        .ar_delay = 200,
        .ar_rate = 40,
        .dragging_deadzone = 3,
        .use_alt_gr = true,
        .enable_mouse_movements = true,
        .use_media_keys = true,
        .default_bindings = true,
        .builtin_bindings = true,
        .builtin_dragging = true,
        .vo_key_input = true,
        .allow_win_drag = true,
        .preprocess_wheel = true,
        .touch_emulate_mouse = true,
    },
    .change_flags = UPDATE_INPUT,
};

static const char builtin_input_conf[] =
#include "etc/input.conf.inc"
;

static bool test_rect(struct mp_rect *rc, int x, int y)
{
    return x >= rc->x0 && y >= rc->y0 && x < rc->x1 && y < rc->y1;
}

static int queue_count_cmds(struct cmd_queue *queue)
{
    int res = 0;
    for (struct mp_cmd *cmd = queue->first; cmd; cmd = cmd->queue_next)
        res++;
    return res;
}

static void queue_remove(struct cmd_queue *queue, struct mp_cmd *cmd)
{
    struct mp_cmd **p_prev = &queue->first;
    while (*p_prev != cmd) {
        p_prev = &(*p_prev)->queue_next;
    }
    // if this fails, cmd was not in the queue
    mp_assert(*p_prev == cmd);
    *p_prev = cmd->queue_next;
}

static struct mp_cmd *queue_remove_head(struct cmd_queue *queue)
{
    struct mp_cmd *ret = queue->first;
    if (ret)
        queue_remove(queue, ret);
    return ret;
}

static void queue_add_tail(struct cmd_queue *queue, struct mp_cmd *cmd)
{
    struct mp_cmd **p_prev = &queue->first;
    while (*p_prev)
        p_prev = &(*p_prev)->queue_next;
    *p_prev = cmd;
    cmd->queue_next = NULL;
}

static struct mp_cmd *queue_peek_tail(struct cmd_queue *queue)
{
    struct mp_cmd *cur = queue->first;
    while (cur && cur->queue_next)
        cur = cur->queue_next;
    return cur;
}

static void queue_cmd(struct input_ctx *ictx, mp_cmd_t *cmd)
{
    if (!cmd)
        return;
    queue_add_tail(&ictx->cmd_queue, cmd);
    mp_input_wakeup(ictx);
}

static void append_bind_info(struct input_ctx *ictx, char **pmsg,
                             struct cmd_bind *bind)
{
    char *msg = *pmsg;
    struct mp_cmd *cmd = mp_input_parse_cmd(ictx, bstr0(bind->cmd),
                                            bind->location);
    char *stripped = cmd ? cmd->original : bind->cmd;
    msg = talloc_asprintf_append(msg, " '%s'", stripped);
    if (!cmd)
        msg = talloc_asprintf_append(msg, " (invalid)");
    if (!bstr_equals0(bind->owner->section, "default"))
        msg = talloc_asprintf_append(msg, " in section {%.*s}",
                                     BSTR_P(bind->owner->section));
    msg = talloc_asprintf_append(msg, " in %s", bind->location);
    if (bind->is_builtin)
        msg = talloc_asprintf_append(msg, " (default)");
    talloc_free(cmd);
    *pmsg = msg;
}

static mp_cmd_t *handle_test(struct input_ctx *ictx, int code)
{
    if (code == MP_KEY_CLOSE_WIN) {
        MP_WARN(ictx,
            "CLOSE_WIN was received. This pseudo key can be remapped too,\n"
            "but --input-test will always quit when receiving it.\n");
        const char *args[] = {"quit", NULL};
        mp_cmd_t *res = mp_input_parse_cmd_strv(ictx->log, args);
        return res;
    }

    char *key_buf = mp_input_get_key_combo_name(&code, 1);
    char *msg = talloc_asprintf(NULL, "Key %s is bound to:\n", key_buf);
    talloc_free(key_buf);

    int count = 0;
    for (int n = 0; n < ictx->num_sections; n++) {
        struct cmd_bind_section *bs = ictx->sections[n];

        for (int i = 0; i < bs->num_binds; i++) {
            if (bs->binds[i].num_keys && bs->binds[i].keys[0] == code) {
                count++;
                if (count > 1)
                    msg = talloc_asprintf_append(msg, "\n");
                msg = talloc_asprintf_append(msg, "%d. ", count);
                append_bind_info(ictx, &msg, &bs->binds[i]);
            }
        }
    }

    if (!count)
        msg = talloc_asprintf_append(msg, "(nothing)");

    MP_INFO(ictx, "%s\n", msg);
    const char *args[] = {"show-text", msg, NULL};
    mp_cmd_t *res = mp_input_parse_cmd_strv(ictx->log, args);
    talloc_free(msg);
    return res;
}

static struct cmd_bind_section *find_section(struct input_ctx *ictx,
                                             bstr section)
{
    for (int n = 0; n < ictx->num_sections; n++) {
        struct cmd_bind_section *bs = ictx->sections[n];
        if (bstr_equals(section, bs->section))
            return bs;
    }
    return NULL;
}

static struct cmd_bind_section *get_bind_section(struct input_ctx *ictx,
                                                 bstr section)
{
    if (section.len == 0)
        section = bstr0("default");
    struct cmd_bind_section *bind_section = find_section(ictx, section);
    if (bind_section)
        return bind_section;
    bind_section = talloc_ptrtype(ictx, bind_section);
    *bind_section = (struct cmd_bind_section) {
        .section = bstrdup(bind_section, section),
        .mouse_area = {INT_MIN, INT_MIN, INT_MAX, INT_MAX},
        .mouse_area_set = true,
    };
    MP_TARRAY_APPEND(ictx, ictx->sections, ictx->num_sections, bind_section);
    return bind_section;
}

static void key_buf_add(int *buf, int code)
{
    for (int n = MP_MAX_KEY_DOWN - 1; n > 0; n--)
        buf[n] = buf[n - 1];
    buf[0] = code;
}

static struct cmd_bind *find_bind_for_key_section(struct input_ctx *ictx,
                                                  bstr section, int code)
{
    struct cmd_bind_section *bs = get_bind_section(ictx, section);

    if (!bs->num_binds)
        return NULL;

    int keys[MP_MAX_KEY_DOWN];
    memcpy(keys, ictx->key_history, sizeof(keys));
    key_buf_add(keys, code);

    struct cmd_bind *best = NULL;

    // Prefer user-defined keys over builtin bindings
    for (int builtin = 0; builtin < 2; builtin++) {
        if (builtin && !ictx->opts->default_bindings)
            break;
        for (int n = 0; n < bs->num_binds; n++) {
            if (bs->binds[n].is_builtin == (bool)builtin) {
                struct cmd_bind *b = &bs->binds[n];
                // we have: keys=[key2 key1 keyX ...]
                // and: b->keys=[key1 key2] (and may be just a prefix)
                for (int i = 0; i < b->num_keys; i++) {
                    if (b->keys[i] != keys[b->num_keys - 1 - i])
                        goto skip;
                }
                if (!best || b->num_keys > best->num_keys)
                    best = b;
            skip: ;
            }
        }
    }
    return best;
}

static struct cmd_bind *find_any_bind_for_key(struct input_ctx *ictx,
                                              bstr force_section, int code)
{
    if (force_section.len)
        return find_bind_for_key_section(ictx, force_section, code);

    bool use_mouse = MP_KEY_DEPENDS_ON_MOUSE_POS(code);

    // First look whether a mouse section is capturing all mouse input
    // exclusively (regardless of the active section stack order).
    if (use_mouse && MP_KEY_IS_MOUSE_BTN_SINGLE(ictx->last_key_down) &&
        !MP_KEY_IS_MOUSE_BTN_DBL(code))
    {
        struct cmd_bind *bind =
            find_bind_for_key_section(ictx, ictx->mouse_section, code);
        if (bind)
            return bind;
    }

    struct cmd_bind *best_bind = NULL;
    for (int i = ictx->num_active_sections - 1; i >= 0; i--) {
        struct active_section *s = &ictx->active_sections[i];
        struct cmd_bind *bind = find_bind_for_key_section(ictx, s->name, code);
        if (bind) {
            struct cmd_bind_section *bs = bind->owner;
            if (!use_mouse || (bs->mouse_area_set && test_rect(&bs->mouse_area,
                                                               ictx->mouse_vo_x,
                                                               ictx->mouse_vo_y)))
            {
                if (!best_bind || bind->num_keys > best_bind->num_keys ||
                    (best_bind->is_builtin && !bind->is_builtin &&
                     bind->num_keys == best_bind->num_keys))
                {
                    best_bind = bind;
                }
            }
        }
        if (s->flags & MP_INPUT_EXCLUSIVE)
            break;
        if (best_bind && (s->flags & MP_INPUT_ON_TOP))
            break;
    }

    return best_bind;
}

static mp_cmd_t *get_cmd_from_keys(struct input_ctx *ictx, bstr force_section,
                                   int code)
{
    if (ictx->opts->test)
        return handle_test(ictx, code);

    struct cmd_bind *cmd = NULL;

    if (MP_KEY_IS_UNICODE(code))
        cmd = find_any_bind_for_key(ictx, force_section, MP_KEY_ANY_UNICODE);
    if (!cmd)
        cmd = find_any_bind_for_key(ictx, force_section, code);
    if (!cmd)
        cmd = find_any_bind_for_key(ictx, force_section, MP_KEY_UNMAPPED);
    if (!cmd) {
        if (code == MP_KEY_CLOSE_WIN)
            return mp_input_parse_cmd_strv(ictx->log, (const char*[]){"quit", 0});
        int msgl = MSGL_WARN;
        if (MP_KEY_IS_MOUSE_MOVE(code))
            msgl = MSGL_TRACE;
        char *key_buf = mp_input_get_key_combo_name(&code, 1);
        MP_MSG(ictx, msgl, "No key binding found for key '%s'.\n", key_buf);
        talloc_free(key_buf);
        return NULL;
    }
    mp_cmd_t *ret = mp_input_parse_cmd(ictx, bstr0(cmd->cmd), cmd->location);
    if (ret) {
        ret->input_section = cmd->owner->section;
        ret->key_name = talloc_steal(ret, mp_input_get_key_combo_name(&code, 1));
        MP_TRACE(ictx, "key '%s' -> '%s' in '%.*s'\n",
                 ret->key_name, cmd->cmd, BSTR_P(ret->input_section));
        if (MP_KEY_IS_UNICODE(code)) {
            bstr text = {0};
            mp_append_utf8_bstr(ret, &text, code);
            ret->key_text = text.start;
        }
        ret->is_mouse_button = code & MP_KEY_EMIT_ON_UP;
    } else {
        char *key_buf = mp_input_get_key_combo_name(&code, 1);
        MP_ERR(ictx, "Invalid command for key binding '%s': '%s'\n",
               key_buf, cmd->cmd);
        talloc_free(key_buf);
    }
    return ret;
}

static void update_mouse_section(struct input_ctx *ictx)
{
    struct cmd_bind *bind =
        find_any_bind_for_key(ictx, (bstr){0}, MP_KEY_MOUSE_MOVE);

    bstr new_section = bind ? bind->owner->section : bstr0("default");

    bstr old = ictx->mouse_section;
    ictx->mouse_section = new_section;

    if (!bstr_equals(old, ictx->mouse_section)) {
        MP_TRACE(ictx, "input: switch section %.*s -> %.*s\n",
                 BSTR_P(old), BSTR_P(ictx->mouse_section));
        queue_cmd(ictx, get_cmd_from_keys(ictx, old, MP_KEY_MOUSE_LEAVE));
    }
}

// Called when the currently held-down key is released. This (usually) sends
// the key-up version of the command associated with the keys that were held
// down.
// If the drop_current parameter is set to true, then don't send the key-up
// command. Unless we've already sent a key-down event, in which case the
// input receiver (the player) must get a key-up event, or it would get stuck
// thinking a key is still held down. In this case, mark the command as
// canceled so that it can be distinguished from a normally triggered command.
static void release_down_cmd(struct input_ctx *ictx, bool drop_current)
{
    if (ictx->current_down_cmd && ictx->current_down_cmd->emit_on_up &&
        (!drop_current || ictx->current_down_cmd->def->on_updown))
    {
        memset(ictx->key_history, 0, sizeof(ictx->key_history));
        ictx->current_down_cmd->is_up = true;
        if (drop_current)
            ictx->current_down_cmd->canceled = true;
        queue_cmd(ictx, ictx->current_down_cmd);
    } else {
        talloc_free(ictx->current_down_cmd);
    }
    ictx->current_down_cmd = NULL;
    ictx->last_key_down = 0;
    ictx->last_key_down_time = 0;
    ictx->ar_state = -1;
    update_mouse_section(ictx);
}

// We don't want it to append to the command queue indefinitely, because that
// could lead to situations where recovery would take too long.
static bool should_drop_cmd(struct input_ctx *ictx, struct mp_cmd *cmd)
{
    struct cmd_queue *queue = &ictx->cmd_queue;
    return queue_count_cmds(queue) >= ictx->opts->key_fifo_size;
}

static struct mp_cmd *resolve_key(struct input_ctx *ictx, int code)
{
    update_mouse_section(ictx);
    struct mp_cmd *cmd = get_cmd_from_keys(ictx, (bstr){0}, code);
    key_buf_add(ictx->key_history, code);
    if (cmd && !cmd->def->is_ignore && !should_drop_cmd(ictx, cmd))
        return cmd;
    talloc_free(cmd);
    return NULL;
}

static void interpret_key(struct input_ctx *ictx, int code, double scale,
                          int scale_units)
{
    int state = code & (MP_KEY_STATE_DOWN | MP_KEY_STATE_UP);
    bool no_emit = code & MP_KEY_STATE_SET_ONLY;
    code = code & ~(unsigned)(state | MP_KEY_STATE_SET_ONLY);

    if (mp_msg_test(ictx->log, MSGL_TRACE)) {
        char *key = mp_input_get_key_name(code);
        MP_TRACE(ictx, "key code=%#x '%s'%s%s\n",
                 code, key, (state & MP_KEY_STATE_DOWN) ? " down" : "",
                 (state & MP_KEY_STATE_UP) ? " up" : "");
        talloc_free(key);
    }

    if (MP_KEY_DEPENDS_ON_MOUSE_POS(code & ~MP_KEY_MODIFIER_MASK)) {
        ictx->mouse_event_counter++;
        mp_input_wakeup(ictx);
    }

    struct mp_cmd *cmd = NULL;

    if (state == MP_KEY_STATE_DOWN) {
        // Protect against VOs which send STATE_DOWN with autorepeat
        if (ictx->last_key_down == code)
            return;
        // Cancel current down-event (there can be only one)
        release_down_cmd(ictx, true);
        cmd = resolve_key(ictx, code);
        if (cmd) {
            cmd->is_up_down = true;
            cmd->emit_on_up = (code & MP_KEY_EMIT_ON_UP) || cmd->def->on_updown;
            ictx->current_down_cmd = mp_cmd_clone(cmd);
        }
        ictx->last_key_down = code;
        ictx->last_key_down_time = mp_time_ns();
        ictx->ar_state = 0;
        mp_input_wakeup(ictx); // possibly start timer for autorepeat
    } else if (state == MP_KEY_STATE_UP) {
        // Most VOs send RELEASE_ALL anyway
        release_down_cmd(ictx, false);
    } else {
        // Press of key with no separate down/up events
        // Mixing press events and up/down with the same key is not supported,
        // and input sources shouldn't do this, but can happen anyway if
        // multiple input sources interfere with each other.
        if (ictx->last_key_down == code)
            release_down_cmd(ictx, false);
        cmd = resolve_key(ictx, code);
    }

    if (!cmd)
        return;

    // Don't emit a command on key-down if the key is designed to emit commands
    // on key-up (like mouse buttons), or setting key state only without emitting commands.
    // Also, if the command specifically should be sent both on key down and key up,
    // still emit the command.
    if ((cmd->emit_on_up && !cmd->def->on_updown) || no_emit) {
        talloc_free(cmd);
        return;
    }

    memset(ictx->key_history, 0, sizeof(ictx->key_history));

    if (mp_input_is_scalable_cmd(cmd)) {
        cmd->scale = scale;
        cmd->scale_units = scale_units;
        queue_cmd(ictx, cmd);
    } else {
        // Non-scalable commands won't understand cmd->scale, so synthesize
        // multiple commands with cmd->scale = 1
        cmd->scale = 1;
        cmd->scale_units = 1;
        // Avoid spamming the player with too many commands
        scale_units = MPMIN(scale_units, 20);
        for (int i = 0; i < scale_units - 1; i++)
            queue_cmd(ictx, mp_cmd_clone(cmd));
        if (scale_units) {
            queue_cmd(ictx, cmd);
        } else {
            talloc_free(cmd);
        }
    }
}

// Pre-processing for MP_WHEEL_* events. If this returns false, the caller
// should discard the event.
static bool process_wheel(struct input_ctx *ictx, int code, double *scale,
                          int *scale_units)
{
    // Size of the deadzone in scroll units. The user must scroll at least this
    // much in any direction before their scroll is registered.
    static const double DEADZONE_DIST = 0.125;
    // The deadzone accumulator is reset if no scrolls happened in this many
    // seconds, e.g. the user is assumed to have finished scrolling.
    static const double DEADZONE_SCROLL_TIME = 0.2;
    // The scale_units accumulator is reset if no scrolls happened in this many
    // seconds. This value should be fairly large, so commands will still be
    // sent when the user scrolls slowly.
    static const double UNIT_SCROLL_TIME = 0.5;

    // Determine which direction is being scrolled
    double dir;
    struct wheel_state *state;
    switch (code) {
    case MP_WHEEL_UP:    dir = -1; state = &ictx->wheel_state_y; break;
    case MP_WHEEL_DOWN:  dir = +1; state = &ictx->wheel_state_y; break;
    case MP_WHEEL_LEFT:  dir = -1; state = &ictx->wheel_state_x; break;
    case MP_WHEEL_RIGHT: dir = +1; state = &ictx->wheel_state_x; break;
    default:
        return true;
    }

    // Reset accumulators if it's determined that the user finished scrolling
    double now = mp_time_sec();
    if (now > ictx->last_wheel_time + DEADZONE_SCROLL_TIME) {
        ictx->wheel_current = NULL;
        ictx->wheel_state_y.dead_zone_accum = 0;
        ictx->wheel_state_x.dead_zone_accum = 0;
    }
    if (now > ictx->last_wheel_time + UNIT_SCROLL_TIME) {
        ictx->wheel_state_y.unit_accum = 0;
        ictx->wheel_state_x.unit_accum = 0;
    }
    ictx->last_wheel_time = now;

    // Process wheel deadzone. A lot of touchpad drivers don't filter scroll
    // input, which makes it difficult for the user to send WHEEL_UP/DOWN
    // without accidentally triggering WHEEL_LEFT/RIGHT. We try to fix this by
    // implementing a deadzone. When the value of either direction breaks out
    // of the deadzone, events from the other direction will be ignored until
    // the user finishes scrolling.
    if (ictx->wheel_current == NULL) {
        state->dead_zone_accum += *scale * dir;
        if (state->dead_zone_accum * dir > DEADZONE_DIST) {
            ictx->wheel_current = state;
            *scale = state->dead_zone_accum * dir;
        }
    }
    if (ictx->wheel_current != state)
        return false;

    // Determine scale_units. This is incremented every time the accumulated
    // scale value crosses 1.0. Non-scalable input commands will be ran that
    // many times.
    state->unit_accum += *scale * dir;
    *scale_units = trunc(state->unit_accum * dir);
    state->unit_accum -= *scale_units * dir;
    return true;
}

static void feed_key(struct input_ctx *ictx, int code, double scale,
                              bool force_mouse)
{
    struct input_opts *opts = ictx->opts;

    code = mp_normalize_keycode(code);
    int unmod = code & ~MP_KEY_MODIFIER_MASK;
    if (code == MP_INPUT_RELEASE_ALL) {
        MP_TRACE(ictx, "release all\n");
        release_down_cmd(ictx, false);
        ictx->dragging_button_down = false;
        return;
    }
    if (code == MP_TOUCH_RELEASE_ALL) {
        MP_TRACE(ictx, "release all touch\n");
        ictx->num_touch_points = 0;
        return;
    }
    if (!opts->enable_mouse_movements && MP_KEY_IS_MOUSE(unmod) && !force_mouse)
        return;
    if (unmod == MP_KEY_MOUSE_LEAVE || unmod == MP_KEY_MOUSE_ENTER) {
        ictx->mouse_hover = unmod == MP_KEY_MOUSE_ENTER;
        update_mouse_section(ictx);

        mp_cmd_t *cmd = get_cmd_from_keys(ictx, (bstr){0}, code);
        if (!cmd)  // queue dummy cmd so that mouse-pos can notify observers
            cmd = mp_input_parse_cmd(ictx, bstr0("ignore"), "<internal>");
        queue_cmd(ictx, cmd);
        return;
    }
    double now = mp_time_sec();
    // ignore system double-click if we generate these events ourselves
    if (!force_mouse && opts->doubleclick_time && MP_KEY_IS_MOUSE_BTN_DBL(unmod))
        return;
    int units = 1;
    if (MP_KEY_IS_WHEEL(unmod) && opts->preprocess_wheel && !process_wheel(ictx, unmod, &scale, &units))
        return;
    interpret_key(ictx, code, scale, units);
    if (code & MP_KEY_STATE_DOWN) {
        code &= ~MP_KEY_STATE_DOWN;
        if (ictx->last_doubleclick_key_down == code &&
            now - ictx->last_doubleclick_time < opts->doubleclick_time / 1000.0 &&
            code >= MP_MBTN_LEFT && code <= MP_MBTN_RIGHT)
        {
            now = 0;
            interpret_key(ictx, code - MP_MBTN_BASE + MP_MBTN_DBL_BASE,
                          1, 1);
        } else if (code == MP_MBTN_LEFT && ictx->opts->allow_win_drag &&
                   !test_mouse(ictx, ictx->mouse_vo_x, ictx->mouse_vo_y, MP_INPUT_ALLOW_VO_DRAGGING))
        {
            // This is a mouse left button down event which isn't part of a double-click,
            // and the mouse is on an input section which allows VO dragging.
            // Mark the dragging mouse button down in this case.
            ictx->dragging_button_down = true;
            // Store the current mouse position for deadzone handling.
            ictx->mouse_drag_x = ictx->mouse_raw_x;
            ictx->mouse_drag_y = ictx->mouse_raw_y;
        }
        ictx->last_doubleclick_key_down = code;
        ictx->last_doubleclick_time = now;
    }
    if (code & MP_KEY_STATE_UP) {
        code &= ~MP_KEY_STATE_UP;
        if (code == MP_MBTN_LEFT) {
            // This is a mouse left button up event. Mark the dragging mouse button up.
            ictx->dragging_button_down = false;
        }
    }
}

void mp_input_put_key(struct input_ctx *ictx, int code)
{
    input_lock(ictx);
    feed_key(ictx, code, 1, false);
    input_unlock(ictx);
}

void mp_input_put_key_artificial(struct input_ctx *ictx, int code, double value)
{
    if (value == 0.0)
        return;
    input_lock(ictx);
    feed_key(ictx, code, value, true);
    input_unlock(ictx);
}

void mp_input_put_key_utf8(struct input_ctx *ictx, int mods, struct bstr t)
{
    if (!t.len)
        return;
    input_lock(ictx);
    while (t.len) {
        int code = bstr_decode_utf8(t, &t);
        if (code < 0)
            break;
        feed_key(ictx, code | mods, 1, false);
    }
    input_unlock(ictx);
}

void mp_input_put_wheel(struct input_ctx *ictx, int direction, double value)
{
    if (value == 0.0)
        return;
    input_lock(ictx);
    feed_key(ictx, direction, value, false);
    input_unlock(ictx);
}

void mp_input_set_mouse_transform(struct input_ctx *ictx, struct mp_rect *dst,
                                  struct mp_rect *src)
{
    input_lock(ictx);
    ictx->mouse_mangle = dst || src;
    if (ictx->mouse_mangle) {
        ictx->mouse_dst = *dst;
        ictx->mouse_src_mangle = !!src;
        if (ictx->mouse_src_mangle)
            ictx->mouse_src = *src;
    }
    input_unlock(ictx);
}

bool mp_input_mouse_enabled(struct input_ctx *ictx)
{
    input_lock(ictx);
    bool r = ictx->opts->enable_mouse_movements;
    input_unlock(ictx);
    return r;
}

bool mp_input_vo_keyboard_enabled(struct input_ctx *ictx)
{
    input_lock(ictx);
    bool r = ictx->opts->vo_key_input;
    input_unlock(ictx);
    return r;
}

static void set_mouse_pos(struct input_ctx *ictx, int x, int y, bool quiet)
{
    MP_TRACE(ictx, "mouse move %d/%d\n", x, y);

    if (ictx->mouse_raw_x == x && ictx->mouse_raw_y == y) {
        return;
    }
    ictx->mouse_raw_x = x;
    ictx->mouse_raw_y = y;

    if (ictx->mouse_mangle) {
        struct mp_rect *src = &ictx->mouse_src;
        struct mp_rect *dst = &ictx->mouse_dst;
        x = MPCLAMP(x, dst->x0, dst->x1) - dst->x0;
        y = MPCLAMP(y, dst->y0, dst->y1) - dst->y0;
        if (ictx->mouse_src_mangle) {
            x = x * 1.0 / (dst->x1 - dst->x0) * (src->x1 - src->x0) + src->x0;
            y = y * 1.0 / (dst->y1 - dst->y0) * (src->y1 - src->y0) + src->y0;
        }
        MP_TRACE(ictx, "-> %d/%d\n", x, y);
    }

    if (!quiet)
        ictx->mouse_event_counter++;
    ictx->mouse_vo_x = x;
    ictx->mouse_vo_y = y;

    update_mouse_section(ictx);
    struct mp_cmd *cmd = get_cmd_from_keys(ictx, (bstr){0}, MP_KEY_MOUSE_MOVE);
    if (!cmd)
        cmd = mp_input_parse_cmd(ictx, bstr0("ignore"), "<internal>");

    if (cmd) {
        cmd->mouse_move = true;
        cmd->mouse_x = x;
        cmd->mouse_y = y;
        if (should_drop_cmd(ictx, cmd)) {
            talloc_free(cmd);
        } else {
            // Coalesce with previous mouse move events (i.e. replace it)
            struct mp_cmd *tail = queue_peek_tail(&ictx->cmd_queue);
            if (tail && tail->mouse_move) {
                queue_remove(&ictx->cmd_queue, tail);
                talloc_free(tail);
            }
            queue_cmd(ictx, cmd);
        }
    }

    bool mouse_outside_dragging_deadzone =
        abs(ictx->mouse_raw_x - ictx->mouse_drag_x) >= ictx->opts->dragging_deadzone ||
        abs(ictx->mouse_raw_y - ictx->mouse_drag_y) >= ictx->opts->dragging_deadzone;
    if (ictx->dragging_button_down && mouse_outside_dragging_deadzone &&
        ictx->opts->builtin_dragging)
    {
        // Begin built-in VO dragging if the mouse moves while the dragging button is down.
        ictx->dragging_button_down = false;
        // Prevent activation of MBTN_LEFT key binding if VO dragging begins.
        release_down_cmd(ictx, true);
        // Prevent activation of MBTN_LEFT_DBL if VO dragging begins.
        ictx->last_doubleclick_time = 0;
        mp_cmd_t *drag_cmd = mp_input_parse_cmd(ictx, bstr0("begin-vo-dragging"), "<internal>");
        queue_cmd(ictx, drag_cmd);
    }
}

void mp_input_set_mouse_pos_artificial(struct input_ctx *ictx, int x, int y)
{
    input_lock(ictx);
    set_mouse_pos(ictx, x, y, false);
    input_unlock(ictx);
}

void mp_input_set_mouse_pos(struct input_ctx *ictx, int x, int y, bool quiet)
{
    input_lock(ictx);
    if (ictx->opts->enable_mouse_movements)
        set_mouse_pos(ictx, x, y, quiet);
    input_unlock(ictx);
}

static int find_touch_point_index(struct input_ctx *ictx, int id)
{
    for (int i = 0; i < ictx->num_touch_points; i++) {
        if (ictx->touch_points[i].id == id)
            return i;
    }
    return -1;
}

static void notify_touch_update(struct input_ctx *ictx)
{
    // queue dummy cmd so that touch-pos can notify observers
    mp_cmd_t *cmd = mp_input_parse_cmd(ictx, bstr0("ignore"), "<internal>");
    queue_cmd(ictx, cmd);
}

static void update_touch_point(struct input_ctx *ictx, int idx, int id, int x, int y)
{
    MP_TRACE(ictx, "Touch point %d update (id %d) %d/%d\n",
             idx, id, x, y);
    if (ictx->touch_points[idx].x == x && ictx->touch_points[idx].y == y)
        return;
    ictx->touch_points[idx].x = x;
    ictx->touch_points[idx].y = y;
    // Emulate mouse input from the primary touch point (the first one added)
    if (ictx->opts->touch_emulate_mouse && idx == 0)
        set_mouse_pos(ictx, x, y, false);
    notify_touch_update(ictx);
}

void mp_input_add_touch_point(struct input_ctx *ictx, int id, int x, int y)
{
    input_lock(ictx);
    int idx = find_touch_point_index(ictx, id);
    if (idx != -1) {
        MP_WARN(ictx, "Touch point %d (id %d) already exists! Treat as update.\n",
                idx, id);
        update_touch_point(ictx, idx, id, x, y);
    } else {
        MP_TRACE(ictx, "Touch point %d add (id %d) %d/%d\n",
                 ictx->num_touch_points, id, x, y);
        MP_TARRAY_APPEND(ictx, ictx->touch_points, ictx->num_touch_points,
                         (struct touch_point){id, x, y});
        // Emulate MBTN_LEFT down if this is the only touch point
        if (ictx->opts->touch_emulate_mouse && ictx->num_touch_points == 1) {
            set_mouse_pos(ictx, x, y, false);
            feed_key(ictx, MP_MBTN_LEFT | MP_KEY_STATE_DOWN, 1, false);
        }
        notify_touch_update(ictx);
    }
    input_unlock(ictx);
}

void mp_input_update_touch_point(struct input_ctx *ictx, int id, int x, int y)
{
    input_lock(ictx);
    int idx = find_touch_point_index(ictx, id);
    if (idx != -1) {
        update_touch_point(ictx, idx, id, x, y);
    } else {
        MP_WARN(ictx, "Touch point id %d does not exist!\n", id);
    }
    input_unlock(ictx);
}

void mp_input_remove_touch_point(struct input_ctx *ictx, int id)
{
    input_lock(ictx);
    int idx = find_touch_point_index(ictx, id);
    if (idx != -1) {
        MP_TRACE(ictx, "Touch point %d remove (id %d)\n", idx, id);
        MP_TARRAY_REMOVE_AT(ictx->touch_points, ictx->num_touch_points, idx);
        // Emulate MBTN_LEFT up if there are no touch points left
        if (ictx->opts->touch_emulate_mouse && ictx->num_touch_points == 0)
            feed_key(ictx, MP_MBTN_LEFT | MP_KEY_STATE_UP, 1, false);
        notify_touch_update(ictx);
    }
    input_unlock(ictx);
}

int mp_input_get_touch_pos(struct input_ctx *ictx, int count, int *x, int *y, int *id)
{
    input_lock(ictx);
    int num_touch_points = ictx->num_touch_points;
    for (int i = 0; i < MPMIN(num_touch_points, count); i++) {
        x[i] = ictx->touch_points[i].x;
        y[i] = ictx->touch_points[i].y;
        id[i] = ictx->touch_points[i].id;
    }
    input_unlock(ictx);
    return num_touch_points;
}

static bool test_mouse(struct input_ctx *ictx, int x, int y, int rej_flags)
{
    bool res = false;
    for (int i = 0; i < ictx->num_active_sections; i++) {
        struct active_section *as = &ictx->active_sections[i];
        if (as->flags & rej_flags)
            continue;
        struct cmd_bind_section *s = get_bind_section(ictx, as->name);
        if (s->mouse_area_set && test_rect(&s->mouse_area, x, y)) {
            res = true;
            break;
        }
    }
    return res;
}

static bool test_mouse_active(struct input_ctx *ictx, int x, int y)
{
    return test_mouse(ictx, x, y, MP_INPUT_ALLOW_HIDE_CURSOR);
}

bool mp_input_test_mouse_active(struct input_ctx *ictx, int x, int y)
{
    input_lock(ictx);
    bool res = test_mouse_active(ictx, x, y);
    input_unlock(ictx);
    return res;
}

bool mp_input_test_dragging(struct input_ctx *ictx, int x, int y)
{
    input_lock(ictx);
    bool r = !ictx->opts->allow_win_drag ||
                        test_mouse(ictx, x, y, MP_INPUT_ALLOW_VO_DRAGGING);
    input_unlock(ictx);
    return r;
}

unsigned int mp_input_get_mouse_event_counter(struct input_ctx *ictx)
{
    // Make the frontend always display the mouse cursor (as long as it's not
    // forced invisible) if mouse input is desired.
    input_lock(ictx);
    if (test_mouse_active(ictx, ictx->mouse_x, ictx->mouse_y))
        ictx->mouse_event_counter++;
    int ret = ictx->mouse_event_counter;
    input_unlock(ictx);
    return ret;
}

// adjust min time to wait until next repeat event
static void adjust_max_wait_time(struct input_ctx *ictx, double *time)
{
    struct input_opts *opts = ictx->opts;
    if (ictx->last_key_down && opts->ar_rate > 0 && ictx->ar_state >= 0) {
        *time = MPMIN(*time, 1.0 / opts->ar_rate);
        *time = MPMIN(*time, opts->ar_delay / 1000.0);
    }
}

int mp_input_queue_cmd(struct input_ctx *ictx, mp_cmd_t *cmd)
{
    if (!cmd)
        return 0;
    input_lock(ictx);
    queue_cmd(ictx, cmd);
    input_unlock(ictx);
    return 1;
}

static mp_cmd_t *check_autorepeat(struct input_ctx *ictx)
{
    struct input_opts *opts = ictx->opts;

    // No input : autorepeat ?
    if (opts->ar_rate <= 0 || !ictx->current_down_cmd || !ictx->last_key_down ||
        (ictx->last_key_down & MP_NO_REPEAT_KEY) ||
        !mp_input_is_repeatable_cmd(ictx->current_down_cmd))
        ictx->ar_state = -1; // disable

    if (ictx->ar_state >= 0) {
        int64_t t = mp_time_ns();
        if (ictx->last_ar + MP_TIME_S_TO_NS(2) < t)
            ictx->last_ar = t;
        // First time : wait delay
        if (ictx->ar_state == 0
            && (t - ictx->last_key_down_time) >= MP_TIME_MS_TO_NS(opts->ar_delay))
        {
            ictx->ar_state = 1;
            ictx->last_ar = ictx->last_key_down_time + MP_TIME_MS_TO_NS(opts->ar_delay);
            // Then send rate / sec event
        } else if (ictx->ar_state == 1
                   && (t - ictx->last_ar) >= 1e9 / opts->ar_rate) {
            ictx->last_ar += 1e9 / opts->ar_rate;
        } else {
            return NULL;
        }
        struct mp_cmd *ret = mp_cmd_clone(ictx->current_down_cmd);
        ret->repeated = true;
        return ret;
    }
    return NULL;
}

double mp_input_get_delay(struct input_ctx *ictx)
{
    input_lock(ictx);
    double seconds = INFINITY;
    adjust_max_wait_time(ictx, &seconds);
    input_unlock(ictx);
    return seconds;
}

void mp_input_wakeup(struct input_ctx *ictx)
{
    ictx->wakeup_cb(ictx->wakeup_ctx);
}

mp_cmd_t *mp_input_read_cmd(struct input_ctx *ictx)
{
    input_lock(ictx);
    struct mp_cmd *ret = queue_remove_head(&ictx->cmd_queue);
    if (!ret)
        ret = check_autorepeat(ictx);
    if (ret && ret->mouse_move) {
        ictx->mouse_x = ret->mouse_x;
        ictx->mouse_y = ret->mouse_y;
    }
    input_unlock(ictx);
    return ret;
}

void mp_input_get_mouse_pos(struct input_ctx *ictx, int *x, int *y, int *hover)
{
    input_lock(ictx);
    *x = ictx->mouse_x;
    *y = ictx->mouse_y;
    *hover = ictx->mouse_hover;
    input_unlock(ictx);
}

// If name is NULL, return "default".
// Return a statically allocated name of the section (i.e. return value never
// gets deallocated).
static bstr normalize_section(struct input_ctx *ictx, bstr name)
{
    return get_bind_section(ictx, name)->section;
}

static void disable_section(struct input_ctx *ictx, bstr name)
{
    name = normalize_section(ictx, name);

    // Remove old section, or make sure it's on top if re-enabled
    for (int i = ictx->num_active_sections - 1; i >= 0; i--) {
        struct active_section *as = &ictx->active_sections[i];
        if (bstr_equals(as->name, name)) {
            MP_TARRAY_REMOVE_AT(ictx->active_sections,
                                ictx->num_active_sections, i);
        }
    }
}

void mp_input_disable_section(struct input_ctx *ictx, char *name)
{
    input_lock(ictx);
    disable_section(ictx, bstr0(name));
    input_unlock(ictx);
}

void mp_input_enable_section(struct input_ctx *ictx, char *name, int flags)
{
    bstr bname = bstr0(name);
    input_lock(ictx);
    bname = normalize_section(ictx, bname);

    disable_section(ictx, bname);

    MP_TRACE(ictx, "enable section '%.*s'\n", BSTR_P(bname));

    int top = ictx->num_active_sections;
    if (!(flags & MP_INPUT_ON_TOP)) {
        // insert before the first top entry
        for (top = 0; top < ictx->num_active_sections; top++) {
            if (ictx->active_sections[top].flags & MP_INPUT_ON_TOP)
                break;
        }
    }
    MP_TARRAY_INSERT_AT(ictx, ictx->active_sections, ictx->num_active_sections,
                        top, (struct active_section){bname, flags});

    MP_TRACE(ictx, "active section stack:\n");
    for (int n = 0; n < ictx->num_active_sections; n++) {
        MP_TRACE(ictx, " %.*s %d\n", BSTR_P(ictx->active_sections[n].name),
                 ictx->active_sections[n].flags);
    }

    input_unlock(ictx);
}

void mp_input_disable_all_sections(struct input_ctx *ictx)
{
    input_lock(ictx);
    ictx->num_active_sections = 0;
    input_unlock(ictx);
}

void mp_input_set_section_mouse_area(struct input_ctx *ictx, char *name,
                                     int x0, int y0, int x1, int y1)
{
    input_lock(ictx);
    struct cmd_bind_section *s = get_bind_section(ictx, bstr0(name));
    s->mouse_area = (struct mp_rect){x0, y0, x1, y1};
    s->mouse_area_set = x0 != x1 && y0 != y1;
    input_unlock(ictx);
}

static void bind_dealloc(struct cmd_bind *bind)
{
    talloc_free(bind->cmd);
    talloc_free(bind->location);
    talloc_free(bind->desc);
}

// builtin: if true, remove all builtin binds, else remove all user binds
static void remove_binds(struct cmd_bind_section *bs, bool builtin)
{
    for (int n = bs->num_binds - 1; n >= 0; n--) {
        if (bs->binds[n].is_builtin == builtin) {
            bind_dealloc(&bs->binds[n]);
            mp_assert(bs->num_binds >= 1);
            bs->binds[n] = bs->binds[bs->num_binds - 1];
            bs->num_binds--;
        }
    }
}

void mp_input_define_section(struct input_ctx *ictx, char *name, char *location,
                             char *contents, bool builtin, char *owner)
{
    bstr bname = bstr0(name);
    if (!bname.len)
        return; // parse_config() changes semantics with restrict_section==empty
    input_lock(ictx);
    // Delete:
    struct cmd_bind_section *bs = get_bind_section(ictx, bname);
    if ((!bs->owner || (owner && strcmp(bs->owner, owner) != 0)) &&
        !bstr_equals0(bs->section, "default"))
    {
        talloc_replace(bs, bs->owner, owner);
    }
    remove_binds(bs, builtin);
    if (contents && contents[0]) {
        // Redefine:
        parse_config(ictx, builtin, bstr0(contents), location, bname);
    } else {
        // Disable:
        disable_section(ictx, bname);
    }
    input_unlock(ictx);
}

void mp_input_remove_sections_by_owner(struct input_ctx *ictx, char *owner)
{
    input_lock(ictx);
    for (int n = 0; n < ictx->num_sections; n++) {
        struct cmd_bind_section *bs = ictx->sections[n];
        if (bs->owner && owner && strcmp(bs->owner, owner) == 0) {
            disable_section(ictx, bs->section);
            remove_binds(bs, false);
            remove_binds(bs, true);
        }
    }
    input_unlock(ictx);
}

static bool bind_matches_key(struct cmd_bind *bind, int num_keys, const int *keys)
{
    if (bind->num_keys != num_keys)
        return false;
    for (int i = 0; i < num_keys; i++) {
        if (bind->keys[i] != keys[i])
            return false;
    }
    return true;
}

static void bind_keys(struct input_ctx *ictx, bool builtin, bstr section,
                      const int *keys, int num_keys, bstr command,
                      const char *loc, const char *desc)
{
    struct cmd_bind_section *bs = get_bind_section(ictx, section);
    struct cmd_bind *bind = NULL;

    mp_assert(num_keys <= MP_MAX_KEY_DOWN);

    for (int n = 0; n < bs->num_binds; n++) {
        struct cmd_bind *b = &bs->binds[n];
        if (bind_matches_key(b, num_keys, keys) && b->is_builtin == builtin) {
            bind = b;
            break;
        }
    }

    if (!bind) {
        struct cmd_bind empty = {{0}};
        MP_TARRAY_APPEND(bs, bs->binds, bs->num_binds, empty);
        bind = &bs->binds[bs->num_binds - 1];
    }

    bind_dealloc(bind);

    *bind = (struct cmd_bind) {
        .cmd = bstrdup0(bs->binds, command),
        .location = talloc_strdup(bs->binds, loc),
        .desc = talloc_strdup(bs->binds, desc),
        .owner = bs,
        .is_builtin = builtin,
        .num_keys = num_keys,
    };
    memcpy(bind->keys, keys, num_keys * sizeof(bind->keys[0]));
    if (mp_msg_test(ictx->log, MSGL_DEBUG)) {
        char *s = mp_input_get_key_combo_name(keys, num_keys);
        MP_TRACE(ictx, "add: section='%.*s' key='%s'%s cmd='%s' location='%s'\n",
                 BSTR_P(bind->owner->section), s, bind->is_builtin ? " builtin" : "",
                 bind->cmd, bind->location);
        talloc_free(s);
    }
}

// restrict_section: every entry is forced to this section name
//          if NULL, load normally and allow any sections
static int parse_config(struct input_ctx *ictx, bool builtin, bstr data,
                        const char *location, const bstr restrict_section)
{
    int n_binds = 0;
    int line_no = 0;
    char *cur_loc = NULL;

    while (data.len) {
        line_no++;
        if (cur_loc)
            talloc_free(cur_loc);
        cur_loc = talloc_asprintf(NULL, "%s:%d", location, line_no);

        bstr line = bstr_strip_linebreaks(bstr_getline(data, &data));
        line = bstr_lstrip(line);
        if (line.len == 0 || bstr_startswith0(line, "#"))
            continue;
        if (bstr_eatstart0(&line, "default-bindings ")) {
            bstr orig = line;
            bstr_split_tok(line, "#", &line, &(bstr){0});
            line = bstr_strip(line);
            if (bstr_equals0(line, "start")) {
                builtin = true;
            } else {
                MP_ERR(ictx, "Broken line: %.*s at %s\n", BSTR_P(orig), cur_loc);
            }
            continue;
        }
        struct bstr command;
        // Find the key name starting a line
        struct bstr keyname = bstr_split(line, WHITESPACE, &command);
        command = bstr_strip(command);
        if (command.len == 0) {
            MP_ERR(ictx, "Unfinished key binding: %.*s at %s\n", BSTR_P(line),
                   cur_loc);
            continue;
        }
        char *name = bstrdup0(NULL, keyname);
        int keys[MP_MAX_KEY_DOWN];
        int num_keys = 0;
        if (!mp_input_get_keys_from_string(name, MP_MAX_KEY_DOWN, &num_keys, keys)) {
            talloc_free(name);
            MP_ERR(ictx, "Unknown key '%.*s' at %s\n", BSTR_P(keyname), cur_loc);
            continue;
        }
        talloc_free(name);

        bstr section = restrict_section;
        if (!section.len) {
            if (bstr_startswith0(command, "{")) {
                int p = bstrchr(command, '}');
                if (p != -1) {
                    section = bstr_strip(bstr_splice(command, 1, p));
                    command = bstr_lstrip(bstr_cut(command, p + 1));
                }
            }
        }

        // Print warnings if invalid commands are encountered.
        struct mp_cmd *cmd = mp_input_parse_cmd(ictx, command, cur_loc);
        const char *desc = NULL;
        if (cmd) {
            desc = cmd->desc;
            command = bstr0(cmd->original);
        }

        bind_keys(ictx, builtin, section, keys, num_keys, command, cur_loc, desc);
        n_binds++;

        talloc_free(cmd);
    }

    talloc_free(cur_loc);

    return n_binds;
}

static bool parse_config_file(struct input_ctx *ictx, char *file)
{
    bool r = false;
    void *tmp = talloc_new(NULL);

    file = mp_get_user_path(tmp, ictx->global, file);

    bstr data = stream_read_file2(file, tmp, STREAM_ORIGIN_DIRECT | STREAM_READ,
                                  ictx->global, 1000000);
    if (data.start) {
        MP_VERBOSE(ictx, "Parsing input config file %s\n", file);
        bstr_eatstart0(&data, "\xEF\xBB\xBF"); // skip BOM
        int num = parse_config(ictx, false, data, file, (bstr){0});
        MP_VERBOSE(ictx, "Input config file %s parsed: %d binds\n", file, num);
        r = true;
    } else {
        MP_ERR(ictx, "Error reading input config file %s\n", file);
    }

    talloc_free(tmp);
    return r;
}

struct input_ctx *mp_input_init(struct mpv_global *global,
                                void (*wakeup_cb)(void *ctx),
                                void *wakeup_ctx)
{

    struct input_ctx *ictx = talloc_ptrtype(NULL, ictx);
    *ictx = (struct input_ctx){
        .global = global,
        .ar_state = -1,
        .log = mp_log_new(ictx, global->log, "input"),
        .mouse_section = bstr0("default"),
        .opts_cache = m_config_cache_alloc(ictx, global, &input_config),
        .wakeup_cb = wakeup_cb,
        .wakeup_ctx = wakeup_ctx,
        .active_sections = talloc_array(ictx, struct active_section, 0),
        .touch_points = talloc_array(ictx, struct touch_point, 0),
    };

    ictx->opts = ictx->opts_cache->opts;

    mp_mutex_init(&ictx->mutex);

    // Setup default section, so that it does nothing.
    mp_input_enable_section(ictx, NULL, MP_INPUT_ALLOW_VO_DRAGGING |
                                        MP_INPUT_ALLOW_HIDE_CURSOR);

    return ictx;
}

static void reload_opts(struct input_ctx *ictx, bool shutdown)
{
    m_config_cache_update(ictx->opts_cache);

#if HAVE_COCOA
    struct input_opts *opts = ictx->opts;

    if (ictx->using_cocoa_media_keys != (opts->use_media_keys && !shutdown)) {
        ictx->using_cocoa_media_keys = !ictx->using_cocoa_media_keys;
        if (ictx->using_cocoa_media_keys) {
            cocoa_init_media_keys();
        } else {
            cocoa_uninit_media_keys();
        }
    }
#endif
}

void mp_input_update_opts(struct input_ctx *ictx)
{
    input_lock(ictx);
    reload_opts(ictx, false);
    input_unlock(ictx);
}

void mp_input_load_config(struct input_ctx *ictx)
{
    input_lock(ictx);

    reload_opts(ictx, false);

    // "Uncomment" the default key bindings in etc/input.conf and add them.
    // All lines that do not start with '# ' are parsed.
    bstr builtin = bstr0(builtin_input_conf);
    while (ictx->opts->builtin_bindings && builtin.len) {
        bstr line = bstr_getline(builtin, &builtin);
        bstr_eatstart0(&line, "#");
        if (!bstr_startswith0(line, " "))
            parse_config(ictx, true, line, "<builtin>", (bstr){0});
    }

    bool config_ok = false;
    if (ictx->opts->config_file && ictx->opts->config_file[0])
        config_ok = parse_config_file(ictx, ictx->opts->config_file);
    if (!config_ok) {
        // Try global conf dir
        void *tmp = talloc_new(NULL);
        char **files = mp_find_all_config_files(tmp, ictx->global, "input.conf");
        for (int n = 0; files && files[n]; n++)
            parse_config_file(ictx, files[n]);
        talloc_free(tmp);
    }

    bool use_gamepad = ictx->opts->use_gamepad;
    input_unlock(ictx);

#if HAVE_SDL2_GAMEPAD
    if (use_gamepad)
        mp_input_sdl_gamepad_add(ictx);
#else
    (void)use_gamepad;
#endif
}

bool mp_input_load_config_file(struct input_ctx *ictx, char *file)
{
    input_lock(ictx);
    bool result = parse_config_file(ictx, file);
    input_unlock(ictx);
    return result;
}

static void clear_queue(struct cmd_queue *queue)
{
    while (queue->first) {
        struct mp_cmd *item = queue->first;
        queue_remove(queue, item);
        talloc_free(item);
    }
}

void mp_input_uninit(struct input_ctx *ictx)
{
    if (!ictx)
        return;

    input_lock(ictx);
    reload_opts(ictx, true);
    input_unlock(ictx);

    close_input_sources(ictx);
    clear_queue(&ictx->cmd_queue);
    talloc_free(ictx->current_down_cmd);
    mp_mutex_destroy(&ictx->mutex);
    talloc_free(ictx);
}

bool mp_input_use_alt_gr(struct input_ctx *ictx)
{
    input_lock(ictx);
    bool r = ictx->opts->use_alt_gr;
    input_unlock(ictx);
    return r;
}

bool mp_input_use_media_keys(struct input_ctx *ictx)
{
    input_lock(ictx);
    bool r = ictx->opts->use_media_keys;
    input_unlock(ictx);
    return r;
}

struct mp_cmd *mp_input_parse_cmd(struct input_ctx *ictx, bstr str,
                                  const char *location)
{
    return mp_input_parse_cmd_str(ictx->log, str, location);
}

void mp_input_run_cmd(struct input_ctx *ictx, const char **cmd)
{
    input_lock(ictx);
    queue_cmd(ictx, mp_input_parse_cmd_strv(ictx->log, cmd));
    input_unlock(ictx);
}

bool mp_input_bind_key(struct input_ctx *ictx, const char *key, bstr command,
                       const char *desc)
{
    char *name = talloc_strdup(NULL, key);
    int keys[MP_MAX_KEY_DOWN];
    int num_keys = 0;
    if (!mp_input_get_keys_from_string(name, MP_MAX_KEY_DOWN, &num_keys, keys)) {
        talloc_free(name);
        return false;
    }
    talloc_free(name);

    input_lock(ictx);
    bind_keys(ictx, false, (bstr){0}, keys, num_keys, command,
              "keybind-command", desc);
    input_unlock(ictx);
    return true;
}

struct mpv_node mp_input_get_bindings(struct input_ctx *ictx)
{
    input_lock(ictx);
    struct mpv_node root;
    node_init(&root, MPV_FORMAT_NODE_ARRAY, NULL);

    for (int x = 0; x < ictx->num_sections; x++) {
        struct cmd_bind_section *s = ictx->sections[x];
        int priority = -1;

        for (int i = 0; i < ictx->num_active_sections; i++) {
            struct active_section *as = &ictx->active_sections[i];
            if (bstr_equals(as->name, s->section)) {
                priority = i;
                break;
            }
        }

        for (int n = 0; n < s->num_binds; n++) {
            struct cmd_bind *b = &s->binds[n];
            struct mpv_node *entry = node_array_add(&root, MPV_FORMAT_NODE_MAP);

            int b_priority = priority;
            if (b->is_builtin && !ictx->opts->default_bindings)
                b_priority = -1;

            // Try to fixup the weird logic so consumer of this bindings list
            // does not get too confused.
            if (b_priority >= 0 && !b->is_builtin)
                b_priority += ictx->num_active_sections;

            node_map_add_bstr(entry, "section", s->section);
            if (s->owner)
                node_map_add_string(entry, "owner", s->owner);
            node_map_add_string(entry, "cmd", b->cmd);
            node_map_add_flag(entry, "is_weak", b->is_builtin);
            node_map_add_int64(entry, "priority", b_priority);
            if (b->desc)
                node_map_add_string(entry, "comment", b->desc);

            char *key = mp_input_get_key_combo_name(b->keys, b->num_keys);
            node_map_add_string(entry, "key", key);
            talloc_free(key);
        }
    }

    input_unlock(ictx);
    return root;
}

struct mp_input_src_internal {
    mp_thread thread;
    bool thread_running;
    bool init_done;

    char *cmd_buffer;
    size_t cmd_buffer_size;
    bool drop;
};

static struct mp_input_src *input_add_src(struct input_ctx *ictx)
{
    input_lock(ictx);
    if (ictx->num_sources == MP_MAX_SOURCES) {
        input_unlock(ictx);
        return NULL;
    }

    char name[80];
    snprintf(name, sizeof(name), "#%d", ictx->num_sources + 1);
    struct mp_input_src *src = talloc_ptrtype(NULL, src);
    *src = (struct mp_input_src){
        .global = ictx->global,
        .log = mp_log_new(src, ictx->log, name),
        .input_ctx = ictx,
        .in = talloc_zero(src, struct mp_input_src_internal),
    };

    ictx->sources[ictx->num_sources++] = src;

    input_unlock(ictx);
    return src;
}

static void input_src_kill(struct mp_input_src *src);

static void close_input_sources(struct input_ctx *ictx)
{
    // To avoid lock-order issues, we first remove each source from the context,
    // and then destroy it.
    while (1) {
        input_lock(ictx);
        struct mp_input_src *src = ictx->num_sources ? ictx->sources[0] : NULL;
        input_unlock(ictx);
        if (!src)
            break;
        input_src_kill(src);
    }
}

static void input_src_kill(struct mp_input_src *src)
{
    if (!src)
        return;
    struct input_ctx *ictx = src->input_ctx;
    input_lock(ictx);
    for (int n = 0; n < ictx->num_sources; n++) {
        if (ictx->sources[n] == src) {
            MP_TARRAY_REMOVE_AT(ictx->sources, ictx->num_sources, n);
            input_unlock(ictx);
            if (src->cancel)
                src->cancel(src);
            if (src->in->thread_running)
                mp_thread_join(src->in->thread);
            if (src->uninit)
                src->uninit(src);
            talloc_free(src);
            return;
        }
    }
    MP_ASSERT_UNREACHABLE();
}

void mp_input_src_init_done(struct mp_input_src *src)
{
    mp_assert(!src->in->init_done);
    mp_assert(src->in->thread_running);
    mp_assert(mp_thread_id_equal(mp_thread_get_id(src->in->thread), mp_thread_current_id()));
    src->in->init_done = true;
    mp_rendezvous(&src->in->init_done, 0);
}

static MP_THREAD_VOID input_src_thread(void *ptr)
{
    void **args = ptr;
    struct mp_input_src *src = args[0];
    void (*loop_fn)(struct mp_input_src *src, void *ctx) = args[1];
    void *ctx = args[2];

    mp_thread_set_name("input");

    src->in->thread_running = true;

    loop_fn(src, ctx);

    if (!src->in->init_done)
        mp_rendezvous(&src->in->init_done, -1);

    MP_THREAD_RETURN();
}

int mp_input_add_thread_src(struct input_ctx *ictx, void *ctx,
    void (*loop_fn)(struct mp_input_src *src, void *ctx))
{
    struct mp_input_src *src = input_add_src(ictx);
    if (!src)
        return -1;

    void *args[] = {src, loop_fn, ctx};
    if (mp_thread_create(&src->in->thread, input_src_thread, args)) {
        input_src_kill(src);
        return -1;
    }
    if (mp_rendezvous(&src->in->init_done, 0) < 0) {
        input_src_kill(src);
        return -1;
    }
    return 0;
}

#define CMD_BUFFER (4 * 4096)

void mp_input_src_feed_cmd_text(struct mp_input_src *src, char *buf, size_t len)
{
    struct mp_input_src_internal *in = src->in;
    if (!in->cmd_buffer)
        in->cmd_buffer = talloc_size(in, CMD_BUFFER);
    while (len) {
        char *next = memchr(buf, '\n', len);
        bool term = !!next;
        next = next ? next + 1 : buf + len;
        size_t copy = next - buf;
        bool overflow = copy > CMD_BUFFER - in->cmd_buffer_size;
        if (overflow || in->drop) {
            in->cmd_buffer_size = 0;
            in->drop = overflow || !term;
            MP_WARN(src, "Dropping overlong line.\n");
        } else {
            memcpy(in->cmd_buffer + in->cmd_buffer_size, buf, copy);
            in->cmd_buffer_size += copy;
            buf += copy;
            len -= copy;
            if (term) {
                bstr s = {in->cmd_buffer, in->cmd_buffer_size};
                s = bstr_strip(s);
                struct mp_cmd *cmd = mp_input_parse_cmd_str(src->log, s, "<>");
                if (cmd) {
                    input_lock(src->input_ctx);
                    queue_cmd(src->input_ctx, cmd);
                    input_unlock(src->input_ctx);
                }
                in->cmd_buffer_size = 0;
            }
        }
    }
}

void mp_input_set_repeat_info(struct input_ctx *ictx, int rate, int delay)
{
    input_lock(ictx);
    ictx->opts->ar_rate = rate;
    ictx->opts->ar_delay = delay;
    input_unlock(ictx);
}
