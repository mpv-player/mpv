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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>

#include "config.h"

#if HAVE_JPEG
#include <jpeglib.h>
#endif

#include "osdep/io.h"

#include "image_writer.h"
#include "talloc.h"
#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/fmt-conversion.h"
#include "video/sws_utils.h"

#include "options/m_option.h"

const struct image_writer_opts image_writer_opts_defaults = {
    .format = "jpg",
    .png_compression = 7,
    .png_filter = 5,
    .jpeg_quality = 90,
    .jpeg_optimize = 100,
    .jpeg_smooth = 0,
    .jpeg_baseline = 1,
    .tag_csp = 1,
};

#define OPT_BASE_STRUCT struct image_writer_opts

const struct m_sub_options image_writer_conf = {
    .opts = (const m_option_t[]) {
        OPT_INTRANGE("jpeg-quality", jpeg_quality, 0, 0, 100),
        OPT_INTRANGE("jpeg-optimize", jpeg_optimize, 0, 0, 100),
        OPT_INTRANGE("jpeg-smooth", jpeg_smooth, 0, 0, 100),
        OPT_FLAG("jpeg-baseline", jpeg_baseline, 0),
        OPT_INTRANGE("png-compression", png_compression, 0, 0, 9),
        OPT_INTRANGE("png-filter", png_filter, 0, 0, 5),
        OPT_STRING("format", format, 0),
        OPT_FLAG("tag-colorspace", tag_csp, 0),
        {0},
    },
    .size = sizeof(struct image_writer_opts),
    .defaults = &image_writer_opts_defaults,
};

struct image_writer_ctx {
    struct mp_log *log;
    const struct image_writer_opts *opts;
    const struct img_writer *writer;
    struct mp_imgfmt_desc original_format;
};

struct img_writer {
    const char *file_ext;
    bool (*write)(struct image_writer_ctx *ctx, mp_image_t *image, FILE *fp);
    const int *pixfmts;
    int lavc_codec;
};

static bool write_lavc(struct image_writer_ctx *ctx, mp_image_t *image, FILE *fp)
{
    bool success = 0;
    AVFrame *pic = NULL;
    AVPacket pkt = {0};
    int got_output = 0;

    av_init_packet(&pkt);

    struct AVCodec *codec = avcodec_find_encoder(ctx->writer->lavc_codec);
    AVCodecContext *avctx = NULL;
    if (!codec)
        goto print_open_fail;
    avctx = avcodec_alloc_context3(codec);
    if (!avctx)
        goto print_open_fail;

    avctx->time_base = AV_TIME_BASE_Q;
    avctx->width = image->w;
    avctx->height = image->h;
    avctx->pix_fmt = imgfmt2pixfmt(image->imgfmt);
    if (avctx->pix_fmt == AV_PIX_FMT_NONE) {
        MP_ERR(ctx, "Image format %s not supported by lavc.\n",
               mp_imgfmt_to_name(image->imgfmt));
        goto error_exit;
    }
    if (ctx->writer->lavc_codec == AV_CODEC_ID_PNG) {
        avctx->compression_level = ctx->opts->png_compression;
        avctx->prediction_method = ctx->opts->png_filter;
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
    if (ctx->opts->tag_csp) {
        pic->color_primaries = mp_csp_prim_to_avcol_pri(image->params.primaries);
        pic->color_trc = mp_csp_trc_to_avcol_trc(image->params.gamma);
    }
    int ret = avcodec_encode_video2(avctx, &pkt, pic, &got_output);
    if (ret < 0)
        goto error_exit;

    fwrite(pkt.data, pkt.size, 1, fp);

    success = !!got_output;
error_exit:
    if (avctx)
        avcodec_close(avctx);
    av_free(avctx);
    av_frame_free(&pic);
    av_free_packet(&pkt);
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
    jpeg_set_quality(&cinfo, ctx->opts->jpeg_quality, ctx->opts->jpeg_baseline);
    cinfo.optimize_coding = ctx->opts->jpeg_optimize;
    cinfo.smoothing_factor = ctx->opts->jpeg_smooth;

    cinfo.comp_info[0].h_samp_factor = 1 << ctx->original_format.chroma_xs;
    cinfo.comp_info[0].v_samp_factor = 1 << ctx->original_format.chroma_ys;

    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row_pointer[1];
        row_pointer[0] = image->planes[0] +
                         cinfo.next_scanline * image->stride[0];
        jpeg_write_scanlines(&cinfo, row_pointer,1);
    }

    jpeg_finish_compress(&cinfo);

    jpeg_destroy_compress(&cinfo);

    return true;
}

