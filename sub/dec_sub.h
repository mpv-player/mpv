#ifndef MPLAYER_DEC_SUB_H
#define MPLAYER_DEC_SUB_H

struct sh_sub;
struct osd_state;
struct ass_track;

enum sub_bitmap_type {
    SUBBITMAP_EMPTY,
    SUBBITMAP_LIBASS,
    SUBBITMAP_RGBA,
};

typedef struct mp_eosd_res {
    int w, h; // screen dimensions, including black borders
    int mt, mb, ml, mr; // borders (top, bottom, left, right)
} mp_eosd_res_t;

typedef struct sub_bitmaps {
    enum sub_bitmap_type type;

    struct ass_image *imgs;

    struct sub_bitmap {
        int w, h;
        int x, y;
        // Note: not clipped, going outside the screen area is allowed
        int dw, dh;
        void *bitmap;
    } *parts;
    int part_count;

    bool scaled;
    int bitmap_id, bitmap_pos_id;
} mp_eosd_images_t;

static inline bool is_text_sub(int type)
{
    return type == 't' || type == 'm' || type == 'a';
}

void sub_decode(struct sh_sub *sh, struct osd_state *osd, void *data,
                int data_len, double pts, double duration);
void sub_get_bitmaps(struct osd_state *osd, struct sub_bitmaps *res);
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
