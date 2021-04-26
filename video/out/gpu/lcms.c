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

#include <string.h>
#include <math.h>

#include "mpv_talloc.h"

#include "config.h"

#include "stream/stream.h"
#include "common/common.h"
#include "misc/bstr.h"
#include "common/msg.h"
#include "options/m_option.h"
#include "options/path.h"
#include "video/csputils.h"
#include "lcms.h"

#include "osdep/io.h"

#if HAVE_LCMS2

#include <lcms2.h>
#include <libavutil/sha.h>
#include <libavutil/mem.h>

struct gl_lcms {
    void *icc_data;
    size_t icc_size;
    struct AVBufferRef *vid_profile;
    char *current_profile;
    bool using_memory_profile;
    bool changed;
    enum mp_csp_prim current_prim;
    enum mp_csp_trc current_trc;

    struct mp_log *log;
    struct mpv_global *global;
    struct mp_icc_opts *opts;
};

static bool parse_3dlut_size(const char *arg, int *p1, int *p2, int *p3)
{
    if (sscanf(arg, "%dx%dx%d", p1, p2, p3) != 3)
        return false;
    for (int n = 0; n < 3; n++) {
        int s = ((int[]) { *p1, *p2, *p3 })[n];
        if (s < 2 || s > 512)
            return false;
    }
    return true;
}

static int validate_3dlut_size_opt(struct mp_log *log, const m_option_t *opt,
                                   struct bstr name, const char **value)
{
    struct bstr param = bstr0(*value);
    int p1, p2, p3;
    char s[20];
    snprintf(s, sizeof(s), "%.*s", BSTR_P(param));
    return parse_3dlut_size(s, &p1, &p2, &p3);
}

#define OPT_BASE_STRUCT struct mp_icc_opts
const struct m_sub_options mp_icc_conf = {
    .opts = (const m_option_t[]) {
        {"use-embedded-icc-profile", OPT_FLAG(use_embedded)},
        {"icc-profile", OPT_STRING(profile), .flags = M_OPT_FILE},
        {"icc-profile-auto", OPT_FLAG(profile_auto)},
        {"icc-cache-dir", OPT_STRING(cache_dir), .flags = M_OPT_FILE},
        {"icc-intent", OPT_INT(intent)},
        {"icc-force-contrast", OPT_CHOICE(contrast, {"no", 0}, {"inf", -1}),
            M_RANGE(0, 1000000)},
        {"icc-3dlut-size", OPT_STRING_VALIDATE(size_str, validate_3dlut_size_opt)},
        {"3dlut-size", OPT_REPLACED("icc-3dlut-size")},
        {"icc-cache", OPT_REMOVED("see icc-cache-dir")},
        {"icc-contrast", OPT_REMOVED("see icc-force-contrast")},
        {0}
    },
    .size = sizeof(struct mp_icc_opts),
    .defaults = &(const struct mp_icc_opts) {
        .size_str = "64x64x64",
        .intent = INTENT_RELATIVE_COLORIMETRIC,
        .use_embedded = true,
    },
};

static void lcms2_error_handler(cmsContext ctx, cmsUInt32Number code,
                                const char *msg)
{
    struct gl_lcms *p = cmsGetContextUserData(ctx);
    MP_ERR(p, "lcms2: %s\n", msg);
}

static void load_profile(struct gl_lcms *p)
{
    talloc_free(p->icc_data);
    p->icc_data = NULL;
    p->icc_size = 0;
    p->using_memory_profile = false;
    talloc_free(p->current_profile);
    p->current_profile = NULL;

    if (!p->opts->profile || !p->opts->profile[0])
        return;

    char *fname = mp_get_user_path(NULL, p->global, p->opts->profile);
    MP_VERBOSE(p, "Opening ICC profile '%s'\n", fname);
    struct bstr iccdata = stream_read_file(fname, p, p->global,
                                           100000000); // 100 MB
    talloc_free(fname);
    if (!iccdata.len)
        return;

    talloc_free(p->icc_data);

    p->icc_data = iccdata.start;
    p->icc_size = iccdata.len;
    p->current_profile = talloc_strdup(p, p->opts->profile);
}

static void gl_lcms_destructor(void *ptr)
{
    struct gl_lcms *p = ptr;
    av_buffer_unref(&p->vid_profile);
}

