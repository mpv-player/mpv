/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>

#include <libswscale/swscale.h>

#include "config.h"
#include "vo.h"
#include "video/mp_image.h"
#include "video/sws_utils.h"
#include "osdep/terminal.h"
#include "options/m_config.h"
#include "osdep/io.h"
#include "sub/osd.h"

// T_* - terminal escape sequence.
// *_F - contains a printf-style format.
// *_P - is a prefix, arguments must be added.
#define ESC "\x1B"
#define CSI "\x1B["
#define OSC "\x1B]"
#define DCS "\x1BP"
#define T_TITLE_STCK_PSH   CSI"22;0t"   // push current title onto the stack.
#define T_TITLE_STCK_POP   CSI"23;0t"   // restore title from the stack top.
#define T_TITLE_SET_F      OSC"0;%s\x7" // change the title to "%s".
#define T_STATE_SAVE       CSI"?47h"    // save current terminal state.
#define T_STATE_RSTR       CSI"?47l"    // restore saved terminal state.
#define T_BUFF_ENBL        CSI"?1049h"  // enable the alternate buffer.
#define T_BUFF_DSBL        CSI"?1049l"  // disable the alternate buffer.
#define T_CURS_STCK_PSH    ESC"7"       // save current cursor pos. on stack.
#define T_CURS_STCK_POP    ESC"8"       // restore cursor pos. from stack.
#define T_CURS_HIDE        CSI"?25l"    // make cursor invisible.
#define T_CURS_SHOW        CSI"?25h"    // make cursor visible.
#define T_CURS_MOV_F       CSI"%d;%df"  // move the cursor to (x,y).
#define T_CURS_FWD_F       CSI"%dC"     // move the cursor n chars right.
#define T_CURS_HOME        CSI"H"       // move the cursor to (0,0).
#define T_CLEAR_SCR_AFTC   CSI"0J"      // clear screen from cursor, to the end.
#define T_CLEAR_SCR_BEFC   CSI"1J"      // clear screen from (0,0) to cursos.
#define T_CLEAR_SCREEN     CSI"2J"      // clear the whole screen.
#define T_ATTR_RESET_ALL   CSI"0m"      // reset all attributes (color,bold,..).
#define T_COLOR_USE_GREG   CSI"?1070l"  // use the global color registry.
#define T_COLOR_BG_P       CSI"48;2"
#define T_COLOR_FG_P       CSI"38;2"
#define T_COLOR_BG256_P    CSI"48;5"
#define T_COLOR_FG256_P    CSI"38;5"
#define T_COLOR_BG_F       CSI"48;2;%d;%d;%dm"
#define T_COLOR_FG_F       CSI"38;2;%d;%d;%dm"
#define T_COLOR_BG256_F    CSI"48;5;%dm"
#define T_COLOR_FG256_F    CSI"38;5;%dm"
#define T_COLOR_P_L        6

#define IMGFMT IMGFMT_BGR24

#define BLIT_1x1 0x1
#define BLIT_1x2 0x2
#define BLIT_2x2 0x3

#define UTF8_F_BLK  "\xe2\x96\x88"
#define UTF8_LH_BLK "\xe2\x96\x84"
#define UTF8_UH_BLK "\xe2\x96\x80"

#define DEFAULT_WIDTH 80
#define DEFAULT_HEIGHT 25
#define FOLAY_JUMP_THLD 1

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

typedef struct {
    char str[4];
    int width;
} lut_item_t;

// This struct is packed to 3 bytes, because its only purpose, is to simplify
// byte pointer casts.
typedef struct {
    uint8_t b;
    uint8_t g;
    uint8_t r;
} __attribute__((packed)) bgr_t;

static inline bool bgr_eq(bgr_t x, bgr_t y)
{
    return (x.b == y.b) && (x.g == y.g) && (x.r == y.r);
}

static inline bgr_t bgr_avg(bgr_t x, bgr_t y)
{
    return (bgr_t){(x.b + y.b) / 2, (x.g + y.g) / 2, (x.r + y.r) / 2};
}

static inline unsigned bgr_dist(bgr_t x, bgr_t y) {
    int a = (int)x.b - y.b;
    int b = (int)x.g - y.g;
    int c = (int)x.r - y.r;
    return ((a*a)+(b*b)+(c*c));
}

