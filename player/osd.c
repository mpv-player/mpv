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

#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <assert.h>

#include "config.h"
#include "talloc.h"

#include "common/msg.h"
#include "common/msg_control.h"
#include "options/options.h"
#include "common/common.h"
#include "options/m_property.h"
#include "common/encode.h"

#include "osdep/terminal.h"
#include "osdep/timer.h"

#include "demux/demux.h"
#include "stream/stream.h"
#include "sub/osd.h"

#include "video/out/vo.h"

#include "core.h"
#include "command.h"

#define saddf(var, ...) (*(var) = talloc_asprintf_append((*var), __VA_ARGS__))

// append time in the hh:mm:ss format (plus fractions if wanted)
static void sadd_hhmmssff(char **buf, double time, bool fractions)
{
    char *s = mp_format_time(time, fractions);
    *buf = talloc_strdup_append(*buf, s);
    talloc_free(s);
}

static void sadd_percentage(char **buf, int percent) {
    if (percent >= 0)
        *buf = talloc_asprintf_append(*buf, " (%d%%)", percent);
}

static char *join_lines(void *ta_ctx, char **parts, int num_parts)
{
    char *res = talloc_strdup(ta_ctx, "");
    for (int n = 0; n < num_parts; n++)
        res = talloc_asprintf_append(res, "%s%s", n ? "\n" : "", parts[n]);
    return res;
}

static void term_osd_update(struct MPContext *mpctx)
{
    int num_parts = 0;
    char *parts[3] = {0};

    if (!mpctx->opts->use_terminal)
        return;

    if (mpctx->term_osd_subs && mpctx->term_osd_subs[0])
        parts[num_parts++] = mpctx->term_osd_subs;
    if (mpctx->term_osd_text && mpctx->term_osd_text[0])
        parts[num_parts++] = mpctx->term_osd_text;
    if (mpctx->term_osd_status && mpctx->term_osd_status[0])
        parts[num_parts++] = mpctx->term_osd_status;

    char *s = join_lines(mpctx, parts, num_parts);

    if (strcmp(mpctx->term_osd_contents, s) == 0 &&
        mp_msg_has_status_line(mpctx->global))
    {
        talloc_free(s);
    } else {
        talloc_free(mpctx->term_osd_contents);
        mpctx->term_osd_contents = s;
        mp_msg(mpctx->statusline, MSGL_STATUS, "%s", s);
    }
}

static void term_osd_set_subs(struct MPContext *mpctx, const char *text)
{
    if (mpctx->video_out || !text)
        text = ""; // disable
    if (strcmp(mpctx->term_osd_subs ? mpctx->term_osd_subs : "", text) == 0)
        return;
    talloc_free(mpctx->term_osd_subs);
    mpctx->term_osd_subs = talloc_strdup(mpctx, text);
    term_osd_update(mpctx);
}

static void term_osd_set_text(struct MPContext *mpctx, const char *text)
{
    if ((mpctx->video_out && mpctx->opts->term_osd != 1) || !mpctx->opts->term_osd)
        text = ""; // disable
    talloc_free(mpctx->term_osd_text);
    mpctx->term_osd_text = talloc_strdup(mpctx, text);
    term_osd_update(mpctx);
}

static void term_osd_set_status(struct MPContext *mpctx, const char *text)
{
    talloc_free(mpctx->term_osd_status);
    mpctx->term_osd_status = talloc_strdup(mpctx, text);
    term_osd_update(mpctx);
}

