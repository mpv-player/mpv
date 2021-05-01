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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>

#include "config.h"

#if HAVE_JPEG
#include <setjmp.h>
#include <jpeglib.h>
#endif

#include "osdep/io.h"

#include "image_writer.h"
#include "mpv_talloc.h"
#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/fmt-conversion.h"
#include "video/sws_utils.h"

#include "options/m_option.h"

const struct image_writer_opts image_writer_opts_defaults = {
    .format = AV_CODEC_ID_MJPEG,
    .high_bit_depth = 1,
    .png_compression = 7,
    .png_filter = 5,
    .jpeg_quality = 90,
    .jpeg_source_chroma = 1,
    .webp_lossless = 0,
    .webp_quality = 75,
    .webp_compression = 4,
    .tag_csp = 0,
};

const struct m_opt_choice_alternatives mp_image_writer_formats[] = {
    {"jpg",  AV_CODEC_ID_MJPEG},
    {"jpeg", AV_CODEC_ID_MJPEG},
    {"png",  AV_CODEC_ID_PNG},
    {"webp", AV_CODEC_ID_WEBP},
    {0}
};

#define OPT_BASE_STRUCT struct image_writer_opts

const struct m_option image_writer_opts[] = {
    {"format", OPT_CHOICE_C(format, mp_image_writer_formats)},
    {"jpeg-quality", OPT_INT(jpeg_quality), M_RANGE(0, 100)},
    {"jpeg-source-chroma", OPT_FLAG(jpeg_source_chroma)},
    {"png-compression", OPT_INT(png_compression), M_RANGE(0, 9)},
    {"png-filter", OPT_INT(png_filter), M_RANGE(0, 5)},
    {"webp-lossless", OPT_FLAG(webp_lossless)},
    {"webp-quality", OPT_INT(webp_quality), M_RANGE(0, 100)},
    {"webp-compression", OPT_INT(webp_compression), M_RANGE(0, 6)},
    {"high-bit-depth", OPT_FLAG(high_bit_depth)},
    {"tag-colorspace", OPT_FLAG(tag_csp)},
    {0},
};

struct image_writer_ctx {
    struct mp_log *log;
    const struct image_writer_opts *opts;
    struct mp_imgfmt_desc original_format;
};

static enum AVPixelFormat replace_j_format(enum AVPixelFormat fmt)
{
    switch (fmt) {
    case AV_PIX_FMT_YUV420P: return AV_PIX_FMT_YUVJ420P;
    case AV_PIX_FMT_YUV422P: return AV_PIX_FMT_YUVJ422P;
    case AV_PIX_FMT_YUV444P: return AV_PIX_FMT_YUVJ444P;
    }
    return fmt;
}

