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

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include <libavutil/common.h>
#include <ass/ass.h>

#include "mpv_talloc.h"

#include "config.h"
#include "options/m_config.h"
#include "options/options.h"
#include "common/common.h"
#include "common/msg.h"
#include "demux/demux.h"
#include "video/csputils.h"
#include "video/mp_image.h"
#include "dec_sub.h"
#include "ass_mp.h"
#include "sd.h"

struct sd_ass_priv {
    struct ass_library *ass_library;
    struct ass_renderer *ass_renderer;
    struct ass_track *ass_track;
    struct ass_track *shadow_track; // for --sub-ass=no rendering
    bool ass_configured;
    bool is_converted;
    struct lavc_conv *converter;
    struct sd_filter **filters;
    int num_filters;
    bool clear_once;
    struct mp_ass_packer *packer;
    struct sub_bitmap_copy_cache *copy_cache;
    bstr last_text;
    struct mp_image_params video_params;
    struct mp_image_params last_params;
    struct mp_osd_res osd;
    struct seen_packet *seen_packets;
    int num_seen_packets;
    int *packets_animated;
    int num_packets_animated;
    bool check_animated;
    bool duration_unknown;
};

struct seen_packet {
    int64_t pos;
    double pts;
};

static void mangle_colors(struct sd *sd, struct sub_bitmaps *parts);
static void fill_plaintext(struct sd *sd, double pts);

static const struct sd_filter_functions *const filters[] = {
    // Note: list order defines filter order.
    &sd_filter_sdh,
#if HAVE_POSIX
    &sd_filter_regex,
#endif
#if HAVE_JAVASCRIPT
    &sd_filter_jsre,
#endif
    NULL,
};

// Add default styles, if the track does not have any styles yet.
// Apply style overrides if the user provides any.
static void mp_ass_add_default_styles(ASS_Track *track, struct mp_subtitle_opts *opts,
                                      struct mp_subtitle_shared_opts *shared_opts, int order)
{
    if (opts->ass_styles_file && shared_opts->ass_style_override[order])
        ass_read_styles(track, opts->ass_styles_file, NULL);

    if (track->n_styles == 0) {
        if (!track->PlayResY) {
            track->PlayResX = MP_ASS_FONT_PLAYRESX;
            track->PlayResY = MP_ASS_FONT_PLAYRESY;
        }
        track->Kerning = true;
        int sid = ass_alloc_style(track);
        track->default_style = sid;
        ASS_Style *style = track->styles + sid;
        style->Name = strdup("Default");
        mp_ass_set_style(style, track->PlayResY, opts->sub_style);
    }

    if (shared_opts->ass_style_override[order])
        ass_process_force_style(track);
}

static const char *const font_mimetypes[] = {
    "application/x-truetype-font",
    "application/vnd.ms-opentype",
    "application/x-font-ttf",
    "application/x-font", // probably incorrect
    "application/font-sfnt",
    "font/collection",
    "font/otf",
    "font/sfnt",
    "font/ttf",
    NULL
};

static const char *const font_exts[] = {".ttf", ".ttc", ".otf", ".otc", NULL};

static bool attachment_is_font(struct mp_log *log, struct demux_attachment *f)
{
    if (!f->name || !f->type || !f->data || !f->data_size)
        return false;
    for (int n = 0; font_mimetypes[n]; n++) {
        if (strcmp(font_mimetypes[n], f->type) == 0)
            return true;
    }
    // fallback: match against file extension
    char *ext = strlen(f->name) > 4 ? f->name + strlen(f->name) - 4 : "";
    for (int n = 0; font_exts[n]; n++) {
        if (strcasecmp(ext, font_exts[n]) == 0) {
            mp_warn(log, "Loading font attachment '%s' with MIME type %s. "
                    "Assuming this is a broken Matroska file, which was "
                    "muxed without setting a correct font MIME type.\n",
                    f->name, f->type);
            return true;
        }
    }
    return false;
}

static void add_subtitle_fonts(struct sd *sd)
{
    struct sd_ass_priv *ctx = sd->priv;
    struct mp_subtitle_opts *opts = sd->opts;
    if (!opts->ass_enabled || !opts->use_embedded_fonts || !sd->attachments)
        return;
    for (int i = 0; i < sd->attachments->num_entries; i++) {
        struct demux_attachment *f = &sd->attachments->entries[i];
        if (attachment_is_font(sd->log, f))
            ass_add_font(ctx->ass_library, f->name, f->data, f->data_size);
    }
}

static void filters_destroy(struct sd *sd)
{
    struct sd_ass_priv *ctx = sd->priv;

    for (int n = 0; n < ctx->num_filters; n++) {
        struct sd_filter *ft = ctx->filters[n];
        if (ft->driver->uninit)
            ft->driver->uninit(ft);
        talloc_free(ft);
    }
    ctx->num_filters = 0;
}

static void filters_init(struct sd *sd)
{
    struct sd_ass_priv *ctx = sd->priv;

    filters_destroy(sd);

    for (int n = 0; filters[n]; n++) {
        struct sd_filter *ft = talloc_ptrtype(ctx, ft);
        *ft = (struct sd_filter){
            .global = sd->global,
            .log = sd->log,
            .opts = mp_get_config_group(ft, sd->global, &mp_sub_filter_opts),
            .driver = filters[n],
            .codec = "ass",
            .event_format = talloc_strdup(ft, ctx->ass_track->event_format),
        };
        if (ft->driver->init(ft)) {
            MP_TARRAY_APPEND(ctx, ctx->filters, ctx->num_filters, ft);
        } else {
            talloc_free(ft);
        }
    }
}

