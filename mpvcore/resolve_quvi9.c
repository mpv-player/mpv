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

#include <stdbool.h>
#include <assert.h>

#include <quvi.h>

#include "talloc.h"
#include "mpvcore/mp_msg.h"
#include "mpvcore/options.h"
#include "mpvcore/playlist.h"
#include "resolve.h"

static bool mp_quvi_ok(quvi_t q)
{
    if (!quvi_ok(q)) {
        mp_msg(MSGT_OPEN, MSGL_ERR, "[quvi] %s\n", quvi_errmsg(q));
        return false;
    }
    return true;
}

struct mp_resolve_result *mp_resolve_quvi(const char *url, struct MPOpts *opts)
{
    int mode = QUVI_SUPPORTS_MODE_OFFLINE;

    quvi_t q = quvi_new();
    if (!quvi_ok(q)) {
        mp_msg(MSGT_OPEN, MSGL_ERR, "[quvi] %s\n", quvi_errmsg(q));

        quvi_free(q);
        return NULL;
    }

    struct mp_resolve_result *res = talloc_zero(NULL, struct mp_resolve_result);

    if (quvi_supports(q, url, mode, QUVI_SUPPORTS_TYPE_PLAYLIST)) {
        mp_msg(MSGT_OPEN, MSGL_INFO, "[quvi] Checking playlist...\n");
        quvi_playlist_t qp = quvi_playlist_new(q, url);
        if (mp_quvi_ok(q)) {
            res->playlist = talloc_zero(res, struct playlist);
            while (quvi_playlist_media_next(qp)) {
                char *entry = NULL;
                quvi_playlist_get(qp, QUVI_PLAYLIST_MEDIA_PROPERTY_URL, &entry);
                if (entry)
                    playlist_add_file(res->playlist, entry);
            }
        }
        quvi_playlist_free(qp);
    }

    if (quvi_supports(q, url, mode, QUVI_SUPPORTS_TYPE_MEDIA)) {
        mp_msg(MSGT_OPEN, MSGL_INFO, "[quvi] Checking URL...\n");
        quvi_media_t media = quvi_media_new(q, url);
        if (mp_quvi_ok(q)) {
            char *format = opts->quvi_format ? opts->quvi_format : "best";
            bool use_default = strcmp(format, "default") == 0;
            if (!use_default)
                quvi_media_stream_select(media, format);

            char *val = NULL;
            quvi_media_get(media, QUVI_MEDIA_STREAM_PROPERTY_URL, &val);
            res->url = talloc_strdup(res, val);

            val = NULL;
            quvi_media_get(media, QUVI_MEDIA_PROPERTY_TITLE, &val);
            res->title = talloc_strdup(res, val);

            double start = 0;
            quvi_media_get(media, QUVI_MEDIA_PROPERTY_START_TIME_MS, &start);
            res->start_time = start / 1000.0;

            quvi_media_stream_reset(media);
            while (quvi_media_stream_next(media)) {
                char *entry = NULL, *id = NULL;
                quvi_media_get(media, QUVI_MEDIA_STREAM_PROPERTY_URL, &entry);
                quvi_media_get(media, QUVI_MEDIA_STREAM_PROPERTY_ID, &id);
                if (entry) {
                    struct mp_resolve_src *src = talloc_ptrtype(res, src);
                    *src = (struct mp_resolve_src) {
                        .url = talloc_strdup(src, entry),
                        .encid = talloc_strdup(src, id),
                    };
                    MP_TARRAY_APPEND(res, res->srcs, res->num_srcs, src);
                    talloc_steal(res->srcs, src);
                }
            }

        }
        quvi_media_free(media);
    }

    if (quvi_supports(q, url, mode, QUVI_SUPPORTS_TYPE_SUBTITLE)) {
        mp_msg(MSGT_OPEN, MSGL_INFO, "[quvi] Getting subtitles...\n");
        quvi_subtitle_t qsub = quvi_subtitle_new(q, url);
        if (mp_quvi_ok(q)) {
            while (1) {
                quvi_subtitle_type_t qst = quvi_subtitle_type_next(qsub);
                if (!qst)
                    break;
                while (1) {
                    quvi_subtitle_lang_t qsl = quvi_subtitle_lang_next(qst);
                    if (!qsl)
                        break;
                    char *lang;
                    quvi_subtitle_lang_get(qsl, QUVI_SUBTITLE_LANG_PROPERTY_ID,
                                           &lang);
                    // Let quvi convert the subtitle to SRT.
                    quvi_subtitle_export_t qse =
                        quvi_subtitle_export_new(qsl, "srt");
                    if (mp_quvi_ok(q)) {
                        const char *subdata = quvi_subtitle_export_data(qse);
                        struct mp_resolve_sub *sub = talloc_ptrtype(res, sub);
                        *sub = (struct mp_resolve_sub) {
                            .lang = talloc_strdup(sub, lang),
                            .data = talloc_strdup(sub, subdata),
                        };
                        MP_TARRAY_APPEND(res, res->subs, res->num_subs, sub);
                        talloc_steal(res->subs, sub);
                    }
                    quvi_subtitle_export_free(qse);
                }
            }
        }
        quvi_subtitle_free(qsub);
    }

    quvi_free(q);

    if (!res->url && (!res->playlist || !res->playlist->first)) {
        talloc_free(res);
        res = NULL;
    }
    return res;
}