struct gl_lcms *gl_lcms_init(void *talloc_ctx, struct mp_log *log,
                             struct mpv_global *global,
                             struct mp_icc_opts *opts)
{
    struct gl_lcms *p = talloc_ptrtype(talloc_ctx, p);
    talloc_set_destructor(p, gl_lcms_destructor);
    *p = (struct gl_lcms) {
        .global = global,
        .log = log,
        .opts = opts,
    };
    gl_lcms_update_options(p);
    return p;
}

void gl_lcms_update_options(struct gl_lcms *p)
{
    if ((p->using_memory_profile && !p->opts->profile_auto) ||
        !bstr_equals(bstr0(p->opts->profile), bstr0(p->current_profile)))
    {
        load_profile(p);
    }

    p->changed = true; // probably
}

// Warning: profile.start must point to a ta allocation, and the function
//          takes over ownership.
// Returns whether the internal profile was changed.
bool gl_lcms_set_memory_profile(struct gl_lcms *p, bstr profile)
{
    if (!p->opts->profile_auto || (p->opts->profile && p->opts->profile[0])) {
        talloc_free(profile.start);
        return false;
    }

    if (p->using_memory_profile &&
        p->icc_data && profile.start &&
        profile.len == p->icc_size &&
        memcmp(profile.start, p->icc_data, p->icc_size) == 0)
    {
        talloc_free(profile.start);
        return false;
    }

    p->changed = true;
    p->using_memory_profile = true;

    talloc_free(p->icc_data);

    p->icc_data = talloc_steal(p, profile.start);
    p->icc_size = profile.len;

    return true;
}

// Guards against NULL and uses bstr_equals to short-circuit some special cases
static bool vid_profile_eq(struct AVBufferRef *a, struct AVBufferRef *b)
{
    if (!a || !b)
        return a == b;

    return bstr_equals((struct bstr){ a->data, a->size },
                       (struct bstr){ b->data, b->size });
}

// Return whether the profile or config has changed since the last time it was
// retrieved. If it has changed, gl_lcms_get_lut3d() should be called.
bool gl_lcms_has_changed(struct gl_lcms *p, enum mp_csp_prim prim,
                         enum mp_csp_trc trc, struct AVBufferRef *vid_profile)
{
    if (p->changed || p->current_prim != prim || p->current_trc != trc)
        return true;

    return !vid_profile_eq(p->vid_profile, vid_profile);
}

// Whether a profile is set. (gl_lcms_get_lut3d() is expected to return a lut,
// but it could still fail due to runtime errors, such as invalid icc data.)
bool gl_lcms_has_profile(struct gl_lcms *p)
{
    return p->icc_size > 0;
}

static cmsHPROFILE get_vid_profile(struct gl_lcms *p, cmsContext cms,
                                   cmsHPROFILE disp_profile,
                                   enum mp_csp_prim prim, enum mp_csp_trc trc)
{
    if (p->opts->use_embedded && p->vid_profile) {
        // Try using the embedded ICC profile
        cmsHPROFILE prof = cmsOpenProfileFromMemTHR(cms, p->vid_profile->data,
                                                    p->vid_profile->size);
        if (prof) {
            MP_VERBOSE(p, "Successfully opened embedded ICC profile\n");
            return prof;
        }

        // Otherwise, warn the user and generate the profile as usual
        MP_WARN(p, "Video contained an invalid ICC profile! Ignoring...\n");
    }

    // The input profile for the transformation is dependent on the video
    // primaries and transfer characteristics
    struct mp_csp_primaries csp = mp_get_csp_primaries(prim);
    cmsCIExyY wp_xyY = {csp.white.x, csp.white.y, 1.0};
    cmsCIExyYTRIPLE prim_xyY = {
        .Red   = {csp.red.x,   csp.red.y,   1.0},
        .Green = {csp.green.x, csp.green.y, 1.0},
        .Blue  = {csp.blue.x,  csp.blue.y,  1.0},
    };