static void enable_output(struct sd *sd, bool enable)
{
    struct sd_ass_priv *ctx = sd->priv;
    if (enable == !!ctx->ass_renderer)
        return;
    if (ctx->ass_renderer) {
        ass_renderer_done(ctx->ass_renderer);
        ctx->ass_renderer = NULL;
    } else {
        ctx->ass_renderer = ass_renderer_init(ctx->ass_library);

        mp_ass_configure_fonts(ctx->ass_renderer, sd->opts->sub_style,
                               sd->global, sd->log);
    }
}

static void assobjects_init(struct sd *sd)
{
    struct sd_ass_priv *ctx = sd->priv;
    struct mp_subtitle_opts *opts = sd->opts;
    struct mp_subtitle_shared_opts *shared_opts = sd->shared_opts;

    ctx->ass_library = mp_ass_init(sd->global, sd->opts->sub_style, sd->log);
    ass_set_extract_fonts(ctx->ass_library, opts->use_embedded_fonts);

    add_subtitle_fonts(sd);

    if (shared_opts->ass_style_override[sd->order])
        ass_set_style_overrides(ctx->ass_library, opts->ass_style_override_list);

    ctx->ass_track = ass_new_track(ctx->ass_library);
    ctx->ass_track->track_type = TRACK_TYPE_ASS;

    ctx->shadow_track = ass_new_track(ctx->ass_library);
    ctx->shadow_track->PlayResX = MP_ASS_FONT_PLAYRESX;
    ctx->shadow_track->PlayResY = MP_ASS_FONT_PLAYRESY;
    mp_ass_add_default_styles(ctx->shadow_track, opts, shared_opts, sd->order);

    char *extradata = sd->codec->extradata;
    int extradata_size = sd->codec->extradata_size;
    if (ctx->converter) {
        extradata = lavc_conv_get_extradata(ctx->converter);
        extradata_size = extradata ? strlen(extradata) : 0;
    }
    if (extradata)
        ass_process_codec_private(ctx->ass_track, extradata, extradata_size);

    mp_ass_add_default_styles(ctx->ass_track, opts, shared_opts, sd->order);

#if LIBASS_VERSION >= 0x01302000
    ass_set_check_readorder(ctx->ass_track, sd->opts->sub_clear_on_seek ? 0 : 1);
#endif

    enable_output(sd, true);
}

static void assobjects_destroy(struct sd *sd)
{
    struct sd_ass_priv *ctx = sd->priv;

    ass_free_track(ctx->ass_track);
    ass_free_track(ctx->shadow_track);
    enable_output(sd, false);
    ass_library_done(ctx->ass_library);
}

static int init(struct sd *sd)
{
    struct sd_ass_priv *ctx = talloc_zero(sd, struct sd_ass_priv);
    sd->priv = ctx;

    // Note: accept "null" as alias for "ass", so EDL delay_open subtitle
    //       streams work.
    if (strcmp(sd->codec->codec, "ass") != 0 &&
        strcmp(sd->codec->codec, "null") != 0)
    {
        ctx->is_converted = true;
        ctx->converter = lavc_conv_create(sd);
        if (!ctx->converter)
            return -1;

        if (strcmp(sd->codec->codec, "eia_608") == 0)
            ctx->duration_unknown = 1;
    }

    assobjects_init(sd);
    filters_init(sd);

    ctx->packer = mp_ass_packer_alloc(ctx);

    // Subtitles does not have any profile value, so put the converted type as a profile.
    const char **desc = ctx->converter ? &sd->codec->codec_profile : &sd->codec->codec_desc;
    switch (ctx->ass_track->track_type) {
    case TRACK_TYPE_ASS:
        *desc = "Advanced Sub Station Alpha";
        break;
    case TRACK_TYPE_SSA:
        *desc = "Sub Station Alpha";
        break;
    }

    return 0;
}

// Check if subtitle has events that would cause it to be animated inside {}
static bool is_animated(const char *str)
{
    const char *begin = str;
    while ((str = strchr(str, '{'))) {
        if (str++ > begin && str[-2] == '\\')
            continue;

        const char *end = strchr(str, '}');
        if (!end)
            return false;

        while ((str = memchr(str, '\\', end - str))) {
            while (str[0] == '\\')
                ++str;
            while (str[0] == ' ' || str[0] == '\t')
                ++str;
            if (str[0] == 'k' || str[0] == 'K' || str[0] == 't' ||
                (str[0] == 'f' && str[1] == 'a' && str[2] == 'd') ||
                (str[0] == 'm' && str[1] == 'o' && str[2] == 'v' && str[3] == 'e'))
            {
                return true;
            }
        }

        str = end + 1;
    }

    return false;
}

