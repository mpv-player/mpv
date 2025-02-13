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

#include <float.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include <sys/types.h>

#include <ass/ass.h>
#include <libavutil/avstring.h>
#include <libavutil/common.h>
#include <libavutil/timecode.h>

#include "mpv_talloc.h"
#include "client.h"
#include "clipboard/clipboard.h"
#include "external_files.h"
#include "common/av_common.h"
#include "common/codecs.h"
#include "common/msg.h"
#include "common/msg_control.h"
#include "common/stats.h"
#include "filters/f_decoder_wrapper.h"
#include "command.h"
#include "osdep/threads.h"
#include "osdep/timer.h"
#include "common/common.h"
#include "input/input.h"
#include "input/keycodes.h"
#include "sub/osd_state.h"
#include "stream/stream.h"
#include "demux/demux.h"
#include "demux/stheader.h"
#include "common/playlist.h"
#include "sub/dec_sub.h"
#include "sub/osd.h"
#include "sub/sd.h"
#include "options/m_option.h"
#include "options/m_property.h"
#include "options/m_config_frontend.h"
#include "options/parse_configfile.h"
#include "osdep/getpid.h"
#include "video/out/vo.h"
#include "video/csputils.h"
#include "video/hwdec.h"
#include "audio/aframe.h"
#include "audio/format.h"
#include "audio/out/ao.h"
#include "video/out/bitmap_packer.h"
#include "options/path.h"
#include "screenshot.h"
#include "misc/dispatch.h"
#include "misc/language.h"
#include "misc/node.h"
#include "misc/thread_pool.h"
#include "misc/thread_tools.h"

#include "osdep/io.h"
#include "osdep/subprocess.h"
#include "osdep/terminal.h"

#include "core.h"

#ifdef _WIN32
#include <windows.h>
#endif

struct command_ctx {
    // All properties, terminated with a {0} item.
    struct m_property *properties;

    double last_seek_time;
    double last_seek_pts;
    double marked_pts;
    bool marked_permanent;

    char **warned_deprecated;
    int num_warned_deprecated;

    bool command_opts_processed;

    struct overlay *overlays;
    int num_overlays;
    // One of these is in use by the OSD; the other one exists so that the
    // bitmap list can be manipulated without additional synchronization.
    struct sub_bitmaps overlay_osd[2];
    int overlay_osd_current;
    struct bitmap_packer *overlay_packer;

    struct hook_handler **hooks;
    int num_hooks;
    int64_t hook_seq; // for hook_handler.seq

    struct ao_hotplug *hotplug;

    struct mp_cmd_ctx *cache_dump_cmd; // in progress cache dumping

    char **script_props;
    mpv_node udata;
    mpv_node mdata;

    double cached_window_scale;
};

static const struct m_option script_props_type = {
    .type = &m_option_type_keyvalue_list
};

static const struct m_option udata_type = {
    .type = CONF_TYPE_NODE
};

static const struct m_option mdata_type = {
    .type = CONF_TYPE_NODE
};

struct overlay {
    struct mp_image *source;
    int x, y;
    int dw, dh;
};

struct hook_handler {
    char *client;   // client mpv_handle name (for logging)
    int64_t client_id; // client mpv_handle ID
    char *type;     // kind of hook, e.g. "on_load"
    uint64_t user_id; // user-chosen ID
    int priority;   // priority for global hook order
    int64_t seq;    // unique ID, != 0, also for fixed order on equal priorities
    bool active;    // hook is currently in progress (only 1 at a time for now)
};

enum load_action_type {
    LOAD_TYPE_REPLACE,
    LOAD_TYPE_INSERT_AT,
    LOAD_TYPE_INSERT_NEXT,
    LOAD_TYPE_APPEND,
};

struct load_action {
    enum load_action_type type;
    bool play;
};

// U+00A0 NO-BREAK SPACE
#define NBSP "\xc2\xa0"

const char list_current[] = BLACK_CIRCLE NBSP;
const char list_normal[] = WHITE_CIRCLE NBSP;

static int edit_filters(struct MPContext *mpctx, struct mp_log *log,
                        enum stream_type mediatype,
                        const char *cmd, const char *arg);
static int set_filters(struct MPContext *mpctx, enum stream_type mediatype,
                       struct m_obj_settings *new_chain);

static bool is_property_set(int action, void *val);

static void hook_remove(struct MPContext *mpctx, struct hook_handler *h)
{
    struct command_ctx *cmd = mpctx->command_ctx;
    for (int n = 0; n < cmd->num_hooks; n++) {
        if (cmd->hooks[n] == h) {
            talloc_free(cmd->hooks[n]);
            MP_TARRAY_REMOVE_AT(cmd->hooks, cmd->num_hooks, n);
            return;
        }
    }
    MP_ASSERT_UNREACHABLE();
}

bool mp_hook_test_completion(struct MPContext *mpctx, char *type)
{
    struct command_ctx *cmd = mpctx->command_ctx;
    for (int n = 0; n < cmd->num_hooks; n++) {
        struct hook_handler *h = cmd->hooks[n];
        if (h->active && strcmp(h->type, type) == 0) {
            if (!mp_client_id_exists(mpctx, h->client_id)) {
                MP_WARN(mpctx, "client removed during hook handling\n");
                // Trigger completion of this hook and continue with the next one.
                mp_hook_continue(mpctx, h->client_id, h->seq);
                hook_remove(mpctx, h);
            }
            return false;
        }
    }
    return true;
}

static int invoke_hook_handler(struct MPContext *mpctx, struct hook_handler *h)
{
    MP_VERBOSE(mpctx, "Running hook: %s/%s\n", h->client, h->type);
    h->active = true;

    uint64_t reply_id = 0;
    mpv_event_hook *m = talloc_ptrtype(NULL, m);
    *m = (mpv_event_hook){
        .name = talloc_strdup(m, h->type),
        .id = h->seq,
    },
    reply_id = h->user_id;
    char *name = mp_tprintf(22, "@%"PRIi64, h->client_id);
    int r = mp_client_send_event(mpctx, name, reply_id, MPV_EVENT_HOOK, m);
    if (r < 0) {
        MP_MSG(mpctx, mp_client_id_exists(mpctx, h->client_id) ? MSGL_WARN : MSGL_V,
               "Failed sending hook command %s/%s. Removing hook.\n", h->client,
               h->type);
        hook_remove(mpctx, h);
        mp_wakeup_core(mpctx); // repeat next iteration to finish
    }
    return r;
}

static int run_next_hook_handler(struct MPContext *mpctx, char *type, int start)
{
    struct command_ctx *cmd = mpctx->command_ctx;

    for (int n = start; n < cmd->num_hooks; n++) {
        struct hook_handler *h = cmd->hooks[n];
        if (strcmp(h->type, type) == 0) {
            int ret = invoke_hook_handler(mpctx, h);
            // Repeat until the hook is successfully started or none are left.
            if (ret < 0) {
                --n;
                continue;
            }
            return ret;
        }
    }

    mp_wakeup_core(mpctx); // finished hook
    return 0;
}

// Start processing script/client API hooks. This is asynchronous, and the
// caller needs to use mp_hook_test_completion() to check whether they're done.
void mp_hook_start(struct MPContext *mpctx, char *type)
{
    run_next_hook_handler(mpctx, type, 0);
}

int mp_hook_continue(struct MPContext *mpctx, int64_t client_id, uint64_t id)
{
    struct command_ctx *cmd = mpctx->command_ctx;

    for (int n = 0; n < cmd->num_hooks; n++) {
        struct hook_handler *h = cmd->hooks[n];
        if (h->client_id == client_id && h->seq == id) {
            if (!h->active)
                break;
            h->active = false;
            return run_next_hook_handler(mpctx, h->type, n + 1);
        }
    }

    MP_ERR(mpctx, "invalid hook API usage\n");
    return MPV_ERROR_INVALID_PARAMETER;
}

static int compare_hook(const void *pa, const void *pb)
{
    struct hook_handler **h1 = (void *)pa;
    struct hook_handler **h2 = (void *)pb;
    if ((*h1)->priority != (*h2)->priority)
        return (*h1)->priority - (*h2)->priority;
    return (*h1)->seq - (*h2)->seq;
}

void mp_hook_add(struct MPContext *mpctx, char *client, int64_t client_id,
                 const char *name, uint64_t user_id, int pri)
{
    struct command_ctx *cmd = mpctx->command_ctx;
    struct hook_handler *h = talloc_ptrtype(cmd, h);
    int64_t seq = ++cmd->hook_seq;
    *h = (struct hook_handler){
        .client = talloc_strdup(h, client),
        .client_id = client_id,
        .type = talloc_strdup(h, name),
        .user_id = user_id,
        .priority = pri,
        .seq = seq,
    };
    MP_TARRAY_APPEND(cmd, cmd->hooks, cmd->num_hooks, h);
    qsort(cmd->hooks, cmd->num_hooks, sizeof(cmd->hooks[0]), compare_hook);
}

// Call before a seek, in order to allow revert-seek to undo the seek.
void mark_seek(struct MPContext *mpctx)
{
    struct command_ctx *cmd = mpctx->command_ctx;
    double now = mp_time_sec();
    if (now > cmd->last_seek_time + 2.0 || cmd->last_seek_pts == MP_NOPTS_VALUE)
        cmd->last_seek_pts = get_current_time(mpctx);
    cmd->last_seek_time = now;
}

static char *append_selected_style(struct MPContext *mpctx, char *str)
{
    if (!mpctx->video_out || !mpctx->opts->video_osd)
        return talloc_strdup_append(str, TERM_ESC_REVERSE_COLORS);

    return talloc_asprintf_append(str,
               "%s{\\b1\\1c&H%02hhx%02hhx%02hhx&\\1a&H%02hhx&\\3c&H%02hhx%02hhx%02hhx&\\3a&H%02hhx&}%s",
               OSD_ASS_0,
               mpctx->video_out->osd->opts->osd_selected_color.b,
               mpctx->video_out->osd->opts->osd_selected_color.g,
               mpctx->video_out->osd->opts->osd_selected_color.r,
               255 - mpctx->video_out->osd->opts->osd_selected_color.a,
               mpctx->video_out->osd->opts->osd_selected_outline_color.b,
               mpctx->video_out->osd->opts->osd_selected_outline_color.g,
               mpctx->video_out->osd->opts->osd_selected_outline_color.r,
               255 - mpctx->video_out->osd->opts->osd_selected_outline_color.a,
               OSD_ASS_1);
}

static const char *get_style_reset(struct MPContext *mpctx)
{
    return mpctx->video_out && mpctx->opts->video_osd
        ? OSD_ASS_0"{\\r}"OSD_ASS_1
        : TERM_ESC_CLEAR_COLORS;
}

static char *skip_n_lines(char *text, int lines)
{
    while (text && lines > 0) {
        char *next = strchr(text, '\n');
        text = next ? next + 1 : NULL;
        lines--;
    }
    return text;
}

static int count_lines(char *text)
{
    if (!text[0])
        return 0;

    int count = 0;
    while (text) {
        count++;
        char *next = strchr(text, '\n');
        if (!next || (next[0] == '\n' && !next[1]))
            break;
        text = next + 1;
    }
    return count;
}

// Given a huge string separated by new lines, attempts to cut off text above
// the current line to keep the line visible, and below to keep rendering
// performance up. pos gives the current line (0 for the first line).
// "text" might be returned as is, or it can be freed and a new allocation is
// returned.
// This is only a heuristic - we can't deal with line breaking.
static char *cut_osd_list(struct MPContext *mpctx, char *header, char *text, int pos)
{
    int count = count_lines(text);
    if (!count)
        return text;

    int max_lines;
    if (mpctx->video_out && mpctx->opts->video_osd) {
        int screen_h, font_h;
        osd_get_text_size(mpctx->osd, &screen_h, &font_h);
        max_lines = screen_h / MPMAX(font_h, 1);
    } else {
        int w = -1;
        max_lines = 24;
        terminal_get_size(&w, &max_lines);
        char *msg = mp_property_expand_escaped_string(mpctx, mpctx->opts->status_msg);
        max_lines -= msg[0] ? count_lines(msg) : 1;
        talloc_free(msg);
    }
    // Subtract 1 for the header.
    max_lines--;

    char *new = talloc_asprintf(NULL, "%s [%d/%d]:\n", header, pos + 1, count);
    int start = MPMIN(MPMAX(pos - max_lines / 2, 0), count - max_lines);
    char *head = skip_n_lines(text, start);
    char *tail = skip_n_lines(head, max_lines);
    new = talloc_asprintf_append_buffer(new, "%.*s",
                            (int)(tail ? tail - head : strlen(head)), head);
    // Strip the final newline to not print it in the terminal.
    new[strlen(new) - 1] = '\0';

    talloc_free(text);
    return new;
}

static char *format_delay(double time)
{
    return talloc_asprintf(NULL, "%.f ms", time * 1000);
}

// Property-option bridge. (Maps the property to the option with the same name.)
static int mp_property_generic_option(void *ctx, struct m_property *prop,
                                      int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct m_config_option *opt =
        m_config_get_co(mpctx->mconfig, bstr0(prop->name));

    if (!opt)
        return M_PROPERTY_UNKNOWN;

    switch (action) {
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = *(opt->opt);
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
        if (!opt->data)
            return M_PROPERTY_NOT_IMPLEMENTED;
        m_option_copy(opt->opt, arg, opt->data);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
        if (m_config_set_option_raw(mpctx->mconfig, opt, arg, 0) < 0)
            return M_PROPERTY_ERROR;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Playback speed (RW)
static int mp_property_playback_speed(void *ctx, struct m_property *prop,
                                      int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (action == M_PROPERTY_PRINT || action == M_PROPERTY_FIXED_LEN_PRINT) {
        *(char **)arg = mp_format_double(NULL, mpctx->opts->playback_speed, 2,
                                         false, false, action != M_PROPERTY_FIXED_LEN_PRINT);
        return M_PROPERTY_OK;
    }
    return mp_property_generic_option(mpctx, prop, action, arg);
}

/// Playback pitch (RW)
static int mp_property_playback_pitch(void *ctx, struct m_property *prop,
                                      int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (action == M_PROPERTY_PRINT || action == M_PROPERTY_FIXED_LEN_PRINT) {
        *(char **)arg = mp_format_double(NULL, mpctx->opts->playback_pitch, 2,
                                         false, false, action != M_PROPERTY_FIXED_LEN_PRINT);
        return M_PROPERTY_OK;
    }
    return mp_property_generic_option(mpctx, prop, action, arg);
}

static int mp_property_av_speed_correction(void *ctx, struct m_property *prop,
                                           int action, void *arg)
{
    MPContext *mpctx = ctx;
    char *type = prop->priv;
    double val = 0;
    switch (type[0]) {
    case 'a': val = mpctx->speed_factor_a; break;
    case 'v': val = mpctx->speed_factor_v; break;
    default: MP_ASSERT_UNREACHABLE();
    }

    if (action == M_PROPERTY_PRINT || action == M_PROPERTY_FIXED_LEN_PRINT) {
        *(char **)arg = mp_format_double(NULL, (val - 1) * 100, 2, true,
                                         true, action != M_PROPERTY_FIXED_LEN_PRINT);
        return M_PROPERTY_OK;
    }

    return m_property_double_ro(action, arg, val);
}

static int mp_property_display_sync_active(void *ctx, struct m_property *prop,
                                           int action, void *arg)
{
    MPContext *mpctx = ctx;
    return m_property_bool_ro(action, arg, mpctx->display_sync_active);
}

static int mp_property_pid(void *ctx, struct m_property *prop,
                           int action, void *arg)
{
    // 32 bit on linux/windows - which C99 `int' is not guaranteed to hold
    return m_property_int64_ro(action, arg, mp_getpid());
}

/// filename with path (RO)
static int mp_property_path(void *ctx, struct m_property *prop,
                            int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->filename)
        return M_PROPERTY_UNAVAILABLE;
    char *path = mp_normalize_path(NULL, mpctx->filename);
    int r = m_property_strdup_ro(action, arg, path);
    talloc_free(path);
    return r;
}

static int mp_property_filename(void *ctx, struct m_property *prop,
                                int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->filename)
        return M_PROPERTY_UNAVAILABLE;
    char *filename = talloc_strdup(NULL, mpctx->filename);
    if (mp_is_url(bstr0(filename)))
        mp_url_unescape_inplace(filename);
    char *f = (char *)mp_basename(filename);
    if (!f[0])
        f = filename;
    if (action == M_PROPERTY_KEY_ACTION) {
        struct m_property_action_arg *ka = arg;
        if (strcmp(ka->key, "no-ext") == 0) {
            action = ka->action;
            arg = ka->arg;
            bstr root;
            if (mp_splitext(f, &root))
                f = bstrto0(filename, root);
        }
    }
    int r = m_property_strdup_ro(action, arg, f);
    talloc_free(filename);
    return r;
}

static int mp_property_stream_open_filename(void *ctx, struct m_property *prop,
                                            int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->stream_open_filename || !mpctx->playing)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_SET: {
        if (mpctx->demuxer)
            return M_PROPERTY_ERROR;
        mpctx->stream_open_filename =
            talloc_strdup(mpctx->stream_open_filename, *(char **)arg);
        mp_notify_property(mpctx, prop->name);
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_GET_TYPE:
    case M_PROPERTY_GET:
        return m_property_strdup_ro(action, arg, mpctx->stream_open_filename);
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_file_size(void *ctx, struct m_property *prop,
                                 int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->demuxer)
        return M_PROPERTY_UNAVAILABLE;

    int64_t size = mpctx->demuxer->filesize;
    if (size < 0)
        return M_PROPERTY_UNAVAILABLE;

    if (action == M_PROPERTY_PRINT) {
        *(char **)arg = format_file_size(size);
        return M_PROPERTY_OK;
    }
    return m_property_int64_ro(action, arg, size);
}

static int mp_property_media_title(void *ctx, struct m_property *prop,
                                   int action, void *arg)
{
    MPContext *mpctx = ctx;
    const char *name = mp_find_non_filename_media_title(mpctx);
    if (name && name[0])
        return m_property_strdup_ro(action, arg, name);
    return mp_property_filename(ctx, prop, action, arg);
}

static int mp_property_stream_path(void *ctx, struct m_property *prop,
                                   int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->demuxer || !mpctx->demuxer->filename)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_strdup_ro(action, arg, mpctx->demuxer->filename);
}

/// Demuxer name (RO)
static int mp_property_demuxer(void *ctx, struct m_property *prop,
                               int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_strdup_ro(action, arg, demuxer->desc->name);
}

static int mp_property_file_format(void *ctx, struct m_property *prop,
                                   int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return M_PROPERTY_UNAVAILABLE;
    const char *name = demuxer->filetype ? demuxer->filetype : demuxer->desc->name;
    return m_property_strdup_ro(action, arg, name);
}

static int mp_property_stream_pos(void *ctx, struct m_property *prop,
                                  int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer || demuxer->filepos < 0)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_int64_ro(action, arg, demuxer->filepos);
}

/// Stream end offset (RO)
static int mp_property_stream_end(void *ctx, struct m_property *prop,
                                  int action, void *arg)
{
    return mp_property_file_size(ctx, prop, action, arg);
}

// Does some magic to handle "<name>/full" as time formatted with milliseconds.
// Assumes prop is the type of the actual property.
static int property_time(int action, void *arg, double time)
{
    if (time == MP_NOPTS_VALUE)
        return M_PROPERTY_UNAVAILABLE;

    const struct m_option time_type = {.type = CONF_TYPE_TIME};
    switch (action) {
    case M_PROPERTY_GET:
        *(double *)arg = time;
        return M_PROPERTY_OK;
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = time_type;
        return M_PROPERTY_OK;
    case M_PROPERTY_KEY_ACTION: {
        struct m_property_action_arg *ka = arg;

        if (strcmp(ka->key, "full") != 0)
            return M_PROPERTY_UNKNOWN;

        switch (ka->action) {
        case M_PROPERTY_GET:
            *(double *)ka->arg = time;
            return M_PROPERTY_OK;
        case M_PROPERTY_PRINT:
            *(char **)ka->arg = mp_format_time(time, true);
            return M_PROPERTY_OK;
        case M_PROPERTY_GET_TYPE:
            *(struct m_option *)ka->arg = time_type;
            return M_PROPERTY_OK;
        }
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_duration(void *ctx, struct m_property *prop,
                                int action, void *arg)
{
    MPContext *mpctx = ctx;
    double len = get_time_length(mpctx);

    if (len < 0)
        return M_PROPERTY_UNAVAILABLE;

    return property_time(action, arg, len);
}

static int mp_property_avsync(void *ctx, struct m_property *prop,
                              int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->ao_chain || !mpctx->vo_chain)
        return M_PROPERTY_UNAVAILABLE;
    if (action == M_PROPERTY_PRINT || action == M_PROPERTY_FIXED_LEN_PRINT) {
        *(char **)arg = mp_format_double(NULL, mpctx->last_av_difference, 4,
                                         true, false, action != M_PROPERTY_FIXED_LEN_PRINT);
        return M_PROPERTY_OK;
    }
    return m_property_double_ro(action, arg, mpctx->last_av_difference);
}

static int mp_property_total_avsync_change(void *ctx, struct m_property *prop,
                                           int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->ao_chain || !mpctx->vo_chain)
        return M_PROPERTY_UNAVAILABLE;
    if (mpctx->total_avsync_change == MP_NOPTS_VALUE)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_double_ro(action, arg, mpctx->total_avsync_change);
}

static int mp_property_frame_drop_dec(void *ctx, struct m_property *prop,
                                      int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct mp_decoder_wrapper *dec = mpctx->vo_chain && mpctx->vo_chain->track
        ? mpctx->vo_chain->track->dec : NULL;
    if (!dec)
        return M_PROPERTY_UNAVAILABLE;

    return m_property_int_ro(action, arg,
                             mp_decoder_wrapper_get_frames_dropped(dec));
}

static int mp_property_mistimed_frame_count(void *ctx, struct m_property *prop,
                                            int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->vo_chain || !mpctx->display_sync_active)
        return M_PROPERTY_UNAVAILABLE;

    return m_property_int_ro(action, arg, mpctx->mistimed_frames_total);
}

static int mp_property_vsync_ratio(void *ctx, struct m_property *prop,
                                   int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->vo_chain || !mpctx->display_sync_active)
        return M_PROPERTY_UNAVAILABLE;

    int vsyncs = 0, frames = 0;
    for (int n = 0; n < mpctx->num_past_frames; n++) {
        int vsync = mpctx->past_frames[n].num_vsyncs;
        if (vsync < 0)
            break;
        vsyncs += vsync;
        frames += 1;
    }

    if (!frames)
        return M_PROPERTY_UNAVAILABLE;

    return m_property_double_ro(action, arg, vsyncs / (double)frames);
}

static int mp_property_frame_drop_vo(void *ctx, struct m_property *prop,
                                     int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->vo_chain)
        return M_PROPERTY_UNAVAILABLE;

    return m_property_int_ro(action, arg, vo_get_drop_count(mpctx->video_out));
}

static int mp_property_vo_delayed_frame_count(void *ctx, struct m_property *prop,
                                              int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->vo_chain)
        return M_PROPERTY_UNAVAILABLE;

    return m_property_int_ro(action, arg, vo_get_delayed_count(mpctx->video_out));
}

/// Current position in percent (RW)
static int mp_property_percent_pos(void *ctx, struct m_property *prop,
                                   int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->playback_initialized)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_SET: {
        double pos = *(double *)arg;
        queue_seek(mpctx, MPSEEK_FACTOR, pos / 100.0, MPSEEK_DEFAULT, 0);
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_GET: {
        double pos = get_current_pos_ratio(mpctx, false) * 100.0;
        if (pos < 0)
            return M_PROPERTY_UNAVAILABLE;
        *(double *)arg = pos;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){
            .type = CONF_TYPE_DOUBLE,
            .min = 0,
            .max = 100,
        };
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT: {
        double pos = get_current_pos_ratio(mpctx, false);
        if (pos < 0)
            return M_PROPERTY_UNAVAILABLE;
        *(char **)arg = talloc_asprintf(NULL, "%.f", pos * 100);
        return M_PROPERTY_OK;
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_time_start(void *ctx, struct m_property *prop,
                                  int action, void *arg)
{
    // minor backwards-compat.
    return property_time(action, arg, 0);
}

/// Current position in seconds (RW)
static int mp_property_time_pos(void *ctx, struct m_property *prop,
                                int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->playback_initialized)
        return M_PROPERTY_UNAVAILABLE;

    if (action == M_PROPERTY_SET) {
        queue_seek(mpctx, MPSEEK_ABSOLUTE, *(double *)arg, MPSEEK_DEFAULT, 0);
        return M_PROPERTY_OK;
    }
    return property_time(action, arg, get_playback_time(mpctx));
}

/// Current audio pts in seconds (R)
static int mp_property_audio_pts(void *ctx, struct m_property *prop,
                                int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->playback_initialized || mpctx->audio_status < STATUS_PLAYING ||
        mpctx->audio_status >= STATUS_EOF)
        return M_PROPERTY_UNAVAILABLE;

    return property_time(action, arg, playing_audio_pts(mpctx));
}

static bool time_remaining(MPContext *mpctx, double *remaining)
{
    double len = get_time_length(mpctx);
    double playback = get_playback_time(mpctx);

    if (playback == MP_NOPTS_VALUE || len <= 0)
        return false;

    *remaining = len - playback;

    return len >= 0;
}

static int mp_property_remaining(void *ctx, struct m_property *prop,
                                 int action, void *arg)
{
    double remaining;
    if (!time_remaining(ctx, &remaining))
        return M_PROPERTY_UNAVAILABLE;

    return property_time(action, arg, remaining);
}

static int mp_property_playtime_remaining(void *ctx, struct m_property *prop,
                                          int action, void *arg)
{
    MPContext *mpctx = ctx;
    double remaining;
    if (!time_remaining(mpctx, &remaining))
        return M_PROPERTY_UNAVAILABLE;

    double speed = mpctx->video_speed;
    return property_time(action, arg, remaining / speed);
}

static int mp_property_remaining_file_loops(void *ctx, struct m_property *prop,
        int action, void *arg)
{
    MPContext *mpctx = ctx;
    return m_property_int_ro(action, arg, mpctx->remaining_file_loops);
}

static int mp_property_remaining_ab_loops(void *ctx, struct m_property *prop,
        int action, void *arg)
{
    MPContext *mpctx = ctx;
    return m_property_int_ro(action, arg, mpctx->remaining_ab_loops);
}

/// Current chapter (RW)
static int mp_property_chapter(void *ctx, struct m_property *prop,
                               int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->playback_initialized)
        return M_PROPERTY_UNAVAILABLE;

    int chapter = get_current_chapter(mpctx);
    int num = get_chapter_count(mpctx);
    if (chapter < -1)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_GET:
        *(int *) arg = chapter;
        return M_PROPERTY_OK;
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){
            .type = CONF_TYPE_INT,
            .min = -1,
            .max = num - 1,
        };
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT: {
        *(char **) arg = chapter_display_name(mpctx, chapter);
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_SWITCH:
    case M_PROPERTY_SET: ;
        mark_seek(mpctx);
        int step_all;
        if (action == M_PROPERTY_SWITCH) {
            struct m_property_switch_arg *sarg = arg;
            step_all = lrint(sarg->inc);
            // Check threshold for relative backward seeks
            if (mpctx->opts->chapter_seek_threshold >= 0 && step_all < 0) {
                double current_chapter_start =
                    chapter_start_time(mpctx, chapter);
                // If we are far enough into a chapter, seek back to the
                // beginning of current chapter instead of previous one
                if (current_chapter_start != MP_NOPTS_VALUE &&
                    get_current_time(mpctx) - current_chapter_start >
                    mpctx->opts->chapter_seek_threshold)
                {
                    step_all++;
                }
            }
        } else // Absolute set
            step_all = *(int *)arg - chapter;
        chapter += step_all;
        if (chapter < 0) // avoid using -1 if first chapter starts at 0
            chapter = (chapter_start_time(mpctx, 0) <= 0) ? 0 : -1;
        if (chapter >= num && step_all > 0) {
            if (mpctx->opts->keep_open) {
                seek_to_last_frame(mpctx);
            } else {
                // semi-broken file; ignore for user convenience
                if (action == M_PROPERTY_SWITCH && num < 2)
                    return M_PROPERTY_UNAVAILABLE;
                if (!mpctx->stop_play)
                    mpctx->stop_play = PT_NEXT_ENTRY;
                mp_wakeup_core(mpctx);
            }
        } else {
            double pts = chapter_start_time(mpctx, chapter);
            if (pts != MP_NOPTS_VALUE) {
                queue_seek(mpctx, MPSEEK_CHAPTER, pts, MPSEEK_DEFAULT, 0);
                mpctx->last_chapter_seek = chapter;
                mpctx->last_chapter_flag = true;
            }
        }
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int get_chapter_entry(int item, int action, void *arg, void *ctx)
{
    struct MPContext *mpctx = ctx;
    char *name = chapter_name(mpctx, item);
    double time = chapter_start_time(mpctx, item);
    struct m_sub_property props[] = {
        {"title",       SUB_PROP_STR(name)},
        {"time",        {.type = CONF_TYPE_TIME}, {.time = time}},
        {0}
    };

    int r = m_property_read_sub(props, action, arg);
    return r;
}

static int parse_node_chapters(struct MPContext *mpctx,
                               struct mpv_node *given_chapters)
{
    if (!mpctx->demuxer)
        return M_PROPERTY_UNAVAILABLE;

    if (given_chapters->format != MPV_FORMAT_NODE_ARRAY)
        return M_PROPERTY_ERROR;

    double len = get_time_length(mpctx);

    talloc_free(mpctx->chapters);
    mpctx->num_chapters = 0;
    mpctx->chapters = talloc_array(NULL, struct demux_chapter, 0);

    for (int n = 0; n < given_chapters->u.list->num; n++) {
        struct mpv_node *chapter_data = &given_chapters->u.list->values[n];

        if (chapter_data->format != MPV_FORMAT_NODE_MAP)
            continue;

        mpv_node_list *chapter_data_elements = chapter_data->u.list;

        double time = -1;
        char *title = 0;

        for (int e = 0; e < chapter_data_elements->num; e++) {
            struct mpv_node *chapter_data_element =
                &chapter_data_elements->values[e];
            char *key = chapter_data_elements->keys[e];
            switch (chapter_data_element->format) {
            case MPV_FORMAT_INT64:
                if (strcmp(key, "time") == 0)
                    time = (double)chapter_data_element->u.int64;
                break;
            case MPV_FORMAT_DOUBLE:
                if (strcmp(key, "time") == 0)
                    time = chapter_data_element->u.double_;
                break;
            case MPV_FORMAT_STRING:
                if (strcmp(key, "title") == 0)
                    title = chapter_data_element->u.string;
                break;
            }
        }

        if (time >= 0 && time < len) {
            struct demux_chapter new = {
                .pts = time,
                .metadata = talloc_zero(mpctx->chapters, struct mp_tags),
            };
            if (title)
                mp_tags_set_str(new.metadata, "title", title);
            MP_TARRAY_APPEND(NULL, mpctx->chapters, mpctx->num_chapters, new);
        }
    }

    mp_notify(mpctx, MP_EVENT_CHAPTER_CHANGE, NULL);
    mp_notify_property(mpctx, "chapter-list");

    return M_PROPERTY_OK;
}

static int mp_property_list_chapters(void *ctx, struct m_property *prop,
                                     int action, void *arg)
{
    MPContext *mpctx = ctx;
    int count = get_chapter_count(mpctx);
    switch (action) {
    case M_PROPERTY_PRINT: {
        int cur = mpctx->playback_initialized ? get_current_chapter(mpctx) : -1;
        char *res = NULL;
        int n;

        if (count < 1) {
            res = talloc_asprintf_append(res, "No chapters.");
        }

        for (n = 0; n < count; n++) {
            if (n == cur)
                res = append_selected_style(mpctx, res);
            char *name = chapter_name(mpctx, n);
            double t = chapter_start_time(mpctx, n);
            char* time = mp_format_time(t, false);
            res = talloc_asprintf_append(res, "%s  %s%s\n", time, name,
                                         n == cur ? get_style_reset(mpctx) : "");
            talloc_free(time);
        }

        *(char **)arg = count ? cut_osd_list(mpctx, "Chapters", res, cur) : res;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_SET: {
        struct mpv_node *given_chapters = arg;
        return parse_node_chapters(mpctx, given_chapters);
    }
    }
    return m_property_read_list(action, arg, count, get_chapter_entry, mpctx);
}

static int mp_property_current_edition(void *ctx, struct m_property *prop,
                                      int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer || demuxer->num_editions <= 0)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(action, arg, demuxer->edition);
}

static int mp_property_edition(void *ctx, struct m_property *prop,
                               int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct demuxer *demuxer = mpctx->demuxer;
    char *name = NULL;

    if (!demuxer)
        return mp_property_generic_option(mpctx, prop, action, arg);

    int ed = demuxer->edition;

    if (demuxer->num_editions <= 1)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_GET_CONSTRICTED_TYPE: {
        *(struct m_option *)arg = (struct m_option){
            .type = CONF_TYPE_INT,
            .min = 0,
            .max = demuxer->num_editions - 1,
        };
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_PRINT: {
        if (ed < 0)
            return M_PROPERTY_UNAVAILABLE;
        name = mp_tags_get_str(demuxer->editions[ed].metadata, "title");
        if (name) {
            *(char **) arg = talloc_strdup(NULL, name);
        } else {
            *(char **) arg = talloc_asprintf(NULL, "%d", ed + 1);
        }
        return M_PROPERTY_OK;
    }
    default:
        return mp_property_generic_option(mpctx, prop, action, arg);
    }
}

static int get_edition_entry(int item, int action, void *arg, void *ctx)
{
    struct MPContext *mpctx = ctx;

    struct demuxer *demuxer = mpctx->demuxer;
    struct demux_edition *ed = &demuxer->editions[item];

    char *title = mp_tags_get_str(ed->metadata, "title");

    struct m_sub_property props[] = {
        {"id",          SUB_PROP_INT(item)},
        {"title",       SUB_PROP_STR(title),
                        .unavailable = !title},
        {"default",     SUB_PROP_BOOL(ed->default_edition)},
        {0}
    };

    return m_property_read_sub(props, action, arg);
}

static int property_list_editions(void *ctx, struct m_property *prop,
                                  int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return M_PROPERTY_UNAVAILABLE;

    if (action == M_PROPERTY_PRINT) {
        char *res = NULL;

        struct demux_edition *editions = demuxer->editions;
        int num_editions = demuxer->num_editions;
        int current = demuxer->edition;

        if (!num_editions)
            res = talloc_asprintf_append(res, "No editions.");

        for (int n = 0; n < num_editions; n++) {
            struct demux_edition *ed = &editions[n];

            if (n == current)
                res = append_selected_style(mpctx, res);
            res = talloc_asprintf_append(res, "%d: ", n);
            char *title = mp_tags_get_str(ed->metadata, "title");
            if (!title)
                title = "unnamed";
            res = talloc_asprintf_append(res, "'%s'%s\n", title,
                                         n == current ? get_style_reset(mpctx) : "");
        }

        *(char **)arg = res;
        return M_PROPERTY_OK;
    }
    return m_property_read_list(action, arg, demuxer->num_editions,
                                get_edition_entry, mpctx);
}

/// Number of chapters in file
static int mp_property_chapters(void *ctx, struct m_property *prop,
                                int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->playback_initialized)
        return M_PROPERTY_UNAVAILABLE;
    int count = get_chapter_count(mpctx);
    return m_property_int_ro(action, arg, count);
}