#endif

static int get_target_format(struct image_writer_ctx *ctx, int srcfmt)
{
    if (!ctx->writer->lavc_codec)
        goto unknown;

    struct AVCodec *codec = avcodec_find_encoder(ctx->writer->lavc_codec);
    if (!codec)
        goto unknown;

    const enum AVPixelFormat *pix_fmts = codec->pix_fmts;
    if (!pix_fmts)
        goto unknown;

    int current = 0;
    for (int n = 0; pix_fmts[n] != AV_PIX_FMT_NONE; n++) {
        int fmt = pixfmt2imgfmt(pix_fmts[n]);
        if (!fmt)
            continue;
        current = current ? mp_imgfmt_select_best(current, fmt, srcfmt) : fmt;
    }

    if (!current)
        goto unknown;

    return current;

unknown:
    return IMGFMT_RGB24;
}

static const struct img_writer img_writers[] = {
    { "png", write_lavc, .lavc_codec = AV_CODEC_ID_PNG },
    { "ppm", write_lavc, .lavc_codec = AV_CODEC_ID_PPM },
    { "pgm", write_lavc, .lavc_codec = AV_CODEC_ID_PGM },
    { "pgmyuv", write_lavc, .lavc_codec = AV_CODEC_ID_PGMYUV },
    { "tga", write_lavc, .lavc_codec = AV_CODEC_ID_TARGA },
#if HAVE_JPEG
    { "jpg", write_jpeg },
    { "jpeg", write_jpeg },
#endif
};

static const struct img_writer *get_writer(const struct image_writer_opts *opts)
{
    const char *type = opts->format;

    for (size_t n = 0; n < sizeof(img_writers) / sizeof(img_writers[0]); n++) {
        const struct img_writer *writer = &img_writers[n];
        if (type && strcmp(type, writer->file_ext) == 0)
            return writer;
    }

    return &img_writers[0];
}

const char *image_writer_file_ext(const struct image_writer_opts *opts)
{
    struct image_writer_opts defs = image_writer_opts_defaults;

    if (!opts)
        opts = &defs;

    return get_writer(opts)->file_ext;
}

struct mp_image *convert_image(struct mp_image *image, int destfmt,
                               struct mp_log *log)
{
    int d_w = image->params.d_w;
    int d_h = image->params.d_h;
    bool is_anamorphic = image->w != d_w || image->h != d_h;

    // Caveat: no colorspace/levels conversion done if pixel formats equal
    //         it's unclear what colorspace/levels the target wants
    if (image->imgfmt == destfmt && !is_anamorphic)
        return mp_image_new_ref(image);

    struct mp_image *dst = mp_image_alloc(destfmt, d_w, d_h);
    if (!dst) {
        mp_err(log, "Out of memory.\n");
        return NULL;
    }
    mp_image_copy_attributes(dst, image);

    if (mp_image_swscale(dst, image, mp_sws_hq_flags) < 0) {
        mp_err(log, "Error when converting image.\n");
        talloc_free(dst);
        return NULL;
    }

    return dst;
}

bool write_image(struct mp_image *image, const struct image_writer_opts *opts,
                const char *filename, struct mp_log *log)
{
    struct image_writer_opts defs = image_writer_opts_defaults;
    if (!opts)
        opts = &defs;

    const struct img_writer *writer = get_writer(opts);
    struct image_writer_ctx ctx = { log, opts, writer, image->fmt };
    int destfmt = get_target_format(&ctx, image->imgfmt);

    struct mp_image *dst = convert_image(image, destfmt, log);
    if (!dst)
        return false;

    FILE *fp = fopen(filename, "wb");
    bool success = false;
    if (fp == NULL) {
        mp_err(log, "Error opening '%s' for writing!\n", filename);
    } else {
        success = writer->write(&ctx, dst, fp);
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
    opts.format = "png";
    write_image(image, &opts, filename, log);
}