// Note: pkt is not necessarily a fully valid refcounted packet.
static void filter_and_add(struct sd *sd, struct demux_packet *pkt)
{
    struct sd_ass_priv *ctx = sd->priv;
    struct demux_packet *orig_pkt = pkt;
    ASS_Track *track = ctx->ass_track;
    int old_n_events = track->n_events;

    for (int n = 0; n < ctx->num_filters; n++) {
        struct sd_filter *ft = ctx->filters[n];
        struct demux_packet *npkt = ft->driver->filter(ft, pkt);
        if (pkt != npkt && pkt != orig_pkt)
            talloc_free(pkt);
        pkt = npkt;
        if (!pkt)
            return;
    }

    ass_process_chunk(ctx->ass_track, pkt->buffer, pkt->len,
                      llrint(pkt->pts * 1000),
                      llrint(pkt->duration * 1000));

    // This bookkeeping only has any practical use for ASS subs
    // over a VO with no video.
    if (!ctx->is_converted) {
        if (!pkt->seen) {
            for (int n = track->n_events - 1; n >= 0; n--) {
                if (n + 1 == old_n_events || pkt->animated == 1)
                    break;
                ASS_Event *event = &track->events[n];
                // Might as well mark pkt->animated here with effects if we can.
                pkt->animated = (event->Effect && event->Effect[0]) ? 1 : -1;
                if (ctx->check_animated && pkt->animated != 1)
                    pkt->animated = is_animated(event->Text);
            }
            MP_TARRAY_APPEND(ctx, ctx->packets_animated, ctx->num_packets_animated, pkt->animated);
        } else {
            if (ctx->check_animated && ctx->packets_animated[pkt->seen_pos] == -1) {
                for (int n = track->n_events - 1; n >= 0; n--) {
                    if (n + 1 == old_n_events || pkt->animated == 1)
                        break;
                    ASS_Event *event = &track->events[n];
                    ctx->packets_animated[pkt->seen_pos] = is_animated(event->Text);
                    pkt->animated = ctx->packets_animated[pkt->seen_pos];
                }
            } else {
                pkt->animated = ctx->packets_animated[pkt->seen_pos];
            }
        }
    }

    if (pkt != orig_pkt)
        talloc_free(pkt);
}

// Test if the packet with the given file position and pts was already consumed.
// Return false if the packet is new (and add it to the internal list), and
// return true if it was already seen.
static bool check_packet_seen(struct sd *sd, struct demux_packet *packet)
{
    struct sd_ass_priv *priv = sd->priv;
    int a = 0;
    int b = priv->num_seen_packets;
    while (a < b) {
        int mid = a + (b - a) / 2;
        struct seen_packet *seen_packet = &priv->seen_packets[mid];
        if (packet->pos == seen_packet->pos && packet->pts == seen_packet->pts) {
            packet->seen_pos = mid;
            return true;
        }
        if (packet->pos > seen_packet->pos ||
            (packet->pos == seen_packet->pos && packet->pts > seen_packet->pts)) {
            a = mid + 1;
        } else {
            b = mid;
        }
    }
    packet->seen_pos = a;
    MP_TARRAY_INSERT_AT(priv, priv->seen_packets, priv->num_seen_packets, a,
                        (struct seen_packet){packet->pos, packet->pts});
    return false;
}

#define UNKNOWN_DURATION (INT_MAX / 1000)

static void decode(struct sd *sd, struct demux_packet *packet)
{
    struct sd_ass_priv *ctx = sd->priv;
    ASS_Track *track = ctx->ass_track;

    packet->sub_duration = packet->duration;

    if (ctx->converter) {
        if (!sd->opts->sub_clear_on_seek && packet->pos >= 0 &&
            check_packet_seen(sd, packet))
            return;

        double sub_pts = 0;
        double sub_duration = 0;
        char **r = lavc_conv_decode(ctx->converter, packet, &sub_pts,
                                    &sub_duration);
        if (sd->opts->sub_stretch_durations ||
            packet->duration < 0 || sub_duration == UINT32_MAX) {
            if (!ctx->duration_unknown) {
                MP_VERBOSE(sd, "Subtitle with unknown duration.\n");
                ctx->duration_unknown = true;
            }
            sub_duration = UNKNOWN_DURATION;
        }

        for (int n = 0; r && r[n]; n++) {
            struct demux_packet pkt2 = {
                .pts = sub_pts,
                .duration = sub_duration,
                .buffer = r[n],
                .len = strlen(r[n]),
            };
            filter_and_add(sd, &pkt2);
        }
        if (ctx->duration_unknown) {
            for (int n = track->n_events - 2; n >= 0; n--) {
                if (track->events[n].Duration == UNKNOWN_DURATION * 1000) {
                    if (track->events[n].Start != track->events[n + 1].Start) {
                        track->events[n].Duration = track->events[n + 1].Start -
                                                    track->events[n].Start;
                    } else {
                        track->events[n].Duration = track->events[n + 1].Duration;
                    }
                }
            }
        }
    } else {
        // Note that for this packet format, libass has an internal mechanism
        // for discarding duplicate (already seen) packets but we check this
        // anyways for our purposes for ASS subtitles.
        packet->seen = check_packet_seen(sd, packet);
        filter_and_add(sd, packet);
    }
}

// Calculate the height used for scaling subtitle text size so --sub-scale-with-window
// can undo this scale and use frame size instead. The algorithm used is the following:
// - If use_margins is disabled, the text is scaled with the visual size of the video.
// - If use_margins is enabled, the text is scaled with the size of the video
//   as if the video is resized to "fit" the size of the frame.
static float get_libass_scale_height(struct mp_osd_res *dim, bool use_margins)
{
    float vidw = dim->w - (dim->ml + dim->mr);
    float vidh = dim->h - (dim->mt + dim->mb);
    if (!use_margins || vidw < 1.0)
        return vidh;
    else
        return MPMIN(dim->h, dim->w / vidw * vidh);
}

