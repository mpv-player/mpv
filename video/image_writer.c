/*
 * This file is part of mplayer.
 *
 * mplayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>

#include "compat/libav.h"

#include "config.h"

#ifdef CONFIG_JPEG
#include <jpeglib.h>
#endif

#include "osdep/io.h"

#include "image_writer.h"
#include "talloc.h"
#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/fmt-conversion.h"
#include "video/sws_utils.h"

#include "core/m_option.h"

const struct image_writer_opts image_writer_opts_defaults = {
    .format = "jpg",
    .png_compression = 7,
    .jpeg_quality = 90,
    .jpeg_optimize = 100,
    .jpeg_smooth = 0,
    .jpeg_dpi = 72,
    .jpeg_progressive = 0,
    .jpeg_baseline = 1,
};

#define OPT_BASE_STRUCT struct image_writer_opts

const struct m_sub_options image_writer_conf = {
    .opts = (m_option_t[]) {
        OPT_INTRANGE("jpeg-quality", jpeg_quality, 0, 0, 100),
        OPT_INTRANGE("jpeg-optimize", jpeg_optimize, 0, 0, 100),
        OPT_INTRANGE("jpeg-smooth", jpeg_smooth, 0, 0, 100),
        OPT_INTRANGE("jpeg-dpi", jpeg_dpi, M_OPT_MIN, 1, 99999),
        OPT_FLAG("jpeg-progressive", jpeg_progressive, 0),
        OPT_FLAG("jpeg-baseline", jpeg_baseline, 0),
        OPT_INTRANGE("png-compression", png_compression, 0, 0, 9),
        OPT_STRING("format", format, 0),
        {0},
    },
    .size = sizeof(struct image_writer_opts),
    .defaults = &image_writer_opts_defaults,
};

struct image_writer_ctx {
    const struct image_writer_opts *opts;
    const struct img_writer *writer;
};

struct img_writer {
    const char *file_ext;
    int (*write)(struct image_writer_ctx *ctx, mp_image_t *image, FILE *fp);
    int *pixfmts;
    int lavc_codec;
};

static int write_lavc(struct image_writer_ctx *ctx, mp_image_t *image, FILE *fp)
{
    int success = 0;
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
    if (ctx->writer->lavc_codec == AV_CODEC_ID_PNG)
        avctx->compression_level = ctx->opts->png_compression;

    if (avcodec_open2(avctx, codec, NULL) < 0) {
     print_open_fail:
        mp_msg(MSGT_CPLAYER, MSGL_INFO, "Could not open libavcodec encoder"
               " for saving images\n");
        goto error_exit;
    }

    pic = avcodec_alloc_frame();
    if (!pic)
        goto error_exit;
    avcodec_get_frame_defaults(pic);
    for (int n = 0; n < 4; n++) {
        pic->data[n] = image->planes[n];
        pic->linesize[n] = image->stride[n];
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
    avcodec_free_frame(&pic);
    av_free_packet(&pkt);
    return success;
}

#ifdef CONFIG_JPEG

static void write_jpeg_error_exit(j_common_ptr cinfo)
{
  // NOTE: do not write error message, too much effort to connect the libjpeg
  //       log callbacks with mplayer's log function mp_msp()

  // Return control to the setjmp point
  longjmp(*(jmp_buf*)cinfo->client_data, 1);
}

static int write_jpeg(struct image_writer_ctx *ctx, mp_image_t *image, FILE *fp)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = write_jpeg_error_exit;

    jmp_buf error_return_jmpbuf;
    cinfo.client_data = &error_return_jmpbuf;
    if (setjmp(cinfo.client_data)) {
        jpeg_destroy_compress(&cinfo);
        return 0;
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
    cinfo.density_unit = 1; /* 0=unknown, 1=dpi, 2=dpcm */
    cinfo.X_density = ctx->opts->jpeg_dpi;
    cinfo.Y_density = ctx->opts->jpeg_dpi;
    cinfo.write_Adobe_marker = TRUE;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, ctx->opts->jpeg_quality, ctx->opts->jpeg_baseline);
    cinfo.optimize_coding = ctx->opts->jpeg_optimize;
    cinfo.smoothing_factor = ctx->opts->jpeg_smooth;

    if (ctx->opts->jpeg_progressive)
        jpeg_simple_progression(&cinfo);

    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row_pointer[1];
        row_pointer[0] = image->planes[0] +
                         cinfo.next_scanline * image->stride[0];
        jpeg_write_scanlines(&cinfo, row_pointer,1);
    }

    jpeg_finish_compress(&cinfo);

    jpeg_destroy_compress(&cinfo);

    return 1;
}

