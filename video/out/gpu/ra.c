#include "common/common.h"
#include "common/msg.h"
#include "video/img_format.h"

#include "ra.h"

void ra_add_native_resource(struct ra *ra, const char *name, void *data)
{
    struct ra_native_resource r = {
        .name = name,
        .data = data,
    };
    MP_TARRAY_APPEND(ra, ra->native_resources, ra->num_native_resources, r);
}

void *ra_get_native_resource(struct ra *ra, const char *name)
{
    for (int n = 0; n < ra->num_native_resources; n++) {
        struct ra_native_resource *r = &ra->native_resources[n];
        if (strcmp(r->name, name) == 0)
            return r->data;
    }

    return NULL;
}

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

struct ra_buf *ra_buf_create(struct ra *ra, const struct ra_buf_params *params)
{
    return ra->fns->buf_create(ra, params);
}

void ra_buf_free(struct ra *ra, struct ra_buf **buf)
{
    if (*buf)
        ra->fns->buf_destroy(ra, *buf);
    *buf = NULL;
}

void ra_free(struct ra **ra)
{
    if (*ra)
        (*ra)->fns->destroy(*ra);
    talloc_free(*ra);
    *ra = NULL;
}

size_t ra_vartype_size(enum ra_vartype type)
{
    switch (type) {
    case RA_VARTYPE_INT:        return sizeof(int);
    case RA_VARTYPE_FLOAT:      return sizeof(float);
    case RA_VARTYPE_BYTE_UNORM: return 1;
    default: return 0;
    }
}

struct ra_layout ra_renderpass_input_layout(struct ra_renderpass_input *input)
{
    size_t el_size = ra_vartype_size(input->type);
    if (!el_size)
        return (struct ra_layout){0};

    // host data is always tightly packed
    return (struct ra_layout) {
        .align  = 1,
        .stride = el_size * input->dim_v,
        .size   = el_size * input->dim_v * input->dim_m,
    };
}

static struct ra_renderpass_input *dup_inputs(void *ta_parent,
            const struct ra_renderpass_input *inputs, int num_inputs)
{
    struct ra_renderpass_input *res =
        talloc_memdup(ta_parent, (void *)inputs, num_inputs * sizeof(inputs[0]));
    for (int n = 0; n < num_inputs; n++)
        res[n].name = talloc_strdup(res, res[n].name);
    return res;
}

// Return a newly allocated deep-copy of params.
struct ra_renderpass_params *ra_renderpass_params_copy(void *ta_parent,
        const struct ra_renderpass_params *params)
{
    struct ra_renderpass_params *res = talloc_ptrtype(ta_parent, res);
    *res = *params;
    res->inputs = dup_inputs(res, res->inputs, res->num_inputs);
    res->vertex_attribs =
        dup_inputs(res, res->vertex_attribs, res->num_vertex_attribs);
    res->cached_program = bstrdup(res, res->cached_program);
    res->vertex_shader = talloc_strdup(res, res->vertex_shader);
    res->frag_shader = talloc_strdup(res, res->frag_shader);
    res->compute_shader = talloc_strdup(res, res->compute_shader);
    return res;
};

struct glsl_fmt {
    enum ra_ctype ctype;
    int num_components;
    int component_depth[4];
    const char *glsl_format;
};

// List taken from the GLSL specification, sans snorm and sint formats
static const struct glsl_fmt ra_glsl_fmts[] = {
    {RA_CTYPE_FLOAT, 1, {16},             "r16f"},
    {RA_CTYPE_FLOAT, 1, {32},             "r32f"},
    {RA_CTYPE_FLOAT, 2, {16, 16},         "rg16f"},
    {RA_CTYPE_FLOAT, 2, {32, 32},         "rg32f"},
    {RA_CTYPE_FLOAT, 4, {16, 16, 16, 16}, "rgba16f"},
    {RA_CTYPE_FLOAT, 4, {32, 32, 32, 32}, "rgba32f"},
    {RA_CTYPE_FLOAT, 3, {11, 11, 10},     "r11f_g11f_b10f"},

    {RA_CTYPE_UNORM, 1, {8},              "r8"},
    {RA_CTYPE_UNORM, 1, {16},             "r16"},
    {RA_CTYPE_UNORM, 2, {8,  8},          "rg8"},
    {RA_CTYPE_UNORM, 2, {16, 16},         "rg16"},
    {RA_CTYPE_UNORM, 4, {8,  8,  8,  8},  "rgba8"},
    {RA_CTYPE_UNORM, 4, {16, 16, 16, 16}, "rgba16"},
    {RA_CTYPE_UNORM, 4, {10, 10, 10,  2}, "rgb10_a2"},

    {RA_CTYPE_UINT,  1, {8},              "r8ui"},
    {RA_CTYPE_UINT,  1, {16},             "r16ui"},
    {RA_CTYPE_UINT,  1, {32},             "r32ui"},
    {RA_CTYPE_UINT,  2, {8,  8},          "rg8ui"},
    {RA_CTYPE_UINT,  2, {16, 16},         "rg16ui"},
    {RA_CTYPE_UINT,  2, {32, 32},         "rg32ui"},
    {RA_CTYPE_UINT,  4, {8,  8,  8,  8},  "rgba8ui"},
    {RA_CTYPE_UINT,  4, {16, 16, 16, 16}, "rgba16ui"},
    {RA_CTYPE_UINT,  4, {32, 32, 32, 32}, "rgba32ui"},
    {RA_CTYPE_UINT,  4, {10, 10, 10,  2}, "rgb10_a2ui"},
};