static void add_term_osd_bar(struct MPContext *mpctx, char **line, int width)
{
    struct MPOpts *opts = mpctx->opts;

    if (width < 5)
        return;

    int pos = get_current_pos_ratio(mpctx, false) * (width - 3);
    pos = MPCLAMP(pos, 0, width - 3);

    bstr chars = bstr0(opts->term_osd_bar_chars);
    bstr parts[5];
    for (int n = 0; n < 5; n++)
        parts[n] = bstr_split_utf8(chars, &chars);

    saddf(line, "\r%.*s", BSTR_P(parts[0]));
    for (int n = 0; n < pos; n++)
        saddf(line, "%.*s", BSTR_P(parts[1]));
    saddf(line, "%.*s", BSTR_P(parts[2]));
    for (int n = 0; n < width - 3 - pos; n++)
        saddf(line, "%.*s", BSTR_P(parts[3]));
    saddf(line, "%.*s", BSTR_P(parts[4]));
}

static bool is_busy(struct MPContext *mpctx)
{
    return !mpctx->restart_complete && mp_time_sec() - mpctx->start_timestamp > 0.3;
}

static void print_status(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;

    update_window_title(mpctx, false);

    if (!opts->use_terminal)
        return;

    if (opts->quiet || !mpctx->playback_initialized || !mpctx->playing_msg_shown)
    {
        term_osd_set_status(mpctx, "");
        return;
    }

    if (opts->status_msg) {
        char *r = mp_property_expand_escaped_string(mpctx, opts->status_msg);
        term_osd_set_status(mpctx, r);
        talloc_free(r);
        return;
    }

    char *line = NULL;

    // Playback status
    if (is_busy(mpctx)) {
        saddf(&line, "(...) ");
    } else if (mpctx->paused_for_cache && !opts->pause) {
        saddf(&line, "(Buffering) ");
    } else if (mpctx->paused) {
        saddf(&line, "(Paused) ");
    }

    if (mpctx->d_audio)
        saddf(&line, "A");
    if (mpctx->d_video)
        saddf(&line, "V");
    saddf(&line, ": ");

    // Playback position
    sadd_hhmmssff(&line, get_playback_time(mpctx), mpctx->opts->osd_fractions);

    double len = get_time_length(mpctx);
    if (len >= 0) {
        saddf(&line, " / ");
        sadd_hhmmssff(&line, len, mpctx->opts->osd_fractions);
    }

    sadd_percentage(&line, get_percent_pos(mpctx));

    // other
    if (opts->playback_speed != 1)
        saddf(&line, " x%4.2f", opts->playback_speed);

    // A-V sync
    if (mpctx->d_audio && mpctx->d_video && mpctx->sync_audio_to_video) {
        if (mpctx->last_av_difference != MP_NOPTS_VALUE)
            saddf(&line, " A-V:%7.3f", mpctx->last_av_difference);
        else
            saddf(&line, " A-V: ???");
        if (fabs(mpctx->total_avsync_change) > 0.05)
            saddf(&line, " ct:%7.3f", mpctx->total_avsync_change);
    }

#if HAVE_ENCODING
    double position = get_current_pos_ratio(mpctx, true);
    char lavcbuf[80];
    if (encode_lavc_getstatus(mpctx->encode_lavc_ctx, lavcbuf, sizeof(lavcbuf),
            position) >= 0)
    {
        // encoding stats
        saddf(&line, " %s", lavcbuf);
    } else
#endif
    {
        // VO stats
        if (mpctx->d_video) {
            int64_t c = vo_get_drop_count(mpctx->video_out);
            if (c > 0 || mpctx->dropped_frames_total > 0) {
                saddf(&line, " Dropped: %"PRId64, c);
                if (mpctx->dropped_frames_total)
                    saddf(&line, "/%d", mpctx->dropped_frames_total);
            }
        }
    }

    if (mpctx->demuxer) {
        int64_t fill = -1;
        demux_stream_control(mpctx->demuxer, STREAM_CTRL_GET_CACHE_FILL, &fill);
        if (fill >= 0) {
            saddf(&line, " Cache: ");

            struct demux_ctrl_reader_state s = {.ts_duration = -1};
            demux_control(mpctx->demuxer, DEMUXER_CTRL_GET_READER_STATE, &s);

            if (s.ts_duration < 0) {
                saddf(&line, "???");
            } else {
                saddf(&line, "%2ds", (int)s.ts_duration);
            }
            saddf(&line, "+%lldKB", (long long)(fill / 1024));
        }
    }

    if (opts->term_osd_bar) {
        saddf(&line, "\n");
        int w = 80, h = 24;
        terminal_get_size(&w, &h);
        add_term_osd_bar(mpctx, &line, w);
    }

    // end
    term_osd_set_status(mpctx, line);
    talloc_free(line);
}