static void configure_ass(struct sd *sd, struct mp_osd_res *dim,
                          bool converted, ASS_Track *track)
{
    struct mp_subtitle_opts *opts = sd->opts;
    struct mp_subtitle_shared_opts *shared_opts = sd->shared_opts;
    struct sd_ass_priv *ctx = sd->priv;
    ASS_Renderer *priv = ctx->ass_renderer;

    ass_set_frame_size(priv, dim->w, dim->h);
    ass_set_margins(priv, dim->mt, dim->mb, dim->ml, dim->mr);

    bool set_use_margins = false;
    float set_sub_pos = 0.0f;
    float set_line_spacing = 0;
    float set_font_scale = 1;
    int set_hinting = 0;
    bool set_scale_with_window = false;
    bool set_scale_by_window = true;
    bool total_override = false;
    // With forced overrides, apply the --sub-* specific options
    if (converted || shared_opts->ass_style_override[sd->order] == ASS_STYLE_OVERRIDE_FORCE) {
        set_scale_with_window = opts->sub_scale_with_window;
        set_use_margins = opts->sub_use_margins;
        set_scale_by_window = opts->sub_scale_by_window;
        total_override = true;
    } else {
        set_scale_with_window = opts->ass_scale_with_window;
        set_use_margins = opts->ass_use_margins;
    }
    if (converted || shared_opts->ass_style_override[sd->order]) {
        set_sub_pos = 100.0f - shared_opts->sub_pos[sd->order];
        set_line_spacing = opts->ass_line_spacing;
        set_hinting = opts->ass_hinting;
    }
    if (total_override || shared_opts->ass_style_override[sd->order] == ASS_STYLE_OVERRIDE_SCALE) {
        set_font_scale = opts->sub_scale;
    }
    if (set_scale_with_window) {
        set_font_scale *= dim->h / MPMAX(get_libass_scale_height(dim, set_use_margins), 1);
    }
    if (!set_scale_by_window) {
        double factor = dim->h / 720.0;
        if (factor != 0.0)
            set_font_scale /= factor;
    }
    ass_set_use_margins(priv, set_use_margins);
    ass_set_line_position(priv, set_sub_pos);
    ass_set_shaper(priv, opts->ass_shaper);
    int set_force_flags = 0;
    if (total_override) {
        set_force_flags |= ASS_OVERRIDE_BIT_FONT_NAME
                            | ASS_OVERRIDE_BIT_FONT_SIZE_FIELDS
                            | ASS_OVERRIDE_BIT_COLORS
                            | ASS_OVERRIDE_BIT_BORDER
                            | ASS_OVERRIDE_BIT_SELECTIVE_FONT_SCALE;
    }
    if (shared_opts->ass_style_override[sd->order] == ASS_STYLE_OVERRIDE_SCALE)
        set_force_flags |= ASS_OVERRIDE_BIT_SELECTIVE_FONT_SCALE;
    if (converted)
        set_force_flags |= ASS_OVERRIDE_BIT_ALIGNMENT;
#if LIBASS_VERSION >= 0x01306000
    if ((converted || shared_opts->ass_style_override[sd->order]) && opts->ass_justify)
        set_force_flags |= ASS_OVERRIDE_BIT_JUSTIFY;
#endif
    ass_set_selective_style_override_enabled(priv, set_force_flags);
    ASS_Style style = {0};
    mp_ass_set_style(&style, track->PlayResY, opts->sub_style);
    ass_set_selective_style_override(priv, &style);
    free(style.FontName);
    if (converted && track->default_style < track->n_styles) {
        mp_ass_set_style(track->styles + track->default_style,
                         track->PlayResY, opts->sub_style);
    }
    ass_set_font_scale(priv, set_font_scale);
    ass_set_hinting(priv, set_hinting);
    ass_set_line_spacing(priv, set_line_spacing);
#if LIBASS_VERSION >= 0x01600010
    if (converted) {
        ass_track_set_feature(track, ASS_FEATURE_WRAP_UNICODE, 1);
        if (!opts->sub_vsfilter_bidi_compat) {
            for (int n = 0; n < track->n_styles; n++) {
                track->styles[n].Encoding = -1;
            }
            ass_track_set_feature(track, ASS_FEATURE_BIDI_BRACKETS, 1);
            ass_track_set_feature(track, ASS_FEATURE_WHOLE_TEXT_LAYOUT, 1);
        }
    }
#endif
    if (converted) {
        bool override_playres = true;
        char **ass_style_override_list = opts->ass_style_override_list;
        for (int i = 0; ass_style_override_list && ass_style_override_list[i]; i++) {
            if (bstr_find0(bstr0(ass_style_override_list[i]), "PlayResX") >= 0)
                override_playres = false;
        }

        // srt to ass conversion from ffmpeg has fixed PlayResX of 384 with an
        // aspect of 4:3. Starting with libass f08f8ea5 (pre 0.17) PlayResX
        // affects shadow and border widths, among others, so to render borders
        // and shadows correctly, we adjust PlayResX according to the DAR.
        // But PlayResX also affects margins, so we adjust those too.
        // This should ensure basic srt-to-ass ffmpeg conversion has correct
        // borders, but there could be other issues with some srt extensions
        // and/or different source formats which would be exposed over time.
        // Make these adjustments only if the user didn't set PlayResX.
        if (override_playres) {
            int vidw = dim->w - (dim->ml + dim->mr);
            int vidh = dim->h - (dim->mt + dim->mb);
            int old_playresx = track->PlayResX;
            track->PlayResX = track->PlayResY * (double)vidw / MPMAX(vidh, 1);
            double fix_margins = track->PlayResX / (double)old_playresx;
            for (int n = 0; n < track->n_styles; n++) {
                track->styles[n].MarginL = lrint(track->styles[n].MarginL * fix_margins);
                track->styles[n].MarginR = lrint(track->styles[n].MarginR * fix_margins);
                track->styles[n].MarginV = lrint(track->styles[n].MarginV * set_font_scale);
            }
        }
    }
}

