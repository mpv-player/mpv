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
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <pthread.h>
#include <assert.h>

#include <libavutil/avstring.h>
#include <libavutil/common.h>

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
#include "stream/stream.h"
#include "common/common.h"

#if HAVE_COCOA
#include "osdep/macosx_events.h"
#endif

#define input_lock(ictx)    pthread_mutex_lock(&ictx->mutex)
#define input_unlock(ictx)  pthread_mutex_unlock(&ictx->mutex)

#define MP_MAX_KEY_DOWN 4

struct cmd_bind {
    int keys[MP_MAX_KEY_DOWN];
    int num_keys;
    char *cmd;
    char *location;     // filename/line number of definition
    bool is_builtin;
    struct cmd_bind_section *owner;
};

struct cmd_bind_section {
    char *owner;
    struct cmd_bind *binds;
    int num_binds;
    char *section;
    struct mp_rect mouse_area;  // set at runtime, if at all
    bool mouse_area_set;        // mouse_area is valid and should be tested
    struct cmd_bind_section *next;
};

#define MP_MAX_SOURCES 10

#define MAX_ACTIVE_SECTIONS 50

struct active_section {
    char *name;
    int flags;
};

struct cmd_queue {
    struct mp_cmd *first;
};

struct wheel_state {
    double dead_zone_accum;
    double unit_accum;
};

struct input_ctx {
    pthread_mutex_t mutex;
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

    // Mouse position on the consumer side (as command.c sees it)
    int mouse_x, mouse_y;
    char *mouse_section; // last section to receive mouse event

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
    struct cmd_bind_section *cmd_bind_sections;

    // List currently active command sections
    struct active_section active_sections[MAX_ACTIVE_SECTIONS];
    int num_active_sections;

    unsigned int mouse_event_counter;

    struct mp_input_src *sources[MP_MAX_SOURCES];
    int num_sources;

    struct cmd_queue cmd_queue;

    void (*cancel)(void *cancel_ctx);
    void *cancel_ctx;

    void (*wakeup_cb)(void *ctx);
    void *wakeup_ctx;
};

static int parse_config(struct input_ctx *ictx, bool builtin, bstr data,
                        const char *location, const char *restrict_section);
static void close_input_sources(struct input_ctx *ictx);

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
    int use_alt_gr;
    int use_appleremote;
    int use_media_keys;
    int default_bindings;
    int enable_mouse_movements;
    int vo_key_input;
    int test;
    int allow_win_drag;
};

const struct m_sub_options input_config = {
    .opts = (const m_option_t[]) {
        OPT_STRING("input-conf", config_file, M_OPT_FIXED | M_OPT_FILE),
        OPT_INT("input-ar-delay", ar_delay, 0),
        OPT_INT("input-ar-rate", ar_rate, 0),
        OPT_PRINT("input-keylist", mp_print_key_list),
        OPT_PRINT("input-cmdlist", mp_print_cmd_list),
        OPT_FLAG("input-default-bindings", default_bindings, 0),
        OPT_FLAG("input-test", test, 0),
        OPT_INTRANGE("input-doubleclick-time", doubleclick_time, 0, 0, 1000),
        OPT_FLAG("input-right-alt-gr", use_alt_gr, 0),
        OPT_INTRANGE("input-key-fifo-size", key_fifo_size, 0, 2, 65000),
        OPT_FLAG("input-cursor", enable_mouse_movements, 0),
        OPT_FLAG("input-vo-keyboard", vo_key_input, 0),
        OPT_FLAG("input-media-keys", use_media_keys, 0),
#if HAVE_COCOA
        OPT_FLAG("input-appleremote", use_appleremote, 0),
#endif
        OPT_FLAG("window-dragging", allow_win_drag, 0),
        OPT_REPLACED("input-x11-keyboard", "input-vo-keyboard"),
        {0}
    },
    .size = sizeof(struct input_opts),
    .defaults = &(const struct input_opts){
        .key_fifo_size = 7,
        .doubleclick_time = 300,
        .ar_delay = 200,
        .ar_rate = 40,
        .use_alt_gr = 1,
        .enable_mouse_movements = 1,
        .use_media_keys = 1,
#if HAVE_COCOA
        .use_appleremote = 1,
#endif
        .default_bindings = 1,
        .vo_key_input = 1,
        .allow_win_drag = 1,
    },
    .change_flags = UPDATE_INPUT,
};

