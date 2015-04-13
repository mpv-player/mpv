#ifndef MPLAYER_SD_H
#define MPLAYER_SD_H

#include "dec_sub.h"
#include "demux/packet.h"

struct sd {
    struct mp_log *log;
    struct MPOpts *opts;

    const struct sd_functions *driver;
    void *priv;

    const char *codec;

    // Extra header data passed from demuxer
    char *extradata;
    int extradata_len;

    // Set to !=NULL if the input packets are being converted from another
    // format.
    const char *converted_from;

    // Video resolution used for subtitle decoding. Doesn't necessarily match
    // the resolution of the VO, nor does it have to be the OSD resolution.
    int sub_video_w, sub_video_h;

    // Shared renderer for ASS - done to avoid reloading embedded fonts.
    struct ass_library *ass_library;
    struct ass_renderer *ass_renderer;

    // If false, try to remove multiple subtitles.
    // (Only for decoders which have accept_packets_in_advance set.)
    bool no_remove_duplicates;

    // Set by sub converter
    const char *output_codec;
    char *output_extradata;
    int output_extradata_len;

    // Internal buffer for sd_conv_* functions
    struct sd_conv_buffer *sd_conv_buffer;
};

struct sd_functions {
    const char *name;
    bool accept_packets_in_advance;
    bool (*supports_format)(const char *format);
    int  (*init)(struct sd *sd);
    void (*decode)(struct sd *sd, struct demux_packet *packet);
    void (*reset)(struct sd *sd);
    void (*uninit)(struct sd *sd);

    void (*fix_events)(struct sd *sd);
    int (*control)(struct sd *sd, enum sd_ctrl cmd, void *arg);

    // decoder
    void (*get_bitmaps)(struct sd *sd, struct mp_osd_res dim, double pts,
                        struct sub_bitmaps *res);
    char *(*get_text)(struct sd *sd, double pts);

    // converter
    struct demux_packet *(*get_converted)(struct sd *sd);
};

void sd_conv_add_packet(struct sd *sd, void *data, int data_len, double pts,
                        double duration);
struct demux_packet *sd_conv_def_get_converted(struct sd *sd);
void sd_conv_def_reset(struct sd *sd);
void sd_conv_def_uninit(struct sd *sd);

#define SD_MAX_LINE_LEN 1000

#endif
