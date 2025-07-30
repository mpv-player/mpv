#ifndef MPLAYER_SD_H
#define MPLAYER_SD_H

#include "dec_sub.h"
#include "demux/packet.h"
#include "misc/bstr.h"

// up to 210 ms overlaps or gaps are removed
#define SUB_GAP_THRESHOLD 0.210
// don't change timings if durations are smaller
#define SUB_GAP_KEEP 0.4
// slight offset when sub seeking or sub stepping
#define SUB_SEEK_OFFSET 0.01
#define SUB_SEEK_WITHOUT_VIDEO_OFFSET 0.1

enum ass_style_override {
    ASS_STYLE_OVERRIDE_NONE,
    ASS_STYLE_OVERRIDE_YES,
    ASS_STYLE_OVERRIDE_SCALE,
    ASS_STYLE_OVERRIDE_FORCE,
    ASS_STYLE_OVERRIDE_STRIP,
};

struct sd {
    struct mpv_global *global;
    struct mp_log *log;
    struct mp_subtitle_opts *opts;
    struct mp_subtitle_shared_opts *shared_opts;

    const struct sd_functions *driver;
    void *priv;
    int order;

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

    struct sub_bitmaps *(*get_bitmaps)(struct sd *sd, struct mp_osd_res dim,
                                       int format, double pts);
    char *(*get_text)(struct sd *sd, double pts, enum sd_text_type type);
    struct sd_times (*get_times)(struct sd *sd, double pts);
};

// lavc_conv.c
struct lavc_conv;
struct lavc_conv *lavc_conv_create(struct sd *sd);
char *lavc_conv_get_extradata(struct lavc_conv *priv);
char **lavc_conv_decode(struct lavc_conv *priv, struct demux_packet *packet,
                        double *sub_pts, double *sub_duration);
bool lavc_conv_is_styled(struct lavc_conv *priv);
void lavc_conv_reset(struct lavc_conv *priv);
void lavc_conv_uninit(struct lavc_conv *priv);

struct mp_sub_filter_opts {
    bool sub_filter_SDH;
    bool sub_filter_SDH_harder;
    char **sub_filter_SDH_enclosures;
    bool rf_enable;
    bool rf_plain;
    char **rf_items;
    char **jsre_items;
    bool rf_warn;
};

struct sd_filter {
    struct mpv_global *global;
    struct mp_log *log;
    struct demux_packet_pool *packet_pool;
    struct mp_sub_filter_opts *opts;
    const struct sd_filter_functions *driver;

    void *priv;

    // Static codec parameters. Set by sd; cannot be changed by filter.
    char *codec;
    char *event_format;
};

struct sd_filter_functions {
    bool (*init)(struct sd_filter *ft);

    // Filter an ASS event (usually in the Matroska format, but event_format
    // can be used to determine details).
    // Returning NULL is interpreted as dropping the event completely.
    // Returning pkt makes it no-op.
    // If the returned packet is not pkt or NULL, it must have been properly
    // allocated.
    // pkt is owned by the caller (and freed by the caller when needed).
    // Note: as by normal demux_packet rules, you must not modify any fields in
    //       it, or the data referenced by it. You must create a new demux_packet
    //       when modifying data.
    struct demux_packet *(*filter)(struct sd_filter *ft,
                                   struct demux_packet *pkt);

    void (*uninit)(struct sd_filter *ft);
};

extern const struct sd_filter_functions sd_filter_sdh;
extern const struct sd_filter_functions sd_filter_regex;
extern const struct sd_filter_functions sd_filter_jsre;


// convenience utils for filters with ass codec

// num commas to skip at an ass-event before the "Text" field (always last)
// (doesn't change, can be retrieved once on filter init)
int sd_ass_fmt_offset(const char *event_format);

// the event (pkt->buffer) "Text" content according to the calculated offset.
// on malformed event: warns and returns (bstr){NULL,0}
bstr sd_ass_pkt_text(struct sd_filter *ft, struct demux_packet *pkt, int offset);

// convert \0-terminated "Text" (ass) content to plaintext, possibly in-place.
// result.start is *out, result.len is strlen(in) or smaller.
// (*out)[result.len] is always set to \0. *out == in is allowed.
// *out must be a talloc-allocated buffer or NULL, and will be reallocated if needed.
// *out will not be reallocated if *out == in.
bstr sd_ass_to_plaintext(char **out, const char *in);

#endif
