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
#include <string.h>
#include <time.h>

#include "config.h"

#include "osdep/io.h"

#include "mpv_talloc.h"
#include "screenshot.h"
#include "core.h"
#include "command.h"
#include "input/cmd.h"
#include "misc/bstr.h"
#include "misc/dispatch.h"
#include "misc/node.h"
#include "misc/thread_tools.h"
#include "common/msg.h"
#include "options/path.h"
#include "video/mp_image.h"
#include "video/mp_image_pool.h"
#include "video/out/vo.h"
#include "video/image_writer.h"
#include "video/sws_utils.h"
#include "sub/osd.h"

#include "video/csputils.h"

#define MODE_FULL_WINDOW 1
#define MODE_SUBTITLES 2

typedef struct screenshot_ctx {
    struct MPContext *mpctx;

    // Command to repeat in each-frame mode.
    struct mp_cmd *each_frame;

    int frameno;
    uint64_t last_frame_count;
} screenshot_ctx;

void screenshot_init(struct MPContext *mpctx)
{
    mpctx->screenshot_ctx = talloc(mpctx, screenshot_ctx);
    *mpctx->screenshot_ctx = (screenshot_ctx) {
        .mpctx = mpctx,
        .frameno = 1,
    };
}

static char *stripext(void *talloc_ctx, const char *s)
{
    const char *end = strrchr(s, '.');
    if (!end)
        end = s + strlen(s);
    return talloc_asprintf(talloc_ctx, "%.*s", (int)(end - s), s);
}

static bool write_screenshot(struct mp_cmd_ctx *cmd, struct mp_image *img,
                             const char *filename, struct image_writer_opts *opts)
{
    struct MPContext *mpctx = cmd->mpctx;
    struct image_writer_opts *gopts = mpctx->opts->screenshot_image_opts;
    struct image_writer_opts opts_copy = opts ? *opts : *gopts;

    mp_cmd_msg(cmd, MSGL_V, "Starting screenshot: '%s'", filename);

    mp_core_unlock(mpctx);

    bool ok = img && write_image(img, &opts_copy, filename, mpctx->global,
                                 mpctx->log);

    mp_core_lock(mpctx);

    if (ok) {
        mp_cmd_msg(cmd, MSGL_INFO, "Screenshot: '%s'", filename);
    } else {
        mp_cmd_msg(cmd, MSGL_ERR, "Error writing screenshot!");
    }
    return ok;
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
        return NULL;

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
            char *video_file = NULL;
            if (mpctx->filename)
                video_file = mp_basename(mpctx->filename);

            if (!video_file)
                video_file = "NO_FILE";

            char *name = video_file;
            if (fmt == 'F')
                name = stripext(res, video_file);
            append_filename(&res, name);
            break;
        }
        case 'x':
        case 'X': {
            char *fallback = "";
            if (fmt == 'X') {
                if (template[0] != '{')
                    goto error_exit;
                char *end = strchr(template, '}');
                if (!end)
                    goto error_exit;
                fallback = talloc_strndup(res, template + 1, end - template - 1);
                template = end + 1;
            }
            if (!mpctx->filename || mp_is_url(bstr0(mpctx->filename))) {
                res = talloc_strdup_append(res, fallback);
            } else {
                bstr dir = mp_dirname(mpctx->filename);
                if (!bstr_equals0(dir, "."))
                    res = talloc_asprintf_append(res, "%.*s", BSTR_P(dir));
            }
            break;
        }
        case 'p':
        case 'P': {
            char *t = mp_format_time(get_playback_time(mpctx), fmt == 'P');
            append_filename(&res, t);
            talloc_free(t);
            break;
        }
        case 'w': {
            char tfmt = *template;
            if (!tfmt)
                goto error_exit;
            template++;
            char fmtstr[] = {'%', tfmt, '\0'};
            char *s = mp_format_time_fmt(fmtstr, get_playback_time(mpctx));
            if (!s)
                goto error_exit;
            append_filename(&res, s);
            talloc_free(s);
            break;
        }
        case 't': {
            char tfmt = *template;
            if (!tfmt)
                goto error_exit;
            template++;
            char fmtstr[] = {'%', tfmt, '\0'};
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
    res = talloc_asprintf_append(res, ".%s", file_ext);
    char *fname = mp_get_user_path(NULL, mpctx->global, res);
    talloc_free(res);
    return fname;

error_exit:
    talloc_free(res);
    return NULL;
}