static bool has_overrides(char *s)
{
    if (!s)
        return false;
    return strstr(s, "\\pos") || strstr(s, "\\move") || strstr(s, "\\clip") ||
           strstr(s, "\\iclip") || strstr(s, "\\org") || strstr(s, "\\p");
}

#define END(ev) ((ev)->Start + (ev)->Duration)

static long long find_timestamp(struct sd *sd, double pts)
{
    struct sd_ass_priv *priv = sd->priv;
    if (pts == MP_NOPTS_VALUE)
        return 0;

    long long ts = llrint(pts * 1000);

    if (!sd->opts->sub_fix_timing ||
        sd->shared_opts->ass_style_override[sd->order] == ASS_STYLE_OVERRIDE_NONE)
        return ts;

    // Try to fix small gaps and overlaps.
    ASS_Track *track = priv->ass_track;
    int threshold = SUB_GAP_THRESHOLD * 1000;
    int keep = SUB_GAP_KEEP * 1000;

    // Find the "current" event.
    ASS_Event *ev[2] = {0};
    int n_ev = 0;
    for (int n = 0; n < track->n_events; n++) {
        ASS_Event *event = &track->events[n];
        if (ts >= event->Start - threshold && ts <= END(event) + threshold) {
            if (n_ev >= MP_ARRAY_SIZE(ev))
                return ts; // multiple overlaps - give up (probably complex subs)
            ev[n_ev++] = event;
        }
    }

    if (n_ev != 2)
        return ts;

    // Simple/minor heuristic against destroying typesetting.
    if (ev[0]->Style != ev[1]->Style || has_overrides(ev[0]->Text) ||
        has_overrides(ev[1]->Text))
        return ts;

    // Sort by start timestamps.
    if (ev[0]->Start > ev[1]->Start)
        MPSWAP(ASS_Event*, ev[0], ev[1]);

    // We want to fix partial overlaps only.
    if (END(ev[0]) >= END(ev[1]))
        return ts;

    if (ev[0]->Duration < keep || ev[1]->Duration < keep)
        return ts;

    // Gap between the events -> move ts to show the end of the first event.
    if (ts >= END(ev[0]) && ts < ev[1]->Start && END(ev[0]) < ev[1]->Start &&
        END(ev[0]) + threshold >= ev[1]->Start)
        return END(ev[0]) - 1;

    // Overlap -> move ts to the (exclusive) end of the first event.
    // Relies on the fact that the ASS_Renderer has no overlap registered, even
    // if there is one. This happens to work because we never render the
    // overlapped state, and libass never resolves a collision.
    if (ts >= ev[1]->Start && ts <= END(ev[0]) && END(ev[0]) > ev[1]->Start &&
        END(ev[0]) <= ev[1]->Start + threshold)
        return END(ev[0]);

    return ts;
}

#undef END

static struct sub_bitmaps *get_bitmaps(struct sd *sd, struct mp_osd_res dim,
                                       int format, double pts)
{
    struct sd_ass_priv *ctx = sd->priv;
    struct mp_subtitle_opts *opts = sd->opts;
    struct mp_subtitle_shared_opts *shared_opts = sd->shared_opts;
    bool no_ass = !opts->ass_enabled ||
        shared_opts->ass_style_override[sd->order] == ASS_STYLE_OVERRIDE_STRIP;
    bool converted = (ctx->is_converted && !lavc_conv_is_styled(ctx->converter)) || no_ass;
    ASS_Track *track = no_ass ? ctx->shadow_track : ctx->ass_track;
    ASS_Renderer *renderer = ctx->ass_renderer;
    struct sub_bitmaps *res = &(struct sub_bitmaps){0};

    // Always update the osd_res
    struct mp_osd_res old_osd = ctx->osd;
    ctx->osd = dim;

    if (pts == MP_NOPTS_VALUE || !renderer)
        goto done;

    // Currently no supported text sub formats support a distinction between forced
    // and unforced lines, so we just assume everything's unforced and discard everything.
    // If we ever see a format that makes this distinction, we can add support here.
    if (opts->sub_forced_events_only)
        goto done;

    double scale = dim.display_par;
    if (!converted && (!shared_opts->ass_style_override[sd->order] ||
                       opts->ass_use_video_data >= 1))
    {
        // Let's factor in video PAR for vsfilter compatibility:
        double par = opts->ass_video_aspect > 0 ?
                opts->ass_video_aspect :
                ctx->video_params.p_w / (double)ctx->video_params.p_h;
        if (isnormal(par))
            scale *= par;
    }
    if (!ctx->ass_configured || !osd_res_equals(old_osd, ctx->osd)) {
        configure_ass(sd, &dim, converted, track);
        ctx->ass_configured = true;
    }
    ass_set_pixel_aspect(renderer, scale);
    if (!converted && (!shared_opts->ass_style_override[sd->order] ||
                       opts->ass_use_video_data >= 2))
    {
        ass_set_storage_size(renderer, ctx->video_params.w, ctx->video_params.h);
    } else {
        ass_set_storage_size(renderer, 0, 0);
    }
    long long ts = find_timestamp(sd, pts);

    if (no_ass)
        fill_plaintext(sd, pts);

    int changed;
    ASS_Image *imgs = ass_render_frame(renderer, track, ts, &changed);
    mp_ass_packer_pack(ctx->packer, &imgs, 1, changed, !converted, format, res);

done:
    // mangle_colors() modifies the color field, so copy the thing _before_.
    res = sub_bitmaps_copy(&ctx->copy_cache, res);

    if (!converted && res)
        mangle_colors(sd, res);

    return res;
}