static bool set_osd_msg_va(struct MPContext *mpctx, int level, int time,
                           const char *fmt, va_list ap)
{
    if (level > mpctx->opts->osd_level)
        return false;

    talloc_free(mpctx->osd_msg_text);
    mpctx->osd_msg_text = talloc_vasprintf(mpctx, fmt, ap);
    mpctx->osd_show_pos = false;
    mpctx->osd_msg_next_duration = time / 1000.0;
    mpctx->osd_force_update = true;
    mpctx->sleeptime = 0;
    if (mpctx->osd_msg_next_duration <= 0)
        mpctx->osd_msg_visible = mp_time_sec();
    return true;
}

bool set_osd_msg(struct MPContext *mpctx, int level, int time,
                 const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    bool r = set_osd_msg_va(mpctx, level, time, fmt, ap);
    va_end(ap);
    return r;
}

// type: mp_osd_font_codepoints, ASCII, or OSD_BAR_*
void set_osd_bar(struct MPContext *mpctx, int type,
                 double min, double max, double neutral, double val)
{
    struct MPOpts *opts = mpctx->opts;
    if (opts->osd_level < 1 || !opts->osd_bar_visible || !mpctx->video_out)
        return;

    mpctx->osd_visible = mp_time_sec() + opts->osd_duration / 1000.0;
    mpctx->sleeptime = 0;
    mpctx->osd_progbar.type = type;
    mpctx->osd_progbar.value = (val - min) / (max - min);
    mpctx->osd_progbar.num_stops = 0;
    if (neutral > min && neutral < max) {
        float pos = (neutral - min) / (max - min);
        MP_TARRAY_APPEND(mpctx, mpctx->osd_progbar.stops,
                         mpctx->osd_progbar.num_stops, pos);
    }
    osd_set_progbar(mpctx->osd, &mpctx->osd_progbar);
}

// Update a currently displayed bar of the same type, without resetting the
// timer.
static void update_osd_bar(struct MPContext *mpctx, int type,
                           double min, double max, double val)
{
    if (mpctx->osd_progbar.type != type)
        return;

    float new_value = (val - min) / (max - min);
    if (new_value != mpctx->osd_progbar.value) {
        mpctx->osd_progbar.value = new_value;
        osd_set_progbar(mpctx->osd, &mpctx->osd_progbar);
    }
}

void set_osd_bar_chapters(struct MPContext *mpctx, int type)
{
    struct MPOpts *opts = mpctx->opts;
    if (mpctx->osd_progbar.type != type)
        return;

    mpctx->osd_progbar.num_stops = 0;
    double len = get_time_length(mpctx);
    if (len > 0) {
        if (opts->ab_loop[0] != MP_NOPTS_VALUE &&
            opts->ab_loop[1] != MP_NOPTS_VALUE)
        {
            MP_TARRAY_APPEND(mpctx, mpctx->osd_progbar.stops,
                        mpctx->osd_progbar.num_stops, opts->ab_loop[0] / len);
            MP_TARRAY_APPEND(mpctx, mpctx->osd_progbar.stops,
                        mpctx->osd_progbar.num_stops, opts->ab_loop[1] / len);
        } else {
            int num = get_chapter_count(mpctx);
            for (int n = 0; n < num; n++) {
                double time = chapter_start_time(mpctx, n);
                if (time >= 0) {
                    float pos = time / len;
                    MP_TARRAY_APPEND(mpctx, mpctx->osd_progbar.stops,
                                     mpctx->osd_progbar.num_stops, pos);
                }
            }
        }
    }
    osd_set_progbar(mpctx->osd, &mpctx->osd_progbar);
}

