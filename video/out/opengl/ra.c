#include "common/common.h"
#include "common/msg.h"
#include "video/img_format.h"

#include "ra.h"

struct ra_tex *ra_tex_create(struct ra *ra, const struct ra_tex_params *params)
{
    return ra->fns->tex_create(ra, params);
}

void ra_tex_free(struct ra *ra, struct ra_tex **tex)
{
    if (*tex)
        ra->fns->tex_destroy(ra, *tex);
    *tex = NULL;
}

// Return whether this is a tightly packed format with no external padding and
// with the same bit size/depth in all components.
static bool ra_format_is_regular(const struct ra_format *fmt)
{
    if (!fmt->pixel_size || !fmt->num_components)
        return false;
    for (int n = 1; n < fmt->num_components; n++) {
        if (fmt->component_size[n] != fmt->component_size[0] ||
            fmt->component_depth[n] != fmt->component_depth[0])
            return false;
    }
    if (fmt->component_size[0] * fmt->num_components != fmt->pixel_size * 8)
        return false;
    return true;
}

// Return a regular filterable format using RA_CTYPE_UNORM.
const struct ra_format *ra_find_unorm_format(struct ra *ra,
                                             int bytes_per_component,
                                             int n_components)
{
    for (int n = 0; n < ra->num_formats; n++) {
        const struct ra_format *fmt = ra->formats[n];
        if (fmt->ctype == RA_CTYPE_UNORM && fmt->num_components == n_components &&
            fmt->pixel_size == bytes_per_component * n_components &&
            fmt->component_depth[0] == bytes_per_component * 8 &&
            fmt->linear_filter && ra_format_is_regular(fmt))
            return fmt;
    }
    return NULL;
}

// Return a regular format using RA_CTYPE_UINT.
const struct ra_format *ra_find_uint_format(struct ra *ra,
                                            int bytes_per_component,
                                            int n_components)
{
    for (int n = 0; n < ra->num_formats; n++) {
        const struct ra_format *fmt = ra->formats[n];
        if (fmt->ctype == RA_CTYPE_UINT && fmt->num_components == n_components &&
            fmt->pixel_size == bytes_per_component * n_components &&
            fmt->component_depth[0] == bytes_per_component * 8 &&
            ra_format_is_regular(fmt))
            return fmt;
    }
    return NULL;
}

// Return a filterable regular format that uses float16 internally, but does 32 bit
// transfer. (This is just so we don't need 32->16 bit conversion on CPU,
// which would be ok but messy.)
const struct ra_format *ra_find_float16_format(struct ra *ra, int n_components)
{
    for (int n = 0; n < ra->num_formats; n++) {
        const struct ra_format *fmt = ra->formats[n];
        if (fmt->ctype == RA_CTYPE_FLOAT && fmt->num_components == n_components &&
            fmt->pixel_size == sizeof(float) * n_components &&
            fmt->component_depth[0] == 16 &&
            fmt->linear_filter && ra_format_is_regular(fmt))
            return fmt;
    }
    return NULL;
}


// Like ra_find_unorm_format(), but if no fixed point format is available,
// return an unsigned integer format.
static const struct ra_format *find_plane_format(struct ra *ra, int bytes,
                                                 int n_channels)
{
    const struct ra_format *f = ra_find_unorm_format(ra, bytes, n_channels);
    if (f)
        return f;
    return ra_find_uint_format(ra, bytes, n_channels);
}