    cmsToneCurve *tonecurve[3] = {0};
    switch (trc) {
    case MP_CSP_TRC_LINEAR:  tonecurve[0] = cmsBuildGamma(cms, 1.0); break;
    case MP_CSP_TRC_GAMMA18: tonecurve[0] = cmsBuildGamma(cms, 1.8); break;
    case MP_CSP_TRC_GAMMA20: tonecurve[0] = cmsBuildGamma(cms, 2.0); break;
    case MP_CSP_TRC_GAMMA22: tonecurve[0] = cmsBuildGamma(cms, 2.2); break;
    case MP_CSP_TRC_GAMMA24: tonecurve[0] = cmsBuildGamma(cms, 2.4); break;
    case MP_CSP_TRC_GAMMA26: tonecurve[0] = cmsBuildGamma(cms, 2.6); break;
    case MP_CSP_TRC_GAMMA28: tonecurve[0] = cmsBuildGamma(cms, 2.8); break;

    case MP_CSP_TRC_SRGB:
        // Values copied from Little-CMS
        tonecurve[0] = cmsBuildParametricToneCurve(cms, 4,
                (double[5]){2.40, 1/1.055, 0.055/1.055, 1/12.92, 0.04045});
        break;

    case MP_CSP_TRC_PRO_PHOTO:
        tonecurve[0] = cmsBuildParametricToneCurve(cms, 4,
                (double[5]){1.8, 1.0, 0.0, 1/16.0, 0.03125});
        break;

    case MP_CSP_TRC_BT_1886: {
        double src_black[3];
        if (p->opts->contrast < 0) {
            // User requested infinite contrast, return 2.4 profile
            tonecurve[0] = cmsBuildGamma(cms, 2.4);
            break;
        } else if (p->opts->contrast > 0) {
            MP_VERBOSE(p, "Using specified contrast: %d\n", p->opts->contrast);
            for (int i = 0; i < 3; i++)
                src_black[i] = 1.0 / p->opts->contrast;
        } else {
            // To build an appropriate BT.1886 transformation we need access to
            // the display's black point, so we use LittleCMS' detection
            // function. Relative colorimetric is used since we want to
            // approximate the BT.1886 to the target device's actual black
            // point even in e.g. perceptual mode
            const int intent = MP_INTENT_RELATIVE_COLORIMETRIC;
            cmsCIEXYZ bp_XYZ;
            if (!cmsDetectBlackPoint(&bp_XYZ, disp_profile, intent, 0))
                return false;

            // Map this XYZ value back into the (linear) source space
            cmsHPROFILE rev_profile;
            cmsToneCurve *linear = cmsBuildGamma(cms, 1.0);
            rev_profile = cmsCreateRGBProfileTHR(cms, &wp_xyY, &prim_xyY,
                    (cmsToneCurve*[3]){linear, linear, linear});
            cmsHPROFILE xyz_profile = cmsCreateXYZProfile();
            cmsHTRANSFORM xyz2src = cmsCreateTransformTHR(cms,
                    xyz_profile, TYPE_XYZ_DBL, rev_profile, TYPE_RGB_DBL,
                    intent, 0);
            cmsFreeToneCurve(linear);
            cmsCloseProfile(rev_profile);
            cmsCloseProfile(xyz_profile);
            if (!xyz2src)
                return false;

            cmsDoTransform(xyz2src, &bp_XYZ, src_black, 1);
            cmsDeleteTransform(xyz2src);

            double contrast = 3.0 / (src_black[0] + src_black[1] + src_black[2]);
            MP_VERBOSE(p, "Detected ICC profile contrast: %f\n", contrast);
        }

        // Build the parametric BT.1886 transfer curve, one per channel
        for (int i = 0; i < 3; i++) {
            const double gamma = 2.40;
            double binv = pow(src_black[i], 1.0/gamma);
            tonecurve[i] = cmsBuildParametricToneCurve(cms, 6,
                    (double[4]){gamma, 1.0 - binv, binv, 0.0});
        }
        break;
    }

    default:
        abort();
    }

    if (!tonecurve[0])
        return false;

    if (!tonecurve[1]) tonecurve[1] = tonecurve[0];
    if (!tonecurve[2]) tonecurve[2] = tonecurve[0];

    cmsHPROFILE *vid_profile = cmsCreateRGBProfileTHR(cms, &wp_xyY, &prim_xyY,
                                                      tonecurve);

    if (tonecurve[2] != tonecurve[0]) cmsFreeToneCurve(tonecurve[2]);
    if (tonecurve[1] != tonecurve[0]) cmsFreeToneCurve(tonecurve[1]);
    cmsFreeToneCurve(tonecurve[0]);

    return vid_profile;
}

bool gl_lcms_get_lut3d(struct gl_lcms *p, struct lut3d **result_lut3d,
                       enum mp_csp_prim prim, enum mp_csp_trc trc,
                       struct AVBufferRef *vid_profile)
{
    int s_r, s_g, s_b;
    bool result = false;

    p->changed = false;
    p->current_prim = prim;
    p->current_trc = trc;