// osd_function is the symbol appearing in the video status, such as OSD_PLAY
void set_osd_function(struct MPContext *mpctx, int osd_function)
{
    struct MPOpts *opts = mpctx->opts;

    mpctx->osd_function = osd_function;
    mpctx->osd_function_visible = mp_time_sec() + opts->osd_duration / 1000.0;
    mpctx->osd_force_update = true;
    mpctx->sleeptime = 0;
}

/**
 * \brief Display text subtitles on the OSD
 */
void set_osd_subtitle(struct MPContext *mpctx, const char *text)
{
    osd_set_text(mpctx->osd, OSDTYPE_SUB, text);
    term_osd_set_subs(mpctx, text);
}

void get_current_osd_sym(struct MPContext *mpctx, char *buf, size_t buf_size)
{
    int sym = mpctx->osd_function;
    if (!sym) {
        if (is_busy(mpctx) || (mpctx->paused_for_cache && !mpctx->opts->pause)) {
            sym = OSD_CLOCK;
        } else if (mpctx->paused || mpctx->step_frames) {
            sym = OSD_PAUSE;
        } else {
            sym = OSD_PLAY;
        }
    }
    osd_get_function_sym(buf, buf_size, sym);
}

static void sadd_osd_status(char **buffer, struct MPContext *mpctx, int level)
{
    assert(level >= 0 && level <= 3);
    if (level == 0)
        return;
    char *msg = mpctx->opts->osd_msg[level - 1];

    if (msg && msg[0]) {
        char *text = mp_property_expand_escaped_string(mpctx, msg);
        *buffer = talloc_strdup_append(*buffer, text);
        talloc_free(text);
    } else if (level >= 2) {
        bool fractions = mpctx->opts->osd_fractions;
        char sym[10];
        get_current_osd_sym(mpctx, sym, sizeof(sym));
        saddf(buffer, "%s ", sym);
        char *custom_msg = mpctx->opts->osd_status_msg;
        if (custom_msg && level == 3) {
            char *text = mp_property_expand_escaped_string(mpctx, custom_msg);
            *buffer = talloc_strdup_append(*buffer, text);
            talloc_free(text);
        } else {
            sadd_hhmmssff(buffer, get_playback_time(mpctx), fractions);
            if (level == 3) {
                double len = get_time_length(mpctx);
                if (len >= 0) {
                    saddf(buffer, " / ");
                    sadd_hhmmssff(buffer, len, fractions);
                }
                sadd_percentage(buffer, get_percent_pos(mpctx));
            }
        }
    }
}

// OSD messages initated by seeking commands are added lazily with this
// function, because multiple successive seek commands can be coalesced.
static void add_seek_osd_messages(struct MPContext *mpctx)
{
    if (mpctx->add_osd_seek_info & OSD_SEEK_INFO_BAR) {
        double pos = get_current_pos_ratio(mpctx, false);
        set_osd_bar(mpctx, OSD_BAR_SEEK, 0, 1, 0, MPCLAMP(pos, 0, 1));
        set_osd_bar_chapters(mpctx, OSD_BAR_SEEK);
    }
    if (mpctx->add_osd_seek_info & OSD_SEEK_INFO_TEXT) {
        // Never in term-osd mode
        if (mpctx->video_out && mpctx->opts->term_osd != 1) {
            if (set_osd_msg(mpctx, 1, mpctx->opts->osd_duration, ""))
                mpctx->osd_show_pos = true;
        }
    }
    if (mpctx->add_osd_seek_info & OSD_SEEK_INFO_CHAPTER_TEXT) {
        char *chapter = chapter_display_name(mpctx, get_current_chapter(mpctx));
        set_osd_msg(mpctx, 1, mpctx->opts->osd_duration,
                     "Chapter: %s", chapter);
        talloc_free(chapter);
    }
    if ((mpctx->add_osd_seek_info & OSD_SEEK_INFO_EDITION)
        && mpctx->master_demuxer)
    {
        set_osd_msg(mpctx, 1, mpctx->opts->osd_duration,
                     "Playing edition %d of %d.",
                     mpctx->master_demuxer->edition + 1,
                     mpctx->master_demuxer->num_editions);
    }
    if (mpctx->add_osd_seek_info & OSD_SEEK_INFO_CURRENT_FILE) {
        if (mpctx->filename) {
            set_osd_msg(mpctx, 1, mpctx->opts->osd_duration, "%s",
                        mpctx->filename);
        }
    }
    mpctx->add_osd_seek_info = 0;
}

