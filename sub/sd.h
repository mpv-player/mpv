#ifndef MPLAYER_SD_H
#define MPLAYER_SD_H

#include "dec_sub.h"
#include "demux/demux_packet.h"

struct sd {
    struct MPOpts *opts;

    const struct sd_functions *driver;
    void *priv;

    const char *codec;

    // Extra header data passed from demuxer
    char *extradata;
    int extradata_len;

    // Video resolution used for subtitle decoding. Doesn't necessarily match
    // the resolution of the VO, nor does it have to be the OSD resolution.
    int sub_video_w, sub_video_h;

    // Make sd_ass use an existing track
    struct ass_track *ass_track;

    // Shared renderer for ASS - done to avoid reloading embedded fonts.
    struct ass_library *ass_library;
    struct ass_renderer *ass_renderer;
};

struct sd_functions {
    bool accept_packets_in_advance;
    bool (*supports_format)(const char *format);
    int  (*init)(struct sd *sd);
    void (*decode)(struct sd *sd, struct demux_packet *packet);
    void (*get_bitmaps)(struct sd *sd, struct mp_osd_res dim, double pts,
                        struct sub_bitmaps *res);
    char *(*get_text)(struct sd *sd, double pts);
    void (*reset)(struct sd *sd);
    void (*uninit)(struct sd *sd);
};

#endif