static char *gen_fname(struct mp_cmd_ctx *cmd, const char *file_ext)
{
    struct MPContext *mpctx = cmd->mpctx;
    screenshot_ctx *ctx = mpctx->screenshot_ctx;

    int sequence = 0;
    for (;;) {
        int prev_sequence = sequence;
        char *fname = create_fname(ctx->mpctx,
                                   ctx->mpctx->opts->screenshot_template,
                                   file_ext,
                                   &sequence,
                                   &ctx->frameno);

        if (!fname) {
            mp_cmd_msg(cmd, MSGL_ERR, "Invalid screenshot filename "
                       "template! Fix or remove the --screenshot-template "
                       "option.");
            return NULL;
        }

        char *dir = ctx->mpctx->opts->screenshot_directory;
        if (dir && dir[0]) {
            void *t = fname;
            dir = mp_get_user_path(t, ctx->mpctx->global, dir);
            fname = mp_path_join(NULL, dir, fname);

            mp_mkdirp(dir);

            talloc_free(t);
        }

        char *full_dir = bstrto0(fname, mp_dirname(fname));
        if (!mp_path_exists(full_dir)) {
            mp_mkdirp(full_dir);
        }

        if (!mp_path_exists(fname))
            return fname;

        if (sequence == prev_sequence) {
            mp_cmd_msg(cmd, MSGL_ERR, "Can't save screenshot, file '%s' "
                       "already exists!", fname);
            talloc_free(fname);
            return NULL;
        }

        talloc_free(fname);
    }
}

static void add_subs(struct MPContext *mpctx, struct mp_image *image)
{
    struct mp_osd_res res = osd_res_from_image_params(&image->params);
    osd_draw_on_image(mpctx->osd, res, mpctx->video_pts,
                      OSD_DRAW_SUB_ONLY, image);
}

static struct mp_image *screenshot_get(struct MPContext *mpctx, int mode,
                                       bool high_depth)
{
    struct mp_image *image = NULL;
    if (mode == MODE_SUBTITLES && osd_get_render_subs_in_filter(mpctx->osd))
        mode = 0;
    bool need_add_subs = mode == MODE_SUBTITLES;

    if (mpctx->video_out && mpctx->video_out->config_ok) {
        vo_wait_frame(mpctx->video_out); // important for each-frame mode

        struct voctrl_screenshot ctrl = {
            .scaled = mode == MODE_FULL_WINDOW,
            .subs = mode != 0,
            .osd = mode == MODE_FULL_WINDOW,
            .high_bit_depth = high_depth &&
                              mpctx->opts->screenshot_image_opts->high_bit_depth,
        };
        if (!mpctx->opts->screenshot_sw)
            vo_control(mpctx->video_out, VOCTRL_SCREENSHOT, &ctrl);
        image = ctrl.res;
        if (image)
            need_add_subs = false;

        if (!image && mode != MODE_FULL_WINDOW)
            image = vo_get_current_frame(mpctx->video_out);
        if (!image) {
            vo_control(mpctx->video_out, VOCTRL_SCREENSHOT_WIN, &image);
            mode = MODE_FULL_WINDOW;
        }
    }

    if (image && (image->fmt.flags & MP_IMGFLAG_HWACCEL)) {
        struct mp_image *nimage = mp_image_hw_download(image, NULL);
        talloc_free(image);
        image = nimage;
    }

    if (image && need_add_subs)
        add_subs(mpctx, image);

    return image;
}

struct mp_image *convert_image(struct mp_image *image, int destfmt,
                               struct mpv_global *global, struct mp_log *log)
{
    int d_w, d_h;
    mp_image_params_get_dsize(&image->params, &d_w, &d_h);

    struct mp_image_params p = {
        .imgfmt = destfmt,
        .w = d_w,
        .h = d_h,
        .p_w = 1,
        .p_h = 1,
    };
    mp_image_params_guess_csp(&p);

    if (mp_image_params_equal(&p, &image->params))
        return mp_image_new_ref(image);

    struct mp_image *dst = mp_image_alloc(p.imgfmt, p.w, p.h);
    if (!dst) {
        mp_err(log, "Out of memory.\n");
        return NULL;
    }
    mp_image_copy_attributes(dst, image);

    dst->params = p;

    struct mp_sws_context *sws = mp_sws_alloc(NULL);
    sws->log = log;
    if (global)
        mp_sws_enable_cmdline_opts(sws, global);
    bool ok = mp_sws_scale(sws, dst, image) >= 0;
    talloc_free(sws);

    if (!ok) {
        mp_err(log, "Error when converting image.\n");
        talloc_free(dst);
        return NULL;
    }

    return dst;
}