// Put a mapping of imgfmt to texture formats into *out. Basically it selects
// the correct texture formats needed to represent an imgfmt in a shader, with
// textures using the same memory organization as on the CPU.
// Each plane is represented by a texture, and each texture has a RGBA
// component order. out->components describes the meaning of them.
// May return integer formats for >8 bit formats, if the driver has no
// normalized 16 bit formats.
// Returns false (and *out is not touched) if no format found.
bool ra_get_imgfmt_desc(struct ra *ra, int imgfmt, struct ra_imgfmt_desc *out)
{
    struct ra_imgfmt_desc res = {0};

    struct mp_regular_imgfmt regfmt;
    if (mp_get_regular_imgfmt(&regfmt, imgfmt)) {
        enum ra_ctype ctype = RA_CTYPE_UNKNOWN;
        res.num_planes = regfmt.num_planes;
        res.component_bits = regfmt.component_size * 8;
        res.component_pad = regfmt.component_pad;
        for (int n = 0; n < regfmt.num_planes; n++) {
            struct mp_regular_imgfmt_plane *plane = &regfmt.planes[n];
            res.planes[n] = find_plane_format(ra, regfmt.component_size,
                                              plane->num_components);
            if (!res.planes[n])
                return false;
            for (int i = 0; i < plane->num_components; i++)
                res.components[n][i] = plane->components[i];
            // Dropping LSBs when shifting will lead to dropped MSBs.
            if (res.component_bits > res.planes[n]->component_depth[0] &&
                res.component_pad < 0)
                return false;
            // Renderer restriction, but actually an unwanted corner case.
            if (ctype != RA_CTYPE_UNKNOWN && ctype != res.planes[n]->ctype)
                return false;
            ctype = res.planes[n]->ctype;
        }
        res.chroma_w = regfmt.chroma_w;
        res.chroma_h = regfmt.chroma_h;
        goto supported;
    }

    for (int n = 0; n < ra->num_formats; n++) {
        if (ra->formats[n]->special_imgfmt == imgfmt) {
            res = *ra->formats[n]->special_imgfmt_desc;
            goto supported;
        }
    }

    // Unsupported format
    return false;

supported:

    *out = res;
    return true;
}

void ra_dump_tex_formats(struct ra *ra, int msgl)
{
    if (!mp_msg_test(ra->log, msgl))
        return;
    MP_MSG(ra, msgl, "Texture formats:\n");
    for (int n = 0; n < ra->num_formats; n++) {
        const struct ra_format *fmt = ra->formats[n];
        const char *ctype = "unknown";
        switch (fmt->ctype) {
        case RA_CTYPE_UNORM:    ctype = "unorm";    break;
        case RA_CTYPE_UINT:     ctype = "uint ";    break;
        case RA_CTYPE_FLOAT:    ctype = "float";    break;
        }
        char cl[40] = "";
        for (int i = 0; i < fmt->num_components; i++) {
            mp_snprintf_cat(cl, sizeof(cl), "%s%d", i ? " " : "",
                            fmt->component_size[i]);
            if (fmt->component_size[i] != fmt->component_depth[i])
                mp_snprintf_cat(cl, sizeof(cl), "/%d", fmt->component_depth[i]);
        }
        MP_MSG(ra, msgl, "  %-10s %d*%s %3dB %s %s %s {%s}\n", fmt->name,
               fmt->num_components, ctype, fmt->pixel_size,
               fmt->luminance_alpha ? "LA" : "  ",
               fmt->linear_filter ? "LF" : "  ",
               fmt->renderable ? "CR" : "  ", cl);
    }
}

void ra_dump_imgfmt_desc(struct ra *ra, const struct ra_imgfmt_desc *desc,
                         int msgl)
{
    char pl[80] = "";
    char pf[80] = "";
    for (int n = 0; n < desc->num_planes; n++) {
        if (n > 0) {
            mp_snprintf_cat(pl, sizeof(pl), "/");
            mp_snprintf_cat(pf, sizeof(pf), "/");
        }
        char t[5] = {0};
        for (int i = 0; i < 4; i++)
            t[i] = "_rgba"[desc->components[n][i]];
        for (int i = 3; i > 0 && t[i] == '_'; i--)
            t[i] = '\0';
        mp_snprintf_cat(pl, sizeof(pl), "%s", t);
        mp_snprintf_cat(pf, sizeof(pf), "%s", desc->planes[n]->name);
    }
    MP_MSG(ra, msgl, "%d planes %dx%d %d/%d [%s] (%s)\n",
           desc->num_planes, desc->chroma_w, desc->chroma_h,
           desc->component_bits, desc->component_pad, pf, pl);
}

void ra_dump_img_formats(struct ra *ra, int msgl)
{
    if (!mp_msg_test(ra->log, msgl))
        return;
    MP_MSG(ra, msgl, "Image formats:\n");
    for (int imgfmt = IMGFMT_START; imgfmt < IMGFMT_END; imgfmt++) {
        const char *name = mp_imgfmt_to_name(imgfmt);
        if (strcmp(name, "unknown") == 0)
            continue;
        MP_MSG(ra, msgl, "  %s", name);
        struct ra_imgfmt_desc desc;
        if (ra_get_imgfmt_desc(ra, imgfmt, &desc)) {
            MP_MSG(ra, msgl, " => ");
            ra_dump_imgfmt_desc(ra, &desc, msgl);
        } else {
            MP_MSG(ra, msgl, "\n");
        }
    }
}