static const char builtin_input_conf[] =
#include "input/input_conf.h"
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
    assert(*p_prev == cmd);
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

static void append_bind_info(struct input_ctx *ictx, char **pmsg,
                             struct cmd_bind *bind)
{
    char *msg = *pmsg;
    struct mp_cmd *cmd = mp_input_parse_cmd(ictx, bstr0(bind->cmd),
                                            bind->location);
    bstr stripped = cmd ? cmd->original : bstr0(bind->cmd);
    msg = talloc_asprintf_append(msg, " '%.*s'", BSTR_P(stripped));
    if (!cmd)
        msg = talloc_asprintf_append(msg, " (invalid)");
    if (strcmp(bind->owner->section, "default") != 0)
        msg = talloc_asprintf_append(msg, " in section {%s}",
                                     bind->owner->section);
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
    for (struct cmd_bind_section *bs = ictx->cmd_bind_sections;
         bs; bs = bs->next)
    {
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

static struct cmd_bind_section *get_bind_section(struct input_ctx *ictx,
                                                 bstr section)
{
    struct cmd_bind_section *bind_section = ictx->cmd_bind_sections;

    if (section.len == 0)
        section = bstr0("default");
    while (bind_section) {
        if (bstrcmp0(section, bind_section->section) == 0)
            return bind_section;
        if (bind_section->next == NULL)
            break;
        bind_section = bind_section->next;
    }
    if (bind_section) {
        bind_section->next = talloc_ptrtype(ictx, bind_section->next);
        bind_section = bind_section->next;
    } else {
        ictx->cmd_bind_sections = talloc_ptrtype(ictx, ictx->cmd_bind_sections);
        bind_section = ictx->cmd_bind_sections;
    }
    *bind_section = (struct cmd_bind_section) {
        .section = bstrdup0(bind_section, section),
        .mouse_area = {INT_MIN, INT_MIN, INT_MAX, INT_MAX},
        .mouse_area_set = true,
    };
    return bind_section;
}

static void key_buf_add(int *buf, int code)
{
    for (int n = MP_MAX_KEY_DOWN - 1; n > 0; n--)
        buf[n] = buf[n - 1];
    buf[0] = code;
}

static struct cmd_bind *find_bind_for_key_section(struct input_ctx *ictx,
                                                  char *section, int code)
{
    struct cmd_bind_section *bs = get_bind_section(ictx, bstr0(section));

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
        if (best)
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
                if (!best || b->num_keys >= best->num_keys)
                    best = b;
            skip: ;
            }
        }
    }
    return best;
}

static struct cmd_bind *find_any_bind_for_key(struct input_ctx *ictx,
                                              char *force_section, int code)
{
    if (force_section)
        return find_bind_for_key_section(ictx, force_section, code);

    bool use_mouse = MP_KEY_DEPENDS_ON_MOUSE_POS(code);

    // First look whether a mouse section is capturing all mouse input
    // exclusively (regardless of the active section stack order).
    if (use_mouse && MP_KEY_IS_MOUSE_BTN_SINGLE(ictx->last_key_down)) {
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
                if (!best_bind || (best_bind->is_builtin && !bind->is_builtin))
                    best_bind = bind;
            }
        }
        if (s->flags & MP_INPUT_EXCLUSIVE)
            break;
        if (best_bind && (s->flags & MP_INPUT_ON_TOP))
            break;
    }

    return best_bind;
}

static mp_cmd_t *get_cmd_from_keys(struct input_ctx *ictx, char *force_section,
                                   int code)
{
    if (ictx->opts->test)
        return handle_test(ictx, code);

    struct cmd_bind *cmd = find_any_bind_for_key(ictx, force_section, code);
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
        MP_TRACE(ictx, "key '%s' -> '%s' in '%s'\n",
                 ret->key_name, cmd->cmd, ret->input_section);
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
        find_any_bind_for_key(ictx, NULL, MP_KEY_MOUSE_MOVE);

    char *new_section = bind ? bind->owner->section : "default";

    char *old = ictx->mouse_section;
    ictx->mouse_section = new_section;

    if (strcmp(old, ictx->mouse_section) != 0) {
        MP_TRACE(ictx, "input: switch section %s -> %s\n",
                 old, ictx->mouse_section);
        mp_input_queue_cmd(ictx, get_cmd_from_keys(ictx, old, MP_KEY_MOUSE_LEAVE));
    }
}