static inline unsigned bgr_to_i(bgr_t x)
{
    return ((unsigned)x.b)<<16 | ((unsigned)x.g)<<8 | ((unsigned)x.r);
}

static inline bgr_t i_to_bgr(unsigned x)
{
    return (bgr_t){(x >> 16) & 0xFF, (x >> 8) & 0xFF, x & 0xFF};
}

struct priv {
    int opt_blit;
    int opt_width; // 0 -> default
    int opt_height; // 0 -> default
    int opt_term256; // 0 -> true color

    int w; // frame char width
    int h; // frame char height
    int scale_w, scale_h; // number of pixels per char.
    int ww; // w * scale_w
    int hh; // h * scale_h,
    int term_x, term_y; // starting position
    int stride, p_stride; // frame stride
    unsigned int bytes_written; // number of bytes written out.
    bool resized, skip_draw, can_overlay;
    uint8_t *data, *p_data;
    char *fbuff; // framebuffer
    size_t fbuff_size;
    uint16_t *folay; // overlay between current and previous frames.
    size_t folay_size;

    struct mp_rect src;
    struct mp_rect dst;
    struct mp_image *frame; // current frame
    struct mp_image *p_frame; // previous frame
    struct mp_sws_context *sws;

    // int -> str lookup table for faster conversion.
    lut_item_t lut[256];

    // Escape code printing functions that are dispatched at reconfig. Used for
    // generalizing the code between different algos and term256.
    int (*sprint_bg)(char*, lut_item_t*, bgr_t);
    int (*sprint_fg)(char*, lut_item_t*, bgr_t);
};

// Convert RGB24 to xterm-256 8-bit value.
// For simplicity, assume RGB space is perceptually uniform.
// There are 5 places where one of two outputs needs to be chosen when the
// input is the exact middle:
// - The r/g/b channels and the gray value: the higher value output is chosen.
// - If the gray and color have same distance from the input - color is chosen.
static int rgb_to_x256(bgr_t c)
{
    // Calculate the nearest 0-based color index at 16 .. 231
#   define v2ci(v) (v < 48 ? 0 : v < 115 ? 1 : (v - 35) / 40)
    int ir = v2ci(c.r), ig = v2ci(c.g), ib = v2ci(c.b);   // 0..5 each
#   define color_index() (36 * ir + 6 * ig + ib)  /* 0..215, lazy evaluation */

    // Calculate the nearest 0-based gray index at 232 .. 255
    int average = (c.r + c.g + c.b) / 3;
    int gray_index = average > 238 ? 23 : (average - 3) / 10;  // 0..23

    // Calculate the represented colors back from the index
    static const int i2cv[6] = {0, 0x5f, 0x87, 0xaf, 0xd7, 0xff};
    int cr = i2cv[ir], cg = i2cv[ig], cb = i2cv[ib];  // r/g/b, 0..255 each
    int gv = 8 + 10 * gray_index;  // same value for r/g/b, 0..255

    // Return the one which is nearer to the original input rgb value
#   define dist_square(A,B,C, a,b,c) ((A-a)*(A-a) + (B-b)*(B-b) + (C-c)*(C-c))
    int color_err = dist_square(cr, cg, cb, c.r, c.g, c.b);
    int gray_err  = dist_square(gv, gv, gv, c.r, c.g, c.b);
    return color_err <= gray_err ? 16 + color_index() : 232 + gray_index;
}

static inline int sprint_seq3(char *buff, lut_item_t *lut, const char *prefix,
                              bgr_t c)
{
    memcpy(buff, prefix,       T_COLOR_P_L);    buff += T_COLOR_P_L;
    memcpy(buff, lut[c.r].str, lut[c.r].width); buff += lut[c.r].width;
    memcpy(buff, lut[c.g].str, lut[c.g].width); buff += lut[c.g].width;
    memcpy(buff, lut[c.b].str, lut[c.b].width); buff += lut[c.b].width;
    *buff = 'm';

    return T_COLOR_P_L + lut[c.r].width + lut[c.g].width + lut[c.b].width + 1;
}

