#ifndef MPLAYER_SD_H
#define MPLAYER_SD_H

#include "dec_sub.h"
#include "demux/packet.h"

// up to 210 ms overlaps or gaps are removed
#define SUB_GAP_THRESHOLD 0.210
// don't change timings if durations are smaller
#define SUB_GAP_KEEP 0.4

struct sd {
    struct mpv_global *global;
    struct mp_log *log;
    struct mp_subtitle_opts *opts;

    const struct sd_functions *driver;
    void *priv;

    struct attachment_list *attachments;
    struct mp_codec_params *codec;

    // Set to false as soon as the decoder discards old subtitle events.
    // (only needed if sd_functions.accept_packets_in_advance == false)
    bool preload_ok;
};

struct sd_functions {
    const char *name;
    bool accept_packets_in_advance;
    int  (*init)(struct sd *sd);
    void (*decode)(struct sd *sd, struct demux_packet *packet);
    void (*reset)(struct sd *sd);
    void (*select)(struct sd *sd, bool selected);
    void (*uninit)(struct sd *sd);

    bool (*accepts_packet)(struct sd *sd, double pts); // implicit default if NULL: true
    int (*control)(struct sd *sd, enum sd_ctrl cmd, void *arg);

    void (*get_bitmaps)(struct sd *sd, struct mp_osd_res dim, int format,
                        double pts, struct sub_bitmaps *res);
    char *(*get_text)(struct sd *sd, double pts);
};

struct lavc_conv;
struct lavc_conv *lavc_conv_create(struct mp_log *log, const char *codec_name,
                                   char *extradata, int extradata_len);
char *lavc_conv_get_extradata(struct lavc_conv *priv);
char **lavc_conv_decode(struct lavc_conv *priv, struct demux_packet *packet);
void lavc_conv_reset(struct lavc_conv *priv);
void lavc_conv_uninit(struct lavc_conv *priv);

char *filter_SDH(struct sd *sd, char *format, int n_ignored, char *data, int length);

#endif
