#ifndef MPLAYER_PACK_RECTANGLES_H
#define MPLAYER_PACK_RECTANGLES_H

struct pos {
    int x;
    int y;
};

struct bitmap_packer {
    int w;
    int h;
    int w_max;
    int h_max;
    int count;
    struct pos *in;
    struct pos *result;

    // internal
    int *scratch;
    int asize;
};

struct ass_image;

/* Reallocate packer->in for at least to desired number of items.
 * Also sets packer->count to the same value.
 */
void packer_set_size(struct bitmap_packer *packer, int size);

/* To use this, set packer->count to number of rectangles, w_max and h_max
 * to maximum output rectangle size, and w and h to start size (may be 0).
 * Write input sizes in packer->in.
 * Resulting packing will be written in packer->result.
 * w and h will be increased if necessary for successful packing.
 * Return value is -1 if packing failed because w and h were set to max
 * values but that wasn't enough, 1 if w or h was increased, and 0 otherwise.
 */
int packer_pack(struct bitmap_packer *packer);

/* Like above, but packer->count will be automatically set and
 * packer->in will be reallocated if needed and filled from the
 * given image list.
 */
int packer_pack_from_assimg(struct bitmap_packer *packer,
                            struct ass_image *imglist);

#endif