// Called when the currently held-down key is released. This (usually) sends
// the a key-up version of the command associated with the keys that were held
// down.
// If the drop_current parameter is set to true, then don't send the key-up
// command. Unless we've already sent a key-down event, in which case the
// input receiver (the player) must get a key-up event, or it would get stuck
// thinking a key is still held down.
static void release_down_cmd(struct input_ctx *ictx, bool drop_current)
{
    if (ictx->current_down_cmd && ictx->current_down_cmd->emit_on_up &&
        (!drop_current || ictx->current_down_cmd->def->on_updown))
    {
        memset(ictx->key_history, 0, sizeof(ictx->key_history));
        ictx->current_down_cmd->is_up = true;
        mp_input_queue_cmd(ictx, ictx->current_down_cmd);
    } else {
        talloc_free(ictx->current_down_cmd);
    }
    ictx->current_down_cmd = NULL;
    ictx->last_key_down = 0;
    ictx->last_key_down_time = 0;
    ictx->ar_state = -1;
    update_mouse_section(ictx);
}

// We don't want the append to the command queue indefinitely, because that
// could lead to situations where recovery would take too long. On the other
// hand, don't drop commands that will abort playback.
static bool should_drop_cmd(struct input_ctx *ictx, struct mp_cmd *cmd)
{
    struct cmd_queue *queue = &ictx->cmd_queue;
    return queue_count_cmds(queue) >= ictx->opts->key_fifo_size &&
           !mp_input_is_abort_cmd(cmd);
}

static struct mp_cmd *resolve_key(struct input_ctx *ictx, int code)
{
    update_mouse_section(ictx);
    struct mp_cmd *cmd = get_cmd_from_keys(ictx, NULL, code);
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
    code = code & ~(unsigned)state;

    if (mp_msg_test(ictx->log, MSGL_DEBUG)) {
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
        ictx->last_key_down_time = mp_time_us();
        ictx->ar_state = 0;
        mp_input_wakeup(ictx); // possibly start timer for autorepeat
    } else if (state == MP_KEY_STATE_UP) {
        // Most VOs send RELEASE_ALL anyway
        release_down_cmd(ictx, false);
    } else {
        // Press of key with no separate down/up events
        // Mixing press events and up/down with the same key is not supported,
        // and input sources shouldn't do this, but can happen anyway if
        // multiple input sources interfere with each others.
        if (ictx->last_key_down == code)
            release_down_cmd(ictx, false);
        cmd = resolve_key(ictx, code);
    }

    if (!cmd)
        return;

    // Don't emit a command on key-down if the key is designed to emit commands
    // on key-up (like mouse buttons). Also, if the command specifically should
    // be sent both on key down and key up, still emit the command.
    if (cmd->emit_on_up && !cmd->def->on_updown) {
        talloc_free(cmd);
        return;
    }

    memset(ictx->key_history, 0, sizeof(ictx->key_history));