static inline int sprint_seq1(char *buff, lut_item_t *lut, const char *prefix,
                              uint8_t c)
{
    memcpy(buff, prefix,     T_COLOR_P_L);  buff += T_COLOR_P_L;
    memcpy(buff, lut[c].str, lut[c].width); buff += lut[c].width;
    *buff = 'm';

    return T_COLOR_P_L + lut[c].width + 1;
}

// Used for generalizing the code between truecolor and xterm-256 options
// (removes the need to check for it on every pixel).
static int __x256_sprint_bg(char *buff, lut_item_t *lut, bgr_t c)
{ return sprint_seq1(buff, lut, T_COLOR_BG256_P, rgb_to_x256(c));}

static int __x256_sprint_fg(char *buff, lut_item_t *lut, bgr_t c)
{ return sprint_seq1(buff, lut, T_COLOR_FG256_P, rgb_to_x256(c));}

static int __tc_sprint_bg(char *buff, lut_item_t *lut, bgr_t c)
{ return sprint_seq3(buff, lut, T_COLOR_BG_P, c); }

static int __tc_sprint_fg(char *buff, lut_item_t *lut, bgr_t c)
{ return sprint_seq3(buff, lut, T_COLOR_FG_P, c); }


// The {f}write() implementation is about 25% faster than the printf one (even
// if using .*s with the lut values). However, on Windows we need to use printf
// in order to translate escape sequences and UTF8 output for the console.
// write() is also faster than fwrite() for large buffers.
static inline void print_buff(char *buff, int size)
{
#   ifdef _WIN32
    printf("%.*s", size, buff);
#   elif __linux__
    fwrite_unlocked(buff, size, 1, stdout);
    // write(1, buff, size);
#   else
    fwrite(buff, size, 1, stdout);
#   endif
}

static inline unsigned solve2x2(bgr_t nw, bgr_t ne, bgr_t sw, bgr_t se,
                                bgr_t *restrict c1, bgr_t *restrict c2)
{
    unsigned nwi = bgr_to_i(nw);
    unsigned nei = bgr_to_i(ne);
    unsigned swi = bgr_to_i(sw);
    unsigned sei = bgr_to_i(se);

    bgr_t    colors[4]   = {nw , ne , sw , se };
    unsigned i_colors[4] = {nwi, nei, swi, sei};
    unsigned ret = 0b0000;

    // count the number of distinct colors using the on/off principle.
    unsigned ccnt = 4 - (nwi == nei)
                  - ((nwi == swi) || (nei == swi))
                  - ((nwi == sei) || (nei == sei) || (swi == sei));

    if (ccnt == 1) {
        *c1 = nw;
        return ret;
    }

    unsigned dists[4][4] = {
        {0               ,bgr_dist(nw, ne),bgr_dist(nw, sw),bgr_dist(nw, se)},
        {bgr_dist(ne, nw),0               ,bgr_dist(ne, sw),bgr_dist(ne, se)},
        {bgr_dist(sw, nw),bgr_dist(sw, ne),0               ,bgr_dist(sw, se)},
        {bgr_dist(se, nw),bgr_dist(se, ne),bgr_dist(se, sw),0               },
    };

    unsigned max_dist_a_idx = 0;
    unsigned max_dist_b_idx = 0;
    unsigned max_dist = 0;

    // find 2 data points (colors) with the highest distance and construct
    // clusters around them.
    for (unsigned i = 0; i < 4; i++) {
        for (unsigned j = i + 1; j < 4; j++) {
            if (dists[i][j] > max_dist) {
                max_dist_a_idx = i;
                max_dist_b_idx = j;
                max_dist = dists[i][j];
            }
        }
    }

    unsigned cluster_a_size = 1;
    unsigned cluster_b_size = 1;
    unsigned cluster_a[4] = {i_colors[max_dist_a_idx]};
    unsigned cluster_b[4] = {i_colors[max_dist_b_idx]};

    for (unsigned i = 0; i < 4; i++) {
        if (i == max_dist_a_idx || i == max_dist_b_idx) continue;
        if (dists[max_dist_a_idx][i] < dists[max_dist_b_idx][i]) {
            cluster_a[cluster_a_size++] = i_colors[i];
        } else {
            cluster_b[cluster_b_size++] = i_colors[i];
        }
    }

    // For some reason subsampling, gives better / sharper results than
    // clusterwise linear interpolation.
    bgr_t _c1 = i_to_bgr(cluster_a[0]);
    bgr_t _c2 = i_to_bgr(cluster_b[0]);

    for (unsigned i = 0; i < 4; i++) {
        ret |= ((bgr_dist(_c1, colors[i]) > bgr_dist(_c2, colors[i])) << i);
    }

    *c1 = _c1;
    *c2 = _c2;

    return ret;
}