const char *ra_fmt_glsl_format(const struct ra_format *fmt)
{
    for (int n = 0; n < MP_ARRAY_SIZE(ra_glsl_fmts); n++) {
        const struct glsl_fmt *gfmt = &ra_glsl_fmts[n];

        if (fmt->ctype != gfmt->ctype)
            continue;
        if (fmt->num_components != gfmt->num_components)
            continue;

        for (int i = 0; i < fmt->num_components; i++) {
            if (fmt->component_depth[i] != gfmt->component_depth[i])
                goto next_fmt;
        }

        return gfmt->glsl_format;

next_fmt: ; // equivalent to `continue`
    }

    return NULL;
}

// Return whether this is a tightly packed format with no external padding and
// with the same bit size/depth in all components, and the shader returns
// components in the same order as in memory.
static bool ra_format_is_regular(const struct ra_format *fmt)
{
    if (!fmt->pixel_size || !fmt->num_components || !fmt->ordered)
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

// Find a float format of any precision that matches the C type of the same
// size for upload.
// May drop bits from the mantissa (such as selecting float16 even if
// bytes_per_component == 32); prefers possibly faster formats first.
static const struct ra_format *ra_find_float_format(struct ra *ra,
                                                    int bytes_per_component,
                                                    int n_components)
{
    // Assumes ra_format are ordered by performance.
    // The >=16 check is to avoid catching fringe formats.
    for (int n = 0; n < ra->num_formats; n++) {
        const struct ra_format *fmt = ra->formats[n];
        if (fmt->ctype == RA_CTYPE_FLOAT && fmt->num_components == n_components &&
            fmt->pixel_size == bytes_per_component * n_components &&
            fmt->component_depth[0] >= 16 &&
            fmt->linear_filter && ra_format_is_regular(fmt))
            return fmt;
    }
    return NULL;
}

// Return a filterable regular format that uses at least float16 internally, and
// uses a normal C float for transfer on the CPU side. (This is just so we don't
// need 32->16 bit conversion on CPU, which would be messy.)
const struct ra_format *ra_find_float16_format(struct ra *ra, int n_components)
{
    return ra_find_float_format(ra, sizeof(float), n_components);
}

const struct ra_format *ra_find_named_format(struct ra *ra, const char *name)
{
    for (int n = 0; n < ra->num_formats; n++) {
        const struct ra_format *fmt = ra->formats[n];
        if (strcmp(fmt->name, name) == 0)
            return fmt;
    }
    return NULL;
}

// Like ra_find_unorm_format(), but if no fixed point format is available,
// return an unsigned integer format.
static const struct ra_format *find_plane_format(struct ra *ra, int bytes,
                                                 int n_channels,
                                                 enum mp_component_type ctype)
{
    switch (ctype) {
    case MP_COMPONENT_TYPE_UINT: {
        const struct ra_format *f = ra_find_unorm_format(ra, bytes, n_channels);
        if (f)
            return f;
        return ra_find_uint_format(ra, bytes, n_channels);
    }
    case MP_COMPONENT_TYPE_FLOAT:
        return ra_find_float_format(ra, bytes, n_channels);
    default: return NULL;
    }
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
                                              plane->num_components,
                                              regfmt.component_type);
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
        if (imgfmt && ra->formats[n]->special_imgfmt == imgfmt) {
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
    MP_MSG(ra, msgl, "  NAME       COMP*TYPE SIZE        DEPTH PER COMP.\n");
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
    MP_MSG(ra, msgl, " LA = LUMINANCE_ALPHA hack format\n");
    MP_MSG(ra, msgl, " LF = linear filterable\n");
    MP_MSG(ra, msgl, " CR = can be used for render targets\n");
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
