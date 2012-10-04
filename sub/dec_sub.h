#ifndef MPLAYER_DEC_SUB_H
#define MPLAYER_DEC_SUB_H

#include <stdbool.h>
#include <stdint.h>

struct sh_sub;
struct osd_state;
struct ass_track;

enum sub_bitmap_format {
    SUBBITMAP_EMPTY = 0,// no bitmaps; always has num_parts==0
    SUBBITMAP_LIBASS,   // A8, with a per-surface blend color (libass.color)
    SUBBITMAP_RGBA,     // B8G8R8A8, can be scaled
    SUBBITMAP_OLD,      // I8A8 (monochrome), premultiplied alpha
    SUBBITMAP_OLD_PLANAR, // like previous, but bitmap points to old_osd_planar

    SUBBITMAP_COUNT
};

// For SUBBITMAP_OLD_PANAR
struct old_osd_planar {
    unsigned char *bitmap;
    unsigned char *alpha;
};

typedef struct mp_eosd_res {
    int w, h; // screen dimensions, including black borders
    int mt, mb, ml, mr; // borders (top, bottom, left, right)
} mp_eosd_res_t;

struct sub_bitmap {
    void *bitmap;
    int stride;
    int w, h;
    int x, y;
    // Note: not clipped, going outside the screen area is allowed
    int dw, dh;

    union {
        struct {
            uint32_t color;
        } libass;
    };
};

typedef struct sub_bitmaps {
    int render_index;   // for VO cache state (limited by MAX_OSD_PARTS)

    enum sub_bitmap_format format;
    bool scaled;        // if false, dw==w && dh==h

    struct sub_bitmap *parts;
    int num_parts;

    // Provided for VOs with old code
    struct ass_image *imgs;

    // Incremented on each change
    int bitmap_id, bitmap_pos_id;
} mp_eosd_images_t;

struct sub_render_params {
    double pts;
    struct mp_eosd_res dim;
    double normal_scale;
    double vsfilter_scale;

    bool support_rgba;
};

static inline bool is_text_sub(int type)
{
    return type == 't' || type == 'm' || type == 'a';
}

void sub_decode(struct sh_sub *sh, struct osd_state *osd, void *data,
                int data_len, double pts, double duration);
void sub_get_bitmaps(struct osd_state *osd, struct sub_render_params *params,
                     struct sub_bitmaps *res);
void sub_init(struct sh_sub *sh, struct osd_state *osd);
void sub_reset(struct sh_sub *sh, struct osd_state *osd);
void sub_switchoff(struct sh_sub *sh, struct osd_state *osd);
void sub_uninit(struct sh_sub *sh);

struct sh_sub *sd_ass_create_from_track(struct ass_track *track,
                                        bool vsfilter_aspect,
                                        struct MPOpts *opts);

#ifdef CONFIG_ASS
struct ass_track *sub_get_ass_track(struct osd_state *osd);
#endif

#endif
