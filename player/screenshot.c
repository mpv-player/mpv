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
#include "misc/thread_pool.h"
#include "common/msg.h"
#include "options/path.h"
#include "video/mp_image.h"
#include "video/mp_image_pool.h"
#include "video/out/vo.h"
#include "video/image_writer.h"
#include "sub/osd.h"

#include "video/csputils.h"

#define MODE_FULL_WINDOW 1
#define MODE_SUBTITLES 2

typedef struct screenshot_ctx {
    struct MPContext *mpctx;

    int mode;
    bool each_frame;
    bool osd;

    int frameno;

    struct mp_thread_pool *thread_pool;
} screenshot_ctx;

void screenshot_init(struct MPContext *mpctx)
{
    mpctx->screenshot_ctx = talloc(mpctx, screenshot_ctx);
    *mpctx->screenshot_ctx = (screenshot_ctx) {
        .mpctx = mpctx,
        .frameno = 1,
    };
}

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

    MP_MSG(ctx->mpctx, status, "%s\n", s);
    if (ctx->osd && status <= MSGL_INFO)
        set_osd_msg(ctx->mpctx, 1, ctx->mpctx->opts->osd_duration, "%s", s);

    talloc_free(s);
}

static char *stripext(void *talloc_ctx, const char *s)
{
    const char *end = strrchr(s, '.');
    if (!end)
        end = s + strlen(s);
    return talloc_asprintf(talloc_ctx, "%.*s", (int)(end - s), s);
}

struct screenshot_item {
    bool on_thread;
    struct MPContext *mpctx;
    const char *filename;
    struct mp_image *img;
    struct image_writer_opts opts;
};

#define LOCK(item) if (item->on_thread) mp_dispatch_lock(item->mpctx->dispatch);
#define UNLOCK(item) if (item->on_thread) mp_dispatch_unlock(item->mpctx->dispatch);

static void write_screenshot_thread(void *arg)
{
    struct screenshot_item *item = arg;
    screenshot_ctx *ctx = item->mpctx->screenshot_ctx;

    LOCK(item)
    screenshot_msg(ctx, MSGL_INFO, "Screenshot: '%s'", item->filename);
    UNLOCK(item)

    if (!item->img || !write_image(item->img, &item->opts, item->filename,
                                   item->mpctx->log))
    {
        LOCK(item)
        screenshot_msg(ctx, MSGL_ERR, "Error writing screenshot!");
        UNLOCK(item)
    }

    if (item->on_thread) {
        mp_dispatch_lock(item->mpctx->dispatch);
        screenshot_msg(ctx, MSGL_V, "Screenshot writing done.");
        item->mpctx->outstanding_async -= 1;
        mp_wakeup_core(item->mpctx);
        mp_dispatch_unlock(item->mpctx->dispatch);
    }

    talloc_free(item);
}