static void draw_1x1(struct vo *vo)
{
    struct priv *p = vo->priv;
    char *buffptr = p->fbuff;

    for (int y = 0; y < p->hh; y++) {
        buffptr += sprintf(buffptr, T_CURS_MOV_F, p->term_y + y, p->term_x);

        // make sure that old != row[0], so the first color gets printed.
        bgr_t *row = (bgr_t*)(p->data + y * p->stride);
        bgr_t old_pix = row[0];

        buffptr += p->sprint_bg(buffptr, p->lut, old_pix);
        *buffptr++ = ' ';

        for (int x = 1; x < p->ww; x++) {
            bgr_t pix = row[x];

            const int idx = y * p->w + x;
            if (p->can_overlay && p->folay[idx] > FOLAY_JUMP_THLD) {
                x += p->folay[idx];
                buffptr += sprintf(buffptr, T_CURS_FWD_F, p->folay[idx] + 1);
                continue;
            }

            if (!bgr_eq(pix, old_pix))
                buffptr += p->sprint_bg(buffptr, p->lut, pix);

            *buffptr++ = ' ';
            old_pix = pix;
        }
        buffptr += sprintf(buffptr, T_ATTR_RESET_ALL);
    }
    *buffptr++ = '\n';
    const int bufflen = buffptr - p->fbuff;
    p->bytes_written += bufflen;
    print_buff(p->fbuff, bufflen);
}

static void draw_1x2(struct vo *vo)
{
    struct priv *p = vo->priv;
    char *buffptr = p->fbuff;

    for (int y = 0; y < p->hh; y += 2) {
        buffptr += sprintf(buffptr, T_CURS_MOV_F, p->term_y + y / 2, p->term_x);

        // make sure that old != row[0], so the first color gets printed.
        bgr_t *row_n = (bgr_t*)(p->data + y * p->stride);
        bgr_t *row_s = (bgr_t*)(p->data + (y + 1) * p->stride);
        bgr_t old_pix_n = row_n[0];
        bgr_t old_pix_s = row_s[0];

        buffptr += p->sprint_bg(buffptr, p->lut, old_pix_n);
        buffptr += p->sprint_fg(buffptr, p->lut, old_pix_s);
        buffptr += sprintf(buffptr, "▄");

        for (int x = 1; x < p->ww; x++) {
            bgr_t pix_n = row_n[x];
            bgr_t pix_s = row_s[x];

            const int idx_n = p->w * y + x;
            const int idx_s = p->w * (y + 1) + x;
            const int folay_offset = MIN(p->folay[idx_n], p->folay[idx_s]) - 1;
            // ^ in theory, this -1 is not needed, but in some cases, the colors
            // were getting fucked up, and this fixes it.

            if (p->can_overlay && folay_offset > FOLAY_JUMP_THLD) {
                x += folay_offset;
                buffptr += sprintf(buffptr, T_CURS_FWD_F, folay_offset + 1);
                continue;
            }

            if (bgr_eq(pix_n, pix_s)) {
                if (bgr_eq(pix_n, old_pix_n)) {
                    // old bg is the same as new (bg,fg).
                    *buffptr++ = ' ';
                    continue;
                }
                if (bgr_eq(pix_n, old_pix_s)) {
                    // old fg is the same as new (bg,fg).
                    buffptr += sprintf(buffptr, "█");
                    continue;
                }
                // change bg, since ' ' is shorter than "█".
                buffptr += p->sprint_bg(buffptr, p->lut, pix_n);
                *buffptr++ = ' ';
                old_pix_n = pix_n;
            } else {
                if (bgr_eq(pix_n, old_pix_s) && bgr_eq(pix_s, old_pix_n)) {
                    // old (bg,fg) are the same as new (fg,bg)
                    buffptr += sprintf(buffptr, "▀");
                    continue;
                }
                if (!bgr_eq(pix_n, old_pix_n))
                    buffptr += p->sprint_bg(buffptr, p->lut, pix_n);

                if (!bgr_eq(pix_s, old_pix_s))
                    buffptr += p->sprint_fg(buffptr, p->lut, pix_s);

                buffptr += sprintf(buffptr, "▄");
                old_pix_n = pix_n;
                old_pix_s = pix_s;
            }
        }
        buffptr += sprintf(buffptr, T_ATTR_RESET_ALL);
    }
    *buffptr++ = '\n';
    unsigned int bufflen = buffptr - p->fbuff;
    p->bytes_written += bufflen;
    print_buff(p->fbuff, bufflen);
}

