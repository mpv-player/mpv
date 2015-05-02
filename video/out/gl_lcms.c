/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 *
 * You can alternatively redistribute this file and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <string.h>

#include "talloc.h"

#include "config.h"

#include "stream/stream.h"
#include "common/common.h"
#include "misc/bstr.h"
#include "common/msg.h"
#include "options/m_option.h"
#include "options/path.h"

#include "gl_video.h"
#include "gl_lcms.h"

#include "osdep/io.h"

#if HAVE_LCMS2

#include <lcms2.h>

struct gl_lcms {
    void *icc_data;
    size_t icc_size;
    char *icc_path;
    bool changed;

    struct mp_log *log;
    struct mpv_global *global;
    struct mp_icc_opts opts;
};

static bool parse_3dlut_size(const char *arg, int *p1, int *p2, int *p3)
{
    if (sscanf(arg, "%dx%dx%d", p1, p2, p3) != 3)
        return false;
    for (int n = 0; n < 3; n++) {
        int s = ((int[]) { *p1, *p2, *p3 })[n];
        if (s < 2 || s > 512 || ((s - 1) & s))
            return false;
    }
    return true;
}

static int validate_3dlut_size_opt(struct mp_log *log, const m_option_t *opt,
                                   struct bstr name, struct bstr param)
{
    int p1, p2, p3;
    char s[20];
    snprintf(s, sizeof(s), "%.*s", BSTR_P(param));
    return parse_3dlut_size(s, &p1, &p2, &p3);
}

#define OPT_BASE_STRUCT struct mp_icc_opts
const struct m_sub_options mp_icc_conf = {
    .opts = (const m_option_t[]) {
        OPT_STRING("icc-profile", profile, 0),
        OPT_FLAG("icc-profile-auto", profile_auto, 0),
        OPT_STRING("icc-cache", cache, 0),
        OPT_INT("icc-intent", intent, 0),
        OPT_STRING_VALIDATE("3dlut-size", size_str, 0, validate_3dlut_size_opt),
        {0}
    },
    .size = sizeof(struct mp_icc_opts),
    .defaults = &(const struct mp_icc_opts) {
        .size_str = "128x256x64",
        .intent = INTENT_RELATIVE_COLORIMETRIC,
    },
};

static void lcms2_error_handler(cmsContext ctx, cmsUInt32Number code,
                                const char *msg)
{
    struct gl_lcms *p = cmsGetContextUserData(ctx);
    MP_ERR(p, "lcms2: %s\n", msg);
}

static struct bstr load_file(void *talloc_ctx, const char *filename,
                             struct mpv_global *global)
{
    struct bstr res = {0};
    char *fname = mp_get_user_path(NULL, global, filename);
    stream_t *s = stream_open(fname, global);
    if (s) {
        res = stream_read_complete(s, talloc_ctx, 1000000000);
        free_stream(s);
    }
    talloc_free(fname);
    return res;
}

static bool load_profile(struct gl_lcms *p)
{
    if (p->icc_data && p->icc_size)
        return true;

    if (!p->icc_path)
        return false;

    MP_INFO(p, "Opening ICC profile '%s'\n", p->icc_path);
    struct bstr iccdata = load_file(p, p->icc_path, p->global);
    if (!iccdata.len)
        return false;

    p->icc_data = iccdata.start;
    p->icc_size = iccdata.len;
    return true;
}

struct gl_lcms *gl_lcms_init(void *talloc_ctx, struct mp_log *log,
                             struct mpv_global *global)
{
    struct gl_lcms *p = talloc_ptrtype(talloc_ctx, p);
    *p = (struct gl_lcms) {
        .global = global,
        .log = log,
        .changed = true,
    };
    return p;
}

void gl_lcms_set_options(struct gl_lcms *p, struct mp_icc_opts *opts)
{
    p->opts = *opts;
    p->icc_path = talloc_strdup(p, p->opts.profile);
    load_profile(p);
    p->changed = true; // probably
}

// Warning: profile.start must point to a ta allocation, and the function
//          takes over ownership.
void gl_lcms_set_memory_profile(struct gl_lcms *p, bstr *profile)
{
    if (!p->opts.profile_auto) {
        talloc_free(profile->start);
        return;
    }

    if (!p->icc_path && p->icc_data && profile->start &&
        profile->len == p->icc_size &&
        memcmp(profile->start, p->icc_data, p->icc_size) == 0)
    {
        talloc_free(profile->start);
        return;
    }

    p->changed = true;

    talloc_free(p->icc_path);
    p->icc_path = NULL;

    talloc_free(p->icc_data);

    p->icc_data = talloc_steal(p, profile->start);
    p->icc_size = profile->len;
}

// Return and _reset_ whether the lookul table has changed since the last call.
// If it has changed, gl_lcms_get_lut3d() should be called.
bool gl_lcms_has_changed(struct gl_lcms *p)
{
    bool change = p->changed;
    p->changed = false;
    return change;
}

#define LUT3D_CACHE_HEADER "mpv 3dlut cache 1.0\n"