static void write_screenshot(struct MPContext *mpctx, struct mp_image *img,
                             const char *filename, struct image_writer_opts *opts,
                             bool async)
{
    screenshot_ctx *ctx = mpctx->screenshot_ctx;
    struct image_writer_opts *gopts = mpctx->opts->screenshot_image_opts;

    struct screenshot_item *item = talloc_zero(NULL, struct screenshot_item);
    *item = (struct screenshot_item){
        .mpctx = mpctx,
        .filename = talloc_strdup(item, filename),
        .img = talloc_steal(item, mp_image_new_ref(img)),
        .opts = opts ? *opts : *gopts,
    };

    if (async) {
        if (!ctx->thread_pool)
            ctx->thread_pool = mp_thread_pool_create(ctx, 1, 1, 3);
        if (ctx->thread_pool) {
            item->on_thread = true;
            mpctx->outstanding_async += 1;
            mp_thread_pool_queue(ctx->thread_pool, write_screenshot_thread, item);
            item = NULL;
        }
    }

    if (item)
        write_screenshot_thread(item);
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

static char *gen_fname(screenshot_ctx *ctx, const char *file_ext)
{
    int sequence = 0;
    for (;;) {
        int prev_sequence = sequence;
        char *fname = create_fname(ctx->mpctx,
                                   ctx->mpctx->opts->screenshot_template,
                                   file_ext,
                                   &sequence,
                                   &ctx->frameno);

        if (!fname) {
            screenshot_msg(ctx, MSGL_ERR, "Invalid screenshot filename "
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
            screenshot_msg(ctx, MSGL_ERR, "Can't save screenshot, file '%s' "
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

// mode is the same as in screenshot_request()
static struct mp_image *screenshot_get_rgb(struct MPContext *mpctx, int mode)
{
    struct mp_image *mpi = screenshot_get(mpctx, mode, false);
    if (!mpi)
        return NULL;
    struct mp_image *res = convert_image(mpi, IMGFMT_BGR0, mpctx->log);
    talloc_free(mpi);
    return res;
}

// filename: where to store the screenshot; doesn't try to find an alternate
//           name if the file already exists
// mode, osd: same as in screenshot_request()
static void screenshot_to_file(struct MPContext *mpctx, const char *filename,
                               int mode, bool osd, bool async)
{
    screenshot_ctx *ctx = mpctx->screenshot_ctx;
    struct image_writer_opts opts = *mpctx->opts->screenshot_image_opts;
    bool old_osd = ctx->osd;
    ctx->osd = osd;

    char *ext = mp_splitext(filename, NULL);
    int format = image_writer_format_from_ext(ext);
    if (format)
        opts.format = format;
    bool high_depth = image_writer_high_depth(&opts);
    struct mp_image *image = screenshot_get(mpctx, mode, high_depth);
    if (!image) {
        screenshot_msg(ctx, MSGL_ERR, "Taking screenshot failed.");
        goto end;
    }
    write_screenshot(mpctx, image, filename, &opts, async);
    talloc_free(image);

end:
    ctx->osd = old_osd;
}

// Request a taking & saving a screenshot of the currently displayed frame.
// mode: 0: -, 1: save the actual output window contents, 2: with subtitles.
// each_frame: If set, this toggles per-frame screenshots, exactly like the
//             screenshot slave command (MP_CMD_SCREENSHOT).
// osd: show status on OSD
static void screenshot_request(struct MPContext *mpctx, int mode, bool each_frame,
                               bool osd, bool async)
{
    screenshot_ctx *ctx = mpctx->screenshot_ctx;

    if (mode == MODE_SUBTITLES && osd_get_render_subs_in_filter(mpctx->osd))
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

    struct image_writer_opts *opts = mpctx->opts->screenshot_image_opts;
    bool high_depth = image_writer_high_depth(opts);

    struct mp_image *image = screenshot_get(mpctx, mode, high_depth);

    if (image) {
        char *filename = gen_fname(ctx, image_writer_file_ext(opts));
        if (filename)
            write_screenshot(mpctx, image, filename, NULL, async);
        talloc_free(filename);
    } else {
        screenshot_msg(ctx, MSGL_ERR, "Taking screenshot failed.");
    }

    talloc_free(image);
}

void cmd_screenshot(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    bool async = cmd->cmd->flags & MP_ASYNC_CMD;
    int mode = cmd->args[0].v.i & 3;
    int freq = (cmd->args[0].v.i | cmd->args[1].v.i) >> 3;
    screenshot_request(mpctx, mode, freq, cmd->msg_osd, async);
}

void cmd_screenshot_to_file(void *p)
{
    struct mp_cmd_ctx *cmd = p;
    struct MPContext *mpctx = cmd->mpctx;
    bool async = cmd->cmd->flags & MP_ASYNC_CMD;
    screenshot_to_file(mpctx, cmd->args[0].v.s, cmd->args[1].v.i, cmd->msg_osd,
                       async);
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

void screenshot_flip(struct MPContext *mpctx)
{
    screenshot_ctx *ctx = mpctx->screenshot_ctx;

    if (!ctx->each_frame)
        return;

    ctx->each_frame = false;
    screenshot_request(mpctx, ctx->mode, true, ctx->osd, false);
}