static void draw_2x2(struct vo *vo)
{
    struct priv *p = vo->priv;
    char *buffptr = p->fbuff;

    for (int y = 0; y < p->hh; y += 2) {
        buffptr += sprintf(buffptr, T_CURS_MOV_F, p->term_y + y / 2, p->term_x);

        // make sure that old != row[0], so the first color gets printed.
        bgr_t *row_n = (bgr_t*)(p->data + y * p->stride);
        bgr_t *row_s = (bgr_t*)(p->data + (y + 1) * p->stride);

        bgr_t old_bg = bgr_avg(row_n[0], row_n[1]);
        bgr_t old_fg = bgr_avg(row_s[0], row_s[1]);

        // print out a half block for the first character, because its easier.
        buffptr += p->sprint_bg(buffptr, p->lut, old_bg);
        buffptr += p->sprint_fg(buffptr, p->lut, old_fg);
        buffptr += sprintf(buffptr, "▄");

        for (int x = 0; x < p->ww; x += 2) {
            // "▖""▗""▘""▝""▞""▚""▙""▟""▛""▜""█""▄""▀""▐""▌"" "

            bgr_t bg = {0}, fg = {0};
            // Don't know why, but flipping both horizontally and vertically
            // fixes the antialiasing / interpolation problems.
            // It worked without this magic fuckery when testing,
            // but on "real" images it does not.
            // DO NOT FUCKING TOUCH.
            unsigned mask = solve2x2(row_s[x+1], row_s[x],
                                     row_n[x+1], row_n[x], &bg, &fg);


            // TODO @cloud11665: use folay for 2x2 and higher*

#           define print_x_or_inverse(alpha, inv_alpha)               \
                if (bgr_eq(bg, old_fg) && bgr_eq(fg, old_bg)) {       \
                    buffptr += sprintf(buffptr, inv_alpha);           \
                } else {                                              \
                    if (!bgr_eq(bg, old_bg))                          \
                        buffptr += p->sprint_bg(buffptr, p->lut, bg); \
                    if (!bgr_eq(fg, old_fg))                          \
                        buffptr += p->sprint_fg(buffptr, p->lut, fg); \
                buffptr += sprintf(buffptr, alpha);                   \
                    old_bg = bg;                                      \
                    old_fg = fg;                                      \
                }                                                     \
                continue

            switch (mask) {
            case 0b0000: {
                if (bgr_eq(bg, old_bg)) {
                    *buffptr++ = ' ';
                    continue;
                }
                if (bgr_eq(bg, old_fg)) {
                    buffptr += sprintf(buffptr, "█");
                    continue;
                }
                buffptr += p->sprint_bg(buffptr, p->lut, bg);
                *buffptr++ = ' ';
                old_bg = bg;
                continue;
            }
            case 0b0001: print_x_or_inverse("▗", "▛");
            case 0b0010: print_x_or_inverse("▖", "▜");
            case 0b0011: print_x_or_inverse("▄", "▀");
            case 0b0100: print_x_or_inverse("▝", "▙");
            case 0b0101: print_x_or_inverse("▐", "▌");
            case 0b0110: print_x_or_inverse("▞", "▚");
            case 0b0111: print_x_or_inverse("▟", "▘");
            case 0b1000: print_x_or_inverse("▘", "▟");
            case 0b1001: print_x_or_inverse("▚", "▞");
            case 0b1010: print_x_or_inverse("▌", "▐");
            case 0b1011: print_x_or_inverse("▙", "▝");
            case 0b1100: print_x_or_inverse("▀", "▄");
            case 0b1101: print_x_or_inverse("▜", "▖");
            case 0b1110: print_x_or_inverse("▛", "▗");
            /* case 0b1111 never happenes. */
            }
        }
        buffptr += sprintf(buffptr, T_ATTR_RESET_ALL);
    }
    *buffptr++ = '\n';
    unsigned int bufflen = buffptr - p->fbuff;
    p->bytes_written += bufflen;
    print_buff(p->fbuff, bufflen);
}