static int mp_property_editions(void *ctx, struct m_property *prop,
                                int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return M_PROPERTY_UNAVAILABLE;
    if (demuxer->num_editions <= 0)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(action, arg, demuxer->num_editions);
}

static int get_tag_entry(int item, int action, void *arg, void *ctx)
{
    struct mp_tags *tags = ctx;

    struct m_sub_property props[] = {
        {"key",     SUB_PROP_STR(tags->keys[item])},
        {"value",   SUB_PROP_STR(tags->values[item])},
        {0}
    };

    return m_property_read_sub(props, action, arg);
}

// tags can be NULL for M_PROPERTY_GET_TYPE. (In all other cases, tags must be
// provided, even for M_PROPERTY_KEY_ACTION GET_TYPE sub-actions.)
static int tag_property(int action, void *arg, struct mp_tags *tags)
{
    switch (action) {
    case M_PROPERTY_GET_NODE: // same as GET, because type==mpv_node
    case M_PROPERTY_GET: {
        mpv_node_list *list = talloc_zero(NULL, mpv_node_list);
        mpv_node node = {
            .format = MPV_FORMAT_NODE_MAP,
            .u.list = list,
        };
        list->num = tags->num_keys;
        list->values = talloc_array(list, mpv_node, list->num);
        list->keys = talloc_array(list, char*, list->num);
        for (int n = 0; n < tags->num_keys; n++) {
            list->keys[n] = talloc_strdup(list, tags->keys[n]);
            list->values[n] = (struct mpv_node){
                .format = MPV_FORMAT_STRING,
                .u.string = talloc_strdup(list, tags->values[n]),
            };
        }
        *(mpv_node*)arg = node;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_GET_TYPE: {
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_NODE};
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_PRINT: {
        char *res = NULL;
        for (int n = 0; n < tags->num_keys; n++) {
            res = talloc_asprintf_append_buffer(res, "%s: %s\n",
                                                tags->keys[n], tags->values[n]);
        }
        if (!res)
            res = talloc_strdup(NULL, "(empty)");
        *(char **)arg = res;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_KEY_ACTION: {
        struct m_property_action_arg *ka = arg;
        bstr key;
        char *rem;
        m_property_split_path(ka->key, &key, &rem);
        if (bstr_equals0(key, "list")) {
            struct m_property_action_arg nka = *ka;
            nka.key = rem;
            return m_property_read_list(action, &nka, tags->num_keys,
                                        get_tag_entry, tags);
        }
        // Direct access without this prefix is allowed for compatibility.
        bstr k = bstr0(ka->key);
        bstr_eatstart0(&k, "by-key/");
        char *meta = mp_tags_get_bstr(tags, k);
        if (!meta)
            return M_PROPERTY_UNKNOWN;
        switch (ka->action) {
        case M_PROPERTY_GET:
            *(char **)ka->arg = talloc_strdup(NULL, meta);
            return M_PROPERTY_OK;
        case M_PROPERTY_GET_TYPE:
            *(struct m_option *)ka->arg = (struct m_option){
                .type = CONF_TYPE_STRING,
            };
            return M_PROPERTY_OK;
        }
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

/// Demuxer meta data
static int mp_property_metadata(void *ctx, struct m_property *prop,
                                int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return M_PROPERTY_UNAVAILABLE;

    return tag_property(action, arg, demuxer->metadata);
}

static int mp_property_filtered_metadata(void *ctx, struct m_property *prop,
                                         int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->filtered_tags)
        return M_PROPERTY_UNAVAILABLE;

    return tag_property(action, arg, mpctx->filtered_tags);
}

static int mp_property_chapter_metadata(void *ctx, struct m_property *prop,
                                        int action, void *arg)
{
    MPContext *mpctx = ctx;
    int chapter = get_current_chapter(mpctx);
    if (chapter < 0)
        return M_PROPERTY_UNAVAILABLE;
    return tag_property(action, arg, mpctx->chapters[chapter].metadata);
}

static int mp_property_filter_metadata(void *ctx, struct m_property *prop,
                                       int action, void *arg)
{
    MPContext *mpctx = ctx;
    const char *type = prop->priv;

    if (action == M_PROPERTY_KEY_ACTION) {
        struct m_property_action_arg *ka = arg;
        bstr key;
        char *rem;
        m_property_split_path(ka->key, &key, &rem);
        struct mp_tags *metadata = NULL;
        struct mp_output_chain *chain = NULL;
        if (strcmp(type, "vf") == 0) {
            chain = mpctx->vo_chain ? mpctx->vo_chain->filter : NULL;
        } else if (strcmp(type, "af") == 0) {
            chain = mpctx->ao_chain ? mpctx->ao_chain->filter : NULL;
        }
        if (!chain)
            return M_PROPERTY_UNAVAILABLE;

        int remaining = strlen(rem);
        if (remaining || ka->action != M_PROPERTY_GET_TYPE) {
            struct mp_filter_command cmd = {
                .type = MP_FILTER_COMMAND_GET_META,
                .res = &metadata,
            };
            mp_output_chain_command(chain, mp_tprintf(80, "%.*s", BSTR_P(key)),
                                    &cmd);

            if (!metadata)
                return M_PROPERTY_ERROR;
        }

        int res;
        if (remaining) {
            struct m_property_action_arg next_ka = *ka;
            next_ka.key = rem;
            res = tag_property(M_PROPERTY_KEY_ACTION, &next_ka, metadata);
        } else {
            res = tag_property(ka->action, ka->arg, metadata);
        }
        talloc_free(metadata);
        return res;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_core_idle(void *ctx, struct m_property *prop,
                                 int action, void *arg)
{
    MPContext *mpctx = ctx;
    return m_property_bool_ro(action, arg, !mpctx->playback_active);
}

static int mp_property_deinterlace(void *ctx, struct m_property *prop,
                                   int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct vo_chain *vo_c = mpctx->vo_chain;
    if (!vo_c)
        return M_PROPERTY_UNAVAILABLE;

    bool deinterlace_active = mp_output_chain_deinterlace_active(vo_c->filter);
    return m_property_bool_ro(action, arg, deinterlace_active);
}

static int mp_property_idle(void *ctx, struct m_property *prop,
                            int action, void *arg)
{
    MPContext *mpctx = ctx;
    return m_property_bool_ro(action, arg, mpctx->stop_play == PT_STOP);
}

static int mp_property_window_id(void *ctx, struct m_property *prop,
                                 int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct vo *vo = mpctx->video_out;
    int64_t wid;
    if (!vo || vo_control(vo, VOCTRL_GET_WINDOW_ID, &wid) <= 0)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_int64_ro(action, arg, wid);
}

static int mp_property_eof_reached(void *ctx, struct m_property *prop,
                                   int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->playback_initialized)
        return M_PROPERTY_UNAVAILABLE;
    bool eof = mpctx->video_status == STATUS_EOF &&
               mpctx->audio_status == STATUS_EOF;
    return m_property_bool_ro(action, arg, eof);
}

static int mp_property_seeking(void *ctx, struct m_property *prop,
                               int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->playback_initialized)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_bool_ro(action, arg, !mpctx->restart_complete);
}

static int mp_property_playback_abort(void *ctx, struct m_property *prop,
                                      int action, void *arg)
{
    MPContext *mpctx = ctx;
    return m_property_bool_ro(action, arg, !mpctx->playing || mpctx->stop_play);
}

static int mp_property_cache_speed(void *ctx, struct m_property *prop,
                                   int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->demuxer)
        return M_PROPERTY_UNAVAILABLE;

    struct demux_reader_state s;
    demux_get_reader_state(mpctx->demuxer, &s);

    uint64_t val = s.bytes_per_second;

    if (action == M_PROPERTY_PRINT) {
        *(char **)arg = talloc_strdup_append(format_file_size(val), "/s");
        return M_PROPERTY_OK;
    }
    return m_property_int64_ro(action, arg, val);
}

static int mp_property_demuxer_cache_duration(void *ctx, struct m_property *prop,
                                              int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->demuxer)
        return M_PROPERTY_UNAVAILABLE;

    struct demux_reader_state s;
    demux_get_reader_state(mpctx->demuxer, &s);

    if (s.ts_info.duration < 0)
        return M_PROPERTY_UNAVAILABLE;

    return m_property_double_ro(action, arg, s.ts_info.duration);
}

static int mp_property_demuxer_cache_time(void *ctx, struct m_property *prop,
                                          int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->demuxer)
        return M_PROPERTY_UNAVAILABLE;

    struct demux_reader_state s;
    demux_get_reader_state(mpctx->demuxer, &s);

    if (s.ts_info.end == MP_NOPTS_VALUE)
        return M_PROPERTY_UNAVAILABLE;

    return m_property_double_ro(action, arg, s.ts_info.end);
}

static int mp_property_demuxer_cache_idle(void *ctx, struct m_property *prop,
                                          int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->demuxer)
        return M_PROPERTY_UNAVAILABLE;

    struct demux_reader_state s;
    demux_get_reader_state(mpctx->demuxer, &s);

    return m_property_bool_ro(action, arg, s.idle);
}

static int mp_property_demuxer_cache_state(void *ctx, struct m_property *prop,
                                           int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->demuxer)
        return M_PROPERTY_UNAVAILABLE;

    if (action == M_PROPERTY_GET_TYPE) {
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_NODE};
        return M_PROPERTY_OK;
    }
    if (action != M_PROPERTY_GET)
        return M_PROPERTY_NOT_IMPLEMENTED;

    struct demux_reader_state s;
    demux_get_reader_state(mpctx->demuxer, &s);

    struct mpv_node *r = (struct mpv_node *)arg;
    node_init(r, MPV_FORMAT_NODE_MAP, NULL);

    if (s.ts_info.end != MP_NOPTS_VALUE)
        node_map_add_double(r, "cache-end", s.ts_info.end);

    if (s.ts_info.reader != MP_NOPTS_VALUE)
        node_map_add_double(r, "reader-pts", s.ts_info.reader);

    if (s.ts_info.duration >= 0)
        node_map_add_double(r, "cache-duration", s.ts_info.duration);

    node_map_add_flag(r, "eof", s.eof);
    node_map_add_flag(r, "underrun", s.underrun);
    node_map_add_flag(r, "idle", s.idle);
    node_map_add_int64(r, "total-bytes", s.total_bytes);
    node_map_add_int64(r, "fw-bytes", s.fw_bytes);
    if (s.file_cache_bytes >= 0)
        node_map_add_int64(r, "file-cache-bytes", s.file_cache_bytes);
    if (s.bytes_per_second > 0)
        node_map_add_int64(r, "raw-input-rate", s.bytes_per_second);
    if (s.seeking != MP_NOPTS_VALUE)
        node_map_add_double(r, "debug-seeking", s.seeking);
    node_map_add_int64(r, "debug-low-level-seeks", s.low_level_seeks);
    node_map_add_int64(r, "debug-byte-level-seeks", s.byte_level_seeks);
    if (s.ts_last != MP_NOPTS_VALUE)
        node_map_add_double(r, "debug-ts-last", s.ts_last);

    struct mpv_node *stream_types =
        node_map_add(r, "ts-per-stream", MPV_FORMAT_NODE_ARRAY);
    for (int n = 0; n < STREAM_TYPE_COUNT; n++) {
        struct demux_ctrl_ts_info ts = s.ts_per_stream[n];
        if (ts.duration == -1)
            continue;

        struct mpv_node *st = node_array_add(stream_types, MPV_FORMAT_NODE_MAP);
        node_map_add_string(st, "type",
            n == STREAM_VIDEO ? "video" :
            n == STREAM_AUDIO ? "audio" :
            n == STREAM_SUB ? "subtitle" : "unknown");
        node_map_add_double(st, "cache-duration", ts.duration);
        if (ts.reader != MP_NOPTS_VALUE)
            node_map_add_double(st, "reader-pts", ts.reader);
        if (ts.end != MP_NOPTS_VALUE)
            node_map_add_double(st, "cache-end", ts.end);
    }

    node_map_add_flag(r, "bof-cached", s.bof_cached);
    node_map_add_flag(r, "eof-cached", s.eof_cached);

    struct mpv_node *ranges =
        node_map_add(r, "seekable-ranges", MPV_FORMAT_NODE_ARRAY);
    for (int n = s.num_seek_ranges - 1; n >= 0; n--) {
        struct demux_seek_range *range = &s.seek_ranges[n];
        struct mpv_node *sub = node_array_add(ranges, MPV_FORMAT_NODE_MAP);
        node_map_add_double(sub, "start", range->start);
        node_map_add_double(sub, "end", range->end);
    }

    return M_PROPERTY_OK;
}

static int mp_property_demuxer_start_time(void *ctx, struct m_property *prop,
                                          int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->demuxer)
        return M_PROPERTY_UNAVAILABLE;

    return m_property_double_ro(action, arg, mpctx->demuxer->start_time);
}

static int mp_property_paused_for_cache(void *ctx, struct m_property *prop,
                                        int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->playback_initialized)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_bool_ro(action, arg, mpctx->paused_for_cache);
}

static int mp_property_cache_buffering(void *ctx, struct m_property *prop,
                                       int action, void *arg)
{
    MPContext *mpctx = ctx;
    int state = get_cache_buffering_percentage(mpctx);
    if (state < 0)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_int_ro(action, arg, state);
}

static int mp_property_demuxer_is_network(void *ctx, struct m_property *prop,
                                          int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->demuxer)
        return M_PROPERTY_UNAVAILABLE;

    return m_property_bool_ro(action, arg, mpctx->demuxer->is_network);
}


static int mp_property_clock(void *ctx, struct m_property *prop,
                             int action, void *arg)
{
    char outstr[6];
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);

    if ((tmp != NULL) && (strftime(outstr, sizeof(outstr), "%H:%M", tmp) == 5))
        return m_property_strdup_ro(action, arg, outstr);
    return M_PROPERTY_UNAVAILABLE;
}

static int mp_property_seekable(void *ctx, struct m_property *prop,
                                int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->demuxer)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_bool_ro(action, arg, mpctx->demuxer->seekable);
}

static int mp_property_partially_seekable(void *ctx, struct m_property *prop,
                                          int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->demuxer)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_bool_ro(action, arg, mpctx->demuxer->partially_seekable);
}

static int mp_property_mixer_active(void *ctx, struct m_property *prop,
                                    int action, void *arg)
{
    MPContext *mpctx = ctx;
    return m_property_bool_ro(action, arg, !!mpctx->ao);
}

/// Volume (RW)
static int mp_property_volume(void *ctx, struct m_property *prop,
                              int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct MPOpts *opts = mpctx->opts;

    switch (action) {
    case M_PROPERTY_GET_CONSTRICTED_TYPE:
        *(struct m_option *)arg = (struct m_option){
            .type = CONF_TYPE_FLOAT,
            .min = 0,
            .max = opts->softvol_max,
        };
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT:
        *(char **)arg = talloc_asprintf(NULL, "%.f", opts->softvol_volume);
        return M_PROPERTY_OK;
    }

    return mp_property_generic_option(mpctx, prop, action, arg);
}

static int mp_property_volume_gain(void *ctx, struct m_property *prop,
                                   int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct MPOpts *opts = mpctx->opts;

    switch (action) {
    case M_PROPERTY_GET_CONSTRICTED_TYPE:
        *(struct m_option *)arg = (struct m_option){
            .type = CONF_TYPE_FLOAT,
            .min = opts->softvol_gain_min,
            .max = opts->softvol_gain_max,
        };
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT:
        *(char **)arg = talloc_asprintf(NULL, "%.1f", opts->softvol_gain);
        return M_PROPERTY_OK;
    }

    return mp_property_generic_option(mpctx, prop, action, arg);
}