    if (mp_input_is_scalable_cmd(cmd)) {
        cmd->scale = scale;
        cmd->scale_units = scale_units;
        mp_input_queue_cmd(ictx, cmd);
    } else {
        // Non-scalable commands won't understand cmd->scale, so synthesize
        // multiple commands with cmd->scale = 1
        cmd->scale = 1;
        cmd->scale_units = 1;
        // Avoid spamming the player with too many commands
        scale_units = FFMIN(scale_units, 20);
        for (int i = 0; i < scale_units - 1; i++)
            mp_input_queue_cmd(ictx, mp_cmd_clone(cmd));
        if (scale_units)
            mp_input_queue_cmd(ictx, cmd);
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
    // seconds, eg. the user is assumed to have finished scrolling.
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

static void mp_input_feed_key(struct input_ctx *ictx, int code, double scale,
                              bool force_mouse)
{
    struct input_opts *opts = ictx->opts;

    code = mp_normalize_keycode(code);
    int unmod = code & ~MP_KEY_MODIFIER_MASK;
    if (code == MP_INPUT_RELEASE_ALL) {
        MP_TRACE(ictx, "release all\n");
        release_down_cmd(ictx, false);
        return;
    }
    if (!opts->enable_mouse_movements && MP_KEY_IS_MOUSE(unmod) && !force_mouse)
        return;
    if (unmod == MP_KEY_MOUSE_LEAVE || unmod == MP_KEY_MOUSE_ENTER) {
        update_mouse_section(ictx);
        mp_input_queue_cmd(ictx, get_cmd_from_keys(ictx, NULL, code));
        return;
    }
    double now = mp_time_sec();
    // ignore system-doubleclick if we generate these events ourselves
    if (!force_mouse && opts->doubleclick_time && MP_KEY_IS_MOUSE_BTN_DBL(unmod))
        return;
    int units = 1;
    if (MP_KEY_IS_WHEEL(unmod) && !process_wheel(ictx, unmod, &scale, &units))
        return;
    interpret_key(ictx, code, scale, units);
    if (code & MP_KEY_STATE_DOWN) {
        code &= ~MP_KEY_STATE_DOWN;
        if (ictx->last_doubleclick_key_down == code &&
            now - ictx->last_doubleclick_time < opts->doubleclick_time / 1000.0)
        {
            if (code >= MP_MBTN_LEFT && code <= MP_MBTN_RIGHT) {
                interpret_key(ictx, code - MP_MBTN_BASE + MP_MBTN_DBL_BASE,
                              1, 1);
            }
        }
        ictx->last_doubleclick_key_down = code;
        ictx->last_doubleclick_time = now;
    }
}

void mp_input_put_key(struct input_ctx *ictx, int code)
{
    input_lock(ictx);
    mp_input_feed_key(ictx, code, 1, false);
    input_unlock(ictx);
}

void mp_input_put_key_artificial(struct input_ctx *ictx, int code)
{
    input_lock(ictx);
    mp_input_feed_key(ictx, code, 1, true);
    input_unlock(ictx);
}

void mp_input_put_key_utf8(struct input_ctx *ictx, int mods, struct bstr t)
{
    while (t.len) {
        int code = bstr_decode_utf8(t, &t);
        if (code < 0)
            break;
        mp_input_put_key(ictx, code | mods);
    }
}

void mp_input_put_wheel(struct input_ctx *ictx, int direction, double value)
{
    if (value == 0.0)
        return;
    input_lock(ictx);
    mp_input_feed_key(ictx, direction, value, false);
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

void mp_input_set_mouse_pos(struct input_ctx *ictx, int x, int y)
{
    input_lock(ictx);
    if (ictx->opts->enable_mouse_movements)
        mp_input_set_mouse_pos_artificial(ictx, x, y);
    input_unlock(ictx);
}

void mp_input_set_mouse_pos_artificial(struct input_ctx *ictx, int x, int y)
{
    input_lock(ictx);
    MP_TRACE(ictx, "mouse move %d/%d\n", x, y);

    if (ictx->mouse_vo_x == x && ictx->mouse_vo_y == y) {
        input_unlock(ictx);
        return;
    }

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

    ictx->mouse_event_counter++;
    ictx->mouse_vo_x = x;
    ictx->mouse_vo_y = y;

    update_mouse_section(ictx);
    struct mp_cmd *cmd = get_cmd_from_keys(ictx, NULL, MP_KEY_MOUSE_MOVE);
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
            mp_input_queue_cmd(ictx, cmd);
        }
    }
    input_unlock(ictx);
}

unsigned int mp_input_get_mouse_event_counter(struct input_ctx *ictx)
{
    // Make the frontend always display the mouse cursor (as long as it's not
    // forced invisible) if mouse input is desired.
    input_lock(ictx);
    if (mp_input_test_mouse_active(ictx, ictx->mouse_x, ictx->mouse_y))
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
        *time = FFMIN(*time, 1.0 / opts->ar_rate);
        *time = FFMIN(*time, opts->ar_delay / 1000.0);
    }
}

static bool test_abort_cmd(struct input_ctx *ictx, struct mp_cmd *new)
{
    if (!mp_input_is_maybe_abort_cmd(new))
        return false;
    if (mp_input_is_abort_cmd(new))
        return true;
    // Abort only if there are going to be at least 2 commands in the queue.
    for (struct mp_cmd *cmd = ictx->cmd_queue.first; cmd; cmd = cmd->queue_next) {
        if (mp_input_is_maybe_abort_cmd(cmd))
            return true;
    }
    return false;
}

