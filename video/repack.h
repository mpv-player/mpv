#pragma once

#include <stdbool.h>

enum {
    // This controls bheavior with different bit widths per component (like
    // RGB565). If ROUND_DOWN is specified, the planar format will use the min.
    // bit width of all components, otherwise the transformation is lossless.
    REPACK_CREATE_ROUND_DOWN    = (1 << 0),

    // Expand some (not all) low bit depth fringe formats to 8 bit on unpack.
    REPACK_CREATE_EXPAND_8BIT   = (1 << 1),

    // For mp_repack_create_planar(). If specified, the planar format uses a
    // float 32 bit sample format. No range expansion is done.
    REPACK_CREATE_PLANAR_F32    = (1 << 2),
};

struct mp_repack;
struct mp_image;

// Create a repacker between any format (imgfmt parameter) and an equivalent
// planar format (that is native endian). If pack==true, imgfmt is the output,
// otherwise it is the input. The respective other input/output is the planar
// format. The planar format can be queried with mp_repack_get_format_*().
// Note that some formats may change the "implied" colorspace (for example,
// packed xyz unpacks as rgb).
// If imgfmt is already planar, a passthrough repacker may be created.
//  imgfmt: src or dst format (usually packed, non-planar, etc.)
//  pack: true if imgfmt is dst, false if imgfmt is src
//  flags: any of REPACK_CREATE_* flags
//  returns: NULL on failure, otherwise free with talloc_free().
struct mp_repack *mp_repack_create_planar(int imgfmt, bool pack, int flags);

// Return input and output formats for which rp was created.
int mp_repack_get_format_src(struct mp_repack *rp);
int mp_repack_get_format_dst(struct mp_repack *rp);

// Return pixel alignment. For x, this is a lowest pixel count at which there is
// a byte boundary and a full chroma pixel (horizontal subsampling) on src/dst.
// For y, this is the pixel height of the vertical subsampling.
// Always returns a power of 2.
int mp_repack_get_align_x(struct mp_repack *rp);
int mp_repack_get_align_y(struct mp_repack *rp);

// Repack a single line from dst to src, as set in repack_config_buffers().
// For subsampled chroma formats, this copies as many luma/alpha rows as needed
// for a complete line (e.g. 2 luma lines, 1 chroma line for 4:2:0).
// dst_x, src_x, y must be aligned to the pixel alignment. w may be unaligned
// if at the right crop-border of the image, but must be always aligned to
// horiz. sub-sampling. y is subject to hslice.
void repack_line(struct mp_repack *rp, int dst_x, int dst_y,
                 int src_x, int src_y, int w);

// Configure with a source and target buffer. The rp instance will keep the
// mp_image pointers and access them on repack_line() calls. Refcounting is
// not respected - the caller needs to make sure dst is always writable.
// The images can have different sizes (as repack_line() lets you use different
// target coordinates for dst/src).
// This also allocaters potentially required temporary buffers.
//  dst_flags: REPACK_BUF_* flags for dst
//  dst: where repack_line() writes to
//  src_flags: REPACK_BUF_* flags for src
//  src: where repack_line() reads from
//  enable_passthrough: if non-NULL, an bool array of size MP_MAX_PLANES indexed
//                      by plane; a true entry requests disabling copying the
//                      plane data to the dst plane. The function will write to
//                      this array whether the plane can really be passed through
//                      (i.e. will set array entries from true to false if pass-
//                      through is not possible). It writes to all MP_MAX_PLANES
//                      entries. If NULL, all entries are implicitly false.
//  returns: success (fails on OOM)
bool repack_config_buffers(struct mp_repack *rp,
                           int dst_flags, struct mp_image *dst,
                           int src_flags, struct mp_image *src,
                           bool *enable_passthrough);