static void draw_frame(struct vo *vo, struct vo_frame *frame)
{
    assert(frame->current->planes[0]);

    struct priv *p = vo->priv;
    mp_sws_scale(p->sws, p->frame, frame->current);

    p->skip_draw = false;
    if (frame->repeat && !frame->redraw && !p->resized) {
        p->skip_draw = true;
        return;
    }

    p->data     = p->frame->planes[0];
    p->stride   = p->frame->stride[0];
    p->p_stride = (p->p_frame ? p->p_frame->stride[0] : -1);
    p->p_data   = (p->p_frame ? p->p_frame->planes[0] : NULL);
    p->can_overlay = p->p_stride == p->stride && !p->resized;

    if (p->can_overlay) {
        for (int y = 0; y < p->hh; y++) {
            bgr_t *row = (bgr_t *)(p->data + y * p->stride);
            bgr_t *p_row = (bgr_t *)(p->p_data + y * p->p_stride);
            p->folay[(y + 1) * p->ww - 1] = 0;
            p->folay[y * p->ww + 1] = 0;
            for (int x = p->ww - 2; x > 1 ; x--) {
                int idx = y * p->ww + x;
                if (bgr_eq(row[x], p_row[x])) {
                    p->folay[idx] = p->folay[idx + 1] + 1;
                } else {
                    p->folay[idx] = 0;
                }
            }
        }
    } else {
        // To clear left-over text AFTER the resize.
        printf(T_CLEAR_SCREEN T_ATTR_RESET_ALL);
    }

    switch (p->opt_blit) {
    case BLIT_1x1:
        draw_1x1(vo); break;
    case BLIT_1x2:
        draw_1x2(vo); break;
    case BLIT_2x2:
        draw_2x2(vo); break;
    }


    // TODO @cloud11665: implement frame switching to prevent this fuckery.
    if (p->p_frame)
        talloc_free(p->p_frame);
    p->p_frame = mp_image_new_copy(p->frame);
}

static void get_win_size(struct vo *vo, int *out_width, int *out_height) {
    struct priv *p = vo->priv;
    *out_width  = DEFAULT_WIDTH;
    *out_height = DEFAULT_HEIGHT;

    terminal_get_size(out_width, out_height);

    if (p->opt_width > 0)
        *out_width  = p->opt_width;
    if (p->opt_height > 0)
        *out_height = p->opt_height;
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct priv *p = vo->priv;

    get_win_size(vo, &vo->dwidth, &vo->dheight);

    struct mp_osd_res osd;
    vo_get_src_dst_rects(vo, &p->src, &p->dst, &osd);
    p->w = p->dst.x1 - p->dst.x0;
    p->h = p->dst.y1 - p->dst.y0;
    p->ww = p->w * p->scale_w;
    p->hh = p->h * p->scale_h;
    p->term_x   = (vo->dwidth - p->w) / 2;
    p->term_y   = (vo->dheight - p->h) / 2;

    p->sws->src = *params;
    p->sws->dst = (struct mp_image_params) {
        .imgfmt = IMGFMT,
        .w = p->w,
        .h = p->h,
        .p_w = 1,
        .p_h = 1,
    };

    if (p->frame)
        talloc_free(p->frame);
    p->frame = mp_image_alloc(IMGFMT, p->w * p->scale_w, p->h * p->scale_h);

    const int max_seq_size = (p->opt_term256 ? 11 : 19);
    const int chr_size     = (p->opt_blit == BLIT_1x1 ? 1 : 3);
    p->fbuff_size = (max_seq_size * 2 + chr_size) * (p->w * p->h)
                  + (10 + 4) * p->h // T_CURS_MOV_F + ESC_CLEAR_COLORS
                  + 2; // "\n\0"
    p->fbuff = realloc(p->fbuff, p->fbuff_size * sizeof(char));

    p->folay_size = p->ww * p->hh * 2;
    p->folay = realloc(p->folay, p->folay_size * sizeof(uint16_t));
    p->can_overlay = false;
    memset(p->folay, 0, p->folay_size);

    if (p->p_frame)
        talloc_free(p->p_frame);
    p->p_frame = NULL;

    if (p->opt_term256) {
        p->sprint_bg = &__x256_sprint_bg;
        p->sprint_fg = &__x256_sprint_fg;
    } else {
        p->sprint_bg = &__tc_sprint_bg;
        p->sprint_fg = &__tc_sprint_fg;
    }

    if (!p->frame)
        return -1;
    if (!p->fbuff)
        return -1;
    if (!p->folay)
        return -1;
    if (mp_sws_reinit(p->sws) < 0)
        return -1;

    printf(T_CLEAR_SCREEN T_ATTR_RESET_ALL);
    vo->want_redraw = true;
    return 0;
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;
    int width, height;
    get_win_size(vo, &width, &height);

    p->resized = false;
    if (vo->dwidth != width || vo->dheight != height) {
        p->resized = true;
        reconfig(vo, vo->params);
    }

    if (p->skip_draw)
        return;

    fflush(stdout);
}