// mode is the same as in screenshot_get()
static struct mp_image *screenshot_get_rgb(struct MPContext *mpctx, int mode)
{
    struct mp_image *mpi = screenshot_get(mpctx, mode, false);
    if (!mpi)
        return NULL;
    struct mp_image *res = convert_image(mpi, IMGFMT_BGR0, mpctx->global,
                                         mpctx->log);
    talloc_free(mpi);
    return res;
}

void cmd_screenshot_to_file(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    const char *filename = cmd->args[0].v.s;
    int mode = cmd->args[1].v.i;
    struct image_writer_opts opts = *mpctx->opts->screenshot_image_opts;

    char *ext = mp_splitext(filename, NULL);
    int format = image_writer_format_from_ext(ext);
    if (format)
        opts.format = format;
    bool high_depth = image_writer_high_depth(&opts);
    struct mp_image *image = screenshot_get(mpctx, mode, high_depth);
    if (!image) {
        mp_cmd_msg(cmd, MSGL_ERR, "Taking screenshot failed.");
        cmd->success = false;
        return;
    }
    cmd->success = write_screenshot(cmd, image, filename, &opts);
    talloc_free(image);
}

void cmd_screenshot(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    int mode = cmd->args[0].v.i & 3;
    bool each_frame_toggle = (cmd->args[0].v.i | cmd->args[1].v.i) & 8;
    bool each_frame_mode = cmd->args[0].v.i & 16;

    screenshot_ctx *ctx = mpctx->screenshot_ctx;

    if (mode == MODE_SUBTITLES && osd_get_render_subs_in_filter(mpctx->osd))
        mode = 0;

    if (!each_frame_mode) {
        if (each_frame_toggle) {
            if (ctx->each_frame) {
                TA_FREEP(&ctx->each_frame);
                return;
            }
            ctx->each_frame = talloc_steal(ctx, mp_cmd_clone(cmd->cmd));
            ctx->each_frame->args[0].v.i |= 16;
        } else {
            TA_FREEP(&ctx->each_frame);
        }
    }

    cmd->success = false;

    struct image_writer_opts *opts = mpctx->opts->screenshot_image_opts;
    bool high_depth = image_writer_high_depth(opts);

    struct mp_image *image = screenshot_get(mpctx, mode, high_depth);

    if (image) {
        char *filename = gen_fname(cmd, image_writer_file_ext(opts));
        if (filename)
            cmd->success = write_screenshot(cmd, image, filename, NULL);
        talloc_free(filename);
    } else {
        mp_cmd_msg(cmd, MSGL_ERR, "Taking screenshot failed.");
    }

    talloc_free(image);
}

void cmd_screenshot_raw(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    struct mpv_node *res = &cmd->result;

    struct mp_image *img = screenshot_get_rgb(mpctx, cmd->args[0].v.i);
    if (!img) {
        cmd->success = false;
        return;
    }

    node_init(res, MPV_FORMAT_NODE_MAP, NULL);
    node_map_add_int64(res, "w", img->w);
    node_map_add_int64(res, "h", img->h);
    node_map_add_int64(res, "stride", img->stride[0]);
    node_map_add_string(res, "format", "bgr0");
    struct mpv_byte_array *ba =
        node_map_add(res, "data", MPV_FORMAT_BYTE_ARRAY)->u.ba;
    *ba = (struct mpv_byte_array){
        .data = img->planes[0],
        .size = img->stride[0] * img->h,
    };
    talloc_steal(ba, img);
}

static void screenshot_fin(struct mp_cmd_ctx *cmd)
{
    void **a = cmd->on_completion_priv;
    struct MPContext *mpctx = a[0];
    struct mp_waiter *waiter = a[1];

    mp_waiter_wakeup(waiter, 0);
    mp_wakeup_core(mpctx);
}

void handle_each_frame_screenshot(struct MPContext *mpctx)
{
    screenshot_ctx *ctx = mpctx->screenshot_ctx;

    if (!ctx->each_frame)
        return;

    if (ctx->last_frame_count == mpctx->shown_vframes)
        return;
    ctx->last_frame_count = mpctx->shown_vframes;

    struct mp_waiter wait = MP_WAITER_INITIALIZER;
    void *a[] = {mpctx, &wait};
    run_command(mpctx, mp_cmd_clone(ctx->each_frame), NULL, screenshot_fin, a);

    // Block (in a reentrant way) until the screenshot was written. Otherwise,
    // we could pile up screenshot requests forever.
    while (!mp_waiter_poll(&wait))
        mp_idle(mpctx);

    mp_waiter_wait(&wait);
}