static bool write_lavc(struct image_writer_ctx *ctx, mp_image_t *image, FILE *fp)
{
    bool success = 0;
    AVFrame *pic = NULL;
    AVPacket pkt = {0};
    int got_output = 0;

    av_init_packet(&pkt);

    const AVCodec *codec;
    if (ctx->opts->format == AV_CODEC_ID_WEBP) {
        codec = avcodec_find_encoder_by_name("libwebp"); // non-animated encoder
    } else {
        codec = avcodec_find_encoder(ctx->opts->format);
    }

    AVCodecContext *avctx = NULL;
    if (!codec)
        goto print_open_fail;
    avctx = avcodec_alloc_context3(codec);
    if (!avctx)
        goto print_open_fail;

    avctx->time_base = AV_TIME_BASE_Q;
    avctx->width = image->w;
    avctx->height = image->h;
    avctx->color_range = mp_csp_levels_to_avcol_range(image->params.color.levels);
    avctx->pix_fmt = imgfmt2pixfmt(image->imgfmt);
    if (codec->id == AV_CODEC_ID_MJPEG) {
        // Annoying deprecated garbage for the jpg encoder.
        if (image->params.color.levels == MP_CSP_LEVELS_PC)
            avctx->pix_fmt = replace_j_format(avctx->pix_fmt);
    }
    if (avctx->pix_fmt == AV_PIX_FMT_NONE) {
        MP_ERR(ctx, "Image format %s not supported by lavc.\n",
               mp_imgfmt_to_name(image->imgfmt));
        goto error_exit;
    }
    if (codec->id == AV_CODEC_ID_PNG) {
        avctx->compression_level = ctx->opts->png_compression;
        av_opt_set_int(avctx, "pred", ctx->opts->png_filter,
                       AV_OPT_SEARCH_CHILDREN);
    } else if (codec->id == AV_CODEC_ID_WEBP) {
        avctx->compression_level = ctx->opts->webp_compression;
        av_opt_set_int(avctx, "lossless", ctx->opts->webp_lossless,
                       AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(avctx, "quality", ctx->opts->webp_quality,
                       AV_OPT_SEARCH_CHILDREN);
    }

    if (avcodec_open2(avctx, codec, NULL) < 0) {
     print_open_fail:
        MP_ERR(ctx, "Could not open libavcodec encoder for saving images\n");
        goto error_exit;
    }

    pic = av_frame_alloc();
    if (!pic)
        goto error_exit;
    for (int n = 0; n < 4; n++) {
        pic->data[n] = image->planes[n];
        pic->linesize[n] = image->stride[n];
    }
    pic->format = avctx->pix_fmt;
    pic->width = avctx->width;
    pic->height = avctx->height;
    pic->color_range = avctx->color_range;
    if (ctx->opts->tag_csp) {
        pic->color_primaries = mp_csp_prim_to_avcol_pri(image->params.color.primaries);
        pic->color_trc = mp_csp_trc_to_avcol_trc(image->params.color.gamma);
    }

    int ret = avcodec_send_frame(avctx, pic);
    if (ret < 0)
        goto error_exit;
    ret = avcodec_send_frame(avctx, NULL); // send EOF
    if (ret < 0)
        goto error_exit;
    ret = avcodec_receive_packet(avctx, &pkt);
    if (ret < 0)
        goto error_exit;
    got_output = 1;

    fwrite(pkt.data, pkt.size, 1, fp);

    success = !!got_output;
error_exit:
    avcodec_free_context(&avctx);
    av_frame_free(&pic);
    av_packet_unref(&pkt);
    return success;
}

#if HAVE_JPEG

static void write_jpeg_error_exit(j_common_ptr cinfo)
{
  // NOTE: do not write error message, too much effort to connect the libjpeg
  //       log callbacks with mplayer's log function mp_msp()

  // Return control to the setjmp point
  longjmp(*(jmp_buf*)cinfo->client_data, 1);
}

static bool write_jpeg(struct image_writer_ctx *ctx, mp_image_t *image, FILE *fp)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = write_jpeg_error_exit;

    jmp_buf error_return_jmpbuf;
    cinfo.client_data = &error_return_jmpbuf;
    if (setjmp(cinfo.client_data)) {
        jpeg_destroy_compress(&cinfo);
        return false;
    }

    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);

    cinfo.image_width = image->w;
    cinfo.image_height = image->h;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    cinfo.write_JFIF_header = TRUE;
    cinfo.JFIF_major_version = 1;
    cinfo.JFIF_minor_version = 2;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, ctx->opts->jpeg_quality, 0);

    if (ctx->opts->jpeg_source_chroma) {
        cinfo.comp_info[0].h_samp_factor = 1 << ctx->original_format.chroma_xs;
        cinfo.comp_info[0].v_samp_factor = 1 << ctx->original_format.chroma_ys;
    }

    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row_pointer[1];
        row_pointer[0] = image->planes[0] +
                         (ptrdiff_t)cinfo.next_scanline * image->stride[0];
        jpeg_write_scanlines(&cinfo, row_pointer,1);
    }

    jpeg_finish_compress(&cinfo);

    jpeg_destroy_compress(&cinfo);

    return true;
}

#endif

static int get_encoder_format(const AVCodec *codec, int srcfmt, bool highdepth)
{
    const enum AVPixelFormat *pix_fmts = codec->pix_fmts;
    int current = 0;
    for (int n = 0; pix_fmts && pix_fmts[n] != AV_PIX_FMT_NONE; n++) {
        int fmt = pixfmt2imgfmt(pix_fmts[n]);
        if (!fmt)
            continue;
        if (!highdepth) {
            // Ignore formats larger than 8 bit per pixel. (Or which are unknown.)
            struct mp_regular_imgfmt rdesc;
            if (!mp_get_regular_imgfmt(&rdesc, fmt)) {
                int ofmt = mp_find_other_endian(fmt);
                if (!mp_get_regular_imgfmt(&rdesc, ofmt))
                    continue;
            }
            if (rdesc.component_size > 1)
                continue;
        }
        current = current ? mp_imgfmt_select_best(current, fmt, srcfmt) : fmt;
    }
    return current;
}

