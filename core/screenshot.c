/*
 * This file is part of mplayer2.
 *
 * mplayer2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer2; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "config.h"

#include "osdep/io.h"

#include "talloc.h"
#include "core/screenshot.h"
#include "core/mp_core.h"
#include "core/command.h"
#include "core/bstr.h"
#include "core/mp_msg.h"
#include "core/mp_osd.h"
#include "core/path.h"
#include "video/mp_image.h"
#include "video/decode/dec_video.h"
#include "video/filter/vf.h"
#include "video/out/vo.h"
#include "video/image_writer.h"
#include "sub/sub.h"

#include "video/csputils.h"

#define MODE_FULL_WINDOW 1
#define MODE_SUBTITLES 2

typedef struct screenshot_ctx {
    struct MPContext *mpctx;

    int mode;
    bool each_frame;
    bool osd;

    int frameno;
} screenshot_ctx;

void screenshot_init(struct MPContext *mpctx)
{
    mpctx->screenshot_ctx = talloc(mpctx, screenshot_ctx);
    *mpctx->screenshot_ctx = (screenshot_ctx) {
        .mpctx = mpctx,
        .frameno = 1,
    };
}

#define SMSG_OK 0
#define SMSG_ERR 1

static void screenshot_msg(screenshot_ctx *ctx, int status, const char *msg,
                           ...) PRINTF_ATTRIBUTE(3,4);

static void screenshot_msg(screenshot_ctx *ctx, int status, const char *msg,
                           ...)
{
    va_list ap;
    char *s;

    va_start(ap, msg);
    s = talloc_vasprintf(NULL, msg, ap);
    va_end(ap);

    mp_msg(MSGT_CPLAYER, status == SMSG_ERR ? MSGL_ERR : MSGL_INFO, "%s\n", s);
    if (ctx->osd) {
        set_osd_tmsg(ctx->mpctx, OSD_MSG_TEXT, 1, ctx->mpctx->opts.osd_duration,
                     "%s", s);
    }

    talloc_free(s);
}

static char *stripext(void *talloc_ctx, const char *s)
{
    const char *end = strrchr(s, '.');
    if (!end)
        end = s + strlen(s);
    return talloc_asprintf(talloc_ctx, "%.*s", (int)(end - s), s);
}

#ifdef _WIN32
#define ILLEGAL_FILENAME_CHARS "?\"/\\<>*|:"
#else
#define ILLEGAL_FILENAME_CHARS "/"
#endif

// Replace all characters disallowed in filenames with '_' and return the newly
// allocated result string.
static char *sanitize_filename(void *talloc_ctx, const char *s)
{
    char *res = talloc_strdup(talloc_ctx, s);
    char *cur = res;
    while (*cur) {
        if (strchr(ILLEGAL_FILENAME_CHARS, *cur) || ((unsigned char)*cur) < 32)
            *cur = '_';
        cur++;
    }
    return res;
}

static void append_filename(char **s, const char *f)
{
    char *append = sanitize_filename(NULL, f);
    *s = talloc_strdup_append(*s, append);
    talloc_free(append);
}

static char *create_fname(struct MPContext *mpctx, char *template,
                          const char *file_ext, int *sequence, int *frameno)
{
    char *res = talloc_strdup(NULL, ""); //empty string, non-NULL context

    time_t raw_time = time(NULL);
    struct tm *local_time = localtime(&raw_time);

    if (!template || *template == '\0')
        template = "shot%n";

    for (;;) {
        char *next = strchr(template, '%');
        if (!next)
            break;
        res = talloc_strndup_append(res, template, next - template);
        template = next + 1;
        char fmt = *template++;
        switch (fmt) {
        case '#':
        case '0':
        case 'n': {
            int digits = '4';
            if (fmt == '#') {
                if (!*sequence) {
                    *frameno = 1;
                }
                fmt = *template++;
            }
            if (fmt == '0') {
                digits = *template++;
                if (digits < '0' || digits > '9')
                    goto error_exit;
                fmt = *template++;
            }
            if (fmt != 'n')
                goto error_exit;
            char fmtstr[] = {'%', '0', digits, 'd', '\0'};
            res = talloc_asprintf_append(res, fmtstr, *frameno);
            if (*frameno < 100000 - 1) {
                (*frameno) += 1;
                (*sequence) += 1;
            }
            break;
        }
        case 'f':
        case 'F': {
            char *video_file = mp_basename(mpctx->filename);
            if (video_file) {
                char *name = video_file;
                if (fmt == 'F')
                    name = stripext(res, video_file);
                append_filename(&res, name);
            }
            break;
        }
        case 'p':
        case 'P': {
            char *t = mp_format_time(get_current_time(mpctx), fmt == 'P');
            append_filename(&res, t);
            talloc_free(t);
            break;
        }
        case 't': {
            char fmt = *template;
            if (!fmt)
                goto error_exit;
            template++;
            char fmtstr[] = {'%', fmt, '\0'};
            char buffer[80];
            if (strftime(buffer, sizeof(buffer), fmtstr, local_time) == 0)
                buffer[0] = '\0';
            append_filename(&res, buffer);
            break;
        }
        case '{': {
            char *end = strchr(template, '}');
            if (!end)
                goto error_exit;
            struct bstr prop = bstr_splice(bstr0(template), 0, end - template);
            char *tmp = talloc_asprintf(NULL, "${%.*s}", BSTR_P(prop));
            char *s = mp_property_expand_string(mpctx, tmp);
            talloc_free(tmp);
            if (s)
                append_filename(&res, s);
            talloc_free(s);
            template = end + 1;
            break;
        }
        case '%':
            res = talloc_strdup_append(res, "%");
            break;
        default:
            goto error_exit;
        }
    }

    res = talloc_strdup_append(res, template);
    return talloc_asprintf_append(res, ".%s", file_ext);

error_exit:
    talloc_free(res);
    return NULL;
}

static char *gen_fname(screenshot_ctx *ctx, const char *file_ext)
{
    int sequence = 0;
    for (;;) {
        int prev_sequence = sequence;
        char *fname = create_fname(ctx->mpctx,
                                   ctx->mpctx->opts.screenshot_template,
                                   file_ext,
                                   &sequence,
                                   &ctx->frameno);

        if (!fname) {
            screenshot_msg(ctx, SMSG_ERR, "Invalid screenshot filename "
                           "template! Fix or remove the --screenshot-template "
                           "option.");
            return NULL;
        }

        if (!mp_path_exists(fname))
            return fname;

        if (sequence == prev_sequence) {
            screenshot_msg(ctx, SMSG_ERR, "Can't save screenshot, file '%s' "
                           "already exists!", fname);
            talloc_free(fname);
            return NULL;
        }

        talloc_free(fname);
    }
}

static void add_subs(struct MPContext *mpctx, struct mp_image *image)
{
    int d_w = image->display_w ? image->display_w : image->w;
    int d_h = image->display_h ? image->display_h : image->h;

    double sar = (double)image->w / image->h;
    double dar = (double)d_w / d_h;
    struct mp_osd_res res = {
        .w = image->w,
        .h = image->h,
        .display_par = sar / dar,
        .video_par = dar / sar,
    };

    osd_draw_on_image(mpctx->osd, res, mpctx->osd->vo_pts,
                      OSD_DRAW_SUB_ONLY, image);
}

static void screenshot_save(struct MPContext *mpctx, struct mp_image *image,
                            bool with_subs)
{
    screenshot_ctx *ctx = mpctx->screenshot_ctx;

    struct image_writer_opts *opts = mpctx->opts.screenshot_image_opts;

    if (with_subs)
        add_subs(mpctx, image);

    char *filename = gen_fname(ctx, image_writer_file_ext(opts));
    if (filename) {
        screenshot_msg(ctx, SMSG_OK, "Screenshot: '%s'", filename);
        if (!write_image(image, opts, filename))
            screenshot_msg(ctx, SMSG_ERR, "Error writing screenshot!");
        talloc_free(filename);
    }

    talloc_free(image);
}

void screenshot_request(struct MPContext *mpctx, int mode, bool each_frame,
                        bool osd)
{
    if (mpctx->video_out && mpctx->video_out->config_ok) {
        screenshot_ctx *ctx = mpctx->screenshot_ctx;

        if (mode == MODE_SUBTITLES && mpctx->osd->render_subs_in_filter)
            mode = 0;

        if (each_frame) {
            ctx->each_frame = !ctx->each_frame;
            if (!ctx->each_frame)
                return;
        } else {
            ctx->each_frame = false;
        }

        ctx->mode = mode;
        ctx->osd = osd;

        struct voctrl_screenshot_args args =
                            { .full_window = (mode == MODE_FULL_WINDOW) };

        struct vf_instance *vfilter = mpctx->sh_video->vfilter;
        vfilter->control(vfilter, VFCTRL_SCREENSHOT, &args);

        if (!args.out_image)
            vo_control(mpctx->video_out, VOCTRL_SCREENSHOT, &args);

        if (args.out_image) {
            if (args.has_osd)
                mode = 0;
            screenshot_save(mpctx, args.out_image, mode == MODE_SUBTITLES);
        } else {
            screenshot_msg(ctx, SMSG_ERR, "Taking screenshot failed.");
        }
    }
}

void screenshot_flip(struct MPContext *mpctx)
{
    screenshot_ctx *ctx = mpctx->screenshot_ctx;

    if (!ctx->each_frame)
        return;

    ctx->each_frame = false;
    screenshot_request(mpctx, ctx->mode, true, ctx->osd);
}