    // We need to hold on to a reference to the video's ICC profile for as long
    // as we still need to perform equality checking, so generate a new
    // reference here
    av_buffer_unref(&p->vid_profile);
    if (vid_profile) {
        MP_VERBOSE(p, "Got an embedded ICC profile.\n");
        p->vid_profile = av_buffer_ref(vid_profile);
        if (!p->vid_profile)
            abort();
    }

    if (!parse_3dlut_size(p->opts->size_str, &s_r, &s_g, &s_b))
        return false;

    if (!gl_lcms_has_profile(p))
        return false;

    void *tmp = talloc_new(NULL);
    uint16_t *output = talloc_array(tmp, uint16_t, s_r * s_g * s_b * 4);
    struct lut3d *lut = NULL;
    cmsContext cms = NULL;

    char *cache_file = NULL;
    if (p->opts->cache_dir && p->opts->cache_dir[0]) {
        // Gamma is included in the header to help uniquely identify it,
        // because we may change the parameter in the future or make it
        // customizable, same for the primaries.
        char *cache_info = talloc_asprintf(tmp,
                "ver=1.4, intent=%d, size=%dx%dx%d, prim=%d, trc=%d, "
                "contrast=%d\n",
                p->opts->intent, s_r, s_g, s_b, prim, trc, p->opts->contrast);

        uint8_t hash[32];
        struct AVSHA *sha = av_sha_alloc();
        if (!sha)
            abort();
        av_sha_init(sha, 256);
        av_sha_update(sha, cache_info, strlen(cache_info));
        if (vid_profile)
            av_sha_update(sha, vid_profile->data, vid_profile->size);
        av_sha_update(sha, p->icc_data, p->icc_size);
        av_sha_final(sha, hash);
        av_free(sha);

        char *cache_dir = mp_get_user_path(tmp, p->global, p->opts->cache_dir);
        cache_file = talloc_strdup(tmp, "");
        for (int i = 0; i < sizeof(hash); i++)
            cache_file = talloc_asprintf_append(cache_file, "%02X", hash[i]);
        cache_file = mp_path_join(tmp, cache_dir, cache_file);

        mp_mkdirp(cache_dir);
    }

    // check cache
    if (cache_file && stat(cache_file, &(struct stat){0}) == 0) {
        MP_VERBOSE(p, "Opening 3D LUT cache in file '%s'.\n", cache_file);
        struct bstr cachedata = stream_read_file(cache_file, tmp, p->global,
                                                 1000000000); // 1 GB
        if (cachedata.len == talloc_get_size(output)) {
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

    cmsHPROFILE vid_hprofile = get_vid_profile(p, cms, profile, prim, trc);
    if (!vid_hprofile) {
        cmsCloseProfile(profile);
        goto error_exit;
    }

    cmsHTRANSFORM trafo = cmsCreateTransformTHR(cms, vid_hprofile, TYPE_RGB_16,
                                                profile, TYPE_RGBA_16,
                                                p->opts->intent,
                                                cmsFLAGS_HIGHRESPRECALC |
                                                cmsFLAGS_BLACKPOINTCOMPENSATION);
    cmsCloseProfile(profile);
    cmsCloseProfile(vid_hprofile);

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
            size_t base = (b * s_r * s_g + g * s_r) * 4;
            cmsDoTransform(trafo, input, output + base, s_r);
        }
    }

    cmsDeleteTransform(trafo);

    if (cache_file) {
        FILE *out = fopen(cache_file, "wb");
        if (out) {
            fwrite(output, talloc_get_size(output), 1, out);
            fclose(out);
        }
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
                             struct mpv_global *global,
                             struct mp_icc_opts *opts)
{
    return (struct gl_lcms *) talloc_new(talloc_ctx);
}

void gl_lcms_update_options(struct gl_lcms *p) { }
bool gl_lcms_set_memory_profile(struct gl_lcms *p, bstr profile) {return false;}

bool gl_lcms_has_changed(struct gl_lcms *p, enum mp_csp_prim prim,
                         enum mp_csp_trc trc, struct AVBufferRef *vid_profile)
{
    return false;
}

bool gl_lcms_has_profile(struct gl_lcms *p)
{
    return false;
}

bool gl_lcms_get_lut3d(struct gl_lcms *p, struct lut3d **result_lut3d,
                       enum mp_csp_prim prim, enum mp_csp_trc trc,
                       struct AVBufferRef *vid_profile)
{
    return false;
}

#endif
