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
#include <inttypes.h>
#include <unistd.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libplacebo/utils/libav.h>

#include "common/msg.h"
#include "config.h"

#if HAVE_JPEG
#include <setjmp.h>
#include <jpeglib.h>
#endif

#include "osdep/io.h"
#include "misc/path_utils.h"

#include "common/av_common.h"
#include "common/msg.h"
#include "image_writer.h"
#include "mpv_talloc.h"
#include "video/fmt-conversion.h"
#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/sws_utils.h"

#include "options/m_option.h"

const struct image_writer_opts image_writer_opts_defaults = {
    .format = AV_CODEC_ID_MJPEG,
    .high_bit_depth = true,
    .png_compression = 7,
    .png_filter = 5,
    .jpeg_quality = 90,
    .jpeg_source_chroma = true,
    .webp_quality = 75,
    .webp_compression = 4,
    .jxl_distance = 1.0,
    .jxl_effort = 4,
    .avif_encoder = "libaom-av1",
    .avif_opts = (char*[]){
        "usage",    "allintra",
        "crf",      "0",
        "cpu-used", "8",
        NULL
    },
    .tag_csp = true,
};

const struct m_opt_choice_alternatives mp_image_writer_formats[] = {
    {"jpg",  AV_CODEC_ID_MJPEG},
    {"jpeg", AV_CODEC_ID_MJPEG},
    {"png",  AV_CODEC_ID_PNG},
    {"webp", AV_CODEC_ID_WEBP},
#if HAVE_JPEGXL
    {"jxl",  AV_CODEC_ID_JPEGXL},
#endif
#if HAVE_AVIF_MUXER
    {"avif",  AV_CODEC_ID_AV1},
#endif
    {0}
};

#define OPT_BASE_STRUCT struct image_writer_opts