#define MAX_BUF_SIZE 1024 * 1024
#define MIN_EXPAND_SIZE 4096

static void append(bstr *b, char c)
{
    bstr_xappend(NULL, b, (bstr){&c, 1});
}

static void ass_to_plaintext(bstr *b, const char *in)
{
    const char *open_tag_pos = NULL;
    bool in_drawing = false;
    while (*in) {
        if (open_tag_pos) {
            if (in[0] == '}') {
                in += 1;
                open_tag_pos = NULL;
            } else if (in[0] == '\\' && in[1] == 'p' && in[2] != 'o') {
                in += 2;
                // Skip text between \pN and \p0 tags. A \p without a number
                // is the same as \p0, and leading 0s are also allowed.
                in_drawing = false;
                while (in[0] >= '0' && in[0] <= '9') {
                    if (in[0] != '0')
                        in_drawing = true;
                    in += 1;
                }
            } else {
                in += 1;
            }
        } else {
            if (in[0] == '\\' && (in[1] == 'N' || in[1] == 'n')) {
                in += 2;
                append(b, '\n');
            } else if (in[0] == '\\' && in[1] == 'h') {
                in += 2;
                append(b, ' ');
            } else if (in[0] == '{') {
                open_tag_pos = in;
                in += 1;
            } else {
                if (!in_drawing)
                    append(b, in[0]);
                in += 1;
            }
        }
    }
    // A '{' without a closing '}' is always visible.
    if (open_tag_pos) {
        bstr_xappend(NULL, b, bstr0(open_tag_pos));
    }
}

// Empty string counts as whitespace.
static bool is_whitespace_only(bstr b)
{
    for (int n = 0; n < b.len; n++) {
        if (b.start[n] != ' ' && b.start[n] != '\t')
            return false;
    }
    return true;
}

static bstr get_text_buf(struct sd *sd, double pts, enum sd_text_type type)
{
    struct sd_ass_priv *ctx = sd->priv;
    ASS_Track *track = ctx->ass_track;

    if (pts == MP_NOPTS_VALUE)
        return (bstr){0};
    long long ipts = find_timestamp(sd, pts);

    bstr *b = &ctx->last_text;

    if (!b->start)
        b->start = talloc_size(ctx, 4096);

    b->len = 0;

    for (int i = 0; i < track->n_events; ++i) {
        ASS_Event *event = track->events + i;
        if (ipts >= event->Start && ipts < event->Start + event->Duration) {
            if (event->Text) {
                int start = b->len;
                if (type == SD_TEXT_TYPE_PLAIN) {
                    ass_to_plaintext(b, event->Text);
                } else if (type == SD_TEXT_TYPE_ASS_FULL) {
                    long long s = event->Start;
                    long long e = s + event->Duration;

                    ASS_Style *style = (event->Style < 0 || event->Style >= track->n_styles) ? NULL : &track->styles[event->Style];

                    int sh = (s / 60 / 60 / 1000);
                    int sm = (s / 60 / 1000) % 60;
                    int ss = (s / 1000) % 60;
                    int sc = (s / 10) % 100;
                    int eh = (e / 60 / 60 / 1000);
                    int em = (e / 60 / 1000) % 60;
                    int es = (e / 1000) % 60;
                    int ec = (e / 10) % 100;

                    bstr_xappend_asprintf(NULL, b, "Dialogue: %d,%d:%02d:%02d.%02d,%d:%02d:%02d.%02d,%s,%s,%04d,%04d,%04d,%s,%s",
                        event->Layer,
                        sh, sm, ss, sc,
                        eh, em, es, ec,
                        (style && style->Name) ? style->Name : "", event->Name,
                        event->MarginL, event->MarginR, event->MarginV,
                        event->Effect, event->Text);
                } else {
                    bstr_xappend(NULL, b, bstr0(event->Text));
                }
                if (is_whitespace_only(bstr_cut(*b, start))) {
                    b->len = start;
                } else {
                    append(b, '\n');
                }
            }
        }
    }

    bstr_eatend(b, (bstr)bstr0_lit("\n"));

    return *b;
}

static char *get_text(struct sd *sd, double pts, enum sd_text_type type)
{
    return bstrto0(NULL, get_text_buf(sd, pts, type));
}

static struct sd_times get_times(struct sd *sd, double pts)
{
    struct sd_ass_priv *ctx = sd->priv;
    ASS_Track *track = ctx->ass_track;
    struct sd_times res = { .start = MP_NOPTS_VALUE, .end = MP_NOPTS_VALUE };

    if (pts == MP_NOPTS_VALUE)
        return res;

    long long ipts = find_timestamp(sd, pts);

    for (int i = 0; i < track->n_events; ++i) {
        ASS_Event *event = track->events + i;
        if (ipts >= event->Start && ipts < event->Start + event->Duration) {
            double start = event->Start / 1000.0;
            double end = event->Duration == UNKNOWN_DURATION ?
                MP_NOPTS_VALUE : (event->Start + event->Duration) / 1000.0;

            if (res.start == MP_NOPTS_VALUE || res.start > start)
                res.start = start;

            if (res.end == MP_NOPTS_VALUE || res.end < end)
                res.end = end;
        }
    }