static void uninit(struct vo *vo)
{
    printf(T_TITLE_STCK_POP);
    printf(T_CURS_SHOW);
    printf(T_CURS_STCK_POP);
    printf(T_BUFF_DSBL);
    printf(T_STATE_RSTR);

    struct priv *p = vo->priv;
    if (p->fbuff)
        free(p->fbuff);
    if (p->folay)
        free(p->folay);
    if (p->frame)
        talloc_free(p->frame);
    if (p->p_frame)
        talloc_free(p->p_frame);
}

static int preinit(struct vo *vo)
{
    // Most terminal characters aren't 1:1, so we default to 2:1.
    // If user passes their own value of choice, it'll be scaled accordingly.
    vo->monitor_par = vo->opts->monitor_pixel_aspect * 2;

    struct priv *p = vo->priv;
    p->sws = mp_sws_alloc(vo);
    p->sws->log = vo->log;
    mp_sws_enable_cmdline_opts(p->sws, vo->global);

    for (int i = 0; i < 256; ++i) {
        char buff[8];
        p->lut[i].width = sprintf(buff, ";%d", i);
        // some strings may not end on a null byte, but that's ok.
        memcpy(p->lut[i].str, buff, 4);
    }

    p->folay   = NULL;
    p->fbuff   = NULL;
    p->p_frame = NULL;
    p->bytes_written = 0;

    p->scale_w  = (p->opt_blit == BLIT_2x2 ? 2 : 1);
    p->scale_h  = (p->opt_blit == BLIT_1x1 ? 1 : 2);

    printf(T_TITLE_STCK_PSH);
    printf(T_CURS_HIDE);
    printf(T_CURS_STCK_PSH);
    printf(T_BUFF_ENBL);
    printf(T_STATE_SAVE);

    return 0;
}

static int query_format(struct vo *vo, int format)
{
    return format == IMGFMT;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;

    switch (request) {
        case VOCTRL_UPDATE_WINDOW_TITLE:
            printf(T_TITLE_SET_F, (char *)data);
            return VO_TRUE;
        case VOCTRL_GET_DISPLAY_RES:
            ((int *)data)[0] = p->w;
            ((int *)data)[1] = p->h;
            return VO_TRUE;
        case VOCTRL_SET_PANSCAN:
            reconfig(vo, vo->params);
            return VO_TRUE;
    }
    return VO_NOTIMPL;
}

#define OPT_BASE_STRUCT struct priv
const struct vo_driver video_out_tct = {
    .name = "tct",
    .description = "true-color terminals",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_frame = draw_frame,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .opt_blit = BLIT_2x2,
        .opt_term256 = 0,
        .opt_height = 0,
        .opt_width = 0,
    },
    .options = (const m_option_t[]) {
        {"algo", OPT_CHOICE(opt_blit,
            {"plain", BLIT_1x1},
            {"half-blocks", BLIT_1x2},
            {"quad", BLIT_2x2})},
        {"width", OPT_INT(opt_width)},
        {"height", OPT_INT(opt_height)},
        {"256", OPT_FLAG(opt_term256)},
        {0}
    },
    .options_prefix = "vo-tct"
};