#endif

static const struct img_writer img_writers[] = {
    { "png", write_lavc, .lavc_codec = AV_CODEC_ID_PNG },
    { "ppm", write_lavc, .lavc_codec = AV_CODEC_ID_PPM },
    { "pgm", write_lavc,
      .lavc_codec = AV_CODEC_ID_PGM,
      .pixfmts = (int[]) { IMGFMT_Y8, 0 },
    },
    { "pgmyuv", write_lavc,
      .lavc_codec = AV_CODEC_ID_PGMYUV,
      .pixfmts = (int[]) { IMGFMT_420P, 0 },
    },
    { "tga", write_lavc,
      .lavc_codec = AV_CODEC_ID_TARGA,
      .pixfmts = (int[]) { IMGFMT_BGR24, IMGFMT_BGRA, IMGFMT_BGR15_LE,
                           IMGFMT_Y8, 0},
    },
#ifdef CONFIG_JPEG
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

int write_image(struct mp_image *image, const struct image_writer_opts *opts,
                const char *filename)
{
    struct mp_image *allocated_image = NULL;
    struct image_writer_opts defs = image_writer_opts_defaults;
    int d_w = image->display_w ? image->display_w : image->w;
    int d_h = image->display_h ? image->display_h : image->h;
    bool is_anamorphic = image->w != d_w || image->h != d_h;

    if (!opts)
        opts = &defs;

    const struct img_writer *writer = get_writer(opts);
    struct image_writer_ctx ctx = { opts, writer };
    int destfmt = IMGFMT_RGB24;

    if (writer->pixfmts) {
        destfmt = writer->pixfmts[0];   // default to first pixel format
        for (int *fmt = writer->pixfmts; *fmt; fmt++) {
            if (*fmt == image->imgfmt) {
                destfmt = *fmt;
                break;
            }
        }
    }

    // Caveat: no colorspace/levels conversion done if pixel formats equal
    //         it's unclear what colorspace/levels the target wants
    if (image->imgfmt != destfmt || is_anamorphic) {
        struct mp_image *dst = mp_image_alloc(destfmt, d_w, d_h);
        mp_image_copy_attributes(dst, image);

        int flags = SWS_LANCZOS | SWS_FULL_CHR_H_INT | SWS_FULL_CHR_H_INP |
                    SWS_ACCURATE_RND | SWS_BITEXACT;

        mp_image_swscale(dst, image, flags);

        allocated_image = dst;
        image = dst;
    }

    FILE *fp = fopen(filename, "wb");
    int success = 0;
    if (fp == NULL) {
        mp_msg(MSGT_CPLAYER, MSGL_ERR,
               "Error opening '%s' for writing!\n", filename);
    } else {
        success = writer->write(&ctx, image, fp);
        success = !fclose(fp) && success;
        if (!success)
            mp_msg(MSGT_CPLAYER, MSGL_ERR, "Error writing file '%s'!\n",
                   filename);
    }

    talloc_free(allocated_image);

    return success;
}

void dump_png(struct mp_image *image, const char *filename)
{
    struct image_writer_opts opts = image_writer_opts_defaults;
    opts.format = "png";
    write_image(image, &opts, filename);
}