bool gl_lcms_get_lut3d(struct gl_lcms *p, struct lut3d **result_lut3d)
{
    int s_r, s_g, s_b;
    bool result = false;

    if (!parse_3dlut_size(p->opts.size_str, &s_r, &s_g, &s_b))
        return false;

    if (!p->icc_data && !p->icc_path)
        return false;

    void *tmp = talloc_new(NULL);
    uint16_t *output = talloc_array(tmp, uint16_t, s_r * s_g * s_b * 3);
    struct lut3d *lut = NULL;
    cmsContext cms = NULL;

    char *cache_info =
        // Gamma is included in the header to help uniquely identify it,
        // because we may change the parameter in the future or make it
        // customizable, same for the primaries.
        talloc_asprintf(tmp, "intent=%d, size=%dx%dx%d, gamma=2.4, prim=bt2020\n",
                        p->opts.intent, s_r, s_g, s_b);

    bstr iccdata = (bstr) {
        .start = p->icc_data,
        .len   = p->icc_size,
    };

    // check cache
    if (p->opts.cache) {
        MP_INFO(p, "Opening 3D LUT cache in file '%s'.\n", p->opts.cache);
        struct bstr cachedata = load_file(tmp, p->opts.cache, p->global);
        if (bstr_eatstart(&cachedata, bstr0(LUT3D_CACHE_HEADER))
            && bstr_eatstart(&cachedata, bstr0(cache_info))
            && bstr_eatstart(&cachedata, iccdata)
            && cachedata.len == talloc_get_size(output))
        {
            memcpy(output, cachedata.start, cachedata.len);
            goto done;
        } else {
            MP_WARN(p, "3D LUT cache invalid!\n");
        }
    }

    cms = cmsCreateContext(NULL, p);
    if (!cms)
        goto error_exit;
    cmsSetLogErrorHandlerTHR(cms, lcms2_error_handler);

    cmsHPROFILE profile =
        cmsOpenProfileFromMemTHR(cms, p->icc_data, p->icc_size);
    if (!profile)
        goto error_exit;

    // We always generate the 3DLUT against BT.2020, and transform into this
    // space inside the shader if the source differs.
    struct mp_csp_primaries csp = mp_get_csp_primaries(MP_CSP_PRIM_BT_2020);

    cmsCIExyY wp = {csp.white.x, csp.white.y, 1.0};
    cmsCIExyYTRIPLE prim = {
        .Red   = {csp.red.x,   csp.red.y,   1.0},
        .Green = {csp.green.x, csp.green.y, 1.0},
        .Blue  = {csp.blue.x,  csp.blue.y,  1.0},
    };

    // 2.4 is arbitrarily used as a gamma compression factor for the 3DLUT,
    // reducing artifacts due to rounding errors on wide gamut profiles
    cmsToneCurve *tonecurve = cmsBuildGamma(cms, 2.4);
    cmsHPROFILE vid_profile = cmsCreateRGBProfileTHR(cms, &wp, &prim,
                        (cmsToneCurve*[3]){tonecurve, tonecurve, tonecurve});
    cmsFreeToneCurve(tonecurve);
    cmsHTRANSFORM trafo = cmsCreateTransformTHR(cms, vid_profile, TYPE_RGB_16,
                                                profile, TYPE_RGB_16,
                                                p->opts.intent,
                                                cmsFLAGS_HIGHRESPRECALC);
    cmsCloseProfile(profile);
    cmsCloseProfile(vid_profile);

    if (!trafo)
        goto error_exit;

    // transform a (s_r)x(s_g)x(s_b) cube, with 3 components per channel
    uint16_t *input = talloc_array(tmp, uint16_t, s_r * 3);
    for (int b = 0; b < s_b; b++) {
        for (int g = 0; g < s_g; g++) {
            for (int r = 0; r < s_r; r++) {
                input[r * 3 + 0] = r * 65535 / (s_r - 1);
                input[r * 3 + 1] = g * 65535 / (s_g - 1);
                input[r * 3 + 2] = b * 65535 / (s_b - 1);
            }
            size_t base = (b * s_r * s_g + g * s_r) * 3;
            cmsDoTransform(trafo, input, output + base, s_r);
        }
    }

    cmsDeleteTransform(trafo);

    if (p->opts.cache) {
        char *fname = mp_get_user_path(NULL, p->global, p->opts.cache);
        FILE *out = fopen(fname, "wb");
        if (out) {
            fprintf(out, "%s%s", LUT3D_CACHE_HEADER, cache_info);
            fwrite(p->icc_data, p->icc_size, 1, out);
            fwrite(output, talloc_get_size(output), 1, out);
            fclose(out);
        }
        talloc_free(fname);
    }

done: ;

    lut = talloc_ptrtype(NULL, lut);
    *lut = (struct lut3d) {
        .data = talloc_steal(lut, output),
        .size = {s_r, s_g, s_b},
    };

    *result_lut3d = lut;
    result = true;

error_exit:

    if (cms)
        cmsDeleteContext(cms);

    if (!lut)
        MP_FATAL(p, "Error loading ICC profile.\n");

    talloc_free(tmp);
    return result;
}

#else /* HAVE_LCMS2 */

const struct m_sub_options mp_icc_conf = {
    .opts = (const m_option_t[]) { {0} },
    .size = sizeof(struct mp_icc_opts),
    .defaults = &(const struct mp_icc_opts) {0},
};


struct gl_lcms *gl_lcms_init(void *talloc_ctx, struct mp_log *log,
                             struct mpv_global *global)
{
    return (struct gl_lcms *) talloc_new(talloc_ctx);
}

void gl_lcms_set_options(struct gl_lcms *p, struct mp_icc_opts *opts) { }
void gl_lcms_set_memory_profile(struct gl_lcms *p, bstr *profile) { }
bool gl_lcms_get_lut3d(struct gl_lcms *p, struct lut3d **x) { return false; }
bool gl_lcms_has_changed(struct gl_lcms *p) { return false; }

#endif