// Update the OSD text (both on VO and terminal status line).
void update_osd_msg(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    struct osd_state *osd = mpctx->osd;

    double now = mp_time_sec();

    if (!mpctx->osd_force_update) {
        double delay = 0.050; // update the OSD at most this often
        double diff = now - mpctx->osd_last_update;
        if (diff < delay) {
            mpctx->sleeptime = MPMIN(mpctx->sleeptime, delay - diff);
            return;
        }
    }
    mpctx->osd_force_update = false;
    mpctx->osd_last_update = now;

    if (mpctx->osd_visible) {
        double sleep = mpctx->osd_visible - now;
        if (sleep > 0) {
            mpctx->sleeptime = MPMIN(mpctx->sleeptime, sleep);
        } else {
            mpctx->osd_visible = 0;
            mpctx->osd_progbar.type = -1; // disable
            osd_set_progbar(mpctx->osd, &mpctx->osd_progbar);
        }
    }

    if (mpctx->osd_function_visible) {
        double sleep = mpctx->osd_function_visible - now;
        if (sleep > 0) {
            mpctx->sleeptime = MPMIN(mpctx->sleeptime, sleep);
        } else {
            mpctx->osd_function_visible = 0;
            mpctx->osd_function = 0;
        }
    }

    if (mpctx->osd_msg_next_duration > 0) {
        // This is done to avoid cutting the OSD message short if slow commands
        // are executed between setting the OSD message and showing it.
        mpctx->osd_msg_visible = now + mpctx->osd_msg_next_duration;
        mpctx->osd_msg_next_duration = 0;
    }

    if (mpctx->osd_msg_visible) {
        double sleep = mpctx->osd_msg_visible - now;
        if (sleep > 0) {
            mpctx->sleeptime = MPMIN(mpctx->sleeptime, sleep);
        } else {
            talloc_free(mpctx->osd_msg_text);
            mpctx->osd_msg_text = NULL;
            mpctx->osd_msg_visible = 0;
            mpctx->osd_show_pos = false;
        }
    }

    add_seek_osd_messages(mpctx);

    if (mpctx->osd_progbar.type == OSD_BAR_SEEK) {
        double pos = get_current_pos_ratio(mpctx, false);
        update_osd_bar(mpctx, OSD_BAR_SEEK, 0, 1, MPCLAMP(pos, 0, 1));
    }

    print_status(mpctx);

    // Look if we have a msg
    if (mpctx->osd_msg_text && !mpctx->osd_show_pos) {
        osd_set_text(osd, OSDTYPE_OSD, mpctx->osd_msg_text);
        term_osd_set_text(mpctx, mpctx->osd_msg_text);
        return;
    }

    int osd_level = opts->osd_level;
    if (mpctx->osd_msg_text && mpctx->osd_show_pos)
        osd_level = 3;

    // clear, or if OSD level demands it, show the status
    char *text = NULL;
    sadd_osd_status(&text, mpctx, osd_level);

    osd_set_text(osd, OSDTYPE_OSD, text);
    talloc_free(text);

    // always clear (term-osd has separate status line)
    term_osd_set_text(mpctx, "");
}
