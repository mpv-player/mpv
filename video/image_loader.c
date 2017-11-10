#include <libavcodec/avcodec.h>

#include "common/common.h"
#include "mp_image.h"
#include "image_writer.h"

#include "image_loader.h"

struct mp_image *load_image_png_buf(void *buffer, size_t buffer_size, int imgfmt)
{
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_PNG);
    if (!codec)
        return NULL;

    AVCodecContext *avctx = avcodec_alloc_context3(codec);
    if (!avctx)
        return NULL;

    if (avcodec_open2(avctx, codec, NULL) < 0) {
        avcodec_free_context(&avctx);
        return NULL;
    }

    AVPacket *pkt = av_packet_alloc();
    if (pkt) {
        if (av_new_packet(pkt, buffer_size) >= 0)
            memcpy(pkt->data, buffer, buffer_size);
    }

    // (There is only 1 outcome: either it takes it and decodes it, or not.)
    avcodec_send_packet(avctx, pkt);
    avcodec_send_packet(avctx, NULL);

    av_packet_free(&pkt);

    struct mp_image *res = NULL;
    AVFrame *frame = av_frame_alloc();
    if (frame && avcodec_receive_frame(avctx, frame) >= 0) {
        struct mp_image *r = mp_image_from_av_frame(frame);
        if (r)
            res = convert_image(r, imgfmt, mp_null_log);
        talloc_free(r);
    }
    av_frame_free(&frame);

    avcodec_free_context(&avctx);
    return res;
}