const struct m_option image_writer_opts[] = {
    {"format", OPT_CHOICE_C(format, mp_image_writer_formats)},
    {"jpeg-quality", OPT_INT(jpeg_quality), M_RANGE(0, 100)},
    {"jpeg-source-chroma", OPT_BOOL(jpeg_source_chroma)},
    {"png-compression", OPT_INT(png_compression), M_RANGE(0, 9)},
    {"png-filter", OPT_INT(png_filter), M_RANGE(0, 5)},
    {"webp-lossless", OPT_BOOL(webp_lossless)},
    {"webp-quality", OPT_INT(webp_quality), M_RANGE(0, 100)},
    {"webp-compression", OPT_INT(webp_compression), M_RANGE(0, 6)},
#if HAVE_JPEGXL
    {"jxl-distance", OPT_DOUBLE(jxl_distance), M_RANGE(0.0, 15.0)},
    {"jxl-effort", OPT_INT(jxl_effort), M_RANGE(1, 9)},
#endif
#if HAVE_AVIF_MUXER
    {"avif-encoder", OPT_STRING(avif_encoder)},
    {"avif-opts", OPT_KEYVALUELIST(avif_opts)},
    {"avif-pixfmt", OPT_STRING(avif_pixfmt)},
#endif
    {"high-bit-depth", OPT_BOOL(high_bit_depth)},
    {"tag-colorspace", OPT_BOOL(tag_csp)},
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

static void prepare_avframe(AVFrame *pic, AVCodecContext *avctx,
                            mp_image_t *image, bool tag_csp,
                            struct mp_log *log)
{
    for (int n = 0; n < 4; n++) {
        pic->data[n] = image->planes[n];
        pic->linesize[n] = image->stride[n];
    }
    pic->format = avctx->pix_fmt;
    pic->width = avctx->width;
    pic->height = avctx->height;
    pl_avframe_set_repr(pic, image->params.repr);
    avctx->colorspace = pic->colorspace;
    avctx->color_range = pic->color_range;

    if (!tag_csp)
        return;
    pl_avframe_set_color(pic, image->params.color);
    avctx->color_primaries = pic->color_primaries;
    avctx->color_trc = pic->color_trc;
    avctx->chroma_sample_location = pic->chroma_location =
        pl_chroma_to_av(image->params.chroma_location);

    mp_dbg(log, "mapped color params:\n"
        "  trc = %s\n"
        "  primaries = %s\n"
        "  range = %s\n"
        "  colorspace = %s\n"
        "  chroma_location = %s\n",
        av_color_transfer_name(avctx->color_trc),
        av_color_primaries_name(avctx->color_primaries),
        av_color_range_name(avctx->color_range),
        av_color_space_name(avctx->colorspace),
        av_chroma_location_name(avctx->chroma_sample_location)
    );
}

static bool write_lavc(struct image_writer_ctx *ctx, mp_image_t *image, FILE *fp)
{
    bool success = false;
    AVFrame *pic = NULL;
    AVPacket *pkt = NULL;

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
    avctx->pix_fmt = imgfmt2pixfmt(image->imgfmt);
    if (codec->id == AV_CODEC_ID_MJPEG) {
        // Annoying deprecated garbage for the jpg encoder.
        if (image->params.repr.levels == PL_COLOR_LEVELS_FULL)
            avctx->pix_fmt = replace_j_format(avctx->pix_fmt);
    }
    if (avctx->pix_fmt == AV_PIX_FMT_NONE) {
        MP_ERR(ctx, "Image format %s not supported by lavc.\n",
               mp_imgfmt_to_name(image->imgfmt));
        goto error_exit;
    }

    if (codec->id == AV_CODEC_ID_MJPEG) {
        avctx->flags |= AV_CODEC_FLAG_QSCALE;
        // jpeg_quality is set below
    } else if (codec->id == AV_CODEC_ID_PNG) {
        avctx->compression_level = ctx->opts->png_compression;
        av_opt_set_int(avctx, "pred", ctx->opts->png_filter,
                       AV_OPT_SEARCH_CHILDREN);
    } else if (codec->id == AV_CODEC_ID_WEBP) {
        avctx->compression_level = ctx->opts->webp_compression;
        av_opt_set_int(avctx, "lossless", ctx->opts->webp_lossless,
                       AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(avctx, "quality", ctx->opts->webp_quality,
                       AV_OPT_SEARCH_CHILDREN);
#if HAVE_JPEGXL
    } else if (codec->id == AV_CODEC_ID_JPEGXL) {
        av_opt_set_double(avctx, "distance", ctx->opts->jxl_distance,
                          AV_OPT_SEARCH_CHILDREN);
        av_opt_set_int(avctx, "effort", ctx->opts->jxl_effort,
                       AV_OPT_SEARCH_CHILDREN);
#endif
    }

    if (avcodec_open2(avctx, codec, NULL) < 0) {
     print_open_fail:
        MP_ERR(ctx, "Could not open libavcodec encoder for saving images\n");
        goto error_exit;
    }

    pic = av_frame_alloc();
    if (!pic)
        goto error_exit;
    prepare_avframe(pic, avctx, image, ctx->opts->tag_csp, ctx->log);
    if (codec->id == AV_CODEC_ID_MJPEG) {
        int qscale = 1 + (100 - ctx->opts->jpeg_quality) * 30 / 100;
        pic->quality = qscale * FF_QP2LAMBDA;
    }

    int ret = avcodec_send_frame(avctx, pic);
    if (ret < 0)
        goto error_exit;
    ret = avcodec_send_frame(avctx, NULL); // send EOF
    if (ret < 0)
        goto error_exit;
    pkt = av_packet_alloc();
    if (!pkt)
        goto error_exit;
    ret = avcodec_receive_packet(avctx, pkt);
    if (ret < 0)
        goto error_exit;

    success = fwrite(pkt->data, pkt->size, 1, fp) == 1;

error_exit:
    avcodec_free_context(&avctx);
    av_frame_free(&pic);
    av_packet_free(&pkt);
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

#if HAVE_AVIF_MUXER

static void log_side_data(struct image_writer_ctx *ctx, AVPacketSideData *data,
                          size_t size)
{
    if (!mp_msg_test(ctx->log, MSGL_DEBUG))
        return;
    char dbgbuff[129];
    if (size)
        MP_DBG(ctx, "write_avif() packet side data:\n");
    for (int i = 0; i < size; i++) {
        AVPacketSideData *sd = &data[i];
        for (int k = 0; k < MPMIN(sd->size, 64); k++)
            snprintf(dbgbuff + k*2, 3, "%02x", (int)sd->data[k]);
        MP_DBG(ctx, "  [%d] = {[%s], '%s'}\n",
               i, av_packet_side_data_name(sd->type), dbgbuff);
    }
}

static bool write_avif(struct image_writer_ctx *ctx, mp_image_t *image, FILE *fp)
{
    const AVCodec *codec = NULL;
    const AVOutputFormat *ofmt = NULL;
    AVCodecContext *avctx = NULL;
    AVIOContext *avioctx = NULL;
    AVFormatContext *fmtctx = NULL;
    AVStream *stream = NULL;
    AVFrame *pic = NULL;
    AVPacket *pkt = NULL;
    int ret;
    bool success = false;

    codec = avcodec_find_encoder_by_name(ctx->opts->avif_encoder);
    if (!codec) {
        MP_ERR(ctx, "Could not find encoder '%s', for saving images\n",
               ctx->opts->avif_encoder);
        goto free_data;
    }

    ofmt = av_guess_format("avif", NULL, NULL);
    if (!ofmt) {
        MP_ERR(ctx, "Could not guess output format 'avif'\n");
        goto free_data;
    }

    avctx = avcodec_alloc_context3(codec);
    if (!avctx) {
        MP_ERR(ctx, "Failed to allocate AVContext.\n");
        goto free_data;
    }

    avctx->width = image->w;
    avctx->height = image->h;
    avctx->time_base = (AVRational){1, 30};
    avctx->pkt_timebase = (AVRational){1, 30};
    avctx->codec_type = AVMEDIA_TYPE_VIDEO;
    avctx->pix_fmt = imgfmt2pixfmt(image->imgfmt);
    if (avctx->pix_fmt == AV_PIX_FMT_NONE) {
        MP_ERR(ctx, "Image format %s not supported by lavc.\n",
               mp_imgfmt_to_name(image->imgfmt));
        goto free_data;
    }

    av_opt_set_int(avctx, "still-picture", 1, AV_OPT_SEARCH_CHILDREN);

    AVDictionary *avd = NULL;
    mp_set_avdict(&avd, ctx->opts->avif_opts);
    av_opt_set_dict2(avctx, &avd, AV_OPT_SEARCH_CHILDREN);
    av_dict_free(&avd);

    pic = av_frame_alloc();
    if (!pic) {
        MP_ERR(ctx, "Could not allocate AVFrame\n");
        goto free_data;
    }

    prepare_avframe(pic, avctx, image, ctx->opts->tag_csp, ctx->log);
    // Not setting this flag caused ffmpeg to output avif that was not passing
    // standard checks but ffmpeg would still read and not complain...
    avctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    ret = avcodec_open2(avctx, codec, NULL);
    if (ret < 0) {
        MP_ERR(ctx, "Could not open libavcodec encoder for saving images\n");
        goto free_data;
    }

    avio_open_dyn_buf(&avioctx);
    MP_HANDLE_OOM(avioctx);

    fmtctx = avformat_alloc_context();
    if (!fmtctx) {
        MP_ERR(ctx, "Could not allocate format context\n");
        goto free_data;
    }
    fmtctx->pb = avioctx;
    fmtctx->oformat = ofmt;

    stream = avformat_new_stream(fmtctx, codec);
    if (!stream) {
        MP_ERR(ctx, "Could not allocate stream\n");
        goto free_data;
    }

    ret = avcodec_parameters_from_context(stream->codecpar, avctx);
    if (ret < 0) {
        MP_ERR(ctx, "Could not copy parameters from context\n");
        goto free_data;
    }

    ret = avformat_init_output(fmtctx, NULL);
    if (ret < 0) {
        MP_ERR(ctx, "Could not initialize output\n");
        goto free_data;
    }

    ret = avformat_write_header(fmtctx, NULL);
    if (ret < 0) {
        MP_ERR(ctx, "Could not write format header\n");
        goto free_data;
    }

    pkt = av_packet_alloc();
    if (!pkt) {
        MP_ERR(ctx, "Could not allocate packet\n");
        goto free_data;
    }

    ret = avcodec_send_frame(avctx, pic);
    if (ret < 0) {
        MP_ERR(ctx, "Error sending frame\n");
        goto free_data;
    }
    ret = avcodec_send_frame(avctx, NULL); // send EOF
    if (ret < 0)
        goto free_data;

    int pts = 0;
    log_side_data(ctx, avctx->coded_side_data, avctx->nb_coded_side_data);
    while (ret >= 0) {
        ret = avcodec_receive_packet(avctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        if (ret < 0) {
            MP_ERR(ctx, "Error receiving packet\n");
            goto free_data;
        }
        pkt->dts = pkt->pts = ++pts;
        pkt->stream_index = stream->index;
        log_side_data(ctx, pkt->side_data, pkt->side_data_elems);

        ret = av_write_frame(fmtctx, pkt);
        if (ret < 0) {
            MP_ERR(ctx, "Error writing frame\n");
            goto free_data;
        }
        av_packet_unref(pkt);
    }

    ret = av_write_trailer(fmtctx);
    if (ret < 0) {
        MP_ERR(ctx, "Could not write trailer\n");
        goto free_data;
    }
    MP_DBG(ctx, "write_avif(): avio_size() = %"PRIi64"\n", avio_size(avioctx));

    uint8_t *buf = NULL;
    int written_size = avio_close_dyn_buf(avioctx, &buf);
    success = fwrite(buf, written_size, 1, fp) == 1;
    av_freep(&buf);

free_data:
    avformat_free_context(fmtctx);
    avcodec_free_context(&avctx);
    av_packet_free(&pkt);
    av_frame_free(&pic);

    return success;
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
    if (!target) {
        mp_dbg(ctx->log, "Falling back to high-depth format.\n");
        target = get_encoder_format(codec, srcfmt, true);
    }

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
    return opts->format == AV_CODEC_ID_PNG
#if HAVE_JPEGXL
           || opts->format == AV_CODEC_ID_JPEGXL
#endif
#if HAVE_AVIF_MUXER
           || opts->format == AV_CODEC_ID_AV1
#endif
    ;
}

bool image_writer_flexible_csp(const struct image_writer_opts *opts)
{
    if (!opts->tag_csp)
        return false;
    return false
#if HAVE_JPEGXL
        || opts->format == AV_CODEC_ID_JPEGXL
#endif
#if HAVE_AVIF_MUXER
        || opts->format == AV_CODEC_ID_AV1
#endif
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 58, 100)
        // This version added support for cICP tag writing
        || opts->format == AV_CODEC_ID_PNG
#endif
    ;
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
                                      enum pl_color_levels yuv_levels,
                                      const struct image_writer_opts *opts,
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
        .color = image->params.color,
        .repr = image->params.repr,
        .chroma_location = image->params.chroma_location,
        .crop = {0, 0, d_w, d_h},
    };
    mp_image_params_guess_csp(&p);

    if (!image_writer_flexible_csp(opts)) {
        // If our format can't tag csps, set something sane
        p.color.primaries = PL_COLOR_PRIM_BT_709;
        p.color.transfer = PL_COLOR_TRC_UNKNOWN;
        p.light = MP_CSP_LIGHT_DISPLAY;
        p.color.hdr = (struct pl_hdr_metadata){0};
        if (p.repr.sys != PL_COLOR_SYSTEM_RGB) {
            p.repr.levels = yuv_levels;
            p.repr.sys = PL_COLOR_SYSTEM_BT_601;
            p.chroma_location = PL_CHROMA_CENTER;
        }
        mp_image_params_guess_csp(&p);
    }

    if (mp_image_params_equal(&p, &image->params))
        return mp_image_new_ref(image);

    mp_verbose(log, "will convert image to %s\n", mp_imgfmt_to_name(p.imgfmt));

    struct mp_image *src = image;
    if (mp_image_crop_valid(&src->params) &&
        (mp_rect_w(src->params.crop) != src->w ||
         mp_rect_h(src->params.crop) != src->h))
    {
        src = mp_image_new_ref(src);
        if (!src) {
            mp_err(log, "mp_image_new_ref failed!\n");
            return NULL;
        }
        mp_image_crop_rc(src, src->params.crop);
    }

    struct mp_image *dst = mp_image_alloc(p.imgfmt, p.w, p.h);
    if (!dst) {
        mp_err(log, "Out of memory.\n");
        return NULL;
    }
    mp_image_copy_attributes(dst, src);

    dst->params = p;

    struct mp_sws_context *sws = mp_sws_alloc(NULL);
    sws->log = log;
    if (global)
        mp_sws_enable_cmdline_opts(sws, global);
    bool ok = mp_sws_scale(sws, dst, src) >= 0;
    talloc_free(sws);

    if (src != image)
        talloc_free(src);

    if (!ok) {
        mp_err(log, "Error when converting image.\n");
        talloc_free(dst);
        return NULL;
    }

    return dst;
}

bool write_image(struct mp_image *image, const struct image_writer_opts *opts,
                 const char *filename, struct mpv_global *global,
                 struct mp_log *log, bool overwrite)
{
    struct image_writer_opts defs = image_writer_opts_defaults;
    if (!opts)
        opts = &defs;

    mp_verbose(log, "input: %s\n", mp_image_params_to_str(&image->params));

    struct image_writer_ctx ctx = { log, opts, image->fmt };
    bool (*write)(struct image_writer_ctx *, mp_image_t *, FILE *) = write_lavc;
    int destfmt = 0;

#if HAVE_JPEG
    if (opts->format == AV_CODEC_ID_MJPEG) {
        write = write_jpeg;
        destfmt = IMGFMT_RGB24;
    }
#endif
#if HAVE_AVIF_MUXER
    if (opts->format == AV_CODEC_ID_AV1) {
        write = write_avif;
        if (opts->avif_pixfmt && opts->avif_pixfmt[0])
            destfmt = mp_imgfmt_from_name(bstr0(opts->avif_pixfmt));
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

    enum pl_color_levels levels; // Ignored if destfmt is a RGB format
    if (opts->format == AV_CODEC_ID_WEBP) {
        levels = PL_COLOR_LEVELS_LIMITED;
    } else {
        levels = PL_COLOR_LEVELS_FULL;
    }

    struct mp_image *dst = convert_image(image, destfmt, levels, opts, global, log);
    if (!dst)
        return false;

    bool success = false;
    FILE *fp = fopen(filename, overwrite ? "wb" : "wbx");
    if (!fp) {
        mp_err(log, "Error creating '%s' for writing: %s!\n",
               filename, mp_strerror(errno));
        goto done;
    }

    success = write(&ctx, dst, fp);
    if (fclose(fp) || !success) {
        mp_err(log, "Error writing file '%s'!\n", filename);
        unlink(filename);
    }

done:
    talloc_free(dst);
    return success;
}

void dump_png(struct mp_image *image, const char *filename, struct mp_log *log)
{
    struct image_writer_opts opts = image_writer_opts_defaults;
    opts.format = AV_CODEC_ID_PNG;
    write_image(image, &opts, filename, NULL, log, true);
}