int mp_input_queue_cmd(struct input_ctx *ictx, mp_cmd_t *cmd)
{
    input_lock(ictx);
    if (cmd) {
        if (ictx->cancel && test_abort_cmd(ictx, cmd))
            ictx->cancel(ictx->cancel_ctx);
        queue_add_tail(&ictx->cmd_queue, cmd);
        mp_input_wakeup(ictx);
    }
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
        int64_t t = mp_time_us();
        if (ictx->last_ar + 2000000 < t)
            ictx->last_ar = t;
        // First time : wait delay
        if (ictx->ar_state == 0
            && (t - ictx->last_key_down_time) >= opts->ar_delay * 1000)
        {
            ictx->ar_state = 1;
            ictx->last_ar = ictx->last_key_down_time + opts->ar_delay * 1000;
            // Then send rate / sec event
        } else if (ictx->ar_state == 1
                   && (t - ictx->last_ar) >= 1000000 / opts->ar_rate) {
            ictx->last_ar += 1000000 / opts->ar_rate;
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

void mp_input_get_mouse_pos(struct input_ctx *ictx, int *x, int *y)
{
    input_lock(ictx);
    *x = ictx->mouse_x;
    *y = ictx->mouse_y;
    input_unlock(ictx);
}

// If name is NULL, return "default".
// Return a statically allocated name of the section (i.e. return value never
// gets deallocated).
static char *normalize_section(struct input_ctx *ictx, char *name)
{
    return get_bind_section(ictx, bstr0(name))->section;
}

void mp_input_disable_section(struct input_ctx *ictx, char *name)
{
    input_lock(ictx);
    name = normalize_section(ictx, name);

    // Remove old section, or make sure it's on top if re-enabled
    for (int i = ictx->num_active_sections - 1; i >= 0; i--) {
        struct active_section *as = &ictx->active_sections[i];
        if (strcmp(as->name, name) == 0) {
            MP_TARRAY_REMOVE_AT(ictx->active_sections,
                                ictx->num_active_sections, i);
        }
    }
    input_unlock(ictx);
}

void mp_input_enable_section(struct input_ctx *ictx, char *name, int flags)
{
    input_lock(ictx);
    name = normalize_section(ictx, name);

    mp_input_disable_section(ictx, name);

    MP_TRACE(ictx, "enable section '%s'\n", name);

    if (ictx->num_active_sections < MAX_ACTIVE_SECTIONS) {
        int top = ictx->num_active_sections;
        if (!(flags & MP_INPUT_ON_TOP)) {
            // insert before the first top entry
            for (top = 0; top < ictx->num_active_sections; top++) {
                if (ictx->active_sections[top].flags & MP_INPUT_ON_TOP)
                    break;
            }
            for (int n = ictx->num_active_sections; n > top; n--)
                ictx->active_sections[n] = ictx->active_sections[n - 1];
        }
        ictx->active_sections[top] = (struct active_section){name, flags};
        ictx->num_active_sections++;
    }

    MP_TRACE(ictx, "active section stack:\n");
    for (int n = 0; n < ictx->num_active_sections; n++) {
        MP_TRACE(ictx, " %s %d\n", ictx->active_sections[n].name,
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

static bool test_mouse(struct input_ctx *ictx, int x, int y, int rej_flags)
{
    input_lock(ictx);
    bool res = false;
    for (int i = 0; i < ictx->num_active_sections; i++) {
        struct active_section *as = &ictx->active_sections[i];
        if (as->flags & rej_flags)
            continue;
        struct cmd_bind_section *s = get_bind_section(ictx, bstr0(as->name));
        if (s->mouse_area_set && test_rect(&s->mouse_area, x, y)) {
            res = true;
            break;
        }
    }
    input_unlock(ictx);
    return res;
}

bool mp_input_test_mouse_active(struct input_ctx *ictx, int x, int y)
{
    return test_mouse(ictx, x, y, MP_INPUT_ALLOW_HIDE_CURSOR);
}

bool mp_input_test_dragging(struct input_ctx *ictx, int x, int y)
{
    input_lock(ictx);
    bool r = !ictx->opts->allow_win_drag ||
                        test_mouse(ictx, x, y, MP_INPUT_ALLOW_VO_DRAGGING);
    input_unlock(ictx);
    return r;
}

static void bind_dealloc(struct cmd_bind *bind)
{
    talloc_free(bind->cmd);
    talloc_free(bind->location);
}

// builtin: if true, remove all builtin binds, else remove all user binds
static void remove_binds(struct cmd_bind_section *bs, bool builtin)
{
    for (int n = bs->num_binds - 1; n >= 0; n--) {
        if (bs->binds[n].is_builtin == builtin) {
            bind_dealloc(&bs->binds[n]);
            assert(bs->num_binds >= 1);
            bs->binds[n] = bs->binds[bs->num_binds - 1];
            bs->num_binds--;
        }
    }
}

void mp_input_define_section(struct input_ctx *ictx, char *name, char *location,
                             char *contents, bool builtin, char *owner)
{
    if (!name || !name[0])
        return; // parse_config() changes semantics with restrict_section==empty
    input_lock(ictx);
    // Delete:
    struct cmd_bind_section *bs = get_bind_section(ictx, bstr0(name));
    if ((!bs->owner || (owner && strcmp(bs->owner, owner) != 0)) &&
        strcmp(bs->section, "default") != 0)
    {
        talloc_free(bs->owner);
        bs->owner = talloc_strdup(bs, owner);
    }
    remove_binds(bs, builtin);
    if (contents && contents[0]) {
        // Redefine:
        parse_config(ictx, builtin, bstr0(contents), location, name);
    } else {
        // Disable:
        mp_input_disable_section(ictx, name);
    }
    input_unlock(ictx);
}

void mp_input_remove_sections_by_owner(struct input_ctx *ictx, char *owner)
{
    input_lock(ictx);
    struct cmd_bind_section *bs = ictx->cmd_bind_sections;
    while (bs) {
        if (bs->owner && owner && strcmp(bs->owner, owner) == 0) {
            mp_input_disable_section(ictx, bs->section);
            remove_binds(bs, false);
            remove_binds(bs, true);
        }
        bs = bs->next;
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
                      const char *loc)
{
    struct cmd_bind_section *bs = get_bind_section(ictx, section);
    struct cmd_bind *bind = NULL;

    assert(num_keys <= MP_MAX_KEY_DOWN);

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
        .owner = bs,
        .is_builtin = builtin,
        .num_keys = num_keys,
    };
    memcpy(bind->keys, keys, num_keys * sizeof(bind->keys[0]));
    if (mp_msg_test(ictx->log, MSGL_DEBUG)) {
        char *s = mp_input_get_key_combo_name(keys, num_keys);
        MP_TRACE(ictx, "add: section='%s' key='%s'%s cmd='%s' location='%s'\n",
                 bind->owner->section, s, bind->is_builtin ? " builtin" : "",
                 bind->cmd, bind->location);
        talloc_free(s);
    }
}

// restrict_section: every entry is forced to this section name
//                   if NULL, load normally and allow any sections
static int parse_config(struct input_ctx *ictx, bool builtin, bstr data,
                        const char *location, const char *restrict_section)
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
        if (!mp_input_get_keys_from_string(name, MP_MAX_KEY_DOWN, &num_keys, keys))
        {
            talloc_free(name);
            MP_ERR(ictx, "Unknown key '%.*s' at %s\n", BSTR_P(keyname), cur_loc);
            continue;
        }
        talloc_free(name);

        bstr section = bstr0(restrict_section);
        if (!section.len) {
            if (bstr_startswith0(command, "{")) {
                int p = bstrchr(command, '}');
                if (p != -1) {
                    section = bstr_strip(bstr_splice(command, 1, p));
                    command = bstr_lstrip(bstr_cut(command, p + 1));
                }
            }
        }

        bind_keys(ictx, builtin, section, keys, num_keys, command, cur_loc);
        n_binds++;

        // Print warnings if invalid commands are encountered.
        talloc_free(mp_input_parse_cmd(ictx, command, cur_loc));
    }

    talloc_free(cur_loc);

    return n_binds;
}

static int parse_config_file(struct input_ctx *ictx, char *file, bool warn)
{
    int r = 0;
    void *tmp = talloc_new(NULL);
    stream_t *s = NULL;

    file = mp_get_user_path(tmp, ictx->global, file);

    s = stream_open(file, ictx->global);
    if (!s) {
        MP_ERR(ictx, "Can't open input config file %s.\n", file);
        goto done;
    }
    stream_skip_bom(s);
    bstr data = stream_read_complete(s, tmp, 1000000);
    if (data.start) {
        MP_VERBOSE(ictx, "Parsing input config file %s\n", file);
        int num = parse_config(ictx, false, data, file, NULL);
        MP_VERBOSE(ictx, "Input config file %s parsed: %d binds\n", file, num);
        r = 1;
    } else {
        MP_ERR(ictx, "Error reading input config file %s\n", file);
    }

done:
    free_stream(s);
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
        .mouse_section = "default",
        .opts_cache = m_config_cache_alloc(ictx, global, &input_config),
        .wakeup_cb = wakeup_cb,
        .wakeup_ctx = wakeup_ctx,
    };

    ictx->opts = ictx->opts_cache->opts;

    mpthread_mutex_init_recursive(&ictx->mutex);

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

    if (ictx->using_ar != (opts->use_appleremote && !shutdown)) {
        ictx->using_ar = !ictx->using_ar;
        if (ictx->using_ar) {
            cocoa_init_apple_remote();
        } else {
            cocoa_uninit_apple_remote();
        }
    }

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
    while (builtin.len) {
        bstr line = bstr_getline(builtin, &builtin);
        bstr_eatstart0(&line, "#");
        if (!bstr_startswith0(line, " "))
            parse_config(ictx, true, line, "<builtin>", NULL);
    }

    bool config_ok = false;
    if (ictx->opts->config_file && ictx->opts->config_file[0])
        config_ok = parse_config_file(ictx, ictx->opts->config_file, true);
    if (!config_ok) {
        // Try global conf dir
        void *tmp = talloc_new(NULL);
        char **files = mp_find_all_config_files(tmp, ictx->global, "input.conf");
        for (int n = 0; files && files[n]; n++)
            parse_config_file(ictx, files[n], false);
        talloc_free(tmp);
    }

#if HAVE_WIN32_PIPES
    if (ictx->global->opts->input_file && *ictx->global->opts->input_file)
        mp_input_pipe_add(ictx, ictx->global->opts->input_file);
#endif

    input_unlock(ictx);
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
    pthread_mutex_destroy(&ictx->mutex);
    talloc_free(ictx);
}

void mp_input_set_cancel(struct input_ctx *ictx, void (*cb)(void *c), void *c)
{
    input_lock(ictx);
    ictx->cancel = cb;
    ictx->cancel_ctx = c;
    input_unlock(ictx);
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
    mp_input_queue_cmd(ictx, mp_input_parse_cmd_strv(ictx->log, cmd));
}

struct mp_input_src_internal {
    pthread_t thread;
    bool thread_running;
    bool init_done;

    char *cmd_buffer;
    size_t cmd_buffer_size;
    bool drop;
};

static struct mp_input_src *mp_input_add_src(struct input_ctx *ictx)
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

static void mp_input_src_kill(struct mp_input_src *src);

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
        mp_input_src_kill(src);
    }
}

