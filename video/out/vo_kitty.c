/*
 * Video output device using the kitty terminal graphics protocol
 * See https://sw.kovidgoyal.net/kitty/graphics-protocol/
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "config.h"

#if HAVE_POSIX
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <libswscale/swscale.h>
#include <libavutil/base64.h>

#include "options/m_config.h"
#include "osdep/terminal.h"
#include "sub/osd.h"
#include "vo.h"
#include "video/sws_utils.h"
#include "video/mp_image.h"

#define IMGFMT IMGFMT_RGB24
#define BYTES_PER_PX 3
#define DEFAULT_WIDTH_PX  320
#define DEFAULT_HEIGHT_PX 240
#define DEFAULT_WIDTH 80
#define DEFAULT_HEIGHT 25

static inline void write_str(const char *s)
{
    // On POSIX platforms, write() is the fastest method. It also is the only
    // one that allows atomic writes so mpv’s output will not be interrupted
    // by other processes or threads that write to stdout, which would cause
    // screen corruption. POSIX does not guarantee atomicity for writes
    // exceeding PIPE_BUF, but at least Linux does seem to implement it that
    // way.
#if HAVE_POSIX
    int remain = strlen(s);
    while (remain > 0) {
        ssize_t written = write(STDOUT_FILENO, s, remain);
        if (written < 0)
            return;
        remain -= written;
        s += written;
    }
#else
    printf("%s", s);
    fflush(stdout);
#endif
}

#define KITTY_ESC_IMG        "\033_Ga=T,f=24,s=%d,v=%d,C=1,q=2,m=1;"
#define KITTY_ESC_IMG_SHM    "\033_Ga=T,t=s,f=24,s=%d,v=%d,C=1,q=2,m=1;%s\033\\"
#define KITTY_ESC_CONTINUE   "\033_Gm=%d;"
#define KITTY_ESC_END        "\033\\"
#define KITTY_ESC_DELETE_ALL "\033_Ga=d;\033\\"

struct vo_kitty_opts {
    int width, height, top, left, rows, cols;
    bool config_clear, alt_screen;
    bool use_shm;
};

struct priv {
    struct vo_kitty_opts opts;

    uint8_t *buffer;
    char    *output;
    char    *shm_path, *shm_path_b64;
    int     buffer_size, output_size;
    int     shm_fd;

    int left, top, width, height, cols, rows;

    struct mp_rect src;
    struct mp_rect dst;
    struct mp_osd_res osd;
    struct mp_image *frame;
    struct mp_sws_context *sws;
};

#if HAVE_POSIX
static struct sigaction saved_sigaction = {0};
static bool resized;
#endif

static void close_shm(struct priv *p)
{
#if HAVE_POSIX_SHM
    if (p->buffer != NULL) {
        munmap(p->buffer, p->buffer_size);
        p->buffer = NULL;
    }
    if (p->shm_fd != -1) {
        close(p->shm_fd);
        p->shm_fd = -1;
    }
#endif
}

static void free_bufs(struct vo* vo)
{
    struct priv* p = vo->priv;

    talloc_free(p->frame);
    talloc_free(p->output);

    if (p->opts.use_shm) {
        close_shm(p);
    } else {
        talloc_free(p->buffer);
    }
}

static void get_win_size(struct vo *vo, int *out_rows, int *out_cols,
                         int *out_width, int *out_height)
{
    struct priv *p = vo->priv;
    *out_rows = DEFAULT_HEIGHT;
    *out_cols = DEFAULT_WIDTH;
    *out_width = DEFAULT_WIDTH_PX;
    *out_height = DEFAULT_HEIGHT_PX;

    terminal_get_size2(out_rows, out_cols, out_width, out_height);

    *out_rows = p->opts.rows > 0 ? p->opts.rows : *out_rows;
    *out_cols = p->opts.cols > 0 ? p->opts.cols : *out_cols;
    *out_width = p->opts.width > 0 ? p->opts.width : *out_width;
    *out_height = p->opts.height > 0 ? p->opts.height : *out_height;
}

static void set_out_params(struct vo *vo)
{
    struct priv *p = vo->priv;

    vo_get_src_dst_rects(vo, &p->src, &p->dst, &p->osd);

    p->width  = p->dst.x1 - p->dst.x0;
    p->height = p->dst.y1 - p->dst.y0;
    p->top  = p->opts.top > 0 ?
        p->opts.top : p->rows * p->dst.y0 / vo->dheight;
    p->left = p->opts.left > 0 ?
        p->opts.left : p->cols * p->dst.x0 / vo->dwidth;

    p->buffer_size = 3 * p->width * p->height;
    p->output_size = AV_BASE64_SIZE(p->buffer_size);
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct priv *p = vo->priv;

    vo->want_redraw = true;
    write_str(KITTY_ESC_DELETE_ALL);
    if (p->opts.config_clear)
        write_str(TERM_ESC_CLEAR_SCREEN);

    get_win_size(vo, &p->rows, &p->cols, &vo->dwidth, &vo->dheight);
    set_out_params(vo);
    free_bufs(vo);

    p->sws->src = *params;
    p->sws->src.w = mp_rect_w(p->src);
    p->sws->src.h = mp_rect_h(p->src);
    p->sws->dst = (struct mp_image_params) {
        .imgfmt = IMGFMT,
        .w = p->width,
        .h = p->height,
        .p_w = 1,
        .p_h = 1,
    };

    p->frame = mp_image_alloc(IMGFMT, p->width, p->height);
    if (!p->frame)
        return -1;

    if (mp_sws_reinit(p->sws) < 0)
        return -1;

    if (!p->opts.use_shm) {
        p->buffer = talloc_array(NULL, uint8_t, p->buffer_size);
        p->output = talloc_array(NULL, char, p->output_size);
    }

    return 0;
}

static int create_shm(struct vo *vo)
{
#if HAVE_POSIX_SHM
    struct priv *p = vo->priv;
    p->shm_fd = shm_open(p->shm_path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (p->shm_fd == -1) {
        MP_ERR(vo, "Failed to create shared memory object");
        return 0;
    }

    if (ftruncate(p->shm_fd, p->buffer_size) == -1) {
        MP_ERR(vo, "Failed to truncate shared memory object");
        shm_unlink(p->shm_path);
        close(p->shm_fd);
        return 0;
    }

    p->buffer = mmap(NULL, p->buffer_size,
                        PROT_READ | PROT_WRITE, MAP_SHARED, p->shm_fd, 0);

    if (p->buffer == MAP_FAILED) {
        MP_ERR(vo, "Failed to mmap shared memory object");
        shm_unlink(p->shm_path);
        close(p->shm_fd);
        return 0;
    }
    return 1;
#else
    return 0;
#endif
}

static void draw_frame(struct vo *vo, struct vo_frame *frame)
{
    struct priv *p = vo->priv;
    mp_image_t *mpi = NULL;

#if !HAVE_POSIX
    int prev_height = vo->dheight;
    int prev_width = vo->dwidth;
    get_win_size(vo, &p->rows, &p->cols, &vo->dwidth, &vo->dheight);
    bool resized = (prev_width != vo->dwidth || prev_height != vo->dheight);
#endif

    if (resized)
        reconfig(vo, vo->params);

    resized = false;

    if (frame->current) {
        mpi = mp_image_new_ref(frame->current);
        struct mp_rect src_rc = p->src;
        src_rc.x0 = MP_ALIGN_DOWN(src_rc.x0, mpi->fmt.align_x);
        src_rc.y0 = MP_ALIGN_DOWN(src_rc.y0, mpi->fmt.align_y);
        mp_image_crop_rc(mpi, src_rc);

        mp_sws_scale(p->sws, p->frame, mpi);
    } else {
        mp_image_clear(p->frame, 0, 0, p->width, p->height);
    }

    struct mp_osd_res res = { .w = p->width, .h = p->height };
    osd_draw_on_image(vo->osd, res, mpi ? mpi->pts : 0, 0, p->frame);


    if (p->opts.use_shm && !create_shm(vo))
        return;

    memcpy_pic(p->buffer, p->frame->planes[0], p->width * BYTES_PER_PX,
               p->height, p->width * BYTES_PER_PX, p->frame->stride[0]);

    if (!p->opts.use_shm)
        av_base64_encode(p->output, p->output_size, p->buffer, p->buffer_size);

    talloc_free(mpi);
}

static void flip_page(struct vo *vo)
{
    struct priv* p = vo->priv;

    if (p->buffer == NULL)
        return;

    char *cmd = talloc_asprintf(NULL, TERM_ESC_GOTO_YX, p->top, p->left);

    if (p->opts.use_shm) {
        cmd = talloc_asprintf_append(cmd, KITTY_ESC_IMG_SHM, p->width, p->height, p->shm_path_b64);
    } else {
        if (p->output == NULL) {
            talloc_free(cmd);
            return;
        }

        cmd = talloc_asprintf_append(cmd, KITTY_ESC_IMG, p->width, p->height);
        for (int offset = 0, noffset;; offset += noffset) {
            if (offset)
                cmd = talloc_asprintf_append(cmd, KITTY_ESC_CONTINUE, offset < p->output_size);
            noffset = MPMIN(4096, p->output_size - offset);
            cmd = talloc_strndup_append(cmd, p->output + offset, noffset);
            cmd = talloc_strdup_append(cmd, KITTY_ESC_END);

            if (offset >= p->output_size)
                break;
        }
    }

    write_str(cmd);
    talloc_free(cmd);

#if HAVE_POSIX
    if (p->opts.use_shm)
        close_shm(p);
#endif
}

#if HAVE_POSIX
static void handle_winch(int sig) {
    resized = true;
    if (saved_sigaction.sa_handler)
        saved_sigaction.sa_handler(sig);
}
#endif

static int preinit(struct vo *vo)
{
    struct priv *p = vo->priv;

    p->sws = mp_sws_alloc(vo);
    p->sws->log = vo->log;
    mp_sws_enable_cmdline_opts(p->sws, vo->global);

#if HAVE_POSIX
    struct sigaction sa = {
        .sa_handler = handle_winch,
    };
    sigaction(SIGWINCH, &sa, &saved_sigaction);
#endif

#if HAVE_POSIX_SHM
    if (p->opts.use_shm) {
        p->shm_path = talloc_asprintf(vo, "/mpv-kitty-%p", vo);
        int p_size = strlen(p->shm_path) - 1;
        int b64_size = AV_BASE64_SIZE(p_size);
        p->shm_path_b64 = talloc_array(vo, char, b64_size);
        av_base64_encode(p->shm_path_b64, b64_size, p->shm_path + 1, p_size);
    }
#else
    if (p->opts.use_shm) {
        MP_ERR(vo, "Shared memory support is not available on this platform.");
        return -1;
    }
#endif

    write_str(TERM_ESC_HIDE_CURSOR);
    terminal_set_mouse_input(true);
    if (p->opts.alt_screen)
        write_str(TERM_ESC_ALT_SCREEN);

    return 0;
}

static int query_format(mp_unused struct vo *vo, int format)
{
    return format == IMGFMT;
}

static int control(struct vo *vo, uint32_t request, mp_unused void *data)
{
    if (request == VOCTRL_SET_PANSCAN)
        return (vo->config_ok && !reconfig(vo, vo->params)) ? VO_TRUE : VO_FALSE;
    return VO_NOTIMPL;
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;

#if HAVE_POSIX
    sigaction(SIGWINCH, &saved_sigaction, NULL);
#endif

    write_str(TERM_ESC_RESTORE_CURSOR);
    terminal_set_mouse_input(false);

    if (p->opts.alt_screen) {
        write_str(TERM_ESC_NORMAL_SCREEN);
    } else {
        char *cmd = talloc_asprintf(vo, TERM_ESC_GOTO_YX, p->cols, 0);
        write_str(cmd);
    }

    free_bufs(vo);
}

#define OPT_BASE_STRUCT struct priv

const struct vo_driver video_out_kitty = {
    .name = "kitty",
    .description = "Kitty terminal graphics protocol",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_frame = draw_frame,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .shm_fd = -1,
        .opts.config_clear = true,
        .opts.alt_screen = true,
    },
    .options = (const m_option_t[]) {
        {"width", OPT_INT(opts.width)},
        {"height", OPT_INT(opts.height)},
        {"top", OPT_INT(opts.top)},
        {"left", OPT_INT(opts.left)},
        {"rows", OPT_INT(opts.rows)},
        {"cols", OPT_INT(opts.cols)},
        {"config-clear", OPT_BOOL(opts.config_clear), },
        {"alt-screen", OPT_BOOL(opts.alt_screen), },
        {"use-shm", OPT_BOOL(opts.use_shm), },
        {0}
    },
    .options_prefix = "vo-kitty",
};
