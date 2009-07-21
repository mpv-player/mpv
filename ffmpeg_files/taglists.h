#include "libavcodec/avcodec.h"

struct mp_AVCodecTag {
    int id;
    unsigned int tag;
};

unsigned int mp_av_codec_get_tag(const struct mp_AVCodecTag * const *tags, enum CodecID id);
enum CodecID mp_av_codec_get_id(const struct mp_AVCodecTag * const *tags, unsigned int tag);