static void mp_input_src_kill(struct mp_input_src *src)
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
                pthread_join(src->in->thread, NULL);
            if (src->uninit)
                src->uninit(src);
            talloc_free(src);
            return;
        }
    }
    abort();
}

void mp_input_src_init_done(struct mp_input_src *src)
{
    assert(!src->in->init_done);
    assert(src->in->thread_running);
    assert(pthread_equal(src->in->thread, pthread_self()));
    src->in->init_done = true;
    mp_rendezvous(&src->in->init_done, 0);
}

static void *input_src_thread(void *ptr)
{
    void **args = ptr;
    struct mp_input_src *src = args[0];
    void (*loop_fn)(struct mp_input_src *src, void *ctx) = args[1];
    void *ctx = args[2];

    mpthread_set_name("input source");

    src->in->thread_running = true;

    loop_fn(src, ctx);

    if (!src->in->init_done)
        mp_rendezvous(&src->in->init_done, -1);

    return NULL;
}

int mp_input_add_thread_src(struct input_ctx *ictx, void *ctx,
    void (*loop_fn)(struct mp_input_src *src, void *ctx))
{
    struct mp_input_src *src = mp_input_add_src(ictx);
    if (!src)
        return -1;

    void *args[] = {src, loop_fn, ctx};
    if (pthread_create(&src->in->thread, NULL, input_src_thread, args)) {
        mp_input_src_kill(src);
        return -1;
    }
    if (mp_rendezvous(&src->in->init_done, 0) < 0) {
        mp_input_src_kill(src);
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
                if (cmd)
                    mp_input_queue_cmd(src->input_ctx, cmd);
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