    return res;
}

static void fill_plaintext(struct sd *sd, double pts)
{
    struct sd_ass_priv *ctx = sd->priv;
    ASS_Track *track = ctx->shadow_track;

    ass_flush_events(track);

    bstr text = get_text_buf(sd, pts, SD_TEXT_TYPE_PLAIN);
    if (!text.len)
        return;

    bstr dst = {0};

    while (text.len) {
        if (*text.start == '{') {
            bstr_xappend(NULL, &dst, bstr0("\\{"));
            text = bstr_cut(text, 1);
        } else if (*text.start == '\\') {
            bstr_xappend(NULL, &dst, bstr0("\\"));
            // Break ASS escapes with U+2060 WORD JOINER
            mp_append_utf8_bstr(NULL, &dst, 0x2060);
            text = bstr_cut(text, 1);
        }

        int i = bstrcspn(text, "{\\");
        bstr_xappend(NULL, &dst, (bstr){text.start, i});
        text = bstr_cut(text, i);
    }

    if (!dst.start)
        return;

    int n = ass_alloc_event(track);
    ASS_Event *event = track->events + n;
    event->Start = 0;
    event->Duration = INT_MAX;
    event->Style = track->default_style;
    event->Text = strdup(dst.start);

    talloc_free(dst.start);
}

static void reset(struct sd *sd)
{
    struct sd_ass_priv *ctx = sd->priv;
    if (sd->opts->sub_clear_on_seek || ctx->clear_once) {
        ass_flush_events(ctx->ass_track);
        ctx->num_seen_packets = 0;
        sd->preload_ok = false;
        ctx->clear_once = false;
    }
    if (ctx->converter)
        lavc_conv_reset(ctx->converter);
}

static void uninit(struct sd *sd)
{
    struct sd_ass_priv *ctx = sd->priv;

    filters_destroy(sd);
    if (ctx->converter)
        lavc_conv_uninit(ctx->converter);
    assobjects_destroy(sd);
    talloc_free(ctx->copy_cache);
}

static int control(struct sd *sd, enum sd_ctrl cmd, void *arg)
{
    struct sd_ass_priv *ctx = sd->priv;
    switch (cmd) {
    case SD_CTRL_SUB_STEP: {
        double *a = arg;
        long long ts = llrint(a[0] * 1000.0);
        long long res = ass_step_sub(ctx->ass_track, ts, a[1]);
        if (!res)
            return false;
        // Try to account for overlapping durations
        a[0] += res / 1000.0 + SUB_SEEK_OFFSET;
        return true;
    }
    case SD_CTRL_SET_ANIMATED_CHECK:
        ctx->check_animated = *(bool *)arg;
        return CONTROL_OK;
    case SD_CTRL_SET_VIDEO_PARAMS:
        ctx->video_params = *(struct mp_image_params *)arg;
        return CONTROL_OK;
    case SD_CTRL_UPDATE_OPTS: {
        int flags = (uintptr_t)arg;
        if (flags & UPDATE_SUB_FILT) {
            filters_destroy(sd);
            filters_init(sd);
            ctx->clear_once = true; // allow reloading on seeks
            reset(sd);
        }
        if (flags & UPDATE_SUB_HARD) {
            // ass_track will be recreated, so clear duplicate cache
            ctx->clear_once = true;
            reset(sd);
            assobjects_destroy(sd);
            assobjects_init(sd);
        }
        ctx->ass_configured = false; // ass always needs to be reconfigured
        return CONTROL_OK;
    }
    default:
        return CONTROL_UNKNOWN;
    }
}

const struct sd_functions sd_ass = {
    .name = "ass",
    .accept_packets_in_advance = true,
    .init = init,
    .decode = decode,
    .get_bitmaps = get_bitmaps,
    .get_text = get_text,
    .get_times = get_times,
    .control = control,
    .reset = reset,
    .select = enable_output,
    .uninit = uninit,
};

