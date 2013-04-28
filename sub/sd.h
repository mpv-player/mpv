#ifndef MPLAYER_SD_H
#define MPLAYER_SD_H

#include "dec_sub.h"

struct sd_functions {
    bool accept_packets_in_advance;
    bool (*supports_format)(const char *format);
    int  (*init)(struct sh_sub *sh, struct osd_state *osd);
    void (*decode)(struct sh_sub *sh, struct osd_state *osd,
                   void *data, int data_len, double pts, double duration);
    void (*get_bitmaps)(struct sh_sub *sh, struct osd_state *osd,
                        struct mp_osd_res dim, double pts,
                        struct sub_bitmaps *res);
    char *(*get_text)(struct sh_sub *sh, struct osd_state *osd, double pts);
    void (*reset)(struct sh_sub *sh, struct osd_state *osd);
    void (*switch_off)(struct sh_sub *sh, struct osd_state *osd);
    void (*uninit)(struct sh_sub *sh);
};

#endif