static int mp_property_ao_volume(void *ctx, struct m_property *prop,
                                 int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct ao *ao = mpctx->ao;
    if (!ao)
        return M_PROPERTY_NOT_IMPLEMENTED;

    switch (action) {
    case M_PROPERTY_SET: {
        float vol = *(float *)arg;
        if (ao_control(ao, AOCONTROL_SET_VOLUME, &vol) != CONTROL_OK)
            return M_PROPERTY_UNAVAILABLE;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_GET: {
        if (ao_control(ao, AOCONTROL_GET_VOLUME, arg) != CONTROL_OK)
            return M_PROPERTY_UNAVAILABLE;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){
            .type = CONF_TYPE_FLOAT,
            .min = 0,
            .max = 100,
        };
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT: {
        float vol = 0;
        if (ao_control(ao, AOCONTROL_GET_VOLUME, &vol) != CONTROL_OK)
            return M_PROPERTY_UNAVAILABLE;
        *(char **)arg = talloc_asprintf(NULL, "%.f", vol);
        return M_PROPERTY_OK;
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}


static int mp_property_ao_mute(void *ctx, struct m_property *prop,
                               int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct ao *ao = mpctx->ao;
    if (!ao)
        return M_PROPERTY_NOT_IMPLEMENTED;

    switch (action) {
    case M_PROPERTY_SET: {
        bool value = *(int *)arg;
        if (ao_control(ao, AOCONTROL_SET_MUTE, &value) != CONTROL_OK)
            return M_PROPERTY_UNAVAILABLE;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_GET: {
        bool value = false;
        if (ao_control(ao, AOCONTROL_GET_MUTE, &value) != CONTROL_OK)
            return M_PROPERTY_UNAVAILABLE;
        *(int *)arg = value;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_BOOL};
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int get_device_entry(int item, int action, void *arg, void *ctx)
{
    struct ao_device_list *list = ctx;
    struct ao_device_desc *entry = &list->devices[item];

    struct m_sub_property props[] = {
        {"name",        SUB_PROP_STR(entry->name)},
        {"description", SUB_PROP_STR(entry->desc)},
        {0}
    };

    return m_property_read_sub(props, action, arg);
}

static void create_hotplug(struct MPContext *mpctx)
{
    struct command_ctx *cmd = mpctx->command_ctx;

    if (!cmd->hotplug) {
        cmd->hotplug = ao_hotplug_create(mpctx->global, mp_wakeup_core_cb,
                                         mpctx);
    }
}

static int mp_property_audio_device(void *ctx, struct m_property *prop,
                                    int action, void *arg)
{
    struct MPContext *mpctx = ctx;
    struct command_ctx *cmd = mpctx->command_ctx;
    if (action == M_PROPERTY_PRINT) {
        create_hotplug(mpctx);

        char *name = NULL;
        if (mp_property_generic_option(mpctx, prop, M_PROPERTY_GET, &name) < 1)
            name = NULL;

        struct ao_device_list *list = ao_hotplug_get_device_list(cmd->hotplug, mpctx->ao);
        for (int n = 0; n < list->num_devices; n++) {
            struct ao_device_desc *dev = &list->devices[n];
            if (dev->name && name && strcmp(dev->name, name) == 0) {
                *(char **)arg = talloc_strdup(NULL, dev->desc ? dev->desc : "?");
                talloc_free(name);
                return M_PROPERTY_OK;
            }
        }

        talloc_free(name);
    }
    return mp_property_generic_option(mpctx, prop, action, arg);
}

static int mp_property_audio_devices(void *ctx, struct m_property *prop,
                                     int action, void *arg)
{
    struct MPContext *mpctx = ctx;
    struct command_ctx *cmd = mpctx->command_ctx;
    create_hotplug(mpctx);

    struct ao_device_list *list = ao_hotplug_get_device_list(cmd->hotplug, mpctx->ao);
    return m_property_read_list(action, arg, list->num_devices,
                                get_device_entry, list);
}

static int mp_property_ao(void *ctx, struct m_property *p, int action, void *arg)
{
    MPContext *mpctx = ctx;
    return m_property_strdup_ro(action, arg, mpctx->ao ? ao_get_name(mpctx->ao) : NULL);
}

/// Audio delay (RW)
static int mp_property_audio_delay(void *ctx, struct m_property *prop,
                                   int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (action == M_PROPERTY_PRINT) {
        *(char **)arg = format_delay(mpctx->opts->audio_delay);
        return M_PROPERTY_OK;
    }
    return mp_property_generic_option(mpctx, prop, action, arg);
}

static int property_audiofmt(struct mp_aframe *fmt, int action, void *arg)
{
    if (!fmt || !mp_aframe_config_is_valid(fmt))
        return M_PROPERTY_UNAVAILABLE;

    struct mp_chmap chmap = {0};
    mp_aframe_get_chmap(fmt, &chmap);

    struct m_sub_property props[] = {
        {"samplerate",      SUB_PROP_INT(mp_aframe_get_rate(fmt))},
        {"channel-count",   SUB_PROP_INT(chmap.num)},
        {"channels",        SUB_PROP_STR(mp_chmap_to_str(&chmap))},
        {"hr-channels",     SUB_PROP_STR(mp_chmap_to_str_hr(&chmap))},
        {"format",          SUB_PROP_STR(af_fmt_to_str(mp_aframe_get_format(fmt)))},
        {0}
    };

    return m_property_read_sub(props, action, arg);
}

static int mp_property_audio_params(void *ctx, struct m_property *prop,
                                    int action, void *arg)
{
    MPContext *mpctx = ctx;
    return property_audiofmt(mpctx->ao_chain ?
        mpctx->ao_chain->filter->input_aformat : NULL, action, arg);
}

static int mp_property_audio_out_params(void *ctx, struct m_property *prop,
                                        int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct mp_aframe *frame = NULL;
    if (mpctx->ao) {
        frame = mp_aframe_create();
        int samplerate;
        int format;
        struct mp_chmap channels;
        ao_get_format(mpctx->ao, &samplerate, &format, &channels);
        mp_aframe_set_rate(frame, samplerate);
        mp_aframe_set_format(frame, format);
        mp_aframe_set_chmap(frame, &channels);
    }
    int r = property_audiofmt(frame, action, arg);
    talloc_free(frame);
    return r;
}

static struct track* track_next(struct MPContext *mpctx, enum stream_type type,
                                int direction, struct track *track)
{
    assert(direction == -1 || direction == +1);
    struct track *prev = NULL, *next = NULL;
    bool seen = track == NULL;
    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct track *cur = mpctx->tracks[n];
        if (cur->type == type) {
            if (cur == track) {
                seen = true;
            } else if (!cur->selected) {
                if (seen && !next) {
                    next = cur;
                }
                if (!seen || !track) {
                    prev = cur;
                }
            }
        }
    }
    return direction > 0 ? next : prev;
}

static int mp_property_switch_track(void *ctx, struct m_property *prop,
                                    int action, void *arg)
{
    MPContext *mpctx = ctx;
    const int *def = prop->priv;
    int order = def[0];
    enum stream_type type = def[1];

    struct track *track = mpctx->current_track[order][type];

    switch (action) {
    case M_PROPERTY_GET:
        if (mpctx->playback_initialized) {
            *(int *)arg = track ? track->user_tid : -2;
        } else {
            *(int *)arg = mpctx->opts->stream_id[order][type];
        }
        return M_PROPERTY_OK;
    case M_PROPERTY_PRINT:
        if (track) {
            void *talloc_ctx = talloc_new(NULL);
            *(char **)arg = talloc_asprintf(NULL, "(%d) %s", track->user_tid,
                mp_format_track_metadata(talloc_ctx, track, true));
            talloc_free(talloc_ctx);
        } else {
            const char *msg = "no";
            if (!mpctx->playback_initialized &&
                mpctx->opts->stream_id[order][type] == -1)
                msg = "auto";
            *(char **) arg = talloc_strdup(NULL, msg);
        }
        return M_PROPERTY_OK;

    case M_PROPERTY_SWITCH: {
        if (mpctx->playback_initialized) {
            struct m_property_switch_arg *sarg = arg;
            do {
                track = track_next(mpctx, type, sarg->inc >= 0 ? +1 : -1, track);
                mp_switch_track_n(mpctx, order, type, track, FLAG_MARK_SELECTION);
            } while (mpctx->current_track[order][type] != track);
            print_track_list(mpctx, "Track switched:");
        } else {
            // Simply cycle between "no" and "auto". It's possible that this does
            // not always do what the user means, but keep the complexity low.
            mark_track_selection(mpctx, order, type,
                mpctx->opts->stream_id[order][type] == -1 ? -2 : -1);
        }
        return M_PROPERTY_OK;
    }
    }
    return mp_property_generic_option(mpctx, prop, action, arg);
}

static int track_channels(struct track *track)
{
    return track->stream ? track->stream->codec->channels.num : 0;
}

static int get_track_entry(int item, int action, void *arg, void *ctx)
{
    struct MPContext *mpctx = ctx;
    struct track *track = mpctx->tracks[item];

    char *external_filename = mp_normalize_user_path(NULL, mpctx->global,
                                                     track->external_filename);

    struct mp_codec_params p =
        track->stream ? *track->stream->codec : (struct mp_codec_params){0};

    bool has_rg = track->stream && track->stream->codec->replaygain_data;
    struct replaygain_data rg = has_rg ? *track->stream->codec->replaygain_data
                                       : (struct replaygain_data){0};

    struct mp_tags *tags = track->stream ? track->stream->tags : &(struct mp_tags){0};
    char **tag_list = talloc_zero_array(NULL, char *, tags->num_keys * 2 + 1);
    for (int i = 0; i < tags->num_keys; i++) {
        tag_list[2 * i] = talloc_strdup(tag_list, tags->keys[i]);
        tag_list[2 * i + 1] = talloc_strdup(tag_list, tags->values[i]);
    }

    double par = 0.0;
    if (p.par_h)
        par = p.par_w / (double) p.par_h;

    int order = -1;
    if (track->selected) {
        for (int i = 0; i < num_ptracks[track->type]; i++) {
            if (mpctx->current_track[i][track->type] == track) {
                order = i;
                break;
            }
        }
    }

    bool has_crop = mp_rect_w(p.crop) > 0 && mp_rect_h(p.crop) > 0;
    struct m_sub_property props[] = {
        {"id",          SUB_PROP_INT(track->user_tid)},
        {"type",        SUB_PROP_STR(stream_type_name(track->type)),
                        .unavailable = !stream_type_name(track->type)},
        {"src-id",      SUB_PROP_INT(track->demuxer_id),
                        .unavailable = track->demuxer_id == -1},
        {"title",       SUB_PROP_STR(track->title),
                        .unavailable = !track->title},
        {"lang",        SUB_PROP_STR(track->lang),
                        .unavailable = !track->lang},
        {"audio-channels", SUB_PROP_INT(track_channels(track)),
                        .unavailable = track_channels(track) <= 0},
        {"image",       SUB_PROP_BOOL(track->image)},
        {"albumart",    SUB_PROP_BOOL(track->attached_picture)},
        {"default",     SUB_PROP_BOOL(track->default_track)},
        {"forced",      SUB_PROP_BOOL(track->forced_track)},
        {"dependent",   SUB_PROP_BOOL(track->dependent_track)},
        {"visual-impaired",  SUB_PROP_BOOL(track->visual_impaired_track)},
        {"hearing-impaired", SUB_PROP_BOOL(track->hearing_impaired_track)},
        {"external",    SUB_PROP_BOOL(track->is_external)},
        {"selected",    SUB_PROP_BOOL(track->selected)},
        {"main-selection", SUB_PROP_INT(order), .unavailable = order < 0},
        {"external-filename", SUB_PROP_STR(external_filename),
                        .unavailable = !external_filename},
        {"ff-index",    SUB_PROP_INT(track->ff_index)},
        {"hls-bitrate", SUB_PROP_INT(track->hls_bitrate),
                        .unavailable = !track->hls_bitrate},
        {"program-id",  SUB_PROP_INT(track->program_id),
                        .unavailable = track->program_id < 0},
        {"decoder",     SUB_PROP_STR(p.decoder),
                        .unavailable = !p.decoder},
        {"decoder-desc", SUB_PROP_STR(p.decoder_desc),
                        .unavailable = !p.decoder_desc},
        {"codec",       SUB_PROP_STR(p.codec),
                        .unavailable = !p.codec},
        {"codec-desc",  SUB_PROP_STR(p.codec_desc),
                        .unavailable = !p.codec_desc},
        {"codec-profile", SUB_PROP_STR(p.codec_profile),
                        .unavailable = !p.codec_profile},
        {"demux-w",     SUB_PROP_INT(p.disp_w), .unavailable = !p.disp_w},
        {"demux-h",     SUB_PROP_INT(p.disp_h), .unavailable = !p.disp_h},
        {"demux-crop-x",SUB_PROP_INT(p.crop.x0), .unavailable = !has_crop},
        {"demux-crop-y",SUB_PROP_INT(p.crop.y0), .unavailable = !has_crop},
        {"demux-crop-w",SUB_PROP_INT(mp_rect_w(p.crop)), .unavailable = !has_crop},
        {"demux-crop-h",SUB_PROP_INT(mp_rect_h(p.crop)), .unavailable = !has_crop},
        {"demux-channel-count", SUB_PROP_INT(p.channels.num),
                        .unavailable = !p.channels.num},
        {"demux-channels", SUB_PROP_STR(mp_chmap_to_str(&p.channels)),
                        .unavailable = !p.channels.num},
        {"demux-samplerate", SUB_PROP_INT(p.samplerate),
                        .unavailable = !p.samplerate},
        {"demux-fps",   SUB_PROP_DOUBLE(p.fps), .unavailable = p.fps <= 0},
        {"demux-bitrate",  SUB_PROP_INT(p.bitrate), .unavailable = p.bitrate <= 0},
        {"demux-rotation", SUB_PROP_INT(p.rotate),  .unavailable = p.rotate <= 0},
        {"demux-par",      SUB_PROP_DOUBLE(par),    .unavailable = par <= 0},
        {"format-name", SUB_PROP_STR(p.format_name), .unavailable = !p.format_name},
        {"replaygain-track-peak", SUB_PROP_FLOAT(rg.track_peak),
                        .unavailable = !has_rg},
        {"replaygain-track-gain", SUB_PROP_FLOAT(rg.track_gain),
                        .unavailable = !has_rg},
        {"replaygain-album-peak", SUB_PROP_FLOAT(rg.album_peak),
                        .unavailable = !has_rg},
        {"replaygain-album-gain", SUB_PROP_FLOAT(rg.album_gain),
                        .unavailable = !has_rg},
        {"dolby-vision-profile", SUB_PROP_INT(p.dv_profile),
                        .unavailable = !p.dovi},
        {"dolby-vision-level", SUB_PROP_INT(p.dv_level),
                        .unavailable = !p.dovi},
        {"metadata", SUB_PROP_KEYVALUE_LIST(tag_list),
                        .unavailable = !tags->num_keys},
        {0}
    };

    int ret = 0;
    switch (action) {
    case M_PROPERTY_KEY_ACTION: ;
        struct m_property_action_arg *ka = arg;
        if (!strncmp(ka->key, "metadata/", 9)) {
            bstr key = {0};
            char *rem = "";
            m_property_split_path(ka->key, &key, &rem);
            ka->key = rem;
            if (!rem[0]) {
                ret = M_PROPERTY_ERROR;
            } else if (!tags || tags->num_keys == 0) {
                ret = M_PROPERTY_UNAVAILABLE;
            } else {
                ret = tag_property(action, (void *)ka, tags);
            }
            goto done;
        }
        MP_FALLTHROUGH;
    default:
        ret = m_property_read_sub(props, action, arg);
    }

done:
    talloc_free(external_filename);
    talloc_free(tag_list);
    return ret;
}

static const char *track_type_name(struct track *t)
{
    if (t->image)
        return "Image";

    switch (t->type) {
    case STREAM_VIDEO: return "Video";
    case STREAM_AUDIO: return "Audio";
    case STREAM_SUB: return "Sub";
    }
    return NULL;
}

static char *append_track_info(char *res, struct track *track)
{
    res = talloc_strdup_append(res, track->selected ? list_current : list_normal);
    res = talloc_asprintf_append(res, "(%d) ", track->user_tid);
    res = talloc_strdup_append(res, mp_format_track_metadata(res, track, true));

    return res;
}

static int mp_property_list_tracks(void *ctx, struct m_property *prop,
                                   int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (action == M_PROPERTY_PRINT) {
        char *res = talloc_strdup(NULL, "");

        for (int type = 0; type < STREAM_TYPE_COUNT; type++) {
            bool found = false;

            for (int n = 0; n < mpctx->num_tracks; n++) {
                struct track *track = mpctx->tracks[n];
                if (track->type == type) {
                    res = talloc_asprintf_append(res, "%s: ", track_type_name(track));
                    res = append_track_info(res, track);
                    res = talloc_asprintf_append(res, "\n");
                    found = true;
                }
            }

            if (found && type < STREAM_TYPE_COUNT - 1) {
                res = talloc_asprintf_append(res, "\n");
                found = false;
            }
        }

        struct demuxer *demuxer = mpctx->demuxer;
        if (demuxer && demuxer->num_editions > 1) {
            res = talloc_asprintf_append(res, "\nEdition: %d of %d",
                                         demuxer->edition + 1,
                                         demuxer->num_editions);
        } else {
            res[strlen(res) - 1] = '\0';
        }

        *(char **)arg = res;
        return M_PROPERTY_OK;
    }

    if (action == M_PROPERTY_KEY_ACTION) {
        struct m_property_action_arg *ka = arg;

        int type = -1;
        if (!strcmp(ka->key, "video")) {
            type = STREAM_VIDEO;
        } else if (!strcmp(ka->key, "audio")) {
            type = STREAM_AUDIO;
        } else if (!strcmp(ka->key, "sub")) {
            type = STREAM_SUB;
        }

        if (type != -1) {
            char *res;

            switch (ka->action) {
                case M_PROPERTY_GET_TYPE:
                    *(struct m_option *)ka->arg = (struct m_option){.type = CONF_TYPE_STRING};
                    return M_PROPERTY_OK;
                case M_PROPERTY_PRINT:
                    res = talloc_asprintf(NULL, "Available %s tracks:",
                              type == STREAM_SUB ? "subtitle" : stream_type_name(type));

                    for (int n = 0; n < mpctx->num_tracks; n++) {
                        if (mpctx->tracks[n]->type == type) {
                            res = talloc_strdup_append(res, "\n");
                            res = append_track_info(res, mpctx->tracks[n]);
                        }
                    }

                    *(char **)ka->arg = res;
                    return M_PROPERTY_OK;
                default:
                    return M_PROPERTY_NOT_IMPLEMENTED;
            }
        }
    }

    return m_property_read_list(action, arg, mpctx->num_tracks,
                                get_track_entry, mpctx);
}

static int mp_property_current_tracks(void *ctx, struct m_property *prop,
                                      int action, void *arg)
{
    MPContext *mpctx = ctx;

    if (action != M_PROPERTY_KEY_ACTION)
        return M_PROPERTY_UNAVAILABLE;

    int type = -1;
    int order = 0;

    struct m_property_action_arg *ka = arg;
    bstr key;
    char *rem;
    m_property_split_path(ka->key, &key, &rem);

    if (bstr_equals0(key, "video")) {
        type = STREAM_VIDEO;
    } else if (bstr_equals0(key, "audio")) {
        type = STREAM_AUDIO;
    } else if (bstr_equals0(key, "sub")) {
        type = STREAM_SUB;
    } else if (bstr_equals0(key, "sub2")) {
        type = STREAM_SUB;
        order = 1;
    }

    if (type < 0)
        return M_PROPERTY_UNKNOWN;

    struct track *t = mpctx->current_track[order][type];

    if (!t && mpctx->lavfi) {
        for (int n = 0; n < mpctx->num_tracks; n++) {
            if (mpctx->tracks[n]->type == type && mpctx->tracks[n]->selected) {
                t = mpctx->tracks[n];
                break;
            }
        }
    }

    if (!t)
        return M_PROPERTY_UNAVAILABLE;

    int index = -1;
    for (int n = 0; n < mpctx->num_tracks; n++) {
        if (mpctx->tracks[n] == t) {
            index = n;
            break;
        }
    }
    assert(index >= 0);

    char *name = mp_tprintf(80, "track-list/%d%s%s", index, *rem ? "/" : "", rem);
    return mp_property_do(name, ka->action, ka->arg, ctx);
}

static int mp_property_hwdec_current(void *ctx, struct m_property *prop,
                                     int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct track *track = mpctx->current_track[0][STREAM_VIDEO];
    struct mp_decoder_wrapper *dec = track ? track->dec : NULL;

    if (!dec)
        return M_PROPERTY_UNAVAILABLE;

    char *current = NULL;
    mp_decoder_wrapper_control(dec, VDCTRL_GET_HWDEC, &current);
    if (!current || !current[0])
        current = "no";
    return m_property_strdup_ro(action, arg, current);
}

static int mp_property_hwdec_interop(void *ctx, struct m_property *prop,
                                     int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->video_out || !mpctx->video_out->hwdec_devs)
        return M_PROPERTY_UNAVAILABLE;

    char *names = hwdec_devices_get_names(mpctx->video_out->hwdec_devs);
    int res = m_property_strdup_ro(action, arg, names);
    talloc_free(names);
    return res;
}

static int get_frame_count(struct MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return -1;
    if (!mpctx->vo_chain)
        return -1;
    double len = get_time_length(mpctx);
    double fps = mpctx->vo_chain->filter->container_fps;
    if (len < 0 || fps <= 0)
        return 0;

    return len * fps;
}

static int mp_property_frame_number(void *ctx, struct m_property *prop,
                                    int action, void *arg)
{
    MPContext *mpctx = ctx;
    int frames = get_frame_count(mpctx);
    if (frames < 0)
        return M_PROPERTY_UNAVAILABLE;

    return m_property_int_ro(action, arg,
        lrint(get_current_pos_ratio(mpctx, false) * frames));
}

static int mp_property_frame_count(void *ctx, struct m_property *prop,
                                   int action, void *arg)
{
    MPContext *mpctx = ctx;
    int frames = get_frame_count(mpctx);
    if (frames < 0)
        return M_PROPERTY_UNAVAILABLE;

    return m_property_int_ro(action, arg, frames);
}

static const char *get_aspect_ratio_name(double ratio)
{
    // Depending on cropping/mastering exact ratio may differ.
#define RATIO_THRESH 0.025
#define RATIO_CASE(ref, name)                     \
    if (fabs(ratio - (ref)) < RATIO_THRESH)       \
        return name;                              \

    // https://en.wikipedia.org/wiki/Aspect_ratio_(image)
    RATIO_CASE(9.0 / 16.0, "Vertical")
    RATIO_CASE(1.0, "Square");
    RATIO_CASE(19.0 / 16.0, "Movietone Ratio");
    RATIO_CASE(5.0 / 4.0, "5:4");
    RATIO_CASE(4.0 / 3.0, "4:3");
    RATIO_CASE(11.0 / 8.0, "Academy Ratio");
    RATIO_CASE(1.43, "IMAX Ratio");
    RATIO_CASE(3.0 / 2.0, "VistaVision Ratio");
    RATIO_CASE(16.0 / 10.0, "16:10");
    RATIO_CASE(5.0 / 3.0, "35mm Widescreen Ratio");
    RATIO_CASE(16.0 / 9.0, "16:9");
    RATIO_CASE(7.0 / 4.0, "Early 35mm Widescreen Ratio");
    RATIO_CASE(1.85, "Academy Flat");
    RATIO_CASE(256.0 / 135.0, "SMPTE/DCI Ratio");
    RATIO_CASE(2.0, "Univisium");
    RATIO_CASE(2.208, "70mm film");
    RATIO_CASE(2.35, "Scope");
    RATIO_CASE(2.39, "Panavision");
    RATIO_CASE(2.55, "Original CinemaScope");
    RATIO_CASE(2.59, "Full-frame Cinerama");
    RATIO_CASE(24.0 / 9.0, "Full-frame Super 16mm");
    RATIO_CASE(2.76, "Ultra Panavision 70");
    RATIO_CASE(32.0 / 9.0, "32:9");
    RATIO_CASE(3.6, "Ultra-WideScreen 3.6");
    RATIO_CASE(4.0, "Polyvision");
    RATIO_CASE(12.0, "Circle-Vision 360°");

    return NULL;

#undef RATIO_THRESH
#undef RATIO_CASE
}

static int property_imgparams(const struct mp_image_params *p, int action, void *arg)
{
    if (!p->imgfmt && !p->imgfmt_name)
        return M_PROPERTY_UNAVAILABLE;

    int d_w, d_h;
    mp_image_params_get_dsize(p, &d_w, &d_h);

    int bpp = 0;
    enum pl_alpha_mode alpha = p->repr.alpha;
    int fmt = p->hw_subfmt ? p->hw_subfmt : p->imgfmt;
    if (fmt) {
        struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(fmt);
        for (int i = 0; i < desc.num_planes; i++)
            bpp += desc.bpp[i] >> (desc.xs[i] + desc.ys[i]);

        // Alpha type is not supported by FFmpeg, so PL_ALPHA_UNKNOWN may mean alpha
        // is of an unknown type, or simply not present. Normalize to AUTO=no alpha.
        if (!!(desc.flags & MP_IMGFLAG_ALPHA) != (alpha != PL_ALPHA_UNKNOWN))
            alpha = (desc.flags & MP_IMGFLAG_ALPHA) ? PL_ALPHA_INDEPENDENT : PL_ALPHA_UNKNOWN;
    }

    const struct pl_hdr_metadata *hdr = &p->color.hdr;
    bool has_cie_y     = pl_hdr_metadata_contains(hdr, PL_HDR_METADATA_CIE_Y);
    bool has_hdr10     = pl_hdr_metadata_contains(hdr, PL_HDR_METADATA_HDR10);
    bool has_hdr10plus = pl_hdr_metadata_contains(hdr, PL_HDR_METADATA_HDR10PLUS);

    bool has_crop = mp_rect_w(p->crop) > 0 && mp_rect_h(p->crop) > 0;
    const char *aspect_name = get_aspect_ratio_name(d_w / (double)d_h);
    const char *sar_name = get_aspect_ratio_name(p->w / (double)p->h);
    const char *pixelformat_name = p->imgfmt_name ? p->imgfmt_name :
                                                   mp_imgfmt_to_name(p->imgfmt);
    struct m_sub_property props[] = {
        {"pixelformat",     SUB_PROP_STR(pixelformat_name)},
        {"hw-pixelformat",  SUB_PROP_STR(mp_imgfmt_to_name(p->hw_subfmt)),
                            .unavailable = !p->hw_subfmt},
        {"average-bpp",     SUB_PROP_INT(bpp),
                            .unavailable = !bpp},
        {"w",               SUB_PROP_INT(p->w)},
        {"h",               SUB_PROP_INT(p->h)},
        {"dw",              SUB_PROP_INT(d_w)},
        {"dh",              SUB_PROP_INT(d_h)},
        {"crop-x",          SUB_PROP_INT(p->crop.x0), .unavailable = !has_crop},
        {"crop-y",          SUB_PROP_INT(p->crop.y0), .unavailable = !has_crop},
        {"crop-w",          SUB_PROP_INT(mp_rect_w(p->crop)), .unavailable = !has_crop},
        {"crop-h",          SUB_PROP_INT(mp_rect_h(p->crop)), .unavailable = !has_crop},
        {"aspect",          SUB_PROP_DOUBLE(d_w / (double)d_h)},
        {"aspect-name",     SUB_PROP_STR(aspect_name), .unavailable = !aspect_name},
        {"par",             SUB_PROP_DOUBLE(p->p_w / (double)p->p_h)},
        {"sar",             SUB_PROP_DOUBLE(p->w / (double)p->h)},
        {"sar-name",        SUB_PROP_STR(sar_name), .unavailable = !sar_name},
        {"colormatrix",
            SUB_PROP_STR(m_opt_choice_str(pl_csp_names, p->repr.sys))},
        {"colorlevels",
            SUB_PROP_STR(m_opt_choice_str(pl_csp_levels_names, p->repr.levels))},
        {"primaries",
            SUB_PROP_STR(m_opt_choice_str(pl_csp_prim_names, p->color.primaries))},
        {"gamma",
            SUB_PROP_STR(m_opt_choice_str(pl_csp_trc_names, p->color.transfer))},
        {"sig-peak", SUB_PROP_FLOAT(p->color.hdr.max_luma / MP_REF_WHITE)},
        {"light",
            SUB_PROP_STR(m_opt_choice_str(mp_csp_light_names, p->light))},
        {"chroma-location",
            SUB_PROP_STR(m_opt_choice_str(pl_chroma_names, p->chroma_location))},
        {"stereo-in",
            SUB_PROP_STR(m_opt_choice_str(mp_stereo3d_names, p->stereo3d))},
        {"rotate",          SUB_PROP_INT(p->rotate)},
        {"alpha",
            SUB_PROP_STR(m_opt_choice_str(pl_alpha_names, alpha)),
            // avoid using "auto" for "no", so just make it unavailable
            .unavailable = alpha == PL_ALPHA_UNKNOWN},
        {"min-luma",    SUB_PROP_FLOAT(hdr->min_luma),     .unavailable = !has_hdr10},
        {"max-luma",    SUB_PROP_FLOAT(hdr->max_luma),     .unavailable = !has_hdr10},
        {"max-cll",     SUB_PROP_FLOAT(hdr->max_cll),      .unavailable = !has_hdr10},
        {"max-fall",    SUB_PROP_FLOAT(hdr->max_fall),     .unavailable = !has_hdr10},
        {"scene-max-r", SUB_PROP_FLOAT(hdr->scene_max[0]), .unavailable = !has_hdr10plus},
        {"scene-max-g", SUB_PROP_FLOAT(hdr->scene_max[1]), .unavailable = !has_hdr10plus},
        {"scene-max-b", SUB_PROP_FLOAT(hdr->scene_max[2]), .unavailable = !has_hdr10plus},
        {"scene-avg",   SUB_PROP_FLOAT(hdr->scene_avg),    .unavailable = !has_hdr10plus},
        {"max-pq-y",    SUB_PROP_FLOAT(hdr->max_pq_y),     .unavailable = !has_cie_y},
        {"avg-pq-y",    SUB_PROP_FLOAT(hdr->avg_pq_y),     .unavailable = !has_cie_y},
        {0}
    };

    return m_property_read_sub(props, action, arg);
}

static struct mp_image_params get_video_out_params(struct MPContext *mpctx)
{
    if (!mpctx->vo_chain)
        return (struct mp_image_params){0};

    struct mp_image_params o_params = mpctx->vo_chain->filter->output_params;
    if (mpctx->video_out) {
        struct m_geometry *gm = &mpctx->video_out->opts->video_crop;
        if (gm->xy_valid || (gm->wh_valid && (gm->w > 0 || gm->h > 0)))
        {
            m_rect_apply(&o_params.crop, o_params.w, o_params.h, gm);
        }
    }

    return o_params;
}

static int mp_property_vo_imgparams(void *ctx, struct m_property *prop,
                                    int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct vo *vo = mpctx->video_out;
    if (!vo)
        return M_PROPERTY_UNAVAILABLE;

    int valid = m_property_read_sub_validate(ctx, prop, action, arg);
    if (valid != M_PROPERTY_VALID)
        return valid;

    struct mp_image_params p = vo_get_current_params(vo);
    return property_imgparams(&p, action, arg);
}

static int mp_property_tgt_imgparams(void *ctx, struct m_property *prop,
                                     int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct vo *vo = mpctx->video_out;
    if (!mpctx->video_out)
        return M_PROPERTY_UNAVAILABLE;

    int valid = m_property_read_sub_validate(ctx, prop, action, arg);
    if (valid != M_PROPERTY_VALID)
        return valid;

    struct mp_image_params p = vo_get_target_params(vo);
    return property_imgparams(&p, action, arg);
}

static int mp_property_dec_imgparams(void *ctx, struct m_property *prop,
                                    int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct mp_image_params p = {0};
    struct vo_chain *vo_c = mpctx->vo_chain;
    if (!vo_c || !vo_c->track)
        return M_PROPERTY_UNAVAILABLE;

    int valid = m_property_read_sub_validate(ctx, prop, action, arg);
    if (valid != M_PROPERTY_VALID)
        return valid;

    mp_decoder_wrapper_get_video_dec_params(vo_c->track->dec, &p);
    if (!p.imgfmt)
        return M_PROPERTY_UNAVAILABLE;
    return property_imgparams(&p, action, arg);
}

static int mp_property_vd_imgparams(void *ctx, struct m_property *prop,
                                    int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct vo_chain *vo_c = mpctx->vo_chain;
    if (!vo_c)
        return M_PROPERTY_UNAVAILABLE;
    struct track *track = mpctx->current_track[0][STREAM_VIDEO];
    struct mp_codec_params *c =
        track && track->stream ? track->stream->codec : NULL;
    if (vo_c->filter->input_params.imgfmt) {
        return property_imgparams(&vo_c->filter->input_params, action, arg);
    } else if (c && c->disp_w && c->disp_h) {
        // Simplistic fallback for stupid scripts querying "width"/"height"
        // before the first frame is decoded.
        struct m_sub_property props[] = {
            {"w", SUB_PROP_INT(c->disp_w)},
            {"h", SUB_PROP_INT(c->disp_h)},
            {0}
        };
        return m_property_read_sub(props, action, arg);
    }
    return M_PROPERTY_UNAVAILABLE;
}

static int mp_property_video_frame_info(void *ctx, struct m_property *prop,
                                        int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->video_out)
        return M_PROPERTY_UNAVAILABLE;

    int valid = m_property_read_sub_validate(ctx, prop, action, arg);
    if (valid != M_PROPERTY_VALID)
        return valid;

    struct mp_image *f = vo_get_current_frame(mpctx->video_out);
    if (!f)
        return M_PROPERTY_UNAVAILABLE;

    const char *pict_types[] = {0, "I", "P", "B"};
    const char *pict_type = f->pict_type >= 1 && f->pict_type <= 3
                          ? pict_types[f->pict_type] : NULL;

    char gop_tc[AV_TIMECODE_STR_SIZE] = {0};
    char s12m_tc[AV_TIMECODE_STR_SIZE] = {0};
    for (int n = 0; n < f->num_ff_side_data; n++) {

        struct mp_ff_side_data *mpsd = &f->ff_side_data[n];
        if (mpsd->type == AV_FRAME_DATA_GOP_TIMECODE)
            av_timecode_make_mpeg_tc_string(gop_tc, *(int64_t*)(mpsd->buf->data));
        if (mpctx->vo_chain && mpsd->type == AV_FRAME_DATA_S12M_TIMECODE) {
            av_timecode_make_smpte_tc_string2(s12m_tc,
                                              av_d2q(mpctx->vo_chain->filter->container_fps, INT_MAX),
                                              *(uint32_t*)(mpsd->buf->data), 0, 0);
        }
    }

    char approx_smpte[AV_TIMECODE_STR_SIZE] = {0};
    if (s12m_tc[0] == '\0' && mpctx->vo_chain) {
        unsigned container_fps = lrint(mpctx->vo_chain->filter->container_fps);
        // Avoid division-by-zero in av_timecode_make_string() if reported
        // container_fps is or rounds to 0.
        if (container_fps) {
            const AVTimecode tcr = {
                .start = 0,
                .flags = AV_TIMECODE_FLAG_DROPFRAME,
                .rate = av_d2q(mpctx->vo_chain->filter->container_fps, INT_MAX),
                .fps = container_fps,
            };
            int frame = lrint(get_current_pos_ratio(mpctx, false) * get_frame_count(mpctx));
            av_timecode_make_string(&tcr, approx_smpte, frame);
        }
    }

    struct m_sub_property props[] = {
        {"picture-type",    SUB_PROP_STR(pict_type), .unavailable = !pict_type},
        {"interlaced",      SUB_PROP_BOOL(!!(f->fields & MP_IMGFIELD_INTERLACED))},
        {"tff",             SUB_PROP_BOOL(!!(f->fields & MP_IMGFIELD_TOP_FIRST))},
        {"repeat",          SUB_PROP_BOOL(!!(f->fields & MP_IMGFIELD_REPEAT_FIRST))},
        {"gop-timecode",    SUB_PROP_STR(gop_tc), .unavailable = gop_tc[0] == '\0'},
        {"smpte-timecode",  SUB_PROP_STR(s12m_tc), .unavailable = s12m_tc[0] == '\0'},
        {"estimated-smpte-timecode", SUB_PROP_STR(approx_smpte), .unavailable = approx_smpte[0] == '\0'},
        {0}
    };

    talloc_free(f);
    return m_property_read_sub(props, action, arg);
}

static int mp_property_current_window_scale(void *ctx, struct m_property *prop,
                                            int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct vo *vo = mpctx->video_out;
    if (!vo)
        return M_PROPERTY_UNAVAILABLE;

    struct mp_image_params params = get_video_out_params(mpctx);
    int vid_w, vid_h;
    mp_image_params_get_dsize(&params, &vid_w, &vid_h);
    if (vid_w < 1 || vid_h < 1)
        return M_PROPERTY_UNAVAILABLE;

    if (params.rotate % 180 == 90 && (vo->driver->caps & VO_CAP_ROTATE90))
        MPSWAP(int, vid_w, vid_h);

    if (vo->monitor_par < 1) {
        vid_h = MPCLAMP(vid_h / vo->monitor_par, 1, 16000);
    } else {
        vid_w = MPCLAMP(vid_w * vo->monitor_par, 1, 16000);
    }

    if (action == M_PROPERTY_SET) {
        // Also called by update_window_scale as a NULL property.
        double scale = *(double *)arg;
        int s[2] = {vid_w * scale, vid_h * scale};
        if (s[0] <= 0 || s[1] <= 0)
            return M_PROPERTY_INVALID_FORMAT;
        vo_control(vo, VOCTRL_SET_UNFS_WINDOW_SIZE, s);
        return M_PROPERTY_OK;
    }

    int s[2];
    if (vo_control(vo, VOCTRL_GET_UNFS_WINDOW_SIZE, s) <= 0 ||
        s[0] < 1 || s[1] < 1)
        return M_PROPERTY_UNAVAILABLE;

    double xs = (double)s[0] / vid_w;
    double ys = (double)s[1] / vid_h;
    return m_property_double_ro(action, arg, (xs + ys) / 2);
}

static void update_window_scale(struct MPContext *mpctx)
{
    double scale = mpctx->opts->vo->window_scale;
    mp_property_current_window_scale(mpctx, (struct m_property *)NULL,
                                     M_PROPERTY_SET, (void*)&scale);
}

static int mp_property_display_fps(void *ctx, struct m_property *prop,
                                   int action, void *arg)
{
    MPContext *mpctx = ctx;
    double fps = mpctx->video_out ? vo_get_display_fps(mpctx->video_out) : 0;
    switch (action) {
    case M_PROPERTY_GET:
        if (fps <= 0)
            return M_PROPERTY_UNAVAILABLE;
        return m_property_double_ro(action, arg, fps);
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_DOUBLE};
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_estimated_display_fps(void *ctx, struct m_property *prop,
                                             int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct vo *vo = mpctx->video_out;
    if (!vo)
        return M_PROPERTY_UNAVAILABLE;
    double interval = vo_get_estimated_vsync_interval(vo) / 1e9;
    if (interval <= 0)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_double_ro(action, arg, 1.0 / interval);
}

static int mp_property_vsync_jitter(void *ctx, struct m_property *prop,
                                    int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct vo *vo = mpctx->video_out;
    if (!vo)
        return M_PROPERTY_UNAVAILABLE;
    double stddev = vo_get_estimated_vsync_jitter(vo);
    if (stddev < 0)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_double_ro(action, arg, stddev);
}

static int mp_property_display_resolution(void *ctx, struct m_property *prop,
                                          int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct vo *vo = mpctx->video_out;
    if (!vo)
        return M_PROPERTY_UNAVAILABLE;
    int res[2];
    if (vo_control(vo, VOCTRL_GET_DISPLAY_RES, &res) <= 0)
        return M_PROPERTY_UNAVAILABLE;
    if (strcmp(prop->name, "display-width") == 0) {
        return m_property_int_ro(action, arg, res[0]);
    } else {
        return m_property_int_ro(action, arg, res[1]);
    }
}

static int mp_property_hidpi_scale(void *ctx, struct m_property *prop,
                                   int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct command_ctx *cmd = mpctx->command_ctx;
    struct vo *vo = mpctx->video_out;
    if (!vo)
        return M_PROPERTY_UNAVAILABLE;
    if (!cmd->cached_window_scale) {
        double scale = 0;
        if (vo_control(vo, VOCTRL_GET_HIDPI_SCALE, &scale) < 1 || !scale)
            scale = -1;
        cmd->cached_window_scale = scale;
    }
    if (cmd->cached_window_scale < 0)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_double_ro(action, arg, cmd->cached_window_scale);
}

static void update_hidpi_window_scale(struct MPContext *mpctx, bool hidpi_scale)
{
    struct command_ctx *cmd = mpctx->command_ctx;
    struct vo *vo = mpctx->video_out;
    if (!vo || cmd->cached_window_scale <= 0)
        return;

    double scale = hidpi_scale ? cmd->cached_window_scale : 1 / cmd->cached_window_scale;

    int s[2];
    if (vo_control(vo, VOCTRL_GET_UNFS_WINDOW_SIZE, s) <= 0 || s[0] < 1 || s[1] < 1)
        return;

    s[0] *= scale;
    s[1] *= scale;
    if (s[0] <= 0 || s[1] <= 0)
        return;
    vo_control(vo, VOCTRL_SET_UNFS_WINDOW_SIZE, s);
}

static int mp_property_ambient_light(void *ctx, struct m_property *prop,
                                     int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct vo *vo = mpctx->video_out;
    double lux;
    if (!vo || vo_control(vo, VOCTRL_GET_AMBIENT_LUX, &lux) < 1)
        return M_PROPERTY_UNAVAILABLE;

    return m_property_double_ro(action, arg, lux);
}

static int mp_property_focused(void *ctx, struct m_property *prop,
                                     int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct vo *vo = mpctx->video_out;
    if (!vo)
        return M_PROPERTY_UNAVAILABLE;

    bool focused;
    if (vo_control(vo, VOCTRL_GET_FOCUSED, &focused) < 1)
        return M_PROPERTY_UNAVAILABLE;

    return m_property_bool_ro(action, arg, focused);
}

static int mp_property_display_names(void *ctx, struct m_property *prop,
                                     int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct vo *vo = mpctx->video_out;
    if (!vo)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_STRING_LIST};
        return M_PROPERTY_OK;
    case M_PROPERTY_GET: {
        char** display_names;
        if (vo_control(vo, VOCTRL_GET_DISPLAY_NAMES, &display_names) < 1)
            return M_PROPERTY_UNAVAILABLE;

        *(char ***)arg = display_names;
        return M_PROPERTY_OK;
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_vo_configured(void *ctx, struct m_property *prop,
                                     int action, void *arg)
{
    MPContext *mpctx = ctx;
    return m_property_bool_ro(action, arg,
                        mpctx->video_out && mpctx->video_out->config_ok);
}

static void get_frame_perf(struct mpv_node *node, struct mp_frame_perf *perf)
{
    for (int i = 0; i < perf->count; i++) {
        struct mp_pass_perf *data = &perf->perf[i];
        struct mpv_node *pass = node_array_add(node, MPV_FORMAT_NODE_MAP);

        node_map_add_string(pass, "desc", perf->desc[i]);
        node_map_add(pass, "last", MPV_FORMAT_INT64)->u.int64 = data->last;
        node_map_add(pass, "avg", MPV_FORMAT_INT64)->u.int64 = data->avg;
        node_map_add(pass, "peak", MPV_FORMAT_INT64)->u.int64 = data->peak;
        node_map_add(pass, "count", MPV_FORMAT_INT64)->u.int64 = data->count;
        struct mpv_node *samples = node_map_add(pass, "samples", MPV_FORMAT_NODE_ARRAY);
        for (int n = 0; n < data->count; n++)
            node_array_add(samples, MPV_FORMAT_INT64)->u.int64 = data->samples[n];
    }
}

static char *asprint_perf(char *res, struct mp_frame_perf *perf)
{
    for (int i = 0; i < perf->count; i++) {
        struct mp_pass_perf *pass = &perf->perf[i];
        res = talloc_asprintf_append(res,
                  "- %s: last %dus avg %dus peak %dus\n", perf->desc[i],
                  (int)pass->last/1000, (int)pass->avg/1000, (int)pass->peak/1000);
    }

    return res;
}

static int mp_property_vo_passes(void *ctx, struct m_property *prop,
                                 int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->video_out)
        return M_PROPERTY_UNAVAILABLE;

    // Return early, to avoid having to go through a completely unnecessary VOCTRL
    switch (action) {
    case M_PROPERTY_PRINT:
    case M_PROPERTY_GET:
        break;
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_NODE};
        return M_PROPERTY_OK;
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }

    struct voctrl_performance_data *data = talloc_ptrtype(NULL, data);
    if (vo_control(mpctx->video_out, VOCTRL_PERFORMANCE_DATA, data) <= 0) {
        talloc_free(data);
        return M_PROPERTY_UNAVAILABLE;
    }

    switch (action) {
    case M_PROPERTY_PRINT: {
        char *res = NULL;
        res = talloc_asprintf_append(res, "fresh:\n");
        res = asprint_perf(res, &data->fresh);
        res = talloc_asprintf_append(res, "\nredraw:\n");
        res = asprint_perf(res, &data->redraw);
        *(char **)arg = res;
        break;
    }

    case M_PROPERTY_GET: {
        struct mpv_node node;
        node_init(&node, MPV_FORMAT_NODE_MAP, NULL);
        struct mpv_node *fresh = node_map_add(&node, "fresh", MPV_FORMAT_NODE_ARRAY);
        struct mpv_node *redraw = node_map_add(&node, "redraw", MPV_FORMAT_NODE_ARRAY);
        get_frame_perf(fresh, &data->fresh);
        get_frame_perf(redraw, &data->redraw);
        *(struct mpv_node *)arg = node;
        break;
    }
    }

    talloc_free(data);
    return M_PROPERTY_OK;
}

static int mp_property_perf_info(void *ctx, struct m_property *p, int action,
                                 void *arg)
{
    MPContext *mpctx = ctx;

    switch (action) {
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_NODE};
        return M_PROPERTY_OK;
    case M_PROPERTY_GET: {
        stats_global_query(mpctx->global, (struct mpv_node *)arg);
        return M_PROPERTY_OK;
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_vo(void *ctx, struct m_property *p, int action, void *arg)
{
    MPContext *mpctx = ctx;
    return m_property_strdup_ro(action, arg, mpctx->video_out ?
                                mpctx->video_out->driver->name : NULL);
}

static int mp_property_gpu_context(void *ctx, struct m_property *p, int action, void *arg)
{
    MPContext *mpctx = ctx;
    return m_property_strdup_ro(action, arg, mpctx->video_out ?
                                mpctx->video_out->context_name : NULL);
}

static int mp_property_osd_dim(void *ctx, struct m_property *prop,
                               int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct mp_osd_res vo_res = osd_get_vo_res(mpctx->osd);

    if (!mpctx->video_out || !mpctx->video_out->config_ok)
        vo_res = (struct mp_osd_res){0};

    double aspect = 1.0 * vo_res.w / MPMAX(vo_res.h, 1) /
                    (vo_res.display_par ? vo_res.display_par : 1);

    struct m_sub_property props[] = {
        {"w",       SUB_PROP_INT(vo_res.w)},
        {"h",       SUB_PROP_INT(vo_res.h)},
        {"par",     SUB_PROP_DOUBLE(vo_res.display_par)},
        {"aspect",  SUB_PROP_DOUBLE(aspect)},
        {"mt",      SUB_PROP_INT(vo_res.mt)},
        {"mb",      SUB_PROP_INT(vo_res.mb)},
        {"ml",      SUB_PROP_INT(vo_res.ml)},
        {"mr",      SUB_PROP_INT(vo_res.mr)},
        {0}
    };

    return m_property_read_sub(props, action, arg);
}

static int mp_property_osd_sym(void *ctx, struct m_property *prop,
                               int action, void *arg)
{
    MPContext *mpctx = ctx;
    char temp[20];
    get_current_osd_sym(mpctx, temp, sizeof(temp));
    return m_property_strdup_ro(action, arg, temp);
}

static int mp_property_osd_ass(void *ctx, struct m_property *prop,
                               int action, void *arg)
{
    struct m_sub_property props[] = {
        {"0",   SUB_PROP_STR(OSD_ASS_0)},
        {"1",   SUB_PROP_STR(OSD_ASS_1)},
        {0}
    };
    return m_property_read_sub(props, action, arg);
}

static int mp_property_term_clip(void *ctx, struct m_property *prop,
                               int action, void *arg)
{
    return m_property_strdup_ro(action, arg, TERM_MSG_0);
}

static int mp_property_term_size(void *ctx, struct m_property *prop,
                                  int action, void *arg)
{
    int w = -1, h = -1;
    terminal_get_size(&w, &h);
    if (w == -1 || h == -1)
        return M_PROPERTY_UNAVAILABLE;

    struct m_sub_property props[] = {
        {"w",      SUB_PROP_INT(w)},
        {"h",      SUB_PROP_INT(h)},
        {0}
    };

    return m_property_read_sub(props, action, arg);
}

static int mp_property_mouse_pos(void *ctx, struct m_property *prop,
                                    int action, void *arg)
{
    MPContext *mpctx = ctx;

    switch (action) {
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_NODE};
        return M_PROPERTY_OK;

    case M_PROPERTY_GET: {
        struct mpv_node node;
        int x, y, hover;
        mp_input_get_mouse_pos(mpctx->input, &x, &y, &hover);

        node_init(&node, MPV_FORMAT_NODE_MAP, NULL);
        node_map_add_int64(&node, "x", x);
        node_map_add_int64(&node, "y", y);
        node_map_add_flag(&node, "hover", hover);
        *(struct mpv_node *)arg = node;

        return M_PROPERTY_OK;
    }
    }

    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int get_touch_pos(int item, int action, void *arg, void *ctx)
{
    const int **pos = (const int **)ctx;
    struct m_sub_property props[] = {
        {"x", SUB_PROP_INT(pos[0][item])},
        {"y", SUB_PROP_INT(pos[1][item])},
        {"id", SUB_PROP_INT(pos[2][item])},
        {0}
    };

    int r = m_property_read_sub(props, action, arg);
    return r;
}

#define MAX_TOUCH_POINTS 10
static int mp_property_touch_pos(void *ctx, struct m_property *prop,
                                 int action, void *arg)
{
    MPContext *mpctx = ctx;
    int xs[MAX_TOUCH_POINTS], ys[MAX_TOUCH_POINTS], ids[MAX_TOUCH_POINTS];
    int count = mp_input_get_touch_pos(mpctx->input, MAX_TOUCH_POINTS, xs, ys, ids);
    const int *pos[3] = {xs, ys, ids};
    return m_property_read_list(action, arg, MPMIN(MAX_TOUCH_POINTS, count),
                                get_touch_pos, (void *)pos);
}

/// Video fps (RO)
static int mp_property_fps(void *ctx, struct m_property *prop,
                           int action, void *arg)
{
    MPContext *mpctx = ctx;
    float fps = mpctx->vo_chain ? mpctx->vo_chain->filter->container_fps : 0;
    if (fps < 0.1 || !isfinite(fps))
        return M_PROPERTY_UNAVAILABLE;;
    return m_property_float_ro(action, arg, fps);
}

static int mp_property_vf_fps(void *ctx, struct m_property *prop,
                              int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->vo_chain)
        return M_PROPERTY_UNAVAILABLE;
    double avg = calc_average_frame_duration(mpctx);
    if (avg <= 0)
        return M_PROPERTY_UNAVAILABLE;
    return m_property_double_ro(action, arg, 1.0 / avg);
}

#define doubles_equal(x, y) (fabs((x) - (y)) <= 0.001)

static int mp_property_video_aspect_override(void *ctx, struct m_property *prop,
                                             int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (action == M_PROPERTY_PRINT) {
        double aspect_ratio;
        mp_property_generic_option(mpctx, prop, M_PROPERTY_GET, &aspect_ratio);

        if (doubles_equal(aspect_ratio, 2.35 / 1.0))
            *(char **)arg = talloc_asprintf(NULL, "2.35:1");
        else if (doubles_equal(aspect_ratio, 16.0 / 9.0))
            *(char **)arg = talloc_asprintf(NULL, "16:9");
        else if (doubles_equal(aspect_ratio, 16.0 / 10.0))
            *(char **)arg = talloc_asprintf(NULL, "16:10");
        else if (doubles_equal(aspect_ratio, 4.0 / 3.0))
            *(char **)arg = talloc_asprintf(NULL, "4:3");
        else if (doubles_equal(aspect_ratio, -1.0))
            *(char **)arg = talloc_asprintf(NULL, "Original");
        else
            *(char **)arg = talloc_asprintf(NULL, "%.3f", aspect_ratio);

        return M_PROPERTY_OK;
    }
    return mp_property_generic_option(mpctx, prop, action, arg);
}

/// Subtitle delay (RW)
static int mp_property_sub_delay(void *ctx, struct m_property *prop,
                                 int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct MPOpts *opts = mpctx->opts;
    int track_ind = *(int *)prop->priv;
    switch (action) {
    case M_PROPERTY_PRINT:
        *(char **)arg = format_delay(opts->subs_shared->sub_delay[track_ind]);
        return M_PROPERTY_OK;
    }
    return mp_property_generic_option(mpctx, prop, action, arg);
}

/// Subtitle speed (RW)
static int mp_property_sub_speed(void *ctx, struct m_property *prop,
                                 int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct MPOpts *opts = mpctx->opts;
    if (action == M_PROPERTY_PRINT) {
        *(char **)arg =
            talloc_asprintf(NULL, "%4.1f%%", 100 * opts->subs_rend->sub_speed);
        return M_PROPERTY_OK;
    }
    return mp_property_generic_option(mpctx, prop, action, arg);
}

static int mp_property_sub_pos(void *ctx, struct m_property *prop,
                               int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct MPOpts *opts = mpctx->opts;
    int track_ind = *(int *)prop->priv;
    if (action == M_PROPERTY_PRINT || action == M_PROPERTY_FIXED_LEN_PRINT) {
        *(char **)arg = mp_format_double(NULL, opts->subs_shared->sub_pos[track_ind], 2,
                                         false, true, action != M_PROPERTY_FIXED_LEN_PRINT);
        return M_PROPERTY_OK;
    }
    return mp_property_generic_option(mpctx, prop, action, arg);
}

static int mp_property_sub_ass_extradata(void *ctx, struct m_property *prop,
                                     int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct track *track = mpctx->current_track[0][STREAM_SUB];
    struct dec_sub *sub = track ? track->d_sub : NULL;
    if (!sub)
        return M_PROPERTY_UNAVAILABLE;
    switch (action) {
    case M_PROPERTY_GET: {
        char *data = sub_ass_get_extradata(sub);
        if (!data)
            return M_PROPERTY_UNAVAILABLE;
        *(char **)arg = data;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_STRING};
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_sub_text(void *ctx, struct m_property *prop,
                                int action, void *arg)
{
    MPContext *mpctx = ctx;
    const int *def = prop->priv;
    int sub_index = def[0];
    int type = def[1];

    if (action == M_PROPERTY_KEY_ACTION) {
        struct m_property_action_arg *ka = arg;

        if (!strcmp(ka->key, "ass"))
            type = SD_TEXT_TYPE_ASS;
        else if (!strcmp(ka->key, "ass-full"))
            type = SD_TEXT_TYPE_ASS_FULL;
        else
            return M_PROPERTY_UNKNOWN;

        action = ka->action;
        arg = ka->arg;
    }

    struct track *track = mpctx->current_track[sub_index][STREAM_SUB];
    struct dec_sub *sub = track ? track->d_sub : NULL;
    double pts = mpctx->playback_pts;
    if (!sub || pts == MP_NOPTS_VALUE)
        return M_PROPERTY_UNAVAILABLE;

    switch (action) {
    case M_PROPERTY_GET: {
        char *text = sub_get_text(sub, pts, type);
        if (!text)
            text = talloc_strdup(NULL, "");
        *(char **)arg = text;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_STRING};
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static struct sd_times get_times(void *ctx, struct m_property *prop,
                                int action, void *arg)
{
    struct sd_times res = { .start = MP_NOPTS_VALUE, .end = MP_NOPTS_VALUE };
    MPContext *mpctx = ctx;
    int track_ind = *(int *)prop->priv;
    struct track *track = mpctx->current_track[track_ind][STREAM_SUB];
    struct dec_sub *sub = track ? track->d_sub : NULL;
    double pts = mpctx->playback_pts;
    if (!sub || pts == MP_NOPTS_VALUE)
        return res;
    return sub_get_times(sub, pts);
}

static int mp_property_sub_start(void *ctx, struct m_property *prop,
                                int action, void *arg)
{
    double start = get_times(ctx, prop, action, arg).start;
    if (start == MP_NOPTS_VALUE)
        return M_PROPERTY_UNAVAILABLE;
    return property_time(action, arg, start);
}


static int mp_property_sub_end(void *ctx, struct m_property *prop,
                                int action, void *arg)
{
    double end = get_times(ctx, prop, action, arg).end;
    if (end == MP_NOPTS_VALUE)
        return M_PROPERTY_UNAVAILABLE;
    return property_time(action, arg, end);
}

static int mp_property_playlist_current_pos(void *ctx, struct m_property *prop,
                                            int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct playlist *pl = mpctx->playlist;

    switch (action) {
    case M_PROPERTY_GET: {
        *(int *)arg = playlist_entry_to_index(pl, pl->current);
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_SET: {
        pl->current = playlist_entry_from_index(pl, *(int *)arg);
        mp_notify(mpctx, MP_EVENT_CHANGE_PLAYLIST, NULL);
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_INT};
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}


static int mp_property_playlist_playing_pos(void *ctx, struct m_property *prop,
                                            int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct playlist *pl = mpctx->playlist;
    return m_property_int_ro(action, arg,
                             playlist_entry_to_index(pl, mpctx->playing));
}

static int mp_property_playlist_pos_x(void *ctx, struct m_property *prop,
                                      int action, void *arg, int base)
{
    MPContext *mpctx = ctx;
    struct playlist *pl = mpctx->playlist;

    switch (action) {
    case M_PROPERTY_GET: {
        int pos = playlist_entry_to_index(pl, pl->current);
        *(int *)arg = pos < 0 ? -1 : pos + base;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_SET: {
        int pos = *(int *)arg - base;
        if (pos >= 0 && playlist_entry_to_index(pl, pl->current) == pos)
            return M_PROPERTY_OK;
        mp_set_playlist_entry(mpctx, playlist_entry_from_index(pl, pos));
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_INT};
        return M_PROPERTY_OK;
    case M_PROPERTY_GET_CONSTRICTED_TYPE: {
        struct m_option opt = {
            .type = CONF_TYPE_INT,
            .min = base,
            .max = playlist_entry_count(pl) - 1 + base,
        };
        *(struct m_option *)arg = opt;
        return M_PROPERTY_OK;
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_playlist_pos(void *ctx, struct m_property *prop,
                                    int action, void *arg)
{
    return mp_property_playlist_pos_x(ctx, prop, action, arg, 0);
}

static int mp_property_playlist_pos_1(void *ctx, struct m_property *prop,
                                      int action, void *arg)
{
    return mp_property_playlist_pos_x(ctx, prop, action, arg, 1);
}

static int get_playlist_entry(int item, int action, void *arg, void *ctx)
{
    struct MPContext *mpctx = ctx;

    struct playlist_entry *e = playlist_entry_from_index(mpctx->playlist, item);
    if (!e)
        return M_PROPERTY_ERROR;

    bool current = mpctx->playlist->current == e;
    bool playing = mpctx->playing == e;
    struct m_sub_property props[] = {
        {"filename",      SUB_PROP_STR(e->filename)},
        {"current",       SUB_PROP_BOOL(1), .unavailable = !current},
        {"playing",       SUB_PROP_BOOL(1), .unavailable = !playing},
        {"title",         SUB_PROP_STR(e->title), .unavailable = !e->title},
        {"id",            SUB_PROP_INT64(e->id)},
        {"playlist-path", SUB_PROP_STR(e->playlist_path), .unavailable = !e->playlist_path},
        {0}
    };

    return m_property_read_sub(props, action, arg);
}

static int mp_property_playlist_path(void *ctx, struct m_property *prop,
                                     int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (!mpctx->playlist->current)
        return M_PROPERTY_UNAVAILABLE;

    struct playlist_entry *e = mpctx->playlist->current;
    return m_property_strdup_ro(action, arg, e->playlist_path);
}

static int mp_property_playlist(void *ctx, struct m_property *prop,
                                int action, void *arg)
{
    MPContext *mpctx = ctx;
    if (action == M_PROPERTY_PRINT) {
        struct playlist *pl = mpctx->playlist;
        char *res = talloc_strdup(NULL, "");

        for (int n = 0; n < pl->num_entries; n++) {
            struct playlist_entry *e = pl->entries[n];
            if (pl->current == e)
                res = append_selected_style(mpctx, res);
            const char *reset = pl->current == e ? get_style_reset(mpctx) : "";
            char *p = e->title;
            if (!p || mpctx->opts->playlist_entry_name > 0) {
                p = e->filename;
                if (!mp_is_url(bstr0(p))) {
                    char *s = mp_basename(e->filename);
                    if (s[0])
                        p = s;
                }
            }
            if (!e->title || p == e->title || mpctx->opts->playlist_entry_name == 1) {
                res = talloc_asprintf_append(res, "%s%s\n", p, reset);
            } else {
                res = talloc_asprintf_append(res, "%s (%s)%s\n", e->title, p, reset);
            }
        }

        *(char **)arg = cut_osd_list(mpctx, "Playlist", res,
                                     playlist_entry_to_index(pl, pl->current));
        return M_PROPERTY_OK;
    }

    return m_property_read_list(action, arg, playlist_entry_count(mpctx->playlist),
                                get_playlist_entry, mpctx);
}

static char *print_obj_osd_list(struct m_obj_settings *list)
{
    char *res = NULL;
    for (int n = 0; list && list[n].name; n++) {
        res = talloc_asprintf_append(res, "%s [", list[n].name);
        for (int i = 0; list[n].attribs && list[n].attribs[i]; i += 2) {
            res = talloc_asprintf_append(res, "%s%s=%s", i > 0 ? " " : "",
                                         list[n].attribs[i],
                                         list[n].attribs[i + 1]);
        }
        res = talloc_asprintf_append(res, "]");
        if (!list[n].enabled)
            res = talloc_strdup_append(res, " (disabled)");
        res = talloc_strdup_append(res, "\n");
    }
    if (!res)
        res = talloc_strdup(NULL, "(empty)");
    return res;
}

static int property_filter(struct m_property *prop, int action, void *arg,
                           MPContext *mpctx, enum stream_type mt)
{
    if (action == M_PROPERTY_PRINT) {
        struct m_config_option *opt = m_config_get_co(mpctx->mconfig,
                                                      bstr0(prop->name));
        *(char **)arg = print_obj_osd_list(*(struct m_obj_settings **)opt->data);
        return M_PROPERTY_OK;
    }
    return mp_property_generic_option(mpctx, prop, action, arg);
}

static int mp_property_vf(void *ctx, struct m_property *prop,
                          int action, void *arg)
{
    return property_filter(prop, action, arg, ctx, STREAM_VIDEO);
}

static int mp_property_af(void *ctx, struct m_property *prop,
                          int action, void *arg)
{
    return property_filter(prop, action, arg, ctx, STREAM_AUDIO);
}

static int mp_property_ab_loop(void *ctx, struct m_property *prop,
                               int action, void *arg)
{
    struct MPContext *mpctx = ctx;
    if (action == M_PROPERTY_KEY_ACTION) {
        double val;
        if (mp_property_generic_option(mpctx, prop, M_PROPERTY_GET, &val) < 1)
            return M_PROPERTY_ERROR;

        return property_time(action, arg, val);
    }
    return mp_property_generic_option(mpctx, prop, action, arg);
}

static int mp_property_packet_bitrate(void *ctx, struct m_property *prop,
                                      int action, void *arg)
{
    MPContext *mpctx = ctx;
    int type = *(int *)prop->priv;

    struct demuxer *demuxer = NULL;
    if (mpctx->current_track[0][type])
        demuxer = mpctx->current_track[0][type]->demuxer;
    if (!demuxer)
        demuxer = mpctx->demuxer;
    if (!demuxer)
        return M_PROPERTY_UNAVAILABLE;

    double r[STREAM_TYPE_COUNT];
    demux_get_bitrate_stats(demuxer, r);
    if (r[type] < 0)
        return M_PROPERTY_UNAVAILABLE;

    // r[type] is in bytes/second -> bits
    double rate = r[type] * 8;

    if (action == M_PROPERTY_PRINT) {
        rate /= 1000;
        if (rate < 1000) {
            *(char **)arg = talloc_asprintf(NULL, "%.f kbps", rate);
        } else {
            *(char **)arg = talloc_asprintf(NULL, "%.3f Mbps", rate / 1000.0);
        }
        return M_PROPERTY_OK;
    }
    return m_property_int64_ro(action, arg, llrint(rate));
}

static int mp_property_cwd(void *ctx, struct m_property *prop,
                           int action, void *arg)
{
    switch (action) {
    case M_PROPERTY_GET: {
        char *cwd = mp_getcwd(NULL);
        if (!cwd)
            return M_PROPERTY_ERROR;
        *(char **)arg = cwd;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_STRING};
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_current_watch_later_dir(void *ctx, struct m_property *prop,
                                               int action, void *arg)
{
    MPContext *mpctx = ctx;
    return m_property_strdup_ro(action, arg, mp_get_playback_resume_dir(mpctx));
}

static int mp_property_protocols(void *ctx, struct m_property *prop,
                                 int action, void *arg)
{
    switch (action) {
    case M_PROPERTY_GET:
        *(char ***)arg = stream_get_proto_list();
        return M_PROPERTY_OK;
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_STRING_LIST};
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_keylist(void *ctx, struct m_property *prop,
                               int action, void *arg)
{
    switch (action) {
    case M_PROPERTY_GET:
        *(char ***)arg = mp_get_key_list();
        return M_PROPERTY_OK;
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_STRING_LIST};
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int get_decoder_entry(int item, int action, void *arg, void *ctx)
{
    struct mp_decoder_list *codecs = ctx;
    struct mp_decoder_entry *c = &codecs->entries[item];

    struct m_sub_property props[] = {
        {"codec",       SUB_PROP_STR(c->codec)},
        {"driver" ,     SUB_PROP_STR(c->decoder)},
        {"description", SUB_PROP_STR(c->desc)},
        {0}
    };

    return m_property_read_sub(props, action, arg);
}

static int mp_property_decoders(void *ctx, struct m_property *prop,
                                int action, void *arg)
{
    struct mp_decoder_list *codecs = talloc_zero(NULL, struct mp_decoder_list);
    struct mp_decoder_list *v = talloc_steal(codecs, video_decoder_list());
    struct mp_decoder_list *a = talloc_steal(codecs, audio_decoder_list());
    mp_append_decoders(codecs, v);
    mp_append_decoders(codecs, a);
    int r = m_property_read_list(action, arg, codecs->num_entries,
                                 get_decoder_entry, codecs);
    talloc_free(codecs);
    return r;
}

static int mp_property_encoders(void *ctx, struct m_property *prop,
                                int action, void *arg)
{
    struct mp_decoder_list *codecs = talloc_zero(NULL, struct mp_decoder_list);
    mp_add_lavc_encoders(codecs);
    int r = m_property_read_list(action, arg, codecs->num_entries,
                                 get_decoder_entry, codecs);
    talloc_free(codecs);
    return r;
}

static int mp_property_lavf_demuxers(void *ctx, struct m_property *prop,
                                 int action, void *arg)
{
    switch (action) {
    case M_PROPERTY_GET:
        *(char ***)arg = mp_get_lavf_demuxers();
        return M_PROPERTY_OK;
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_STRING_LIST};
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_version(void *ctx, struct m_property *prop,
                               int action, void *arg)
{
    return m_property_strdup_ro(action, arg, mpv_version);
}

static int mp_property_configuration(void *ctx, struct m_property *prop,
                                     int action, void *arg)
{
    return m_property_strdup_ro(action, arg, CONFIGURATION);
}

static int mp_property_ffmpeg(void *ctx, struct m_property *prop,
                               int action, void *arg)
{
    return m_property_strdup_ro(action, arg, av_version_info());
}

static int mp_property_libass_version(void *ctx, struct m_property *prop,
                                      int action, void *arg)
{
    return m_property_int64_ro(action, arg, ass_library_version());
}

static int mp_property_platform(void *ctx, struct m_property *prop,
                                int action, void *arg)
{
    return m_property_strdup_ro(action, arg, PLATFORM);
}

static int mp_property_alias(void *ctx, struct m_property *prop,
                             int action, void *arg)
{
    const char *real_property = prop->priv;
    return mp_property_do(real_property, action, arg, ctx);
}

static int mp_property_deprecated_alias(void *ctx, struct m_property *prop,
                                        int action, void *arg)
{
    MPContext *mpctx = ctx;
    struct command_ctx *cmd = mpctx->command_ctx;
    const char *real_property = prop->priv;
    for (int n = 0; n < cmd->num_warned_deprecated; n++) {
        if (strcmp(cmd->warned_deprecated[n], prop->name) == 0)
            goto done;
    }
    MP_WARN(mpctx, "Warning: property '%s' was replaced with '%s' and "
            "might be removed in the future.\n", prop->name, real_property);
    MP_TARRAY_APPEND(cmd, cmd->warned_deprecated, cmd->num_warned_deprecated,
                     (char *)prop->name);

done:
    return mp_property_do(real_property, action, arg, ctx);
}

static int access_options(struct m_property_action_arg *ka, bool local,
                          MPContext *mpctx)
{
    struct m_config_option *opt = m_config_get_co(mpctx->mconfig,
                                                  bstr0(ka->key));
    if (!opt)
        return M_PROPERTY_UNKNOWN;
    if (!opt->data)
        return M_PROPERTY_UNAVAILABLE;

    switch (ka->action) {
    case M_PROPERTY_GET:
        m_option_copy(opt->opt, ka->arg, opt->data);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET: {
        if (local && !mpctx->playing)
            return M_PROPERTY_ERROR;
        int flags = local ? M_SETOPT_BACKUP : 0;
        int r = m_config_set_option_raw(mpctx->mconfig, opt, ka->arg, flags);
        mp_wakeup_core(mpctx);
        return r < 0 ? M_PROPERTY_ERROR : M_PROPERTY_OK;
    }
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)ka->arg = *opt->opt;
        return M_PROPERTY_OK;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int access_option_list(int action, void *arg, bool local, MPContext *mpctx)
{
    switch (action) {
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_STRING_LIST};
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
        *(char ***)arg = m_config_list_options(NULL, mpctx->mconfig);
        return M_PROPERTY_OK;
    case M_PROPERTY_KEY_ACTION:
        return access_options(arg, local, mpctx);
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}


static int mp_property_options(void *ctx, struct m_property *prop,
                               int action, void *arg)
{
    MPContext *mpctx = ctx;
    return access_option_list(action, arg, false, mpctx);
}

static int mp_property_local_options(void *ctx, struct m_property *prop,
                                     int action, void *arg)
{
    MPContext *mpctx = ctx;
    return access_option_list(action, arg, true, mpctx);
}

static int mp_property_option_info(void *ctx, struct m_property *prop,
                                   int action, void *arg)
{
    MPContext *mpctx = ctx;
    switch (action) {
    case M_PROPERTY_KEY_ACTION: {
        struct m_property_action_arg *ka = arg;
        bstr key;
        char *rem;
        m_property_split_path(ka->key, &key, &rem);
        struct m_config_option *co = m_config_get_co(mpctx->mconfig, key);
        if (!co)
            return M_PROPERTY_UNKNOWN;
        const struct m_option *opt = co->opt;

        union m_option_value def = m_option_value_default;
        const void *def_ptr = m_config_get_co_default(mpctx->mconfig, co);
        bool has_default = def_ptr && opt->type->size > 0;
        if (has_default)
            memcpy(&def, def_ptr, opt->type->size);

        bool has_minmax = opt->min < opt->max &&
            (opt->type->flags & M_OPT_TYPE_USES_RANGE);
        char **choices = NULL;

        if (opt->type == &m_option_type_choice) {
            const struct m_opt_choice_alternatives *alt = opt->priv;
            int num = 0;
            for ( ; alt->name; alt++)
                MP_TARRAY_APPEND(NULL, choices, num, alt->name);
            MP_TARRAY_APPEND(NULL, choices, num, NULL);
        }
        if (opt->type == &m_option_type_obj_settings_list) {
            const struct m_obj_list *objs = opt->priv;
            int num = 0;
            for (int n = 0; ; n++) {
                struct m_obj_desc desc = {0};
                if (!objs->get_desc(&desc, n))
                    break;
                MP_TARRAY_APPEND(NULL, choices, num, (char *)desc.name);
            }
            if (objs->get_lavfi_filters) {
                const char **filters = objs->get_lavfi_filters(choices);
                for (int n = 0; filters[n]; n++) {
                    MP_TARRAY_APPEND(NULL, choices, num, (char *)filters[n]);
                }
            }
            MP_TARRAY_APPEND(NULL, choices, num, NULL);
        }

        struct m_sub_property props[] = {
            {"name",                    SUB_PROP_STR(co->name)},
            {"type",                    SUB_PROP_STR(opt->type->name)},
            {"set-from-commandline",    SUB_PROP_BOOL(co->is_set_from_cmdline)},
            {"set-locally",             SUB_PROP_BOOL(co->is_set_locally)},
            {"expects-file",            SUB_PROP_BOOL(opt->flags & M_OPT_FILE)},
            {"default-value",           *opt, def, .unavailable = !has_default},
            {"min",                     SUB_PROP_DOUBLE(opt->min),
             .unavailable = !(has_minmax && opt->min != DBL_MIN)},
            {"max",                     SUB_PROP_DOUBLE(opt->max),
             .unavailable = !(has_minmax && opt->max != DBL_MAX)},
            {"choices", .type = {.type = CONF_TYPE_STRING_LIST},
             .value = {.string_list = choices}, .unavailable = !choices},
            {0}
        };

        struct m_property_action_arg next_ka = *ka;
        next_ka.key = rem;
        int r = m_property_read_sub(props, M_PROPERTY_KEY_ACTION, &next_ka);
        talloc_free(choices);
        return r;
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_list(void *ctx, struct m_property *prop,
                            int action, void *arg)
{
    struct MPContext *mpctx = ctx;
    struct command_ctx *cmd = mpctx->command_ctx;

    switch (action) {
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_STRING_LIST};
        return M_PROPERTY_OK;
    case M_PROPERTY_GET: {
        char **list = NULL;
        int num = 0;
        for (int n = 0; cmd->properties[n].name; n++) {
            MP_TARRAY_APPEND(NULL, list, num,
                                talloc_strdup(NULL, cmd->properties[n].name));
        }
        MP_TARRAY_APPEND(NULL, list, num, NULL);
        *(char ***)arg = list;
        return M_PROPERTY_OK;
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_profile_list(void *ctx, struct m_property *prop,
                           int action, void *arg)
{
    MPContext *mpctx = ctx;
    switch (action) {
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_NODE};
        return M_PROPERTY_OK;
    case M_PROPERTY_GET: {
        *(struct mpv_node *)arg = m_config_get_profiles(mpctx->mconfig);
        return M_PROPERTY_OK;
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_commands(void *ctx, struct m_property *prop,
                           int action, void *arg)
{
    switch (action) {
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_NODE};
        return M_PROPERTY_OK;
    case M_PROPERTY_GET: {
        struct mpv_node *root = arg;
        node_init(root, MPV_FORMAT_NODE_ARRAY, NULL);

        for (int n = 0; mp_cmds[n].name; n++) {
            const struct mp_cmd_def *cmd = &mp_cmds[n];
            struct mpv_node *entry = node_array_add(root, MPV_FORMAT_NODE_MAP);

            node_map_add_string(entry, "name", cmd->name);

            struct mpv_node *args =
                node_map_add(entry, "args", MPV_FORMAT_NODE_ARRAY);
            for (int i = 0; i < MP_CMD_DEF_MAX_ARGS; i++) {
                const struct m_option *a = &cmd->args[i];
                if (!a->type)
                    break;
                struct mpv_node *ae = node_array_add(args, MPV_FORMAT_NODE_MAP);
                node_map_add_string(ae, "name", a->name);
                node_map_add_string(ae, "type", a->type->name);
                node_map_add_flag(ae, "optional", a->flags & MP_CMD_OPT_ARG);
            }

            node_map_add_flag(entry, "vararg", cmd->vararg);
        }

        return M_PROPERTY_OK;
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_bindings(void *ctx, struct m_property *prop,
                                int action, void *arg)
{
    MPContext *mpctx = ctx;
    switch (action) {
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_NODE};
        return M_PROPERTY_OK;
    case M_PROPERTY_GET: {
        *(struct mpv_node *)arg = mp_input_get_bindings(mpctx->input);
        return M_PROPERTY_OK;
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int mp_property_mdata(void *ctx, struct m_property *prop,
                                int action, void *arg)
{
    MPContext *mpctx = ctx;
    mpv_node *node = &mpctx->command_ctx->mdata;

    switch (action) {
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){.type = CONF_TYPE_NODE};
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
    case M_PROPERTY_GET_NODE:
        m_option_copy(&mdata_type, arg, node);
        return M_PROPERTY_OK;
    case M_PROPERTY_SET:
    case M_PROPERTY_SET_NODE: {
        m_option_copy(&mdata_type, node, arg);
        talloc_steal(mpctx->command_ctx, node_get_alloc(node));
        mp_notify_property(mpctx, prop->name);

        struct vo *vo = mpctx->video_out;
        if (vo)
            vo_control(vo, VOCTRL_UPDATE_MENU, arg);
        return M_PROPERTY_OK;
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int do_list_udata(int item, int action, void *arg, void *ctx);

struct udata_ctx {
    MPContext *mpctx;
    const char *path;
    mpv_node *node;
    void *ta_parent;
    int depth;
};

static int do_op_udata(struct udata_ctx* ctx, int action, void *arg)
{
    MPContext *mpctx = ctx->mpctx;
    mpv_node *node = ctx->node;

    switch (action) {
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = udata_type;
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
    case M_PROPERTY_GET_NODE: // same as GET, because type==mpv_node
        assert(node);
        m_option_copy(&udata_type, arg, node);
        return M_PROPERTY_OK;
    case M_PROPERTY_FIXED_LEN_PRINT:
    case M_PROPERTY_PRINT: {
        char *str = m_option_pretty_print(&udata_type, node, action == M_PROPERTY_FIXED_LEN_PRINT);
        *(char **)arg = str;
        return str != NULL;
    }
    case M_PROPERTY_SET:
    case M_PROPERTY_SET_NODE:
        assert(node);
        m_option_copy(&udata_type, node, arg);
        talloc_steal(ctx->ta_parent, node_get_alloc(node));
        mp_notify_property(mpctx, ctx->path);
        return M_PROPERTY_OK;
    case M_PROPERTY_KEY_ACTION: {
        assert(node);

        // If we're operating on an array, sub-object access is handled by m_property_read_list
        if (node->format == MPV_FORMAT_NODE_ARRAY)
            return m_property_read_list(action, arg, node->u.list->num, &do_list_udata, ctx);

        // Sub-objects only make sense for arrays and maps
        if (node->format != MPV_FORMAT_NODE_MAP)
            return M_PROPERTY_NOT_IMPLEMENTED;

        struct m_property_action_arg *act = arg;

        // See if the next layer down will also be a sub-object access
        bstr key;
        char *rem;
        bool has_split = m_property_split_path(act->key, &key, &rem);

        if (!has_split && act->action == M_PROPERTY_DELETE) {
            // Find the object we're looking for
            int i;
            for (i = 0; i < node->u.list->num; i++) {
                if (bstr_equals0(key, node->u.list->keys[i]))
                    break;
            }

            // Return if it didn't exist
            if (i == node->u.list->num)
                return M_PROPERTY_UNKNOWN;

            // Delete the item
            m_option_free(&udata_type, &node->u.list->values[i]);
            talloc_free(node->u.list->keys[i]);

            // Shift the remaining items back
            for (i++; i < node->u.list->num; i++) {
                node->u.list->values[i - 1] = node->u.list->values[i];
                node->u.list->keys[i - 1] = node->u.list->keys[i];
            }

            // And decrement the count
            node->u.list->num--;

            return M_PROPERTY_OK;
        }

        // Look up the next level down
        mpv_node *cnode = node_map_bget(node, key);

        if (!cnode) {
            switch (act->action) {
                case M_PROPERTY_SET:
                case M_PROPERTY_SET_NODE: {
                    // If we're doing a set, and the key doesn't exist, create it.
                    // If we're recursing another layer down, make it an empty map;
                    // otherwise, make it NONE, since we'll be overwriting it at the next level.
                    cnode = node_map_badd(node, key, has_split ? MPV_FORMAT_NODE_MAP : MPV_FORMAT_NONE);
                    if (!cnode)
                        return M_PROPERTY_ERROR;
                    break;
                case M_PROPERTY_GET_TYPE:
                    // Nonexistent keys have type NODE, so they can be overwritten
                    *(struct m_option *)act->arg = udata_type;
                    return M_PROPERTY_OK;
                default:
                    // We can't perform any other options on nonexistent keys
                    return M_PROPERTY_UNKNOWN;
                }
            }
        }

        struct udata_ctx nctx = *ctx;
        nctx.node = cnode;
        nctx.ta_parent = node_get_alloc(node);

        // If we're going down another level, set up a new key-action.
        if (has_split) {
            struct m_property_action_arg sub_act = {
                .key = rem,
                .action = act->action,
                .arg = act->arg,
            };

            if (nctx.depth++ > 100)
                return M_PROPERTY_ERROR;
            return do_op_udata(&nctx, M_PROPERTY_KEY_ACTION, &sub_act);
        } else {
            return do_op_udata(&nctx, act->action, act->arg);
        }
    }
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

static int do_list_udata(int item, int action, void *arg, void *ctx)
{
    struct udata_ctx nctx = *(struct udata_ctx*)ctx;
    nctx.node = &nctx.node->u.list->values[item];
    nctx.ta_parent = nctx.node->u.list;
    nctx.depth = 0;

    return do_op_udata(&nctx, action, arg);
}

static int mp_property_udata(void *ctx, struct m_property *prop,
                                 int action, void *arg)
{
    // The root of udata is a shared map; don't allow overwriting
    // or deleting the whole thing
    if (action == M_PROPERTY_SET || action == M_PROPERTY_SET_NODE ||
        action == M_PROPERTY_DELETE)
        return M_PROPERTY_NOT_IMPLEMENTED;

    char *path = NULL;
    if (action == M_PROPERTY_KEY_ACTION) {
        struct m_property_action_arg *act = arg;
        if (act->action == M_PROPERTY_SET || act->action == M_PROPERTY_SET_NODE)
            path = talloc_asprintf(NULL, "%s/%s", prop->name, act->key);
    }

    struct MPContext *mpctx = ctx;
    struct udata_ctx nctx = {
        .mpctx = mpctx,
        .path = path,
        .node = &mpctx->command_ctx->udata,
        .ta_parent = mpctx->command_ctx,
    };

    int ret = do_op_udata(&nctx, action, arg);

    talloc_free(path);

    return ret;
}

static int mp_property_current_clipboard_backend(void *ctx, struct m_property *p,
                                                 int action, void *arg)
{
    MPContext *mpctx = ctx;
    return m_property_strdup_ro(action, arg, mp_clipboard_get_backend_name(mpctx->clipboard));
}

static int get_clipboard(struct MPContext *mpctx, void *arg,
                         struct clipboard_access_params *params)
{
    struct clipboard_data data;
    void *tmp = talloc_new(NULL);

    int ret = mp_clipboard_get_data(mpctx->clipboard, params, &data, tmp);
    if (ret != CLIPBOARD_SUCCESS) {
        talloc_free(tmp);
        return ret == CLIPBOARD_UNAVAILABLE ? M_PROPERTY_UNAVAILABLE : M_PROPERTY_ERROR;
    }

    switch (data.type) {
    case CLIPBOARD_DATA_TEXT:
        *(char **)arg = talloc_steal(NULL, data.u.text);
        talloc_free(tmp);
        return M_PROPERTY_OK;
    default:
        talloc_free(tmp);
        return M_PROPERTY_NOT_IMPLEMENTED;
    }
}

static int set_clipboard(struct MPContext *mpctx, void *arg,
                         struct clipboard_access_params *params)
{
    struct clipboard_data data = {0};

    switch (params->type) {
    case CLIPBOARD_DATA_TEXT:
        data.type = CLIPBOARD_DATA_TEXT;
        data.u.text = *(char **)arg;
        break;
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }

    int ret = mp_clipboard_set_data(mpctx->clipboard, params, &data);
    if (ret == CLIPBOARD_SUCCESS)
        return M_PROPERTY_OK;
    return ret == CLIPBOARD_UNAVAILABLE ? M_PROPERTY_UNAVAILABLE : M_PROPERTY_ERROR;
}

static int mp_property_clipboard(void *ctx, struct m_property *prop,
                                 int action, void *arg)
{
    struct MPContext *mpctx = ctx;
    struct clipboard_access_params params = {
        .type = CLIPBOARD_DATA_TEXT,
        .target = CLIPBOARD_TARGET_CLIPBOARD,
    };

    switch (action) {
    case M_PROPERTY_GET_TYPE:
        *(struct m_option *)arg = (struct m_option){
            .type = CONF_TYPE_NODE,
        };
        return M_PROPERTY_OK;
    case M_PROPERTY_GET:
    case M_PROPERTY_GET_NODE: {
        struct mpv_node node;
        node_init(&node, MPV_FORMAT_NODE_MAP, NULL);
        char *data = NULL;
        if (get_clipboard(mpctx, &data, &params) == M_PROPERTY_OK) {
            node_map_add_string(&node, "text", data);
            talloc_free(data);
        }
        params.target = CLIPBOARD_TARGET_PRIMARY_SELECTION;
        data = NULL;
        if (get_clipboard(mpctx, &data, &params) == M_PROPERTY_OK) {
            node_map_add_string(&node, "text-primary", data);
            talloc_free(data);
        }
        *(struct mpv_node *)arg = node;
        return M_PROPERTY_OK;
    }
    case M_PROPERTY_KEY_ACTION: {
        struct m_property_action_arg *act = arg;
        const char *key = act->key;

        if (!strcmp(key, "text-primary"))
            params.target = CLIPBOARD_TARGET_PRIMARY_SELECTION;
        else if (strcmp(key, "text"))
            return M_PROPERTY_UNKNOWN;

        switch (act->action) {
        case M_PROPERTY_GET_TYPE:
            switch (params.type) {
            case CLIPBOARD_DATA_TEXT:
                *(struct m_option *)act->arg = (struct m_option){
                    .type = CONF_TYPE_STRING,
                };
                return M_PROPERTY_OK;
            default:
                return M_PROPERTY_UNKNOWN;
            }
        case M_PROPERTY_GET:
            return get_clipboard(mpctx, act->arg, &params);
        case M_PROPERTY_SET:
            return set_clipboard(mpctx, act->arg, &params);
        default:
            return M_PROPERTY_NOT_IMPLEMENTED;
        }
    }
    default:
        return M_PROPERTY_NOT_IMPLEMENTED;
    }
}

// Redirect a property name to another
#define M_PROPERTY_ALIAS(name, real_property) \
    {(name), mp_property_alias, .priv = (real_property)}

#define M_PROPERTY_DEPRECATED_ALIAS(name, real_property) \
    {(name), mp_property_deprecated_alias, .priv = (real_property)}

// Base list of properties. This does not include option-mapped properties.
static const struct m_property mp_properties_base[] = {
    // General
    {"pid", mp_property_pid},
    {"speed", mp_property_playback_speed},
    {"pitch", mp_property_playback_pitch},
    {"audio-speed-correction", mp_property_av_speed_correction, .priv = "a"},
    {"video-speed-correction", mp_property_av_speed_correction, .priv = "v"},
    {"display-sync-active", mp_property_display_sync_active},
    {"filename", mp_property_filename},
    {"stream-open-filename", mp_property_stream_open_filename},
    {"file-size", mp_property_file_size},
    {"path", mp_property_path},
    {"media-title", mp_property_media_title},
    {"stream-path", mp_property_stream_path},
    {"current-demuxer", mp_property_demuxer},
    {"file-format", mp_property_file_format},
    {"stream-pos", mp_property_stream_pos},
    {"stream-end", mp_property_stream_end},
    {"duration", mp_property_duration},
    {"avsync", mp_property_avsync},
    {"total-avsync-change", mp_property_total_avsync_change},
    {"mistimed-frame-count", mp_property_mistimed_frame_count},
    {"vsync-ratio", mp_property_vsync_ratio},
    {"display-width", mp_property_display_resolution},
    {"display-height", mp_property_display_resolution},
    {"decoder-frame-drop-count", mp_property_frame_drop_dec},
    {"frame-drop-count", mp_property_frame_drop_vo},
    {"vo-delayed-frame-count", mp_property_vo_delayed_frame_count},
    {"percent-pos", mp_property_percent_pos},
    {"time-start", mp_property_time_start},
    {"time-pos", mp_property_time_pos},
    {"time-remaining", mp_property_remaining},
    {"audio-pts", mp_property_audio_pts},
    {"playtime-remaining", mp_property_playtime_remaining},
    M_PROPERTY_ALIAS("playback-time", "time-pos"),
    {"remaining-file-loops", mp_property_remaining_file_loops},
    {"remaining-ab-loops", mp_property_remaining_ab_loops},
    {"chapter", mp_property_chapter},
    {"edition", mp_property_edition},
    {"current-edition", mp_property_current_edition},
    {"chapters", mp_property_chapters},
    {"editions", mp_property_editions},
    {"metadata", mp_property_metadata},
    {"filtered-metadata", mp_property_filtered_metadata},
    {"chapter-metadata", mp_property_chapter_metadata},
    {"vf-metadata", mp_property_filter_metadata, .priv = "vf"},
    {"af-metadata", mp_property_filter_metadata, .priv = "af"},
    {"core-idle", mp_property_core_idle},
    {"eof-reached", mp_property_eof_reached},
    {"seeking", mp_property_seeking},
    {"playback-abort", mp_property_playback_abort},
    {"cache-speed", mp_property_cache_speed},
    {"demuxer-cache-duration", mp_property_demuxer_cache_duration},
    {"demuxer-cache-time", mp_property_demuxer_cache_time},
    {"demuxer-cache-idle", mp_property_demuxer_cache_idle},
    {"demuxer-start-time", mp_property_demuxer_start_time},
    {"demuxer-cache-state", mp_property_demuxer_cache_state},
    {"cache-buffering-state", mp_property_cache_buffering},
    {"paused-for-cache", mp_property_paused_for_cache},
    {"demuxer-via-network", mp_property_demuxer_is_network},
    {"clock", mp_property_clock},
    {"seekable", mp_property_seekable},
    {"partially-seekable", mp_property_partially_seekable},
    {"deinterlace-active", mp_property_deinterlace},
    {"idle-active", mp_property_idle},
    {"window-id", mp_property_window_id},

    {"chapter-list", mp_property_list_chapters},
    {"track-list", mp_property_list_tracks},
    {"current-tracks", mp_property_current_tracks},
    {"edition-list", property_list_editions},

    {"playlist", mp_property_playlist},
    {"playlist-path", mp_property_playlist_path},
    {"playlist-pos", mp_property_playlist_pos},
    {"playlist-pos-1", mp_property_playlist_pos_1},
    {"playlist-current-pos", mp_property_playlist_current_pos},
    {"playlist-playing-pos", mp_property_playlist_playing_pos},
    M_PROPERTY_ALIAS("playlist-count", "playlist/count"),

    // Audio
    {"mixer-active", mp_property_mixer_active},
    {"volume", mp_property_volume},
    {"volume-gain", mp_property_volume_gain},
    {"ao-volume", mp_property_ao_volume},
    {"ao-mute", mp_property_ao_mute},
    {"audio-delay", mp_property_audio_delay},
    M_PROPERTY_ALIAS("audio-codec-name", "current-tracks/audio/codec"),
    M_PROPERTY_ALIAS("audio-codec", "current-tracks/audio/codec-desc"),
    {"audio-params", mp_property_audio_params},
    {"audio-out-params", mp_property_audio_out_params},
    {"aid", mp_property_switch_track, .priv = (void *)(const int[]){0, STREAM_AUDIO}},
    {"audio-device", mp_property_audio_device},
    {"audio-device-list", mp_property_audio_devices},
    {"current-ao", mp_property_ao},

    // Video
    {"video-target-params", mp_property_tgt_imgparams},
    {"video-out-params", mp_property_vo_imgparams},
    {"video-dec-params", mp_property_dec_imgparams},
    {"video-params", mp_property_vd_imgparams},
    {"video-frame-info", mp_property_video_frame_info},
    M_PROPERTY_ALIAS("video-format", "current-tracks/video/codec"),
    M_PROPERTY_ALIAS("video-codec", "current-tracks/video/codec-desc"),
    M_PROPERTY_ALIAS("dwidth", "video-out-params/dw"),
    M_PROPERTY_ALIAS("dheight", "video-out-params/dh"),
    M_PROPERTY_ALIAS("width", "video-params/w"),
    M_PROPERTY_ALIAS("height", "video-params/h"),
    {"current-window-scale", mp_property_current_window_scale},
    {"vo-configured", mp_property_vo_configured},
    {"vo-passes", mp_property_vo_passes},
    {"perf-info", mp_property_perf_info},
    {"current-vo", mp_property_vo},
    {"current-gpu-context", mp_property_gpu_context},
    {"container-fps", mp_property_fps},
    {"estimated-vf-fps", mp_property_vf_fps},
    {"video-aspect-override", mp_property_video_aspect_override},
    {"vid", mp_property_switch_track, .priv = (void *)(const int[]){0, STREAM_VIDEO}},
    {"hwdec-current", mp_property_hwdec_current},
    {"hwdec-interop", mp_property_hwdec_interop},

    {"estimated-frame-count", mp_property_frame_count},
    {"estimated-frame-number", mp_property_frame_number},

    {"osd-dimensions", mp_property_osd_dim},
    M_PROPERTY_ALIAS("osd-width", "osd-dimensions/w"),
    M_PROPERTY_ALIAS("osd-height", "osd-dimensions/h"),
    M_PROPERTY_ALIAS("osd-par", "osd-dimensions/par"),

    {"osd-sym-cc", mp_property_osd_sym},
    {"osd-ass-cc", mp_property_osd_ass},

    {"term-clip-cc", mp_property_term_clip},

    {"mouse-pos", mp_property_mouse_pos},
    {"touch-pos", mp_property_touch_pos},

    // Subs
    {"sid", mp_property_switch_track, .priv = (void *)(const int[]){0, STREAM_SUB}},
    {"secondary-sid", mp_property_switch_track,
        .priv = (void *)(const int[]){1, STREAM_SUB}},
    {"sub-delay", mp_property_sub_delay, .priv = (void *)&(const int){0}},
    {"secondary-sub-delay", mp_property_sub_delay,
        .priv = (void *)&(const int){1}},
    {"sub-speed", mp_property_sub_speed},
    {"sub-pos", mp_property_sub_pos, .priv = (void *)&(const int){0}},
    {"secondary-sub-pos", mp_property_sub_pos,
        .priv = (void *)&(const int){1}},
    {"sub-ass-extradata", mp_property_sub_ass_extradata},
    {"sub-text", mp_property_sub_text,
        .priv = (void *)&(const int[]){0, SD_TEXT_TYPE_PLAIN}},
    {"secondary-sub-text", mp_property_sub_text,
        .priv = (void *)&(const int[]){1, SD_TEXT_TYPE_PLAIN}},
    M_PROPERTY_DEPRECATED_ALIAS("sub-text-ass", "sub-text/ass"),
    {"sub-start", mp_property_sub_start,
        .priv = (void *)&(const int){0}},
    {"secondary-sub-start", mp_property_sub_start,
        .priv = (void *)&(const int){1}},
    {"sub-end", mp_property_sub_end,
        .priv = (void *)&(const int){0}},
    {"secondary-sub-end", mp_property_sub_end,
        .priv = (void *)&(const int){1}},

    {"vf", mp_property_vf},
    {"af", mp_property_af},

    {"ab-loop-a", mp_property_ab_loop},
    {"ab-loop-b", mp_property_ab_loop},

    {"video-bitrate", mp_property_packet_bitrate, .priv = (void *)&(const int){STREAM_VIDEO}},
    {"audio-bitrate", mp_property_packet_bitrate, .priv = (void *)&(const int){STREAM_AUDIO}},
    {"sub-bitrate", mp_property_packet_bitrate, .priv = (void *)&(const int){STREAM_SUB}},

    {"focused", mp_property_focused},
    {"display-names", mp_property_display_names},
    {"display-fps", mp_property_display_fps},
    {"estimated-display-fps", mp_property_estimated_display_fps},
    {"vsync-jitter", mp_property_vsync_jitter},
    {"display-hidpi-scale", mp_property_hidpi_scale},
    {"ambient-light", mp_property_ambient_light},

    {"working-directory", mp_property_cwd},
    {"current-watch-later-dir", mp_property_current_watch_later_dir},

    {"protocol-list", mp_property_protocols},
    {"decoder-list", mp_property_decoders},
    {"encoder-list", mp_property_encoders},
    {"demuxer-lavf-list", mp_property_lavf_demuxers},
    {"input-key-list", mp_property_keylist},

    {"mpv-version", mp_property_version},
    {"mpv-configuration", mp_property_configuration},
    {"ffmpeg-version", mp_property_ffmpeg},
    {"libass-version", mp_property_libass_version},
    {"platform", mp_property_platform},

    {"options", mp_property_options},
    {"file-local-options", mp_property_local_options},
    {"option-info", mp_property_option_info},
    {"property-list", mp_property_list},
    {"profile-list", mp_profile_list},
    {"command-list", mp_property_commands},
    {"input-bindings", mp_property_bindings},

    {"menu-data", mp_property_mdata},

    {"user-data", mp_property_udata},
    {"term-size", mp_property_term_size},

    {"clipboard", mp_property_clipboard},
    {"current-clipboard-backend", mp_property_current_clipboard_backend},

    M_PROPERTY_ALIAS("video", "vid"),
    M_PROPERTY_ALIAS("audio", "aid"),
    M_PROPERTY_ALIAS("sub", "sid"),

    // compatibility
    M_PROPERTY_ALIAS("colormatrix", "video-params/colormatrix"),
    M_PROPERTY_ALIAS("colormatrix-input-range", "video-params/colorlevels"),
    M_PROPERTY_ALIAS("colormatrix-primaries", "video-params/primaries"),
    M_PROPERTY_ALIAS("colormatrix-gamma", "video-params/gamma"),

    M_PROPERTY_DEPRECATED_ALIAS("sub-forced-only-cur", "sub-forced-events-only"),
};

// Each entry describes which properties an event (possibly) changes.
#define E(x, ...) [x] = (const char*const[]){__VA_ARGS__, NULL}
static const char *const *const mp_event_property_change[] = {
    E(MPV_EVENT_START_FILE, "*"),
    E(MPV_EVENT_END_FILE, "*"),
    E(MPV_EVENT_FILE_LOADED, "*"),
    E(MP_EVENT_CHANGE_ALL, "*"),
    E(MP_EVENT_TRACKS_CHANGED, "track-list", "current-tracks"),
    E(MP_EVENT_TRACK_SWITCHED, "track-list", "current-tracks"),
    E(MPV_EVENT_IDLE, "*"),
    E(MPV_EVENT_TICK, "time-pos", "audio-pts", "stream-pos", "avsync",
      "percent-pos", "time-remaining", "playtime-remaining", "playback-time",
      "estimated-vf-fps", "total-avsync-change", "audio-speed-correction",
      "video-speed-correction", "vo-delayed-frame-count", "mistimed-frame-count",
      "vsync-ratio", "estimated-display-fps", "vsync-jitter", "sub-text",
      "secondary-sub-text", "audio-bitrate", "video-bitrate", "sub-bitrate",
      "decoder-frame-drop-count", "frame-drop-count", "video-frame-info",
      "vf-metadata", "af-metadata", "sub-start", "sub-end", "secondary-sub-start",
      "secondary-sub-end", "video-out-params", "video-dec-params", "video-params",
      "deinterlace-active", "video-target-params"),
    E(MP_EVENT_DURATION_UPDATE, "duration"),
    E(MPV_EVENT_VIDEO_RECONFIG, "video-out-params", "video-params",
      "video-format", "video-codec", "video-bitrate", "dwidth", "dheight",
      "width", "height", "container-fps", "aspect", "aspect-name", "vo-configured", "current-vo",
      "video-dec-params", "osd-dimensions", "hwdec", "hwdec-current", "hwdec-interop",
      "window-id", "track-list", "current-tracks"),
    E(MPV_EVENT_AUDIO_RECONFIG, "audio-format", "audio-codec", "audio-bitrate",
      "samplerate", "channels", "audio", "volume", "volume-gain", "mute",
      "current-ao", "audio-codec-name", "audio-params", "track-list", "current-tracks",
      "audio-out-params", "volume-max", "volume-gain-min", "volume-gain-max", "mixer-active"),
    E(MPV_EVENT_SEEK, "seeking", "core-idle", "eof-reached"),
    E(MPV_EVENT_PLAYBACK_RESTART, "seeking", "core-idle", "eof-reached"),
    E(MP_EVENT_METADATA_UPDATE, "metadata", "filtered-metadata", "media-title"),
    E(MP_EVENT_CHAPTER_CHANGE, "chapter", "chapter-metadata"),
    E(MP_EVENT_CACHE_UPDATE,
      "demuxer-cache-duration", "demuxer-cache-idle", "paused-for-cache",
      "demuxer-cache-time", "cache-buffering-state", "cache-speed",
      "demuxer-cache-state"),
    E(MP_EVENT_WIN_RESIZE, "current-window-scale", "osd-width", "osd-height",
      "osd-par", "osd-dimensions"),
    E(MP_EVENT_WIN_STATE, "display-names", "display-fps", "display-width",
      "display-height"),
    E(MP_EVENT_WIN_STATE2, "display-hidpi-scale"),
    E(MP_EVENT_FOCUS, "focused"),
    E(MP_EVENT_AMBIENT_LIGHTING_CHANGED, "ambient-light"),
    E(MP_EVENT_CHANGE_PLAYLIST, "playlist", "playlist-pos", "playlist-pos-1",
      "playlist-count", "playlist/count", "playlist-current-pos",
      "playlist-playing-pos"),
    E(MP_EVENT_INPUT_PROCESSED, "mouse-pos", "touch-pos"),
    E(MP_EVENT_CORE_IDLE, "core-idle", "eof-reached"),
};
#undef E

// If there is no prefix, return length+1 (avoids matching full name as prefix).
static int prefix_len(const char *p)
{
    const char *end = strchr(p, '/');
    return end ? end - p : strlen(p) + 1;
}

static bool match_property(const char *a, const char *b)
{
    if (strcmp(a, "*") == 0)
        return true;
    // Give options and properties the same ID each, so change notifications
    // work both way.
    if (strncmp(a, "options/", 8) == 0)
        a += 8;
    if (strncmp(b, "options/", 8) == 0)
        b += 8;
    int len_a = prefix_len(a);
    int len_b = prefix_len(b);
    return strncmp(a, b, MPMIN(len_a, len_b)) == 0;
}

// Return a bitset of events which change the property.
uint64_t mp_get_property_event_mask(const char *name)
{
    uint64_t mask = 0;
    for (int n = 0; n < MP_ARRAY_SIZE(mp_event_property_change); n++) {
        const char *const *const list = mp_event_property_change[n];
        for (int i = 0; list && list[i]; i++) {
            if (match_property(list[i], name))
                mask |= 1ULL << n;
        }
    }
    return mask;
}

// Return an ID for the property. It might not be unique, but is good enough
// for property change handling. Return -1 if property unknown.
int mp_get_property_id(struct MPContext *mpctx, const char *name)
{
    struct command_ctx *ctx = mpctx->command_ctx;
    for (int n = 0; ctx->properties[n].name; n++) {
        if (match_property(ctx->properties[n].name, name))
            return n;
    }
    return -1;
}

static bool is_property_set(int action, void *val)
{
    switch (action) {
    case M_PROPERTY_SET:
    case M_PROPERTY_SWITCH:
    case M_PROPERTY_SET_STRING:
    case M_PROPERTY_SET_NODE:
    case M_PROPERTY_MULTIPLY:
        return true;
    case M_PROPERTY_KEY_ACTION: {
        struct m_property_action_arg *key = val;
        return is_property_set(key->action, key->arg);
    }
    default:
        return false;
    }
}

int mp_property_do(const char *name, int action, void *val,
                   struct MPContext *ctx)
{
    struct command_ctx *cmd = ctx->command_ctx;
    int r = m_property_do(ctx->log, cmd->properties, name, action, val, ctx);

    if (mp_msg_test(ctx->log, MSGL_V) && is_property_set(action, val)) {
        struct m_option option_type = {0};
        void *data = val;
        switch (action) {
        case M_PROPERTY_SET_NODE:
            option_type.type = &m_option_type_node;
            break;
        case M_PROPERTY_SET_STRING:
            option_type.type = &m_option_type_string;
            data = &val;
            break;
        }
        char *t = option_type.type ? m_option_print(&option_type, data) : NULL;
        MP_VERBOSE(ctx, "Set property: %s%s%s -> %d\n",
                   name, t ? "=" : "", t ? t : "", r);
        talloc_free(t);
    }
    return r;
}

char *mp_property_expand_string(struct MPContext *mpctx, const char *str)
{
    struct command_ctx *ctx = mpctx->command_ctx;
    return m_properties_expand_string(ctx->properties, str, mpctx);
}

// Before expanding properties, parse C-style escapes like "\n"
char *mp_property_expand_escaped_string(struct MPContext *mpctx, const char *str)
{
    void *tmp = talloc_new(NULL);
    bstr strb = bstr0(str);
    bstr dst = {0};
    while (strb.len) {
        if (!mp_append_escaped_string(tmp, &dst, &strb)) {
            talloc_free(tmp);
            return talloc_strdup(NULL, "(broken escape sequences)");
        }
        // pass " through literally
        if (!bstr_eatstart0(&strb, "\""))
            break;
        bstr_xappend(tmp, &dst, bstr0("\""));
    }
    char *r = mp_property_expand_string(mpctx, dst.start);
    talloc_free(tmp);
    return r;
}

void property_print_help(struct MPContext *mpctx)
{
    struct command_ctx *ctx = mpctx->command_ctx;
    m_properties_print_help_list(mpctx->log, ctx->properties);
}

/* List of default ways to show a property on OSD.
 *
 * If osd_progbar is set, a bar showing the current position between min/max
 * values of the property is shown. In this case osd_msg is only used for
 * terminal output if there is no video; it'll be a label shown together with
 * percentage.
 */
static const struct property_osd_display {
    // property name
    const char *name;
    // name used on OSD
    const char *osd_name;
    // progressbar type
    int osd_progbar;
    // Needs special ways to display the new value (seeks are delayed)
    int seek_msg, seek_bar;
    // Show a marker thing on OSD bar. Ignored if osd_progbar==0.
    float marker;
    // Free-form message (if NULL, osd_name or the property name is used)
    const char *msg;
} property_osd_display[] = {
    // general
    {"loop-playlist", "Loop playlist"},
    {"loop-file", "Loop current file"},
    {"chapter",
     .seek_msg = OSD_SEEK_INFO_CHAPTER_TEXT,
     .seek_bar = OSD_SEEK_INFO_BAR},
    {"hr-seek", "hr-seek"},
    {"speed", "Speed"},
    {"pitch", "Pitch"},
    {"clock", "Clock"},
    {"edition", "Edition"},
    // audio
    {"volume", "Volume",
     .msg = "Volume: ${?volume:${volume}% ${?mute==yes:(Muted)}}${!volume:${volume}}",
     .osd_progbar = OSD_VOLUME, .marker = 100},
    {"volume-gain", "Volume gain",
     .msg = "Volume gain: ${?volume-gain:${volume-gain} dB ${?mute==yes:(Muted)}}${!volume-gain:${volume-gain}}",
     .osd_progbar = OSD_VOLUME, .marker = 0},
    {"ao-volume", "AO Volume",
     .msg = "AO Volume: ${?ao-volume:${ao-volume}% ${?ao-mute==yes:(Muted)}}${!ao-volume:${ao-volume}}",
     .osd_progbar = OSD_VOLUME, .marker = 100},
    {"mute", "Mute"},
    {"ao-mute", "AO Mute"},
    {"audio-delay", "A-V delay"},
    {"audio", "Audio"},
    // video
    {"panscan", "Panscan", .osd_progbar = OSD_PANSCAN},
    {"taskbar-progress", "Progress in taskbar"},
    {"snap-window", "Snap to screen edges"},
    {"ontop", "Stay on top"},
    {"on-all-workspaces", "Visibility on all workspaces"},
    {"border", "Border"},
    {"framedrop", "Framedrop"},
    {"deinterlace", "Deinterlace"},
    {"gamma", "Gamma", .osd_progbar = OSD_BRIGHTNESS },
    {"brightness", "Brightness", .osd_progbar = OSD_BRIGHTNESS},
    {"contrast", "Contrast", .osd_progbar = OSD_CONTRAST},
    {"saturation", "Saturation", .osd_progbar = OSD_SATURATION},
    {"hue", "Hue", .osd_progbar = OSD_HUE},
    {"angle", "Angle"},
    // subs
    {"sub", "Subtitles"},
    {"secondary-sid", "Secondary subtitles"},
    {"sub-pos", "Sub position"},
    {"secondary-sub-pos", "Secondary sub position"},
    {"sub-delay", "Sub delay"},
    {"secondary-sub-delay", "Secondary sub delay"},
    {"sub-speed", "Sub speed"},
    {"sub-visibility",
     .msg = "Subtitles ${!sub-visibility==yes:hidden}"
      "${?sub-visibility==yes:visible${?sub==no: (but no subtitles selected)}}"},
    {"secondary-sub-visibility",
     .msg = "Secondary Subtitles ${!secondary-sub-visibility==yes:hidden}"
      "${?secondary-sub-visibility==yes:visible${?secondary-sid==no: (but no secondary subtitles selected)}}"},
    {"sub-forced-events-only", "Forced sub only"},
    {"sub-scale", "Sub Scale"},
    {"sub-ass-use-video-data", "Subtitle using video properties"},
    {"sub-ass-video-aspect-override", "Subtitle aspect override"},
    {"sub-ass-override", "ASS subtitle style override"},
    {"secondary-sub-ass-override", "Secondary sub ASS subtitle style override"},
    {"vf", "Video filters", .msg = "Video filters:\n${vf}"},
    {"af", "Audio filters", .msg = "Audio filters:\n${af}"},
    {"ab-loop-a", "A-B loop start"},
    {"ab-loop-b", .msg = "A-B loop: ${ab-loop-a} - ${ab-loop-b}"
                            "${?=ab-loop-count==0: (disabled)}"},
    {"audio-device", "Audio device"},
    {"hwdec", .msg = "Hardware decoding: ${hwdec-current}"},
    {"video-aspect-override", "Aspect ratio override"},
    // By default, don't display the following properties on OSD
    {"pause", NULL},
    {"fullscreen", NULL},
    {"window-minimized", NULL},
    {"window-maximized", NULL},
    {0}
};

static void show_property_osd(MPContext *mpctx, const char *name, int osd_mode)
{
    struct MPOpts *opts = mpctx->opts;
    struct property_osd_display disp = {.name = name, .osd_name = name};

    if (!osd_mode)
        return;

    // look for the command
    for (const struct property_osd_display *p = property_osd_display; p->name; p++)
    {
        if (!strcmp(p->name, name)) {
            disp = *p;
            break;
        }
    }

    if (osd_mode == MP_ON_OSD_AUTO) {
        osd_mode =
            ((disp.msg || disp.osd_name || disp.seek_msg) ? MP_ON_OSD_MSG : 0) |
            ((disp.osd_progbar || disp.seek_bar) ? MP_ON_OSD_BAR : 0);
    }

    if (!disp.osd_progbar)
        disp.osd_progbar = ' ';

    if (!disp.osd_name)
        disp.osd_name = name;

    if (disp.seek_msg || disp.seek_bar) {
        mpctx->add_osd_seek_info |=
            (osd_mode & MP_ON_OSD_MSG ? disp.seek_msg : 0) |
            (osd_mode & MP_ON_OSD_BAR ? disp.seek_bar : 0);
        return;
    }

    struct m_option prop = {0};
    mp_property_do(name, M_PROPERTY_GET_CONSTRICTED_TYPE, &prop, mpctx);
    if ((osd_mode & MP_ON_OSD_BAR)) {
        if (prop.type == CONF_TYPE_INT && prop.min < prop.max) {
            int n = prop.min;
            if (disp.osd_progbar)
                n = disp.marker;
            int i;
            if (mp_property_do(name, M_PROPERTY_GET, &i, mpctx) > 0)
                set_osd_bar(mpctx, disp.osd_progbar, prop.min, prop.max, n, i);
        } else if (prop.type == CONF_TYPE_FLOAT && prop.min < prop.max) {
            float n = prop.min;
            if (disp.osd_progbar)
                n = disp.marker;
            float f;
            if (mp_property_do(name, M_PROPERTY_GET, &f, mpctx) > 0)
                set_osd_bar(mpctx, disp.osd_progbar, prop.min, prop.max, n, f);
        }
    }

    if (osd_mode & MP_ON_OSD_MSG) {
        void *tmp = talloc_new(NULL);

        const char *msg = disp.msg;
        if (!msg)
            msg = talloc_asprintf(tmp, "%s: ${%s}", disp.osd_name, name);

        char *osd_msg = talloc_steal(tmp, mp_property_expand_string(mpctx, msg));

        if (osd_msg && osd_msg[0])
            set_osd_msg(mpctx, 1, opts->osd_duration, "%s", osd_msg);

        talloc_free(tmp);
    }
}

static bool reinit_filters(MPContext *mpctx, enum stream_type mediatype)
{
    switch (mediatype) {
    case STREAM_VIDEO:
        return reinit_video_filters(mpctx) >= 0;
    case STREAM_AUDIO:
        return reinit_audio_filters(mpctx) >= 0;
    }
    return false;
}

static const char *const filter_opt[STREAM_TYPE_COUNT] = {
    [STREAM_VIDEO] = "vf",
    [STREAM_AUDIO] = "af",
};

static int set_filters(struct MPContext *mpctx, enum stream_type mediatype,
                       struct m_obj_settings *new_chain)
{
    bstr option = bstr0(filter_opt[mediatype]);
    struct m_config_option *co = m_config_get_co(mpctx->mconfig, option);
    if (!co)
        return -1;

    struct m_obj_settings **list = co->data;
    struct m_obj_settings *old_settings = *list;
    *list = NULL;
    m_option_copy(co->opt, list, &new_chain);

    bool success = reinit_filters(mpctx, mediatype);

    if (success) {
        m_option_free(co->opt, &old_settings);
        m_config_notify_change_opt_ptr(mpctx->mconfig, list);
    } else {
        m_option_free(co->opt, list);
        *list = old_settings;
    }

    return success ? 0 : -1;
}

static int edit_filters(struct MPContext *mpctx, struct mp_log *log,
                        enum stream_type mediatype,
                        const char *cmd, const char *arg)
{
    bstr option = bstr0(filter_opt[mediatype]);
    struct m_config_option *co = m_config_get_co(mpctx->mconfig, option);
    if (!co)
        return -1;

    // The option parser is used to modify the filter list itself.
    char optname[20];
    snprintf(optname, sizeof(optname), "%.*s-%s", BSTR_P(option), cmd);

    struct m_obj_settings *new_chain = NULL;
    m_option_copy(co->opt, &new_chain, co->data);

    int r = m_option_parse(log, co->opt, bstr0(optname), bstr0(arg), &new_chain);
    if (r >= 0)
        r = set_filters(mpctx, mediatype, new_chain);

    m_option_free(co->opt, &new_chain);

    return r >= 0 ? 0 : -1;
}

static int edit_filters_osd(struct MPContext *mpctx, enum stream_type mediatype,
                            const char *cmd, const char *arg, bool on_osd)
{
    int r = edit_filters(mpctx, mpctx->log, mediatype, cmd, arg);
    if (on_osd) {
        if (r >= 0) {
            const char *prop = filter_opt[mediatype];
            show_property_osd(mpctx, prop, MP_ON_OSD_MSG);
        } else {
            set_osd_msg(mpctx, 1, mpctx->opts->osd_duration,
                         "Changing filters failed!");
        }
    }
    return r;
}

static void recreate_overlays(struct MPContext *mpctx)
{
    struct command_ctx *cmd = mpctx->command_ctx;
    int overlay_next = !cmd->overlay_osd_current;
    struct sub_bitmaps *new = &cmd->overlay_osd[overlay_next];
    new->format = SUBBITMAP_BGRA;
    new->change_id = 1;

    bool valid = false;

    new->num_parts = 0;
    for (int n = 0; n < cmd->num_overlays; n++) {
        struct overlay *o = &cmd->overlays[n];
        if (o->source) {
            struct mp_image *s = o->source;
            struct sub_bitmap b = {
                .bitmap = s->planes[0],
                .stride = s->stride[0],
                .w = s->w, .dw = o->dw,
                .h = s->h, .dh = o->dh,
                .x = o->x,
                .y = o->y,
            };
            MP_TARRAY_APPEND(cmd, new->parts, new->num_parts, b);
        }
    }

    if (!cmd->overlay_packer)
        cmd->overlay_packer = talloc_zero(cmd, struct bitmap_packer);

    cmd->overlay_packer->padding = 1; // assume bilinear scaling
    packer_set_size(cmd->overlay_packer, new->num_parts);

    for (int n = 0; n < new->num_parts; n++)
        cmd->overlay_packer->in[n] = (struct pos){new->parts[n].w, new->parts[n].h};

    if (packer_pack(cmd->overlay_packer) < 0 || new->num_parts == 0)
        goto done;

    struct pos bb[2];
    packer_get_bb(cmd->overlay_packer, bb);

    new->packed_w = bb[1].x;
    new->packed_h = bb[1].y;

    if (!new->packed || new->packed->w < new->packed_w ||
                        new->packed->h < new->packed_h)
    {
        talloc_free(new->packed);
        new->packed = mp_image_alloc(IMGFMT_BGRA, cmd->overlay_packer->w,
                                                  cmd->overlay_packer->h);
        if (!new->packed)
            goto done;
    }

    if (!mp_image_make_writeable(new->packed))
        goto done;

    // clear padding
    mp_image_clear(new->packed, 0, 0, new->packed->w, new->packed->h);

    for (int n = 0; n < new->num_parts; n++) {
        struct sub_bitmap *b = &new->parts[n];
        struct pos pos = cmd->overlay_packer->result[n];

        int stride = new->packed->stride[0];
        void *pdata = (uint8_t *)new->packed->planes[0] + pos.y * stride + pos.x * 4;
        memcpy_pic(pdata, b->bitmap, b->w * 4, b->h, stride, b->stride);

        b->bitmap = pdata;
        b->stride = stride;

        b->src_x = pos.x;
        b->src_y = pos.y;
    }

    valid = true;
done:
    if (!valid) {
        new->format = SUBBITMAP_EMPTY;
        new->num_parts = 0;
    }

    osd_set_external2(mpctx->osd, new);
    mp_wakeup_core(mpctx);
    cmd->overlay_osd_current = overlay_next;
}

// Set overlay with the given ID to the contents as described by "new".
static void replace_overlay(struct MPContext *mpctx, int id, struct overlay *new)
{
    struct command_ctx *cmd = mpctx->command_ctx;
    assert(id >= 0);
    if (id >= cmd->num_overlays) {
        MP_TARRAY_GROW(cmd, cmd->overlays, id);
        while (cmd->num_overlays <= id)
            cmd->overlays[cmd->num_overlays++] = (struct overlay){0};
    }

    struct overlay *ptr = &cmd->overlays[id];

    talloc_free(ptr->source);
    *ptr = *new;

    recreate_overlays(mpctx);
}

static void cmd_overlay_add(void *pcmd)
{
    struct mp_cmd_ctx *cmd = pcmd;
    struct MPContext *mpctx = cmd->mpctx;
    int id = cmd->args[0].v.i, x = cmd->args[1].v.i, y = cmd->args[2].v.i;
    char *file = cmd->args[3].v.s;
    int offset = cmd->args[4].v.i;
    char *fmt = cmd->args[5].v.s;
    int w = cmd->args[6].v.i, h = cmd->args[7].v.i, stride = cmd->args[8].v.i;
    int dw = cmd->args[9].v.i, dh = cmd->args[10].v.i;

    if (dw <= 0)
        dw = w;
    if (dh <= 0)
        dh = h;
    if (strcmp(fmt, "bgra") != 0) {
        MP_ERR(mpctx, "overlay-add: unsupported OSD format '%s'\n", fmt);
        goto error;
    }
    if (id < 0 || id >= 64) { // arbitrary upper limit
        MP_ERR(mpctx, "overlay-add: invalid id %d\n", id);
        goto error;
    }
    if (w <= 0 || h <= 0 || stride < w * 4 || (stride % 4)) {
        MP_ERR(mpctx, "overlay-add: inconsistent parameters\n");
        goto error;
    }
    struct overlay overlay = {
        .source = mp_image_alloc(IMGFMT_BGRA, w, h),
        .x = x,
        .y = y,
        .dw = dw,
        .dh = dh,
    };
    if (!overlay.source)
        goto error;
    int fd = -1;
    bool close_fd = true;
    void *p = NULL;
    if (file[0] == '@') {
        char *end;
        fd = strtol(&file[1], &end, 10);
        if (!file[1] || end[0])
            fd = -1;
        close_fd = false;
    } else if (file[0] == '&') {
        char *end;
        unsigned long long addr = strtoull(&file[1], &end, 0);
        if (!file[1] || end[0])
            addr = 0;
        p = (void *)(uintptr_t)addr;
    } else {
        fd = open(file, O_RDONLY | O_BINARY | O_CLOEXEC);
    }
    int map_size = 0;
    if (fd >= 0) {
        map_size = offset + h * stride;
        void *m = mmap(NULL, map_size, PROT_READ, MAP_SHARED, fd, 0);
        if (close_fd)
            close(fd);
        if (m && m != MAP_FAILED)
            p = m;
    }
    if (!p) {
        MP_ERR(mpctx, "overlay-add: could not open or map '%s'\n", file);
        talloc_free(overlay.source);
        goto error;
    }
    memcpy_pic(overlay.source->planes[0], (char *)p + offset, w * 4, h,
               overlay.source->stride[0], stride);
    if (map_size)
        munmap(p, map_size);

    replace_overlay(mpctx, id, &overlay);
    return;
error:
    cmd->success = false;
}

static void cmd_overlay_remove(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    struct command_ctx *cmdctx = mpctx->command_ctx;
    int id = cmd->args[0].v.i;
    if (id >= 0 && id < cmdctx->num_overlays)
        replace_overlay(mpctx, id, &(struct overlay){0});
}

static void overlay_uninit(struct MPContext *mpctx)
{
    struct command_ctx *cmd = mpctx->command_ctx;
    if (!mpctx->osd)
        return;
    for (int id = 0; id < cmd->num_overlays; id++)
        replace_overlay(mpctx, id, &(struct overlay){0});
    osd_set_external2(mpctx->osd, NULL);
    for (int n = 0; n < 2; n++)
        mp_image_unrefp(&cmd->overlay_osd[n].packed);
}

static void cmd_osd_overlay(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    double rc[4] = {0};

    struct osd_external_ass ov = {
        .owner  = cmd->cmd->sender,
        .id     = cmd->args[0].v.i64,
        .format = cmd->args[1].v.i,
        .data   = cmd->args[2].v.s,
        .res_x  = cmd->args[3].v.i,
        .res_y  = cmd->args[4].v.i,
        .z      = cmd->args[5].v.i,
        .hidden = cmd->args[6].v.b,
        .out_rc = cmd->args[7].v.b ? rc : NULL,
    };

    osd_set_external(mpctx->osd, &ov);

    struct mpv_node *res = &cmd->result;
    node_init(res, MPV_FORMAT_NODE_MAP, NULL);

    // (An empty rc uses INFINITY, avoid in JSON, just leave it unset.)
    if (rc[0] < rc[2] && rc[1] < rc[3]) {
        node_map_add_double(res, "x0", rc[0]);
        node_map_add_double(res, "y0", rc[1]);
        node_map_add_double(res, "x1", rc[2]);
        node_map_add_double(res, "y1", rc[3]);
    }

    mp_wakeup_core(mpctx);
}

static struct track *find_track_with_url(struct MPContext *mpctx, int type, const char *url)
{
    char *path = mp_get_user_path(NULL, mpctx->global, url);
    struct track *t = NULL;
    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct track *track = mpctx->tracks[n];
        if (track && track->type == type && track->is_external) {
            char *normalized = mp_normalize_user_path(NULL, mpctx->global, track->external_filename);
            t = strcmp(normalized, path) == 0 ? track : NULL;
            talloc_free(normalized);
            if (t)
                break;
        }
    }
    talloc_free(path);
    return t;
}

// Whether this property should react to key events generated by auto-repeat.
static bool check_property_autorepeat(char *property,  struct MPContext *mpctx)
{
    struct m_option prop = {0};
    if (mp_property_do(property, M_PROPERTY_GET_TYPE, &prop, mpctx) <= 0)
        return true;

    // This is a heuristic at best.
    if (prop.type->flags & M_OPT_TYPE_CHOICE)
        return false;

    return true;
}

// Whether changes to this property (add/cycle cmds) benefit from cmd->scale
static bool check_property_scalable(char *property, struct MPContext *mpctx)
{
    struct m_option prop = {0};
    if (mp_property_do(property, M_PROPERTY_GET_TYPE, &prop, mpctx) <= 0)
        return true;

    // These properties are backed by a floating-point number
    return prop.type == &m_option_type_float ||
           prop.type == &m_option_type_double ||
           prop.type == &m_option_type_time ||
           prop.type == &m_option_type_aspect;
}

static void show_property_status(struct mp_cmd_ctx *cmd, const char *name, int r)
{
    struct MPContext *mpctx = cmd->mpctx;
    struct MPOpts *opts = mpctx->opts;
    int osd_duration = opts->osd_duration;
    int osdl = cmd->msg_osd ? 1 : OSD_LEVEL_INVISIBLE;

    if (r == M_PROPERTY_OK || r == M_PROPERTY_UNAVAILABLE) {
        show_property_osd(mpctx, name, cmd->on_osd);
        if (r == M_PROPERTY_UNAVAILABLE)
            cmd->success = false;
    } else if (r == M_PROPERTY_UNKNOWN) {
        set_osd_msg(mpctx, osdl, osd_duration, "Unknown property: '%s'", name);
        cmd->success = false;
    } else if (r <= 0) {
        set_osd_msg(mpctx, osdl, osd_duration, "Failed to set property '%s'",
                    name);
        cmd->success = false;
    }
}

static void change_property_cmd(struct mp_cmd_ctx *cmd,
                                const char *name, int action, void *arg)
{
    int r = mp_property_do(name, action, arg, cmd->mpctx);
    show_property_status(cmd, name, r);
}

static void cmd_cycle_values(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    int first = 0, dir = 1;

    if (strcmp(cmd->args[first].v.s, "!reverse") == 0) {
        first += 1;
        dir = -1;
    }

    const char *name = cmd->args[first].v.s;
    first += 1;

    if (first >= cmd->num_args) {
        MP_ERR(mpctx, "cycle-values command does not have any value arguments.\n");
        cmd->success = false;
        return;
    }

    struct m_option prop = {0};
    int r = mp_property_do(name, M_PROPERTY_GET_TYPE, &prop, mpctx);
    if (r <= 0) {
        show_property_status(cmd, name, r);
        return;
    }

    union m_option_value curval = m_option_value_default;
    r = mp_property_do(name, M_PROPERTY_GET, &curval, mpctx);
    if (r <= 0) {
        show_property_status(cmd, name, r);
        return;
    }

    int current = -1;
    for (int n = first; n < cmd->num_args; n++) {
        union m_option_value val = m_option_value_default;
        if (m_option_parse(mpctx->log, &prop, bstr0(name),
                           bstr0(cmd->args[n].v.s), &val) < 0)
            continue;

        if (m_option_equal(&prop, &curval, &val))
            current = n;

        m_option_free(&prop, &val);

        if (current >= 0)
            break;
    }

    m_option_free(&prop, &curval);

    if (current >= 0) {
        current += dir;
        if (current < first)
            current = cmd->num_args - 1;
        if (current >= cmd->num_args)
            current = first;
    } else {
        MP_VERBOSE(mpctx, "Current value not found. Picking default.\n");
        current = dir > 0 ? first : cmd->num_args - 1;
    }

    change_property_cmd(cmd, name, M_PROPERTY_SET_STRING, cmd->args[current].v.s);
}

struct cmd_list_ctx {
    struct MPContext *mpctx;

    // actual list command
    struct mp_cmd_ctx *parent;

    bool current_valid;
    mp_thread_id current_tid;
    bool completed_recursive;

    // list of sub commands yet to run
    struct mp_cmd **sub;
    int num_sub;
};

static void continue_cmd_list(struct cmd_list_ctx *list);

static void on_cmd_list_sub_completion(struct mp_cmd_ctx *cmd)
{
    struct cmd_list_ctx *list = cmd->on_completion_priv;

    if (list->current_valid && mp_thread_id_equal(list->current_tid, mp_thread_current_id())) {
        list->completed_recursive = true;
    } else {
        continue_cmd_list(list);
    }
}

static void continue_cmd_list(struct cmd_list_ctx *list)
{
    while (list->parent->args[0].v.p) {
        struct mp_cmd *sub = list->parent->args[0].v.p;
        list->parent->args[0].v.p = sub->queue_next;

        ta_set_parent(sub, NULL);

        if (sub->flags & MP_ASYNC_CMD) {
            // We run it "detached" (fire & forget)
            run_command(list->mpctx, sub, NULL, NULL, NULL);
        } else {
            // Run the next command once this one completes.

            list->completed_recursive = false;
            list->current_valid = true;
            list->current_tid = mp_thread_current_id();

            run_command(list->mpctx, sub, NULL, on_cmd_list_sub_completion, list);

            list->current_valid = false;

            // run_command() either recursively calls the completion function,
            // or lets the command continue run in the background. If it was
            // completed recursively, we can just continue our loop. Otherwise
            // the completion handler will invoke this loop again elsewhere.
            // We could unconditionally call continue_cmd_list() in the handler
            // instead, but then stack depth would grow with list length.
            if (!list->completed_recursive)
                return;
        }
    }

    mp_cmd_ctx_complete(list->parent);
    talloc_free(list);
}

static void cmd_list(void *p)
{
    struct mp_cmd_ctx *cmd = p;

    cmd->completed = false;

    struct cmd_list_ctx *list = talloc_zero(NULL, struct cmd_list_ctx);
    list->mpctx = cmd->mpctx;
    list->parent = p;

    continue_cmd_list(list);
}

const struct mp_cmd_def mp_cmd_list = { "list", cmd_list, .exec_async = true };

// Signal that the command is complete now. This also deallocates cmd.
// You must call this function in a state where the core is locked for the
// current thread (e.g. from the main thread, or from within mp_dispatch_lock()).
// Completion means the command is finished, even if it errored or never ran.
// Keep in mind that calling this can execute further user command that can
// change arbitrary state (due to cmd_list).
void mp_cmd_ctx_complete(struct mp_cmd_ctx *cmd)
{
    cmd->completed = true;
    if (!cmd->success)
        mpv_free_node_contents(&cmd->result);
    if (cmd->on_completion)
        cmd->on_completion(cmd);
    if (cmd->abort)
        mp_abort_remove(cmd->mpctx, cmd->abort);
    mpv_free_node_contents(&cmd->result);
    talloc_free(cmd);
}

static void run_command_on_worker_thread(void *p)
{
    struct mp_cmd_ctx *ctx = p;
    struct MPContext *mpctx = ctx->mpctx;

    mp_core_lock(mpctx);

    bool exec_async = ctx->cmd->def->exec_async;
    ctx->cmd->def->handler(ctx);
    if (!exec_async)
        mp_cmd_ctx_complete(ctx);

    mpctx->outstanding_async -= 1;
    if (!mpctx->outstanding_async && mp_is_shutting_down(mpctx))
        mp_wakeup_core(mpctx);

    mp_core_unlock(mpctx);
}

// Run the given command. Upon command completion, on_completion is called. This
// can happen within the function, or for async commands, some time after the
// function returns (the caller is supposed to be able to handle both cases). In
// both cases, the callback will be called while the core is locked (i.e. you
// can access the core freely).
// If abort is non-NULL, then the caller creates the abort object. It must have
// been allocated with talloc. run_command() will register/unregister/destroy
// it. Must not be set if cmd->def->can_abort==false.
// on_completion_priv is copied to mp_cmd_ctx.on_completion_priv and can be
// accessed from the completion callback.
// The completion callback is invoked exactly once. If it's NULL, it's ignored.
// Ownership of cmd goes to the caller.
void run_command(struct MPContext *mpctx, struct mp_cmd *cmd,
                 struct mp_abort_entry *abort,
                 void (*on_completion)(struct mp_cmd_ctx *cmd),
                 void *on_completion_priv)
{
    struct mp_cmd_ctx *ctx = talloc(NULL, struct mp_cmd_ctx);
    *ctx = (struct mp_cmd_ctx){
        .mpctx = mpctx,
        .cmd = talloc_steal(ctx, cmd),
        .args = cmd->args,
        .num_args = cmd->nargs,
        .priv = cmd->def->priv,
        .abort = talloc_steal(ctx, abort),
        .success = true,
        .completed = true,
        .on_completion = on_completion,
        .on_completion_priv = on_completion_priv,
    };

    if (!ctx->abort && cmd->def->can_abort)
        ctx->abort = talloc_zero(ctx, struct mp_abort_entry);

    assert(cmd->def->can_abort == !!ctx->abort);

    if (ctx->abort) {
        ctx->abort->coupled_to_playback |= cmd->def->abort_on_playback_end;
        mp_abort_add(mpctx, ctx->abort);
    }

    struct MPOpts *opts = mpctx->opts;
    ctx->on_osd = cmd->flags & MP_ON_OSD_FLAGS;
    bool auto_osd = ctx->on_osd == MP_ON_OSD_AUTO;
    ctx->msg_osd = auto_osd || (ctx->on_osd & MP_ON_OSD_MSG);
    ctx->bar_osd = auto_osd || (ctx->on_osd & MP_ON_OSD_BAR);
    ctx->seek_msg_osd = auto_osd ? opts->osd_on_seek & 2 : ctx->msg_osd;
    ctx->seek_bar_osd = auto_osd ? opts->osd_on_seek & 1 : ctx->bar_osd;

    bool noise = cmd->def->is_noisy || cmd->mouse_move;
    mp_cmd_dump(mpctx->log, noise ? MSGL_TRACE : MSGL_DEBUG, "Run command:", cmd);

    if (cmd->flags & MP_EXPAND_PROPERTIES) {
        for (int n = 0; n < cmd->nargs; n++) {
            if (cmd->args[n].type->type == CONF_TYPE_STRING) {
                char *s = mp_property_expand_string(mpctx, cmd->args[n].v.s);
                if (!s) {
                    ctx->success = false;
                    mp_cmd_ctx_complete(ctx);
                    return;
                }
                talloc_free(cmd->args[n].v.s);
                cmd->args[n].v.s = s;
            }
        }
    }

    if (cmd->def->spawn_thread) {
        mpctx->outstanding_async += 1; // prevent that core disappears
        if (!mp_thread_pool_queue(mpctx->thread_pool,
                                  run_command_on_worker_thread, ctx))
        {
            mpctx->outstanding_async -= 1;
            ctx->success = false;
            mp_cmd_ctx_complete(ctx);
        }
    } else {
        bool exec_async = cmd->def->exec_async;
        cmd->def->handler(ctx);
        if (!exec_async)
            mp_cmd_ctx_complete(ctx);
    }
}

// When a command shows a message. status is the level (e.g. MSGL_INFO), and
// msg+vararg is as in printf (don't include a trailing "\n").
void mp_cmd_msg(struct mp_cmd_ctx *cmd, int status, const char *msg, ...)
{
    va_list ap;
    char *s;

    va_start(ap, msg);
    s = talloc_vasprintf(NULL, msg, ap);
    va_end(ap);

    MP_MSG(cmd->mpctx, status, "%s\n", s);
    if (cmd->msg_osd && status <= MSGL_INFO)
        set_osd_msg(cmd->mpctx, 1, cmd->mpctx->opts->osd_duration, "%s", s);

    talloc_free(s);
}

static void cmd_seek(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    double v = cmd->args[0].v.d * cmd->cmd->scale;
    int abs = cmd->args[1].v.i & 3;
    enum seek_precision precision = MPSEEK_DEFAULT;
    switch (((cmd->args[2].v.i | cmd->args[1].v.i) >> 3) & 3) {
    case 1: precision = MPSEEK_KEYFRAME; break;
    case 2: precision = MPSEEK_EXACT; break;
    }
    if (!mpctx->playback_initialized) {
        cmd->success = false;
        return;
    }

    mark_seek(mpctx);
    switch (abs) {
    case 0: { // Relative seek
        queue_seek(mpctx, MPSEEK_RELATIVE, v, precision, MPSEEK_FLAG_DELAY);
        set_osd_function(mpctx, (v > 0) ? OSD_FFW : OSD_REW);
        break;
    }
    case 1: { // Absolute seek by percentage
        double ratio = v / 100.0;
        double cur_pos = get_current_pos_ratio(mpctx, false);
        queue_seek(mpctx, MPSEEK_FACTOR, ratio, precision, MPSEEK_FLAG_DELAY);
        set_osd_function(mpctx, cur_pos < ratio ? OSD_FFW : OSD_REW);
        break;
    }
    case 2: { // Absolute seek to a timestamp in seconds
        if (v < 0) {
            // Seek from end
            double len = get_time_length(mpctx);
            if (len < 0) {
                cmd->success = false;
                return;
            }
            v = MPMAX(0, len + v);
        }
        queue_seek(mpctx, MPSEEK_ABSOLUTE, v, precision, MPSEEK_FLAG_DELAY);
        set_osd_function(mpctx,
                         v > get_current_time(mpctx) ? OSD_FFW : OSD_REW);
        break;
    }
    case 3: { // Relative seek by percentage
        queue_seek(mpctx, MPSEEK_FACTOR,
                   get_current_pos_ratio(mpctx, false) + v / 100.0,
                   precision, MPSEEK_FLAG_DELAY);
        set_osd_function(mpctx, v > 0 ? OSD_FFW : OSD_REW);
        break;
    }}
    if (cmd->seek_bar_osd)
        mpctx->add_osd_seek_info |= OSD_SEEK_INFO_BAR;
    if (cmd->seek_msg_osd)
        mpctx->add_osd_seek_info |= OSD_SEEK_INFO_TEXT;
}

static void cmd_revert_seek(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    struct command_ctx *cmdctx = mpctx->command_ctx;

    if (!mpctx->playback_initialized) {
        cmd->success = false;
        return;
    }

    double oldpts = cmdctx->last_seek_pts;
    if (cmdctx->marked_pts != MP_NOPTS_VALUE)
        oldpts = cmdctx->marked_pts;
    if (cmd->args[0].v.i & 3) {
        cmdctx->marked_pts = get_current_time(mpctx);
        cmdctx->marked_permanent = cmd->args[0].v.i & 1;
    } else if (oldpts != MP_NOPTS_VALUE) {
        if (!cmdctx->marked_permanent) {
            cmdctx->marked_pts = MP_NOPTS_VALUE;
            cmdctx->last_seek_pts = get_current_time(mpctx);
        }
        queue_seek(mpctx, MPSEEK_ABSOLUTE, oldpts, MPSEEK_EXACT,
                   MPSEEK_FLAG_DELAY);
        set_osd_function(mpctx, OSD_REW);
        if (cmd->seek_bar_osd)
            mpctx->add_osd_seek_info |= OSD_SEEK_INFO_BAR;
        if (cmd->seek_msg_osd)
            mpctx->add_osd_seek_info |= OSD_SEEK_INFO_TEXT;
    } else {
        cmd->success = false;
    }
}

static void cmd_set(void *p)
{
    struct mp_cmd_ctx *cmd = p;

    change_property_cmd(cmd, cmd->args[0].v.s,
                        M_PROPERTY_SET_STRING, cmd->args[1].v.s);
}

static void cmd_del(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    const char *name = cmd->args[0].v.s;
    int osdl = cmd->msg_osd ? 1 : OSD_LEVEL_INVISIBLE;
    int osd_duration = mpctx->opts->osd_duration;

    int r = mp_property_do(name, M_PROPERTY_DELETE, NULL, mpctx);

    if (r == M_PROPERTY_OK) {
        set_osd_msg(mpctx, osdl, osd_duration, "Deleted property: '%s'", name);
        cmd->success = true;
    } else if (r == M_PROPERTY_UNKNOWN) {
        set_osd_msg(mpctx, osdl, osd_duration, "Unknown property: '%s'", name);
        cmd->success = false;
    } else if (r <= 0) {
        set_osd_msg(mpctx, osdl, osd_duration, "Failed to set property '%s'",
                    name);
        cmd->success = false;
    }
}

static void cmd_change_list(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    char *name = cmd->args[0].v.s;
    char *op = cmd->args[1].v.s;
    char *value = cmd->args[2].v.s;
    int osd_duration = mpctx->opts->osd_duration;
    int osdl = cmd->msg_osd ? 1 : OSD_LEVEL_INVISIBLE;

    struct m_option prop = {0};
    if (mp_property_do(name, M_PROPERTY_GET_TYPE, &prop, mpctx) <= 0) {
        set_osd_msg(mpctx, osdl, osd_duration, "Unknown option: '%s'", name);
        cmd->success = false;
        return;
    }

    const struct m_option_type *type = prop.type;
    bool found = false;
    for (int i = 0; type->actions && type->actions[i].name; i++) {
        const struct m_option_action *action = &type->actions[i];
        if (strcmp(action->name, op) == 0)
            found = true;
    }
    if (!found) {
        set_osd_msg(mpctx, osdl, osd_duration, "Unknown action: '%s'", op);
        cmd->success = false;
        return;
    }

    union m_option_value val = m_option_value_default;
    if (mp_property_do(name, M_PROPERTY_GET, &val, mpctx) <= 0) {
        set_osd_msg(mpctx, osdl, osd_duration, "Could not read: '%s'", name);
        cmd->success = false;
        return;
    }

    char *optname = mp_tprintf(80, "%s-%s", name, op); // the dirty truth
    int r = m_option_parse(mpctx->log, &prop, bstr0(optname), bstr0(value), &val);
    if (r >= 0 && mp_property_do(name, M_PROPERTY_SET, &val, mpctx) <= 0)
        r = -1;
    m_option_free(&prop, &val);
    if (r < 0) {
        set_osd_msg(mpctx, osdl, osd_duration,
                    "Failed setting option: '%s'", name);
        cmd->success = false;
        return;
    }

    show_property_osd(mpctx, name, cmd->on_osd);
}

static void cmd_add_cycle(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    bool is_cycle = !!cmd->priv;

    char *property = cmd->args[0].v.s;
    if (cmd->cmd->repeated && !check_property_autorepeat(property, mpctx) &&
        !(cmd->cmd->flags & MP_ALLOW_REPEAT)  /* "repeatable" prefix */ )
    {
        MP_VERBOSE(mpctx, "Dropping command '%s' from auto-repeated key.\n",
                   cmd->cmd->original);
        return;
    }

    double scale = 1;
    int scale_units = cmd->cmd->scale_units;
    if (check_property_scalable(property, mpctx)) {
        scale = cmd->cmd->scale;
        scale_units = 1;
    }

    for (int i = 0; i < scale_units; i++) {
        struct m_property_switch_arg s = {
            .inc = cmd->args[1].v.d * scale,
            .wrap = is_cycle,
        };
        change_property_cmd(cmd, property, M_PROPERTY_SWITCH, &s);
        if (!cmd->success)
            return;
    }
}

static void cmd_multiply(void *p)
{
    struct mp_cmd_ctx *cmd = p;

    change_property_cmd(cmd, cmd->args[0].v.s,
                        M_PROPERTY_MULTIPLY, &cmd->args[1].v.d);
}

static void cmd_frame_step(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    bool backstep = *(bool *)cmd->priv;
    int frames = backstep ? -1 : cmd->args[0].v.i;
    int flags = backstep ? 1 : cmd->args[1].v.i;

    if (!mpctx->playback_initialized || frames == 0) {
        cmd->success = false;
        return;
    }

    if (flags) {
        if (!cmd->cmd->is_up)
            add_step_frame(mpctx, frames, flags);
    } else {
        if (cmd->cmd->is_up) {
            if (mpctx->step_frames < 1)
                set_pause_state(mpctx, true);
        } else {
            if (cmd->cmd->repeated) {
                set_pause_state(mpctx, false);
            } else {
                add_step_frame(mpctx, frames, flags);
            }
        }
    }
}

static void cmd_quit(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    bool write_watch_later = *(bool *)cmd->priv;
    if (write_watch_later || mpctx->opts->position_save_on_quit)
        mp_write_watch_later_conf(mpctx);
    mpctx->stop_play = PT_QUIT;
    mpctx->quit_custom_rc = cmd->args[0].v.i;
    mpctx->has_quit_custom_rc = true;
    mp_wakeup_core(mpctx);
}

static void cmd_playlist_next_prev(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    int dir = *(int *)cmd->priv;
    int force = cmd->args[0].v.i;

    struct playlist_entry *e = mp_next_file(mpctx, dir, force, true);
    if (!e && !force) {
        cmd->success = false;
        return;
    }

    mp_set_playlist_entry(mpctx, e);
    if (cmd->on_osd & MP_ON_OSD_MSG)
        mpctx->add_osd_seek_info |= OSD_SEEK_INFO_CURRENT_FILE;
}

static void cmd_playlist_next_prev_playlist(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    int direction = *(int *)cmd->priv;

    struct playlist_entry *entry =
        playlist_get_first_in_next_playlist(mpctx->playlist, direction);

    if (!entry && mpctx->opts->loop_times != 1 && mpctx->playlist->current) {
        entry = direction > 0 ? playlist_get_first(mpctx->playlist)
                              : playlist_get_last(mpctx->playlist);

        if (entry && entry->playlist_path &&
            mpctx->playlist->current->playlist_path &&
            strcmp(entry->playlist_path,
                   mpctx->playlist->current->playlist_path) == 0)
            entry = NULL;

        if (direction > 0 && entry && mpctx->opts->loop_times > 1) {
            mpctx->opts->loop_times--;
            m_config_notify_change_opt_ptr(mpctx->mconfig,
                                           &mpctx->opts->loop_times);
        }

        if (direction < 0)
            entry = playlist_get_first_in_same_playlist(
                        entry, mpctx->playlist->current->playlist_path);
    }

    if (!entry) {
        cmd->success = false;
        return;
    }

    mp_set_playlist_entry(mpctx, entry);
    if (cmd->on_osd & MP_ON_OSD_MSG)
        mpctx->add_osd_seek_info |= OSD_SEEK_INFO_CURRENT_FILE;
}

static void cmd_playlist_play_index(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    struct playlist *pl = mpctx->playlist;
    int pos = cmd->args[0].v.i;

    if (pos == -2)
        pos = playlist_entry_to_index(pl, pl->current);

    mp_set_playlist_entry(mpctx, playlist_entry_from_index(pl, pos));
    if (cmd->on_osd & MP_ON_OSD_MSG)
        mpctx->add_osd_seek_info |= OSD_SEEK_INFO_CURRENT_FILE;
}

static void cmd_sub_step_seek(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    bool step = *(bool *)cmd->priv;
    int track_ind = cmd->args[1].v.i;

    if (!mpctx->playback_initialized) {
        cmd->success = false;
        return;
    }

    struct track *track = mpctx->current_track[track_ind][STREAM_SUB];
    struct dec_sub *sub = track ? track->d_sub : NULL;
    double refpts = get_current_time(mpctx);
    if (sub && refpts != MP_NOPTS_VALUE) {
        double a[2];
        a[0] = refpts;
        a[1] = cmd->args[0].v.i;
        if (sub_control(sub, SD_CTRL_SUB_STEP, a) > 0) {
            if (step) {
                mpctx->opts->subs_shared->sub_delay[track_ind] -= a[0] - refpts;
                m_config_notify_change_opt_ptr_notify(mpctx->mconfig,
                                &mpctx->opts->subs_shared->sub_delay[track_ind]);
                show_property_osd(
                    mpctx,
                    track_ind == 0 ? "sub-delay" : "secondary-sub-delay",
                    cmd->on_osd);
            } else {
                // We can easily seek/step to the wrong subtitle line (because
                // video frame PTS and sub PTS rarely match exactly).
                // sub/sd_ass.c adds SUB_SEEK_OFFSET as a workaround, and we
                // need an even bigger offset without a video.
                if (!mpctx->current_track[0][STREAM_VIDEO] ||
                    mpctx->current_track[0][STREAM_VIDEO]->image) {
                    a[0] += SUB_SEEK_WITHOUT_VIDEO_OFFSET - SUB_SEEK_OFFSET;
                }
                mark_seek(mpctx);
                queue_seek(mpctx, MPSEEK_ABSOLUTE, a[0], MPSEEK_EXACT,
                           MPSEEK_FLAG_DELAY);
                set_osd_function(mpctx, (a[0] > refpts) ? OSD_FFW : OSD_REW);
                if (cmd->seek_bar_osd)
                    mpctx->add_osd_seek_info |= OSD_SEEK_INFO_BAR;
                if (cmd->seek_msg_osd)
                    mpctx->add_osd_seek_info |= OSD_SEEK_INFO_TEXT;
            }
        }
    }
}

static void cmd_print_text(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    MP_INFO(mpctx, "%s\n", cmd->args[0].v.s);
}

static void cmd_show_text(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    int osd_duration = mpctx->opts->osd_duration;

    // if no argument supplied use default osd_duration, else <arg> ms.
    set_osd_msg(mpctx, cmd->args[2].v.i,
                (cmd->args[1].v.i < 0 ? osd_duration : cmd->args[1].v.i),
                "%s", cmd->args[0].v.s);
}

static void cmd_expand_text(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    cmd->result = (mpv_node){
        .format = MPV_FORMAT_STRING,
        .u.string = mp_property_expand_string(mpctx, cmd->args[0].v.s)
    };
}

static void cmd_expand_path(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    cmd->result = (mpv_node){
        .format = MPV_FORMAT_STRING,
        .u.string = mp_get_user_path(NULL, mpctx->global, cmd->args[0].v.s)
    };
}

static void cmd_normalize_path(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    char *path = mp_get_user_path(NULL, mpctx->global, cmd->args[0].v.s);
    cmd->result = (mpv_node){
        .format = MPV_FORMAT_STRING,
        .u.string = mp_normalize_path(NULL, path),
    };

    talloc_free(path);
}

static void cmd_escape_ass(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    bstr dst = {0};

    osd_mangle_ass(&dst, cmd->args[0].v.s, true);

    cmd->result = (mpv_node){
        .format = MPV_FORMAT_STRING,
        .u.string = dst.len ? (char *)dst.start : talloc_strdup(NULL, ""),
    };
}

static struct load_action get_load_action(struct MPContext *mpctx, int action_flag)
{
    switch (action_flag) {
    case 0: // replace
        return (struct load_action){LOAD_TYPE_REPLACE, .play = true};
    case 1: // append
        return (struct load_action){LOAD_TYPE_APPEND, .play = false};
    case 2: // append-play
        return (struct load_action){LOAD_TYPE_APPEND, .play = true};
    case 3: // insert-next
        return (struct load_action){LOAD_TYPE_INSERT_NEXT, .play = false};
    case 4: // insert-next-play
        return (struct load_action){LOAD_TYPE_INSERT_NEXT, .play = true};
    case 5: // insert-at
        return (struct load_action){LOAD_TYPE_INSERT_AT, .play = false};
    case 6: // insert-at-play
        return (struct load_action){LOAD_TYPE_INSERT_AT, .play = true};
    default: // default: replace
        return (struct load_action){LOAD_TYPE_REPLACE, .play = true};
    }
}

static struct playlist_entry *get_insert_entry(struct MPContext *mpctx, struct load_action *action,
                                               int insert_at_idx)
{
    switch (action->type) {
    case LOAD_TYPE_INSERT_NEXT:
        return playlist_get_next(mpctx->playlist, +1);
    case LOAD_TYPE_INSERT_AT:
        return playlist_entry_from_index(mpctx->playlist, insert_at_idx);
    case LOAD_TYPE_REPLACE:
    case LOAD_TYPE_APPEND:
    default:
        return NULL;
    }
}

static void cmd_loadfile(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    char *filename = cmd->args[0].v.s;
    int action_flag = cmd->args[1].v.i;
    int insert_at_idx = cmd->args[2].v.i;

    struct load_action action = get_load_action(mpctx, action_flag);

    if (action.type == LOAD_TYPE_REPLACE)
        playlist_clear(mpctx->playlist);

    char *path = mp_get_user_path(NULL, mpctx->global, filename);
    struct playlist_entry *entry = playlist_entry_new(path);
    talloc_free(path);
    if (cmd->args[3].v.str_list) {
        char **pairs = cmd->args[3].v.str_list;
        for (int i = 0; pairs[i] && pairs[i + 1]; i += 2)
            playlist_entry_add_param(entry, bstr0(pairs[i]), bstr0(pairs[i + 1]));
    }

    struct playlist_entry *at = get_insert_entry(mpctx, &action, insert_at_idx);
    playlist_insert_at(mpctx->playlist, entry, at);

    struct mpv_node *res = &cmd->result;
    node_init(res, MPV_FORMAT_NODE_MAP, NULL);
    node_map_add_int64(res, "playlist_entry_id", entry->id);

    if (action.type == LOAD_TYPE_REPLACE || (action.play && !mpctx->playlist->current)) {
        if (mpctx->opts->position_save_on_quit) // requested in issue #1148
            mp_write_watch_later_conf(mpctx);
        mp_set_playlist_entry(mpctx, entry);
    }
    mp_notify(mpctx, MP_EVENT_CHANGE_PLAYLIST, NULL);
    mp_wakeup_core(mpctx);
}

static void cmd_loadlist(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    char *filename = cmd->args[0].v.s;
    int action_flag = cmd->args[1].v.i;
    int insert_at_idx = cmd->args[2].v.i;

    struct load_action action = get_load_action(mpctx, action_flag);

    char *path = mp_get_user_path(NULL, mpctx->global, filename);
    struct playlist *pl = playlist_parse_file(path, cmd->abort->cancel,
                                              mpctx->global);
    talloc_free(path);

    if (pl) {
        prepare_playlist(mpctx, pl);
        struct playlist_entry *new = pl->current;
        if (action.type == LOAD_TYPE_REPLACE)
            playlist_clear(mpctx->playlist);
        struct playlist_entry *first = playlist_entry_from_index(pl, 0);
        int num_entries = pl->num_entries;

        struct playlist_entry *at = get_insert_entry(mpctx, &action, insert_at_idx);
        if (at == NULL) {
            playlist_append_entries(mpctx->playlist, pl);
        } else {
            int at_index = playlist_entry_to_index(mpctx->playlist, at);
            playlist_transfer_entries_to(mpctx->playlist, at_index, pl);
        }
        talloc_free(pl);

        if (!new)
            new = playlist_get_first(mpctx->playlist);

        if ((action.type == LOAD_TYPE_REPLACE ||
            (action.play && !mpctx->playlist->current)) && new) {
            mp_set_playlist_entry(mpctx, new);
        }

        struct mpv_node *res = &cmd->result;
        node_init(res, MPV_FORMAT_NODE_MAP, NULL);
        if (num_entries) {
            node_map_add_int64(res, "playlist_entry_id", first->id);
            node_map_add_int64(res, "num_entries", num_entries);
        }

        mp_notify(mpctx, MP_EVENT_CHANGE_PLAYLIST, NULL);
        mp_wakeup_core(mpctx);
    } else {
        MP_ERR(mpctx, "Unable to load playlist %s.\n", filename);
        cmd->success = false;
    }
}

static void cmd_playlist_clear(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    // Supposed to clear the playlist, except the currently played item.
    if (mpctx->playlist->current_was_replaced)
        mpctx->playlist->current = NULL;
    playlist_clear_except_current(mpctx->playlist);
    mp_notify(mpctx, MP_EVENT_CHANGE_PLAYLIST, NULL);
    mp_wakeup_core(mpctx);
}

static void cmd_playlist_remove(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    struct playlist_entry *e = playlist_entry_from_index(mpctx->playlist,
                                                         cmd->args[0].v.i);
    if (cmd->args[0].v.i < 0)
        e = mpctx->playlist->current;
    if (!e) {
        cmd->success = false;
        return;
    }

    // Can't play a removed entry
    if (mpctx->playlist->current == e && !mpctx->stop_play)
        mpctx->stop_play = PT_NEXT_ENTRY;
    playlist_remove(mpctx->playlist, e);
    mp_notify(mpctx, MP_EVENT_CHANGE_PLAYLIST, NULL);
    mp_wakeup_core(mpctx);
}

static void cmd_playlist_move(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    struct playlist_entry *e1 = playlist_entry_from_index(mpctx->playlist,
                                                          cmd->args[0].v.i);
    struct playlist_entry *e2 = playlist_entry_from_index(mpctx->playlist,
                                                          cmd->args[1].v.i);
    if (!e1) {
        cmd->success = false;
        return;
    }

    playlist_move(mpctx->playlist, e1, e2);
    mp_notify(mpctx, MP_EVENT_CHANGE_PLAYLIST, NULL);
}

static void cmd_playlist_shuffle(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    playlist_shuffle(mpctx->playlist);
    mp_notify(mpctx, MP_EVENT_CHANGE_PLAYLIST, NULL);
}

static void cmd_playlist_unshuffle(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    playlist_unshuffle(mpctx->playlist);
    mp_notify(mpctx, MP_EVENT_CHANGE_PLAYLIST, NULL);
}

static void cmd_stop(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    int flags = cmd->args[0].v.i;

    if (!(flags & 1))
        playlist_clear(mpctx->playlist);

    if (mpctx->opts->player_idle_mode < 2 &&
        mpctx->opts->position_save_on_quit)
    {
        mp_write_watch_later_conf(mpctx);
    }

    if (mpctx->stop_play != PT_QUIT)
        mpctx->stop_play = PT_STOP;
    mp_wakeup_core(mpctx);
}

static void cmd_show_progress(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    mpctx->add_osd_seek_info |=
            (cmd->msg_osd ? OSD_SEEK_INFO_TEXT : 0) |
            (cmd->bar_osd ? OSD_SEEK_INFO_BAR : 0);

    // If we got neither (i.e. no-osd) force both like osd-auto.
    if (!mpctx->add_osd_seek_info)
        mpctx->add_osd_seek_info |= OSD_SEEK_INFO_TEXT | OSD_SEEK_INFO_BAR;
    mpctx->osd_force_update = true;
    mp_wakeup_core(mpctx);
}

static void cmd_track_add(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    int type = *(int *)cmd->priv;
    bool is_albumart = type == STREAM_VIDEO &&
                       cmd->args[4].v.b;

    if (mpctx->stop_play) {
        cmd->success = false;
        return;
    }

    if (cmd->args[1].v.i == 2) {
        struct track *t = find_track_with_url(mpctx, type, cmd->args[0].v.s);
        if (t) {
            if (mpctx->playback_initialized) {
                mp_switch_track(mpctx, t->type, t, FLAG_MARK_SELECTION);
                print_track_list(mpctx, "Track switched:");
            } else {
                mark_track_selection(mpctx, 0, t->type, t->user_tid);
            }
            return;
        }
    }
    int first = mp_add_external_file(mpctx, cmd->args[0].v.s, type,
                                     cmd->abort->cancel, is_albumart);
    if (first < 0) {
        cmd->success = false;
        return;
    }

    for (int n = first; n < mpctx->num_tracks; n++) {
        struct track *t = mpctx->tracks[n];
        if (cmd->args[1].v.i != 1 && n == first) {
            if (mpctx->playback_initialized) {
                mp_switch_track(mpctx, t->type, t, FLAG_MARK_SELECTION);
            } else {
                mark_track_selection(mpctx, 0, t->type, t->user_tid);
            }
        }
        char *title = cmd->args[2].v.s;
        if (title && title[0])
            t->title = talloc_strdup(t, title);
        char *lang = cmd->args[3].v.s;
        if (lang && lang[0])
            t->lang = talloc_strdup(t, lang);
    }

    if (mpctx->playback_initialized)
        print_track_list(mpctx, "Track added:");
}

static void cmd_track_remove(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    int type = *(int *)cmd->priv;

    struct track *t = mp_track_by_tid(mpctx, type, cmd->args[0].v.i);
    if (!t) {
        cmd->success = false;
        return;
    }

    mp_remove_track(mpctx, t);
    if (mpctx->playback_initialized)
        print_track_list(mpctx, "Track removed:");
}

static void cmd_track_reload(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    int type = *(int *)cmd->priv;

    if (!mpctx->playback_initialized) {
        MP_ERR(mpctx, "Cannot reload while not initialized.\n");
        cmd->success = false;
        return;
    }

    struct track *t = mp_track_by_tid(mpctx, type, cmd->args[0].v.i);
    int nt_num = -1;

    if (t && t->is_external && t->external_filename) {
        char *filename = talloc_strdup(NULL, t->external_filename);
        bool is_albumart = t->attached_picture;
        mp_remove_track(mpctx, t);
        nt_num = mp_add_external_file(mpctx, filename, type, cmd->abort->cancel,
                                      is_albumart);
        talloc_free(filename);
    }

    if (nt_num < 0) {
        cmd->success = false;
        return;
    }

    struct track *nt = mpctx->tracks[nt_num];

    if (!nt->lang)
        nt->lang = bstrto0(nt, mp_guess_lang_from_filename(bstr0(nt->external_filename), NULL));

    mp_switch_track(mpctx, nt->type, nt, 0);
    print_track_list(mpctx, "Reloaded:");
}

static void cmd_rescan_external_files(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    if (mpctx->stop_play) {
        cmd->success = false;
        return;
    }

    autoload_external_files(mpctx, cmd->abort->cancel);
    if (!cmd->args[0].v.i && mpctx->playback_initialized) {
        // somewhat fuzzy and not ideal
        struct track *a = select_default_track(mpctx, 0, STREAM_AUDIO);
        if (a && a->is_external)
            mp_switch_track(mpctx, STREAM_AUDIO, a, 0);
        struct track *s = select_default_track(mpctx, 0, STREAM_SUB);
        if (s && s->is_external)
            mp_switch_track(mpctx, STREAM_SUB, s, 0);

        print_track_list(mpctx, "Track list:");
    }
}

static void cmd_run(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    char **args = talloc_zero_array(NULL, char *, cmd->num_args + 1);
    for (int n = 0; n < cmd->num_args; n++)
        args[n] = cmd->args[n].v.s;
    mp_msg_flush_status_line(mpctx->log, true);
    struct mp_subprocess_opts opts = {
        .exe = args[0],
        .args = args,
        .fds = { {0, .src_fd = 0}, {1, .src_fd = 1}, {2, .src_fd = 2} },
        .num_fds = 3,
        .detach = true,
    };
    struct mp_subprocess_result res;
    mp_subprocess(mpctx->log, &opts, &res);
    talloc_free(args);
}

struct subprocess_fd_ctx {
    struct mp_log *log;
    void* talloc_ctx;
    int64_t max_size;
    int msgl;
    bool capture;
    bstr output;
};

static void subprocess_read(void *p, char *data, size_t size)
{
    struct subprocess_fd_ctx *ctx = p;
    if (ctx->capture) {
        if (ctx->output.len < ctx->max_size)
            bstr_xappend(ctx->talloc_ctx, &ctx->output, (bstr){data, size});
    } else {
        mp_msg(ctx->log, ctx->msgl, "%.*s", (int)size, data);
    }
}

static void subprocess_write(void *p)
{
    // Unused; we write a full buffer.
}

static void cmd_subprocess(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    char **args = cmd->args[0].v.str_list;
    bool playback_only = cmd->args[1].v.b;
    bool detach = cmd->args[5].v.b;
    char **env = cmd->args[6].v.str_list;
    bstr stdin_data = bstr0(cmd->args[7].v.s);
    bool passthrough_stdin = cmd->args[8].v.b;

    if (env && !env[0])
        env = NULL; // do not actually set an empty environment

    if (!args || !args[0]) {
        MP_ERR(mpctx, "program name missing\n");
        cmd->success = false;
        return;
    }

    if (stdin_data.len && passthrough_stdin) {
        MP_ERR(mpctx, "both stdin_data and passthrough_stdin set\n");
        cmd->success = false;
        return;
    }

    void *tmp = talloc_new(NULL);

    struct mp_log *fdlog = mp_log_new(tmp, mpctx->log, cmd->cmd->sender);
    struct subprocess_fd_ctx fdctx[3];
    for (int fd = 0; fd < 3; fd++) {
        fdctx[fd] = (struct subprocess_fd_ctx) {
            .log = fdlog,
            .talloc_ctx = tmp,
            .max_size = cmd->args[2].v.i,
            .msgl = fd == 2 ? MSGL_ERR : MSGL_INFO,
        };
    }
    fdctx[1].capture = cmd->args[3].v.b;
    fdctx[2].capture = cmd->args[4].v.b;

    mp_mutex_lock(&mpctx->abort_lock);
    cmd->abort->coupled_to_playback = playback_only;
    mp_abort_recheck_locked(mpctx, cmd->abort);
    mp_mutex_unlock(&mpctx->abort_lock);

    mp_core_unlock(mpctx);

    struct mp_subprocess_opts opts = {
        .exe = args[0],
        .args = args,
        .env = env,
        .cancel = cmd->abort->cancel,
        .detach = detach,
        .fds = {
            {
                .fd = 0, // stdin
                .src_fd = passthrough_stdin ? 0 : -1,
            },
        },
        .num_fds = 1,
    };

    // stdout, stderr
    for (int fd = 1; fd < 3; fd++) {
        bool capture = fdctx[fd].capture || !detach;
        opts.fds[opts.num_fds++] = (struct mp_subprocess_fd){
            .fd = fd,
            .src_fd = capture ? -1 : fd,
            .on_read = capture ? subprocess_read : NULL,
            .on_read_ctx = &fdctx[fd],
        };
    }
    // stdin
    if (stdin_data.len) {
        opts.fds[0] = (struct mp_subprocess_fd){
            .fd = 0,
            .src_fd = -1,
            .on_write = subprocess_write,
            .on_write_ctx = &fdctx[0],
            .write_buf = &stdin_data,
        };
    }

    struct mp_subprocess_result sres;
    mp_subprocess(fdlog, &opts, &sres);
    int status = sres.exit_status;
    char *error = NULL;
    if (sres.error < 0) {
        error = (char *)mp_subprocess_err_str(sres.error);
        status = sres.error;
    }

    mp_core_lock(mpctx);

    struct mpv_node *res = &cmd->result;
    node_init(res, MPV_FORMAT_NODE_MAP, NULL);
    node_map_add_int64(res, "status", status);
    node_map_add_flag(res, "killed_by_us", status == MP_SUBPROCESS_EKILLED_BY_US);
    node_map_add_string(res, "error_string", error ? error : "");
    const char *sname[] = {NULL, "stdout", "stderr"};
    for (int fd = 1; fd < 3; fd++) {
        if (!fdctx[fd].capture)
            continue;
        struct mpv_byte_array *ba =
            node_map_add(res, sname[fd], MPV_FORMAT_BYTE_ARRAY)->u.ba;
        *ba = (struct mpv_byte_array){
            .data = talloc_steal(ba, fdctx[fd].output.start),
            .size = fdctx[fd].output.len,
        };
    }

    talloc_free(tmp);
}

static void cmd_enable_input_section(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    mp_input_enable_section(mpctx->input, cmd->args[0].v.s, cmd->args[1].v.i);
}

static void cmd_disable_input_section(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    mp_input_disable_section(mpctx->input, cmd->args[0].v.s);
}

static void cmd_define_input_section(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    mp_input_define_section(mpctx->input, cmd->args[0].v.s, "<api>",
                            cmd->args[1].v.s, !cmd->args[2].v.i,
                            cmd->cmd->sender);
}

static void cmd_ab_loop(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    int osd_duration = mpctx->opts->osd_duration;
    int osdl = cmd->msg_osd ? 1 : OSD_LEVEL_INVISIBLE;

    double now = get_current_time(mpctx);
    if (mpctx->opts->ab_loop[0] == MP_NOPTS_VALUE) {
        mp_property_do("ab-loop-a", M_PROPERTY_SET, &now, mpctx);
        show_property_osd(mpctx, "ab-loop-a", cmd->on_osd);
    } else if (mpctx->opts->ab_loop[1] == MP_NOPTS_VALUE) {
        mp_property_do("ab-loop-b", M_PROPERTY_SET, &now, mpctx);
        show_property_osd(mpctx, "ab-loop-b", cmd->on_osd);
    } else {
        now = MP_NOPTS_VALUE;
        mp_property_do("ab-loop-a", M_PROPERTY_SET, &now, mpctx);
        mp_property_do("ab-loop-b", M_PROPERTY_SET, &now, mpctx);
        set_osd_msg(mpctx, osdl, osd_duration, "Clear A-B loop");
    }
}

static void cmd_align_cache_ab(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    if (!mpctx->demuxer)
        return;

    double a = demux_probe_cache_dump_target(mpctx->demuxer,
                                             mpctx->opts->ab_loop[0], false);
    double b = demux_probe_cache_dump_target(mpctx->demuxer,
                                             mpctx->opts->ab_loop[1], true);

    mp_property_do("ab-loop-a", M_PROPERTY_SET, &a, mpctx);
    mp_property_do("ab-loop-b", M_PROPERTY_SET, &b, mpctx);

    // Happens to cover both properties.
    show_property_osd(mpctx, "ab-loop-b", cmd->on_osd);
}

static void cmd_drop_buffers(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    reset_playback_state(mpctx);

    if (mpctx->demuxer)
        demux_flush(mpctx->demuxer);
}

static void cmd_ao_reload(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    reload_audio_output(mpctx);
}

static void cmd_filter(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    int type = *(int *)cmd->priv;
    cmd->success = edit_filters_osd(mpctx, type, cmd->args[0].v.s,
                                    cmd->args[1].v.s, cmd->msg_osd) >= 0;
}

static void cmd_filter_command(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    int type = *(int *)cmd->priv;

    struct mp_output_chain *chain = NULL;
    if (type == STREAM_VIDEO)
        chain = mpctx->vo_chain ? mpctx->vo_chain->filter : NULL;
    if (type == STREAM_AUDIO)
        chain = mpctx->ao_chain ? mpctx->ao_chain->filter : NULL;
    if (!chain) {
        cmd->success = false;
        return;
    }
    struct mp_filter_command filter_cmd = {
        .type = MP_FILTER_COMMAND_TEXT,
        .target = cmd->args[3].v.s,
        .cmd = cmd->args[1].v.s,
        .arg = cmd->args[2].v.s,
    };
    cmd->success = mp_output_chain_command(chain, cmd->args[0].v.s, &filter_cmd);
}

static void cmd_script_binding(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct mp_cmd *incmd = cmd->cmd;
    struct MPContext *mpctx = cmd->mpctx;

    mpv_event_client_message event = {0};
    char *name = cmd->args[0].v.s;
    if (!name || !name[0]) {
        cmd->success = false;
        return;
    }

    char *sep = strchr(name, '/');
    char *target = NULL;
    char space[MAX_CLIENT_NAME];
    if (sep) {
        snprintf(space, sizeof(space), "%.*s", (int)(sep - name), name);
        target = space;
        name = sep + 1;
    }
    char state[4] = {'p', incmd->is_mouse_button ? 'm' : '-',
                          incmd->canceled ? 'c' : '-'};
    if (incmd->is_up_down)
        state[0] = incmd->repeated ? 'r' : (incmd->is_up ? 'u' : 'd');

    double scale = 1;
    int scale_units = incmd->scale_units;
    if (mp_input_is_scalable_cmd(incmd)) {
        scale = incmd->scale;
        scale_units = 1;
    }
    char *scale_s = mp_format_double(NULL, scale, 6, false, false, false);

    for (int i = 0; i < scale_units; i++) {
        event.num_args = 7;
        event.args = (const char*[7]){"key-binding", name, state,
                                      incmd->key_name ? incmd->key_name : "",
                                      incmd->key_text ? incmd->key_text : "",
                                      scale_s, cmd->args[1].v.s};
        if (mp_client_send_event_dup(mpctx, target,
                                     MPV_EVENT_CLIENT_MESSAGE, &event) < 0)
        {
            MP_VERBOSE(mpctx, "Can't find script '%s' when handling input.\n",
                        target ? target : "-");
            cmd->success = false;
            break;
        }
    }
    talloc_free(scale_s);
}

static void cmd_script_message_to(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    mpv_event_client_message *event = talloc_ptrtype(NULL, event);
    *event = (mpv_event_client_message){0};
    for (int n = 1; n < cmd->num_args; n++) {
        MP_TARRAY_APPEND(event, event->args, event->num_args,
                         talloc_strdup(event, cmd->args[n].v.s));
    }
    if (mp_client_send_event(mpctx, cmd->args[0].v.s, 0,
                                MPV_EVENT_CLIENT_MESSAGE, event) < 0)
    {
        MP_VERBOSE(mpctx, "Can't find script '%s' to send message to.\n",
                   cmd->args[0].v.s);
        cmd->success = false;
    }
}

static void cmd_script_message(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    const char **args = talloc_array(NULL, const char *, cmd->num_args);
    mpv_event_client_message event = {.args = args};
    for (int n = 0; n < cmd->num_args; n++)
        event.args[event.num_args++] = cmd->args[n].v.s;
    mp_client_broadcast_event(mpctx, MPV_EVENT_CLIENT_MESSAGE, &event);
    talloc_free(args);
}

static void cmd_ignore(void *p)
{
}

static void cmd_write_watch_later_config(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    mp_write_watch_later_conf(mpctx);
}

static void cmd_delete_watch_later_config(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    char *filename = cmd->args[0].v.s;
    if (filename && !filename[0])
        filename = NULL;
    mp_delete_watch_later_conf(mpctx, filename);
}

static void cmd_mouse(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    int pre_key = 0;

    const int x = cmd->args[0].v.i, y = cmd->args[1].v.i;
    int button = cmd->args[2].v.i;

    if (mpctx->video_out && mpctx->video_out->config_ok) {
        int oldx, oldy, oldhover;
        mp_input_get_mouse_pos(mpctx->input, &oldx, &oldy, &oldhover);
        struct mp_osd_res vo_res = osd_get_vo_res(mpctx->osd);

        // TODO: VOs don't send outside positions. should we abort if outside?
        int hover = x >= 0 && y >= 0 && x < vo_res.w && y < vo_res.h;

        if (vo_res.w && vo_res.h && hover != oldhover)
            pre_key = hover ? MP_KEY_MOUSE_ENTER : MP_KEY_MOUSE_LEAVE;
    }

    if (button == -1) {// no button
        if (pre_key)
            mp_input_put_key_artificial(mpctx->input, pre_key, 1);
        mp_input_set_mouse_pos_artificial(mpctx->input, x, y);
        return;
    }
    if (button < 0 || button >= MP_KEY_MOUSE_BTN_COUNT) {// invalid button
        MP_ERR(mpctx, "%d is not a valid mouse button number.\n", button);
        cmd->success = false;
        return;
    }
    const bool dbc = cmd->args[3].v.i;
    if (dbc && button > (MP_MBTN_RIGHT - MP_MBTN_BASE)) {
        MP_ERR(mpctx, "%d is not a valid mouse button for double-clicks.\n",
               button);
        cmd->success = false;
        return;
    }
    button += dbc ? MP_MBTN_DBL_BASE : MP_MBTN_BASE;
    if (pre_key)
        mp_input_put_key_artificial(mpctx->input, pre_key, 1);
    mp_input_set_mouse_pos_artificial(mpctx->input, x, y);
    mp_input_put_key_artificial(mpctx->input, button, 1);
}

static void cmd_key(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    int action = *(int *)cmd->priv;

    const char *key_name = cmd->args[0].v.s;
    if (key_name[0] == '\0' && action == MP_KEY_STATE_UP) {
        mp_input_put_key_artificial(mpctx->input, MP_INPUT_RELEASE_ALL, 1);
    } else {
        int code = mp_input_get_key_from_name(key_name);
        if (code < 0) {
            MP_ERR(mpctx, "'%s' is not a valid input name.\n", key_name);
            cmd->success = false;
            return;
        }
        double scale = action == 0 ? cmd->args[1].v.d : 1;
        mp_input_put_key_artificial(mpctx->input, code | action, scale);
    }
}

static void cmd_key_bind(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    const char *key = cmd->args[0].v.s;
    const char *target_cmd = cmd->args[1].v.s;
    const char *comment = cmd->args[2].v.s;
    if (comment && !comment[0])
        comment = NULL;
    if (!mp_input_bind_key(mpctx->input, key, bstr0(target_cmd), comment)) {
        MP_ERR(mpctx, "'%s' is not a valid input name.\n", key);
        cmd->success = false;
    }
}

static void cmd_apply_profile(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    char *profile = cmd->args[0].v.s;
    int mode = cmd->args[1].v.i;
    if (mode == 0) {
        cmd->success = m_config_set_profile(mpctx->mconfig, profile, 0) >= 0;
    } else {
        cmd->success = m_config_restore_profile(mpctx->mconfig, profile) >= 0;
    }
}

static void cmd_load_config_file(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    void *ctx = talloc_new(NULL);
    char *config_file = mp_get_user_path(ctx, mpctx->global, cmd->args[0].v.s);
    int r = m_config_parse_config_file(mpctx->mconfig, mpctx->global,
                                       config_file, NULL, 0);
    talloc_free(ctx);

    if (r < 1) {
        cmd->success = false;
        return;
    }

    mp_notify_property(mpctx, "profile-list");
}

static void cmd_load_input_conf(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    char *config_file = cmd->args[0].v.s;
    cmd->success = mp_input_load_config_file(mpctx->input, config_file);
}

static void cmd_load_script(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    char *script = cmd->args[0].v.s;
    int64_t id = mp_load_user_script(mpctx, script);
    if (id > 0) {
        struct mpv_node *res = &cmd->result;
        node_init(res, MPV_FORMAT_NODE_MAP, NULL);
        node_map_add_int64(res, "client_id", id);
    } else {
        cmd->success = false;
    }
}

static void cache_dump_poll(struct MPContext *mpctx)
{
    struct command_ctx *ctx = mpctx->command_ctx;
    struct mp_cmd_ctx *cmd = ctx->cache_dump_cmd;

    if (!cmd)
        return;

    // Can't close demuxer without stopping dumping.
    assert(mpctx->demuxer);

    if (mp_cancel_test(cmd->abort->cancel)) {
        // Synchronous abort. In particular, the dump command shall not report
        // completion to the user before the dump target file was closed.
        demux_cache_dump_set(mpctx->demuxer, 0, 0, NULL);
        assert(demux_cache_dump_get_status(mpctx->demuxer) <= 0);
    }

    int status = demux_cache_dump_get_status(mpctx->demuxer);
    if (status <= 0) {
        if (status < 0) {
            mp_cmd_msg(cmd, MSGL_ERR, "Cache dumping stopped due to error.");
            cmd->success = false;
        } else {
            mp_cmd_msg(cmd, MSGL_INFO, "Cache dumping successfully ended.");
            cmd->success = true;
        }
        ctx->cache_dump_cmd = NULL;
        mp_cmd_ctx_complete(cmd);
    }
}

void mp_abort_cache_dumping(struct MPContext *mpctx)
{
    struct command_ctx *ctx = mpctx->command_ctx;

    if (ctx->cache_dump_cmd)
        mp_cancel_trigger(ctx->cache_dump_cmd->abort->cancel);
    cache_dump_poll(mpctx);
    assert(!ctx->cache_dump_cmd); // synchronous abort, must have worked
}

static void run_dump_cmd(struct mp_cmd_ctx *cmd, double start, double end,
                         char *filename)
{
    struct MPContext *mpctx = cmd->mpctx;
    struct command_ctx *ctx = mpctx->command_ctx;

    mp_abort_cache_dumping(mpctx);

    if (!mpctx->demuxer) {
        mp_cmd_msg(cmd, MSGL_ERR, "No demuxer open.");
        cmd->success = false;
        mp_cmd_ctx_complete(cmd);
        return;
    }

    char *path = mp_get_user_path(NULL, mpctx->global, filename);
    mp_cmd_msg(cmd, MSGL_INFO, "Cache dumping started.");

    if (!demux_cache_dump_set(mpctx->demuxer, start, end, path)) {
        mp_cmd_msg(cmd, MSGL_INFO, "Cache dumping stopped.");
        mp_cmd_ctx_complete(cmd);
        talloc_free(path);
        return;
    }

    ctx->cache_dump_cmd = cmd;
    cache_dump_poll(mpctx);
    talloc_free(path);
}

static void cmd_dump_cache(void *p)
{
    struct mp_cmd_ctx *cmd = p;

    run_dump_cmd(cmd, cmd->args[0].v.d, cmd->args[1].v.d, cmd->args[2].v.s);
}

static void cmd_dump_cache_ab(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    run_dump_cmd(cmd, mpctx->opts->ab_loop[0], mpctx->opts->ab_loop[1],
                 cmd->args[0].v.s);
}

static void cmd_begin_vo_dragging(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    struct vo *vo = mpctx->video_out;

    if (vo)
        vo_control(vo, VOCTRL_BEGIN_DRAGGING, NULL);
}

static void cmd_context_menu(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    struct vo *vo = mpctx->video_out;

    if (vo)
        vo_control(vo, VOCTRL_SHOW_MENU, NULL);
}

static void cmd_flush_status_line(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    if (!mpctx->log)
        return;

    mp_msg_flush_status_line(mpctx->log, cmd->args[0].v.b);
}

static void cmd_notify_property(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;

    mp_notify_property(mpctx, cmd->args[0].v.s);
}

/* This array defines all known commands.
 * The first field the command name used in libmpv and input.conf.
 * The second field is the handler function (see mp_cmd_def.handler and
 * run_command()).
 * Then comes the definition of each argument. They are defined like options,
 * except that the result is parsed into mp_cmd.args[] (thus the option variable
 * is a field in the mp_cmd_arg union field). Arguments are optional if either
 * defval is set (usually via OPTDEF_ macros), or the MP_CMD_OPT_ARG flag is
 * set, or if it's the last argument and .vararg is set. If .vararg is set, the
 * command has an arbitrary number of arguments, all using the type indicated by
 * the last argument (they are appended to mp_cmd.args[] starting at the last
 * argument's index).
 * Arguments have names, which can be used by named argument functions, e.g. in
 * Lua with mp.command_native().
 */

// This does not specify the real destination of the command parameter values,
// it just provides a dummy for the OPT_ macros. The real destination is an
// array item  in mp_cmd.args[], using the index of the option definition.
#define OPT_BASE_STRUCT struct mp_cmd_arg

const struct mp_cmd_def mp_cmds[] = {
    { "ignore", cmd_ignore, .is_ignore = true, .is_noisy = true, },

    { "seek", cmd_seek,
        {
            {"target", OPT_TIME(v.d)},
            {"flags", OPT_FLAGS(v.i,
                {"relative", 4|0}, {"-", 4|0},
                {"absolute-percent", 4|1},
                {"absolute", 4|2},
                {"relative-percent", 4|3},
                {"keyframes", 32|8},
                {"exact", 32|16}),
                OPTDEF_INT(4|0)},
            // backwards compatibility only
            {"legacy", OPT_CHOICE(v.i,
                {"unused", 0}, {"default-precise", 0},
                {"keyframes", 32|8},
                {"exact", 32|16}),
                .flags = MP_CMD_OPT_ARG},
        },
        .allow_auto_repeat = true,
        .scalable = true,
    },
    { "revert-seek", cmd_revert_seek,
        { {"flags", OPT_FLAGS(v.i, {"mark", 2|0}, {"mark-permanent", 2|1}),
           .flags = MP_CMD_OPT_ARG} },
    },
    { "quit", cmd_quit, { {"code", OPT_INT(v.i), .flags = MP_CMD_OPT_ARG} },
        .priv = &(const bool){0} },
    { "quit-watch-later", cmd_quit, { {"code", OPT_INT(v.i),
                                       .flags = MP_CMD_OPT_ARG} },
        .priv = &(const bool){1} },
    { "stop", cmd_stop,
        { {"flags", OPT_FLAGS(v.i, {"keep-playlist", 1}), .flags = MP_CMD_OPT_ARG} }
    },
    { "frame-step", cmd_frame_step,
        {
            {"frames", OPT_INT(v.i), OPTDEF_INT(1)},
            {"flags", OPT_CHOICE(v.i,
                    {"play", 0},
                    {"seek", 1}),
                    .flags = MP_CMD_OPT_ARG},
        },
        .allow_auto_repeat = true,
        .on_updown = true,
        .priv = &(const bool){false},
    },
    { "frame-back-step", cmd_frame_step,
        .priv = &(const int){true},
        .allow_auto_repeat = true,
    },
    { "playlist-next", cmd_playlist_next_prev,
        {
            {"flags", OPT_CHOICE(v.i,
                {"weak", 0},
                {"force", 1}),
                .flags = MP_CMD_OPT_ARG},
        },
        .priv = &(const int){1},
    },
    { "playlist-prev", cmd_playlist_next_prev,
        {
            {"flags", OPT_CHOICE(v.i,
                {"weak", 0},
                {"force", 1}),
                .flags = MP_CMD_OPT_ARG},
        },
        .priv = &(const int){-1},
    },
    { "playlist-next-playlist", cmd_playlist_next_prev_playlist,
        .priv = &(const int){1} },
    { "playlist-prev-playlist", cmd_playlist_next_prev_playlist,
        .priv = &(const int){-1} },
    { "playlist-play-index", cmd_playlist_play_index,
        {
            {"index", OPT_CHOICE(v.i, {"current", -2}, {"none", -1}),
                M_RANGE(-1, INT_MAX)},
        }
    },
    { "playlist-shuffle", cmd_playlist_shuffle, },
    { "playlist-unshuffle", cmd_playlist_unshuffle, },
    { "sub-step", cmd_sub_step_seek,
        {
            {"skip", OPT_INT(v.i)},
            {"flags", OPT_CHOICE(v.i,
                {"primary", 0},
                {"secondary", 1}),
                OPTDEF_INT(0)},
        },
        .allow_auto_repeat = true,
        .priv = &(const bool){true}
    },
    { "sub-seek", cmd_sub_step_seek,
        {
            {"skip", OPT_INT(v.i)},
            {"flags", OPT_CHOICE(v.i,
                {"primary", 0},
                {"secondary", 1}),
                OPTDEF_INT(0)},
        },
        .allow_auto_repeat = true,
        .priv = &(const bool){false}
    },
    { "print-text", cmd_print_text, { {"text", OPT_STRING(v.s)} },
        .is_noisy = true, .allow_auto_repeat = true },
    { "show-text", cmd_show_text,
        {
            {"text", OPT_STRING(v.s)},
            {"duration", OPT_INT(v.i), OPTDEF_INT(-1)},
            {"level", OPT_INT(v.i), .flags = MP_CMD_OPT_ARG},
        },
        .is_noisy = true, .allow_auto_repeat = true},
    { "expand-text", cmd_expand_text, { {"text", OPT_STRING(v.s)} },
        .is_noisy = true },
    { "expand-path", cmd_expand_path, { {"text", OPT_STRING(v.s)} },
        .is_noisy = true },
    { "normalize-path", cmd_normalize_path, { {"filename", OPT_STRING(v.s)} }},
    { "escape-ass", cmd_escape_ass, { {"text", OPT_STRING(v.s)} },
        .is_noisy = true },
    { "show-progress", cmd_show_progress, .allow_auto_repeat = true,
        .is_noisy = true },

    { "sub-add", cmd_track_add,
        {
            {"url", OPT_STRING(v.s)},
            {"flags", OPT_CHOICE(v.i,
                {"select", 0}, {"auto", 1}, {"cached", 2}),
                .flags = MP_CMD_OPT_ARG},
            {"title", OPT_STRING(v.s), .flags = MP_CMD_OPT_ARG},
            {"lang", OPT_STRING(v.s), .flags = MP_CMD_OPT_ARG},
        },
        .priv = &(const int){STREAM_SUB},
        .spawn_thread = true,
        .can_abort = true,
        .abort_on_playback_end = true,
    },
    { "audio-add", cmd_track_add,
        {
            {"url", OPT_STRING(v.s)},
            {"flags", OPT_CHOICE(v.i,
                {"select", 0}, {"auto", 1}, {"cached", 2}),
                .flags = MP_CMD_OPT_ARG},
            {"title", OPT_STRING(v.s), .flags = MP_CMD_OPT_ARG},
            {"lang", OPT_STRING(v.s), .flags = MP_CMD_OPT_ARG},
        },
        .priv = &(const int){STREAM_AUDIO},
        .spawn_thread = true,
        .can_abort = true,
        .abort_on_playback_end = true,
    },
    { "video-add", cmd_track_add,
        {
            {"url", OPT_STRING(v.s)},
            {"flags", OPT_CHOICE(v.i, {"select", 0}, {"auto", 1}, {"cached", 2}),
                .flags = MP_CMD_OPT_ARG},
            {"title", OPT_STRING(v.s), .flags = MP_CMD_OPT_ARG},
            {"lang", OPT_STRING(v.s), .flags = MP_CMD_OPT_ARG},
            {"albumart", OPT_BOOL(v.b), .flags = MP_CMD_OPT_ARG},
        },
        .priv = &(const int){STREAM_VIDEO},
        .spawn_thread = true,
        .can_abort = true,
        .abort_on_playback_end = true,
    },

    { "sub-remove", cmd_track_remove, { {"id", OPT_INT(v.i), OPTDEF_INT(-1)} },
        .priv = &(const int){STREAM_SUB}, },
    { "audio-remove", cmd_track_remove, { {"id", OPT_INT(v.i), OPTDEF_INT(-1)} },
        .priv = &(const int){STREAM_AUDIO}, },
    { "video-remove", cmd_track_remove, { {"id", OPT_INT(v.i), OPTDEF_INT(-1)} },
        .priv = &(const int){STREAM_VIDEO}, },

    { "sub-reload", cmd_track_reload, { {"id", OPT_INT(v.i), OPTDEF_INT(-1)} },
        .priv = &(const int){STREAM_SUB},
        .spawn_thread = true,
        .can_abort = true,
        .abort_on_playback_end = true,
    },
    { "audio-reload", cmd_track_reload, { {"id", OPT_INT(v.i), OPTDEF_INT(-1)} },
        .priv = &(const int){STREAM_AUDIO},
        .spawn_thread = true,
        .can_abort = true,
        .abort_on_playback_end = true,
    },
    { "video-reload", cmd_track_reload, { {"id", OPT_INT(v.i), OPTDEF_INT(-1)} },
        .priv = &(const int){STREAM_VIDEO},
        .spawn_thread = true,
        .can_abort = true,
        .abort_on_playback_end = true,
    },

    { "rescan-external-files", cmd_rescan_external_files,
        {
            {"flags", OPT_CHOICE(v.i,
                {"keep-selection", 1},
                {"reselect", 0}),
                .flags = MP_CMD_OPT_ARG},
        },
        .spawn_thread = true,
        .can_abort = true,
        .abort_on_playback_end = true,
    },

    { "screenshot", cmd_screenshot,
        {
            {"flags", OPT_FLAGS(v.i,
                {"video", 4|0}, {"-", 4|0},
                {"window", 4|1},
                {"subtitles", 4|2},
                {"each-frame", 8}),
                OPTDEF_INT(4|2)},
            // backwards compatibility
            {"legacy", OPT_CHOICE(v.i,
                {"unused", 0}, {"single", 0},
                {"each-frame", 8}),
                .flags = MP_CMD_OPT_ARG},
        },
        .spawn_thread = true,
    },
    { "screenshot-to-file", cmd_screenshot_to_file,
        {
            {"filename", OPT_STRING(v.s)},
            {"flags", OPT_CHOICE(v.i,
                {"video", 0},
                {"window", 1},
                {"subtitles", 2}),
                OPTDEF_INT(2)},
        },
        .spawn_thread = true,
    },
    { "screenshot-raw", cmd_screenshot_raw,
        {
            {"flags", OPT_CHOICE(v.i,
                {"video", 0},
                {"window", 1},
                {"subtitles", 2}),
                OPTDEF_INT(2)},
             {"format", OPT_CHOICE(v.i,
                {"bgr0", 0},
                {"bgra", 1},
                {"rgba", 2},
                {"rgba64", 3}),
                OPTDEF_INT(0)},
        },
    },
    { "loadfile", cmd_loadfile,
        {
            {"url", OPT_STRING(v.s)},
            {"flags", OPT_CHOICE(v.i,
                {"replace", 0},
                {"append", 1},
                {"append-play", 2},
                {"insert-next", 3},
                {"insert-next-play", 4},
                {"insert-at", 5},
                {"insert-at-play", 6}),
                .flags = MP_CMD_OPT_ARG},
            {"index", OPT_INT(v.i), OPTDEF_INT(-1)},
            {"options", OPT_KEYVALUELIST(v.str_list), .flags = MP_CMD_OPT_ARG},
        },
    },
    { "loadlist", cmd_loadlist,
        {
            {"url", OPT_STRING(v.s)},
            {"flags", OPT_CHOICE(v.i,
                {"replace", 0},
                {"append", 1},
                {"append-play", 2},
                {"insert-next", 3},
                {"insert-next-play", 4},
                {"insert-at", 5},
                {"insert-at-play", 6}),
                .flags = MP_CMD_OPT_ARG},
            {"index", OPT_INT(v.i), OPTDEF_INT(-1)},
        },
        .spawn_thread = true,
        .can_abort = true,
    },
    { "playlist-clear", cmd_playlist_clear },
    { "playlist-remove", cmd_playlist_remove, {
        {"index", OPT_CHOICE(v.i, {"current", -1}),
            .flags = MP_CMD_OPT_ARG, M_RANGE(0, INT_MAX)}, }},
    { "playlist-move", cmd_playlist_move,  { {"index1", OPT_INT(v.i)},
                                             {"index2", OPT_INT(v.i)}, }},
    { "run", cmd_run, { {"command", OPT_STRING(v.s)},
                        {"args", OPT_STRING(v.s)}, },
        .vararg = true,
    },
    { "subprocess", cmd_subprocess,
        {
            {"args", OPT_STRINGLIST(v.str_list)},
            {"playback_only", OPT_BOOL(v.b), OPTDEF_INT(1)},
            {"capture_size", OPT_BYTE_SIZE(v.i64), M_RANGE(0, INT_MAX),
                OPTDEF_INT64(64 * 1024 * 1024)},
            {"capture_stdout", OPT_BOOL(v.b), .flags = MP_CMD_OPT_ARG},
            {"capture_stderr", OPT_BOOL(v.b), .flags = MP_CMD_OPT_ARG},
            {"detach", OPT_BOOL(v.b), .flags = MP_CMD_OPT_ARG},
            {"env", OPT_STRINGLIST(v.str_list), .flags = MP_CMD_OPT_ARG},
            {"stdin_data", OPT_STRING(v.s), .flags = MP_CMD_OPT_ARG},
            {"passthrough_stdin", OPT_BOOL(v.b), .flags = MP_CMD_OPT_ARG},
        },
        .spawn_thread = true,
        .can_abort = true,
    },

    { "set", cmd_set, {{"name", OPT_STRING(v.s)}, {"value", OPT_STRING(v.s)}}},
    { "del", cmd_del, {{"name", OPT_STRING(v.s)}}},
    { "change-list", cmd_change_list, { {"name", OPT_STRING(v.s)},
                                        {"operation", OPT_STRING(v.s)},
                                        {"value", OPT_STRING(v.s)} }},
    { "add", cmd_add_cycle, { {"name", OPT_STRING(v.s)},
                              {"value", OPT_DOUBLE(v.d), OPTDEF_DOUBLE(1)}, },
        .allow_auto_repeat = true,
        .scalable = true,
    },
    { "cycle", cmd_add_cycle, { {"name", OPT_STRING(v.s)},
                                {"value", OPT_CYCLEDIR(v.d), OPTDEF_DOUBLE(1)}, },
        .allow_auto_repeat = true,
        .scalable = true,
        .priv = "",
    },
    { "multiply", cmd_multiply, { {"name", OPT_STRING(v.s)},
                                  {"value", OPT_DOUBLE(v.d)}},
        .allow_auto_repeat = true},

    { "cycle-values", cmd_cycle_values, { {"arg0", OPT_STRING(v.s)},
                                          {"arg1", OPT_STRING(v.s)},
                                          {"argN", OPT_STRING(v.s)}, },
        .vararg = true},

    { "enable-section", cmd_enable_input_section,
        {
            {"name", OPT_STRING(v.s)},
            {"flags", OPT_FLAGS(v.i,
                {"default", 0},
                {"exclusive", MP_INPUT_EXCLUSIVE},
                {"allow-hide-cursor", MP_INPUT_ALLOW_HIDE_CURSOR},
                {"allow-vo-dragging", MP_INPUT_ALLOW_VO_DRAGGING}),
                .flags = MP_CMD_OPT_ARG},
        }
    },
    { "disable-section", cmd_disable_input_section,
        {{"name", OPT_STRING(v.s)} }},
    { "define-section", cmd_define_input_section,
        {
            {"name", OPT_STRING(v.s)},
            {"contents", OPT_STRING(v.s)},
            {"flags", OPT_CHOICE(v.i, {"default", 0}, {"force", 1}),
                .flags = MP_CMD_OPT_ARG},
        },
    },

    { "ab-loop", cmd_ab_loop },

    { "drop-buffers", cmd_drop_buffers, },

    { "af", cmd_filter, { {"operation", OPT_STRING(v.s)},
                          {"value", OPT_STRING(v.s)}, },
        .priv = &(const int){STREAM_AUDIO} },
    { "vf", cmd_filter, { {"operation", OPT_STRING(v.s)},
                          {"value", OPT_STRING(v.s)}, },
        .priv = &(const int){STREAM_VIDEO} },

    { "af-command", cmd_filter_command,
        {
            {"label", OPT_STRING(v.s)},
            {"command", OPT_STRING(v.s)},
            {"argument", OPT_STRING(v.s)},
            {"target", OPT_STRING(v.s), OPTDEF_STR("all"),
                .flags = MP_CMD_OPT_ARG},
        },
        .priv = &(const int){STREAM_AUDIO} },
    { "vf-command", cmd_filter_command,
        {
            {"label", OPT_STRING(v.s)},
            {"command", OPT_STRING(v.s)},
            {"argument", OPT_STRING(v.s)},
            {"target", OPT_STRING(v.s), OPTDEF_STR("all"),
                .flags = MP_CMD_OPT_ARG},
        },
        .priv = &(const int){STREAM_VIDEO} },

    { "ao-reload", cmd_ao_reload },

    { "script-binding", cmd_script_binding,
        {
            {"name", OPT_STRING(v.s)},
            {"arg", OPT_STRING(v.s), OPTDEF_STR(""),
                .flags = MP_CMD_OPT_ARG},
        },
        .allow_auto_repeat = true, .on_updown = true, .scalable = true },

    { "script-message", cmd_script_message, { {"args", OPT_STRING(v.s)} },
        .vararg = true },
    { "script-message-to", cmd_script_message_to, { {"target", OPT_STRING(v.s)},
                                                    {"args", OPT_STRING(v.s)} },
        .vararg = true },

    { "overlay-add", cmd_overlay_add, { {"id", OPT_INT(v.i)},
                                        {"x", OPT_INT(v.i)},
                                        {"y", OPT_INT(v.i)},
                                        {"file", OPT_STRING(v.s)},
                                        {"offset", OPT_INT(v.i)},
                                        {"fmt", OPT_STRING(v.s)},
                                        {"w", OPT_INT(v.i)},
                                        {"h", OPT_INT(v.i)},
                                        {"stride", OPT_INT(v.i)},
                                        {"dw", OPT_INT(v.i), OPTDEF_INT(0)},
                                        {"dh", OPT_INT(v.i), OPTDEF_INT(0)}, }},
    { "overlay-remove", cmd_overlay_remove, { {"id", OPT_INT(v.i)} } },

    { "osd-overlay", cmd_osd_overlay,
        {
            {"id", OPT_INT64(v.i64)},
            {"format", OPT_CHOICE(v.i, {"none", 0}, {"ass-events", 1})},
            {"data", OPT_STRING(v.s)},
            {"res_x", OPT_INT(v.i), OPTDEF_INT(0)},
            {"res_y", OPT_INT(v.i), OPTDEF_INT(720)},
            {"z", OPT_INT(v.i), OPTDEF_INT(0)},
            {"hidden", OPT_BOOL(v.b), OPTDEF_INT(0)},
            {"compute_bounds", OPT_BOOL(v.b), OPTDEF_INT(0)},
        },
        .is_noisy = true,
    },

    { "write-watch-later-config", cmd_write_watch_later_config },
    { "delete-watch-later-config", cmd_delete_watch_later_config,
        {{"filename", OPT_STRING(v.s), .flags = MP_CMD_OPT_ARG} }},

    { "mouse", cmd_mouse, { {"x", OPT_INT(v.i)},
                            {"y", OPT_INT(v.i)},
                            {"button", OPT_INT(v.i), OPTDEF_INT(-1)},
                            {"mode", OPT_CHOICE(v.i,
                                {"single", 0}, {"double", 1}),
                                .flags = MP_CMD_OPT_ARG}}},
    { "keybind", cmd_key_bind, { {"name", OPT_STRING(v.s)},
                                 {"cmd", OPT_STRING(v.s)},
                                 {"comment", OPT_STRING(v.s), .flags = MP_CMD_OPT_ARG} }},
    { "keypress", cmd_key, { {"name", OPT_STRING(v.s)},
                             {"scale", OPT_DOUBLE(v.d), OPTDEF_DOUBLE(1)} },
        .priv = &(const int){0}},
    { "keydown", cmd_key, { {"name", OPT_STRING(v.s)} },
        .priv = &(const int){MP_KEY_STATE_DOWN}},
    { "keyup", cmd_key, { {"name", OPT_STRING(v.s), .flags = MP_CMD_OPT_ARG} },
        .priv = &(const int){MP_KEY_STATE_UP}},

    { "apply-profile", cmd_apply_profile, {
        {"name", OPT_STRING(v.s)},
        {"mode", OPT_CHOICE(v.i, {"apply", 0}, {"restore", 1}),
            .flags = MP_CMD_OPT_ARG}, }
    },

    { "load-config-file", cmd_load_config_file, {{"filename", OPT_STRING(v.s)}} },

    { "load-input-conf", cmd_load_input_conf, {{"filename", OPT_STRING(v.s)}} },

    { "load-script", cmd_load_script, {{"filename", OPT_STRING(v.s)}} },

    { "dump-cache", cmd_dump_cache, { {"start", OPT_TIME(v.d),
                                        .flags = M_OPT_ALLOW_NO},
                                      {"end", OPT_TIME(v.d),
                                        .flags = M_OPT_ALLOW_NO},
                                      {"filename", OPT_STRING(v.s)} },
        .exec_async = true,
        .can_abort = true,
    },

    { "ab-loop-dump-cache", cmd_dump_cache_ab, { {"filename", OPT_STRING(v.s)} },
        .exec_async = true,
        .can_abort = true,
    },

    { "ab-loop-align-cache", cmd_align_cache_ab },

    { "begin-vo-dragging", cmd_begin_vo_dragging },

    { "context-menu", cmd_context_menu },

    { "flush-status-line", cmd_flush_status_line, { {"clear", OPT_BOOL(v.b)} } },

    { "notify-property", cmd_notify_property, { {"property", OPT_STRING(v.s)} } },

    {0}
};

#undef OPT_BASE_STRUCT
#undef ARG

void command_uninit(struct MPContext *mpctx)
{
    struct command_ctx *ctx = mpctx->command_ctx;

    assert(!ctx->cache_dump_cmd); // closing the demuxer must have aborted it

    overlay_uninit(mpctx);
    ao_hotplug_destroy(ctx->hotplug);

    m_option_free(&script_props_type, &ctx->script_props);

    talloc_free(mpctx->command_ctx);
    mpctx->command_ctx = NULL;
}

static int str_compare(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

void command_init(struct MPContext *mpctx)
{
    struct command_ctx *ctx = talloc(NULL, struct command_ctx);
    *ctx = (struct command_ctx){
        .last_seek_pts = MP_NOPTS_VALUE,
    };
    mpctx->command_ctx = ctx;

    int num_base = MP_ARRAY_SIZE(mp_properties_base);
    int num_opts = m_config_get_co_count(mpctx->mconfig);
    ctx->properties =
        talloc_zero_array(ctx, struct m_property, num_base + num_opts + 1);
    memcpy(ctx->properties, mp_properties_base, sizeof(mp_properties_base));

    const char **prop_names = talloc_array(NULL, const char *, num_base);
    for (int i = 0; i < num_base; ++i)
        prop_names[i] = mp_properties_base[i].name;
    qsort(prop_names, num_base, sizeof(const char *), str_compare);

    int count = num_base;
    for (int n = 0; n < num_opts; n++) {
        struct m_config_option *co = m_config_get_co_index(mpctx->mconfig, n);
        assert(co->name[0]);
        if (co->opt->flags & M_OPT_NOPROP)
            continue;

        struct m_property prop = {
            .name = co->name,
            .call = mp_property_generic_option,
            .is_option = true,
        };

        if (co->opt->type == &m_option_type_alias) {
            char buf[M_CONFIG_MAX_OPT_NAME_LEN];
            const char *alias = m_config_shadow_get_alias_from_opt(mpctx->mconfig->shadow, co->opt_id,
                                                                   buf, sizeof(buf));
            prop.priv = talloc_strdup(ctx, alias);

            prop.call = co->opt->deprecation_message ?
                            mp_property_deprecated_alias : mp_property_alias;

            // Check whether this eventually arrives at a real option. If not,
            // it's some CLI special handling thing. For example, "nosound" is
            // mapped to "no-audio", which has CLI special-handling, and cannot
            // be set as property.
            struct m_config_option *co2 = co;
            while (co2 && co2->opt->type == &m_option_type_alias) {
                const char *co2_alias = m_config_shadow_get_alias_from_opt(mpctx->mconfig->shadow, co2->opt_id,
                                                                           buf, sizeof(buf));
                co2 = m_config_get_co_raw(mpctx->mconfig, bstr0(co2_alias));
            }
            if (!co2)
                continue;
        }

        // The option might be covered by a manual property already.
        if (bsearch(&prop.name, prop_names, num_base, sizeof(const char *), str_compare))
            continue;

        ctx->properties[count++] = prop;
    }

    node_init(&ctx->mdata, MPV_FORMAT_NODE_ARRAY, NULL);
    talloc_steal(ctx, ctx->mdata.u.list);

    node_init(&ctx->udata, MPV_FORMAT_NODE_MAP, NULL);
    talloc_steal(ctx, ctx->udata.u.list);
    talloc_free(prop_names);
}

static void command_event(struct MPContext *mpctx, int event, void *arg)
{
    struct command_ctx *ctx = mpctx->command_ctx;

    if (event == MPV_EVENT_START_FILE) {
        ctx->last_seek_pts = MP_NOPTS_VALUE;
        ctx->marked_pts = MP_NOPTS_VALUE;
        ctx->marked_permanent = false;
    }

    if (event == MPV_EVENT_PLAYBACK_RESTART) {
        ctx->last_seek_time = mp_time_sec();
        run_command_opts(mpctx);
    }

    if (event == MPV_EVENT_IDLE)
        run_command_opts(mpctx);

    if (event == MPV_EVENT_END_FILE)
        mp_msg_flush_status_line(mpctx->log, false);

    if (event == MPV_EVENT_END_FILE || event == MPV_EVENT_FILE_LOADED) {
        // Update chapters - does nothing if something else is visible.
        set_osd_bar_chapters(mpctx, OSD_BAR_SEEK);
    }
    if (event == MP_EVENT_WIN_STATE2)
        ctx->cached_window_scale = 0;

    if (event == MP_EVENT_METADATA_UPDATE) {
        struct playlist_entry *const pe = mpctx->playing;
        if (pe && !pe->title) {
            const char *const name = mp_find_non_filename_media_title(mpctx);
            if (name && name[0]) {
                pe->title = talloc_strdup(pe, name);
                mp_notify_property(mpctx, "playlist");
            }
        }
    }
}

void handle_command_updates(struct MPContext *mpctx)
{
    struct command_ctx *ctx = mpctx->command_ctx;

    // This is a bit messy: ao_hotplug wakes up the player, and then we have
    // to recheck the state. Then the client(s) will read the property.
    if (ctx->hotplug && ao_hotplug_check_update(ctx->hotplug))
        mp_notify_property(mpctx, "audio-device-list");

    // Depends on polling demuxer wakeup callback notifications.
    cache_dump_poll(mpctx);
}

void run_command_opts(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    struct command_ctx *ctx = mpctx->command_ctx;

    if (!opts->input_commands || ctx->command_opts_processed)
        return;

    // Take easy way out and add these to the input queue.
    for (int i = 0; opts->input_commands[i]; i++) {
        struct mp_cmd *cmd = mp_input_parse_cmd(mpctx->input, bstr0(opts->input_commands[i]),
                                                "the command line");
        mp_input_queue_cmd(mpctx->input, cmd);
    }
    ctx->command_opts_processed = true;
}

void mp_notify(struct MPContext *mpctx, int event, void *arg)
{
    // The OSD can implicitly reference some properties.
    mpctx->osd_idle_update = true;

    command_event(mpctx, event, arg);

    mp_client_broadcast_event(mpctx, event, arg);
}

static void update_priority(struct MPContext *mpctx)
{
#if HAVE_WIN32_DESKTOP
    struct MPOpts *opts = mpctx->opts;
    if (opts->w32_priority > 0)
        SetPriorityClass(GetCurrentProcess(), opts->w32_priority);
#endif
}

static void update_track_switch(struct MPContext *mpctx, int order, int type)
{
    if (!mpctx->playback_initialized)
        return;

    int tid = mpctx->opts->stream_id[order][type];
    struct track *track;
    if (tid == -1) {
        // If "auto" reset to default track selection
        track = select_default_track(mpctx, order, type);
        mark_track_selection(mpctx, order, type, -1);
    } else {
        track = mp_track_by_tid(mpctx, type, tid);
    }
    mp_switch_track_n(mpctx, order, type, track, (tid == -1) ? 0 : FLAG_MARK_SELECTION);
    print_track_list(mpctx, "Track switched:");
    mp_wakeup_core(mpctx);
}

void mp_option_change_callback(void *ctx, struct m_config_option *co, int flags,
                               bool self_update)
{
    struct MPContext *mpctx = ctx;
    struct MPOpts *opts = mpctx->opts;
    bool init = !co;
    void *opt_ptr = init ? NULL : co->data; // NULL on start

    if (co)
        mp_notify_property(mpctx, co->name);
    if (opt_ptr == &opts->media_title)
        mp_notify(mpctx, MP_EVENT_METADATA_UPDATE, NULL);

    if (self_update)
        return;

    if (flags & UPDATE_TERM)
        mp_update_logging(mpctx, false);

    if (flags & (UPDATE_OSD | UPDATE_SUB_FILT | UPDATE_SUB_HARD)) {
        for (int n = 0; n < num_ptracks[STREAM_SUB]; n++) {
            struct track *track = mpctx->current_track[n][STREAM_SUB];
            struct dec_sub *sub = track ? track->d_sub : NULL;
            if (sub) {
                int ret = sub_control(sub, SD_CTRL_UPDATE_OPTS,
                                      (void *)(uintptr_t)flags);
                if (ret == CONTROL_OK && flags & (UPDATE_SUB_FILT | UPDATE_SUB_HARD)) {
                    sub_redecode_cached_packets(sub);
                    sub_reset(sub);
                    if (track->selected)
                        reselect_demux_stream(mpctx, track, true);
                }
            }
        }
        // For subs on a still image.
        redraw_subs(mpctx);
        osd_changed(mpctx->osd);
    }

    if (flags & UPDATE_BUILTIN_SCRIPTS)
        mp_load_builtin_scripts(mpctx);

    if (flags & UPDATE_IMGPAR) {
        struct track *track = mpctx->current_track[0][STREAM_VIDEO];
        if (track && track->dec) {
            mp_decoder_wrapper_reset_params(track->dec);
            mp_force_video_refresh(mpctx);
        }
    }

    if (flags & UPDATE_INPUT)
        mp_input_update_opts(mpctx->input);

    if (flags & UPDATE_CLIPBOARD)
        reinit_clipboard(mpctx);

    if (flags & UPDATE_SUB_EXTS)
        mp_update_subtitle_exts(mpctx->opts);

    if (init || opt_ptr == &opts->ipc_path || opt_ptr == &opts->ipc_client) {
        mp_uninit_ipc(mpctx->ipc_ctx);
        mpctx->ipc_ctx = mp_init_ipc(mpctx->clients, mpctx->global);
    }

    if (flags & UPDATE_VO && mpctx->video_out) {
        struct track *track = mpctx->current_track[0][STREAM_VIDEO];
        uninit_video_out(mpctx);
        handle_force_window(mpctx, true);
        reinit_video_chain(mpctx);
        if (track)
            queue_seek(mpctx, MPSEEK_RELATIVE, 0.0, MPSEEK_EXACT, 0);

        mp_wakeup_core(mpctx);
    }

    if (flags & UPDATE_AUDIO)
        reload_audio_output(mpctx);

    if (flags & UPDATE_PRIORITY)
        update_priority(mpctx);

    if (flags & UPDATE_SCREENSAVER)
        update_screensaver_state(mpctx);

    if (flags & UPDATE_VOL)
        audio_update_volume(mpctx);

    if (flags & UPDATE_LAVFI_COMPLEX)
        update_lavfi_complex(mpctx);

    if (flags & UPDATE_VIDEO) {
        if (mpctx->video_out) {
            vo_control(mpctx->video_out, VOCTRL_UPDATE_RENDER_OPTS, NULL);
            mp_wakeup_core(mpctx);
        }
    }

    if (flags & UPDATE_HWDEC) {
        struct track *track = mpctx->current_track[0][STREAM_VIDEO];
        struct mp_decoder_wrapper *dec = track ? track->dec : NULL;
        if (dec) {
            mp_decoder_wrapper_control(dec, VDCTRL_REINIT, NULL);
            double last_pts = mpctx->video_pts;
            if (last_pts != MP_NOPTS_VALUE)
                queue_seek(mpctx, MPSEEK_ABSOLUTE, last_pts, MPSEEK_EXACT, 0);
        }
    }

    if (flags & UPDATE_DVB_PROG) {
        if (!mpctx->stop_play)
            mpctx->stop_play = PT_CURRENT_ENTRY;
    }

    if (flags & UPDATE_DEMUXER)
        mpctx->demuxer_changed = true;

    if (flags & UPDATE_AD && mpctx->ao_chain) {
        uninit_audio_chain(mpctx);
        reinit_audio_chain(mpctx);
    }

    if (flags & UPDATE_VD && mpctx->vo_chain) {
        struct track *track = mpctx->current_track[0][STREAM_VIDEO];
        uninit_video_chain(mpctx);
        reinit_video_chain(mpctx);
        if (track)
            queue_seek(mpctx, MPSEEK_RELATIVE, 0.0, MPSEEK_EXACT, 0);
    }

    if (opt_ptr == &opts->vo->android_surface_size) {
        if (mpctx->video_out)
            vo_control(mpctx->video_out, VOCTRL_EXTERNAL_RESIZE, NULL);
    }

    if (opt_ptr == &opts->input_commands) {
        mpctx->command_ctx->command_opts_processed = false;
        run_command_opts(mpctx);
    }

    if (opt_ptr == &opts->playback_speed || opt_ptr == &opts->playback_pitch) {
        update_playback_speed(mpctx);
        mp_wakeup_core(mpctx);
    }

    if (opt_ptr == &opts->play_dir) {
        if (mpctx->play_dir != opts->play_dir) {
            // The option must be set before we seek if we're at EOF.
            if (mpctx->stop_play == AT_END_OF_FILE)
                mpctx->play_dir = opts->play_dir;
            queue_seek(mpctx, MPSEEK_ABSOLUTE, get_current_time(mpctx),
                       MPSEEK_EXACT, 0);
        }
    }

    if (opt_ptr == &opts->edition_id) {
        struct demuxer *demuxer = mpctx->demuxer;
        if (mpctx->playback_initialized && demuxer && demuxer->num_editions > 0) {
            if (opts->edition_id != demuxer->edition) {
                if (!mpctx->stop_play)
                    mpctx->stop_play = PT_CURRENT_ENTRY;
                mp_wakeup_core(mpctx);
            }
        }
    }

    if (opt_ptr == &opts->pause)
        set_pause_state(mpctx, opts->pause);

    if (opt_ptr == &opts->audio_delay) {
        if (mpctx->ao_chain) {
            mpctx->delay += mpctx->opts->audio_delay - mpctx->ao_chain->delay;
            mpctx->ao_chain->delay = mpctx->opts->audio_delay;
        }
        mp_wakeup_core(mpctx);
    }

    if (opt_ptr == &opts->vo->window_scale)
        update_window_scale(mpctx);

    if (opt_ptr == &opts->vo->hidpi_window_scale)
        update_hidpi_window_scale(mpctx, opts->vo->hidpi_window_scale);

    if (opt_ptr == &opts->cursor_autohide_delay)
        mpctx->mouse_timer = 0;

    if (opt_ptr == &opts->loop_file) {
        mpctx->remaining_file_loops = opts->loop_file;
        mp_notify_property(mpctx, "remaining-file-loops");
    }

    if (opt_ptr == &opts->ab_loop[0] || opt_ptr == &opts->ab_loop[1] ||
        opt_ptr == &opts->ab_loop_count) {
        mpctx->remaining_ab_loops = opts->ab_loop_count;
        mp_notify_property(mpctx, "remaining-ab-loops");
    }

    if (opt_ptr == &opts->ab_loop[0] || opt_ptr == &opts->ab_loop[1]) {
        update_ab_loop_clip(mpctx);
        // Update if visible
        set_osd_bar_chapters(mpctx, OSD_BAR_SEEK);
        mp_wakeup_core(mpctx);
    }

    if (opt_ptr == &opts->vf_settings)
        set_filters(mpctx, STREAM_VIDEO, opts->vf_settings);

    if (opt_ptr == &opts->af_settings)
        set_filters(mpctx, STREAM_AUDIO, opts->af_settings);

    for (int type = 0; type < STREAM_TYPE_COUNT; type++) {
        for (int order = 0; order < num_ptracks[type]; order++) {
            if (opt_ptr == &opts->stream_id[order][type])
                update_track_switch(mpctx, order, type);
        }
    }

    if (opt_ptr == &opts->vo->fullscreen && !opts->vo->fullscreen)
        mpctx->mouse_event_ts--; // Show mouse cursor

    if (opt_ptr == &opts->vo->taskbar_progress)
        update_vo_playback_state(mpctx);

    if (opt_ptr == &opts->image_display_duration && mpctx->vo_chain
        && mpctx->vo_chain->is_sparse && !mpctx->ao_chain
        && mpctx->video_status == STATUS_DRAINING)
        mpctx->time_frame = opts->image_display_duration;
}

void mp_notify_property(struct MPContext *mpctx, const char *property)
{
    mp_client_property_change(mpctx, property);
}