// Disgusting hack for (xy-)vsfilter color compatibility.
static void mangle_colors(struct sd *sd, struct sub_bitmaps *parts)
{
    struct mp_subtitle_opts *opts = sd->opts;
    struct sd_ass_priv *ctx = sd->priv;
    enum pl_color_system csp = 0;
    enum pl_color_levels levels = 0;
    if (opts->ass_vsfilter_color_compat == 0) // "no"
        return;
    bool force_601 = opts->ass_vsfilter_color_compat == 3;
    ASS_Track *track = ctx->ass_track;
    static const int ass_csp[] = {
        [YCBCR_BT601_TV]        = PL_COLOR_SYSTEM_BT_601,
        [YCBCR_BT601_PC]        = PL_COLOR_SYSTEM_BT_601,
        [YCBCR_BT709_TV]        = PL_COLOR_SYSTEM_BT_709,
        [YCBCR_BT709_PC]        = PL_COLOR_SYSTEM_BT_709,
        [YCBCR_SMPTE240M_TV]    = PL_COLOR_SYSTEM_SMPTE_240M,
        [YCBCR_SMPTE240M_PC]    = PL_COLOR_SYSTEM_SMPTE_240M,
    };
    static const int ass_levels[] = {
        [YCBCR_BT601_TV]        = PL_COLOR_LEVELS_LIMITED,
        [YCBCR_BT601_PC]        = PL_COLOR_LEVELS_FULL,
        [YCBCR_BT709_TV]        = PL_COLOR_LEVELS_LIMITED,
        [YCBCR_BT709_PC]        = PL_COLOR_LEVELS_FULL,
        [YCBCR_SMPTE240M_TV]    = PL_COLOR_LEVELS_LIMITED,
        [YCBCR_SMPTE240M_PC]    = PL_COLOR_LEVELS_FULL,
    };
    int trackcsp = track->YCbCrMatrix;
    if (force_601)
        trackcsp = YCBCR_BT601_TV;
    // NONE is a bit random, but the intention is: don't modify colors.
    if (trackcsp == YCBCR_NONE)
        return;
    if (trackcsp < sizeof(ass_csp) / sizeof(ass_csp[0]))
        csp = ass_csp[trackcsp];
    if (trackcsp < sizeof(ass_levels) / sizeof(ass_levels[0]))
        levels = ass_levels[trackcsp];
    if (trackcsp == YCBCR_DEFAULT) {
        csp = PL_COLOR_SYSTEM_BT_601;
        levels = PL_COLOR_LEVELS_LIMITED;
    }
    // Unknown colorspace (either YCBCR_UNKNOWN, or a valid value unknown to us)
    if (!csp || !levels)
        return;

    struct mp_image_params params = ctx->video_params;

    if (force_601) {
        params.repr = (struct pl_color_repr){
            .sys = PL_COLOR_SYSTEM_BT_709,
            .levels = PL_COLOR_LEVELS_LIMITED,
        };
    }

    if ((csp == params.repr.sys && levels == params.repr.levels) ||
            params.repr.sys == PL_COLOR_SYSTEM_RGB) // Even VSFilter doesn't mangle on RGB video
        return;

    bool basic_conv = params.repr.sys == PL_COLOR_SYSTEM_BT_709 &&
                      params.repr.levels == PL_COLOR_LEVELS_LIMITED &&
                      csp == PL_COLOR_SYSTEM_BT_601 &&
                      levels == PL_COLOR_LEVELS_LIMITED;

    // With "basic", only do as much as needed for basic compatibility.
    if (opts->ass_vsfilter_color_compat == 1 && !basic_conv)
        return;

    if (params.repr.sys != ctx->last_params.repr.sys ||
        params.repr.levels != ctx->last_params.repr.levels)
    {
        int msgl = basic_conv ? MSGL_V : MSGL_WARN;
        ctx->last_params = params;
        MP_MSG(sd, msgl, "mangling colors like vsfilter: "
               "RGB -> %s %s -> %s %s -> RGB\n",
               m_opt_choice_str(pl_csp_names, csp),
               m_opt_choice_str(pl_csp_levels_names, levels),
               m_opt_choice_str(pl_csp_names, params.repr.sys),
               m_opt_choice_str(pl_csp_names, params.repr.levels));
    }

    // Conversion that VSFilter would use
    struct mp_csp_params vs_params = MP_CSP_PARAMS_DEFAULTS;
    vs_params.repr.sys = csp;
    vs_params.repr.levels = levels;
    struct pl_transform3x3 vs_yuv2rgb;
    mp_get_csp_matrix(&vs_params, &vs_yuv2rgb);
    pl_transform3x3_invert(&vs_yuv2rgb);

    // Proper conversion to RGB
    struct mp_csp_params rgb_params = MP_CSP_PARAMS_DEFAULTS;
    rgb_params.repr = params.repr;
    rgb_params.color = params.color;
    struct pl_transform3x3 vs2rgb;
    mp_get_csp_matrix(&rgb_params, &vs2rgb);

    for (int n = 0; n < parts->num_parts; n++) {
        struct sub_bitmap *sb = &parts->parts[n];
        uint32_t color = sb->libass.color;
        int r = (color >> 24u) & 0xff;
        int g = (color >> 16u) & 0xff;
        int b = (color >>  8u) & 0xff;
        int a = 0xff - (color & 0xff);
        int rgb[3] = {r, g, b}, yuv[3];
        mp_map_fixp_color(&vs_yuv2rgb, 8, rgb, 8, yuv);
        mp_map_fixp_color(&vs2rgb, 8, yuv, 8, rgb);
        sb->libass.color = MP_ASS_RGBA(rgb[0], rgb[1], rgb[2], a);
    }
}

int sd_ass_fmt_offset(const char *evt_fmt)
{
    // "Text" is always last (as it's arbitrary content in buf), e.g. format:
    // "Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text"
    int n = 0;
    while (evt_fmt && (evt_fmt = strchr(evt_fmt, ',')))
         evt_fmt++, n++;
    return n-1;  // buffer is without the format's Start/End, with ReadOrder
}

bstr sd_ass_pkt_text(struct sd_filter *ft, struct demux_packet *pkt, int offset)
{
    // e.g. pkt->buffer ("4" is ReadOrder): "4,0,Default,,0,0,0,,fifth line"
    bstr txt = {(char *)pkt->buffer, pkt->len}, t0 = txt;
    while (offset-- > 0) {
        int n = bstrchr(txt, ',');
        if (n < 0) {  // shouldn't happen
            MP_WARN(ft, "Malformed event '%.*s'\n", BSTR_P(t0));
            return (bstr){NULL, 0};
        }
        txt = bstr_cut(txt, n+1);
    }
    return txt;
}

bstr sd_ass_to_plaintext(char **out, const char *in)
{
    bstr b = {*out};
    ass_to_plaintext(&b, in);
    *out = b.start;
    return b;
}