static int get_target_format(struct image_writer_ctx *ctx)
{
    const AVCodec *codec = avcodec_find_encoder(ctx->opts->format);
    if (!codec)
        goto unknown;

    int srcfmt = ctx->original_format.id;

    int target = get_encoder_format(codec, srcfmt, ctx->opts->high_bit_depth);
    if (!target)
        target = get_encoder_format(codec, srcfmt, true);

    if (!target)
        goto unknown;

    return target;

unknown:
    return IMGFMT_RGB0;
}

const char *image_writer_file_ext(const struct image_writer_opts *opts)
{
    struct image_writer_opts defs = image_writer_opts_defaults;

    if (!opts)
        opts = &defs;

    return m_opt_choice_str(mp_image_writer_formats, opts->format);
}

bool image_writer_high_depth(const struct image_writer_opts *opts)
{
    return opts->format == AV_CODEC_ID_PNG;
}

int image_writer_format_from_ext(const char *ext)
{
    for (int n = 0; mp_image_writer_formats[n].name; n++) {
        if (ext && strcmp(mp_image_writer_formats[n].name, ext) == 0)
            return mp_image_writer_formats[n].value;
    }
    return 0;
}

static struct mp_image *convert_image(struct mp_image *image, int destfmt,
                                      enum mp_csp_levels yuv_levels,
                                      struct mpv_global *global,
                                      struct mp_log *log)
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

    // If RGB, just assume everything is correct.
    if (p.color.space != MP_CSP_RGB) {
        // Currently, assume what FFmpeg's jpg encoder or libwebp needs.
        // Of course this works only for non-HDR (no HDR support in libswscale).
        p.color.levels = yuv_levels;
        p.color.space = MP_CSP_BT_601;
        p.chroma_location = MP_CHROMA_CENTER;
        mp_image_params_guess_csp(&p);
    }

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

bool write_image(struct mp_image *image, const struct image_writer_opts *opts,
                const char *filename, struct mpv_global *global,
                 struct mp_log *log)
{
    struct image_writer_opts defs = image_writer_opts_defaults;
    if (!opts)
        opts = &defs;

    struct image_writer_ctx ctx = { log, opts, image->fmt };
    bool (*write)(struct image_writer_ctx *, mp_image_t *, FILE *) = write_lavc;
    int destfmt = 0;

#if HAVE_JPEG
    if (opts->format == AV_CODEC_ID_MJPEG) {
        write = write_jpeg;
        destfmt = IMGFMT_RGB24;
    }
#endif
    if (opts->format == AV_CODEC_ID_WEBP && !opts->webp_lossless) {
        // For lossy images, libwebp has its own RGB->YUV conversion.
        // We don't want that, so force YUV/YUVA here.
        int alpha = image->fmt.flags & MP_IMGFLAG_ALPHA;
        destfmt = alpha ? pixfmt2imgfmt(AV_PIX_FMT_YUVA420P) : IMGFMT_420P;
    }

    if (!destfmt)
        destfmt = get_target_format(&ctx);

    enum mp_csp_levels levels; // Ignored if destfmt is a RGB format
    if (opts->format == AV_CODEC_ID_WEBP) {
        levels = MP_CSP_LEVELS_TV;
    } else {
        levels = MP_CSP_LEVELS_PC;
    }

    struct mp_image *dst = convert_image(image, destfmt, levels, global, log);
    if (!dst)
        return false;

    FILE *fp = fopen(filename, "wb");
    bool success = false;
    if (fp == NULL) {
        mp_err(log, "Error opening '%s' for writing!\n", filename);
    } else {
        success = write(&ctx, dst, fp);
        success = !fclose(fp) && success;
        if (!success)
            mp_err(log, "Error writing file '%s'!\n", filename);
    }

    talloc_free(dst);
    return success;
}

void dump_png(struct mp_image *image, const char *filename, struct mp_log *log)
{
    struct image_writer_opts opts = image_writer_opts_defaults;
    opts.format = AV_CODEC_ID_PNG;
    write_image(image, &opts, filename, NULL, log);
}
