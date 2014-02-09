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
#include "bstr/bstr.h"
#include "common/msg.h"
#include "options/m_option.h"
#include "options/path.h"

#include "gl_video.h"
#include "gl_lcms.h"

#if HAVE_LCMS2

#include <pthread.h>
#include <lcms2.h>

// lcms2 only provides a global error handler function, so we have to do this.
// Not setting a lcms2 error handler will suppress any error messages.
static pthread_mutex_t lcms2_dumb_crap_lock = PTHREAD_MUTEX_INITIALIZER;
static struct mp_log *lcms2_dumb_crap;

static bool parse_3dlut_size(const char *arg, int *p1, int *p2, int *p3)
{
    if (sscanf(arg, "%dx%dx%d", p1, p2, p3) != 3)
        return false;
    for (int n = 0; n < 3; n++) {
        int s = ((int[]) { *p1, *p2, *p3 })[n];
        if (s < 2 || s > 256 || ((s - 1) & s))
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
    .opts = (m_option_t[]) {
        OPT_STRING("icc-profile", profile, 0),
        OPT_STRING("icc-cache", cache, 0),
        OPT_INT("icc-intent", intent, 0),
        OPT_STRING_VALIDATE("3dlut-size", size_str, 0, validate_3dlut_size_opt),
        {0}
    },
    .size = sizeof(struct mp_icc_opts),
    .defaults = &(const struct mp_icc_opts) {
        .size_str = "128x256x64",
        .intent = INTENT_ABSOLUTE_COLORIMETRIC,
    },
};

static void lcms2_error_handler(cmsContext ctx, cmsUInt32Number code,
                                const char *msg)
{
    pthread_mutex_lock(&lcms2_dumb_crap_lock);
    if (lcms2_dumb_crap)
        mp_msg(lcms2_dumb_crap, MSGL_ERR, "lcms2: %s\n", msg);
    pthread_mutex_unlock(&lcms2_dumb_crap_lock);
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

#define LUT3D_CACHE_HEADER "mpv 3dlut cache 1.0\n"

struct lut3d *mp_load_icc(struct mp_icc_opts *opts, struct mp_log *log,
                          struct mpv_global *global)
{
    int s_r, s_g, s_b;
    if (!parse_3dlut_size(opts->size_str, &s_r, &s_g, &s_b))
        return NULL;

    if (!opts->profile)
        return NULL;

    void *tmp = talloc_new(NULL);
    uint16_t *output = talloc_array(tmp, uint16_t, s_r * s_g * s_b * 3);
    struct lut3d *lut = NULL;
    bool locked = false;

    mp_msg(log, MSGL_INFO, "Opening ICC profile '%s'\n", opts->profile);
    struct bstr iccdata = load_file(tmp, opts->profile, global);
    if (!iccdata.len)
        goto error_exit;

    char *cache_info = talloc_asprintf(tmp, "intent=%d, size=%dx%dx%d\n",
                                       opts->intent, s_r, s_g, s_b);

    // check cache
    if (opts->cache) {
        mp_msg(log, MSGL_INFO, "Opening 3D LUT cache in file '%s'.\n",
                   opts->cache);
        struct bstr cachedata = load_file(tmp, opts->cache, global);
        if (bstr_eatstart(&cachedata, bstr0(LUT3D_CACHE_HEADER))
            && bstr_eatstart(&cachedata, bstr0(cache_info))
            && bstr_eatstart(&cachedata, iccdata)
            && cachedata.len == talloc_get_size(output))
        {
            memcpy(output, cachedata.start, cachedata.len);
            goto done;
        } else {
            mp_msg(log, MSGL_WARN, "3D LUT cache invalid!\n");
        }
    }

    locked = true;
    pthread_mutex_lock(&lcms2_dumb_crap_lock);
    lcms2_dumb_crap = log;
    cmsSetLogErrorHandler(lcms2_error_handler);

    cmsHPROFILE profile = cmsOpenProfileFromMem(iccdata.start, iccdata.len);
    if (!profile)
        goto error_exit;

    cmsCIExyY d65 = {0.3127, 0.3290, 1.0};
    static const cmsCIExyYTRIPLE bt709prim = {
        .Red   = {0.64, 0.33, 1.0},
        .Green = {0.30, 0.60, 1.0},
        .Blue  = {0.15, 0.06, 1.0},
    };

    /* Rec BT.709 defines the tone curve as:
       V = 1.099 * L^0.45 - 0.099 for L >= 0.018
       V = 4.500 * L              for L <  0.018

       The 0.18 parameter comes from inserting 0.018 into the function */
    cmsToneCurve *tonecurve = cmsBuildParametricToneCurve(NULL, 4,
            (cmsFloat64Number[5]){1/0.45, 1/1.099, 0.099, 1/4.5, 0.18});
    cmsHPROFILE vid_profile = cmsCreateRGBProfile(&d65, &bt709prim,
                        (cmsToneCurve*[3]){tonecurve, tonecurve, tonecurve});
    cmsFreeToneCurve(tonecurve);
    cmsHTRANSFORM trafo = cmsCreateTransform(vid_profile, TYPE_RGB_16,
                                             profile, TYPE_RGB_16,
                                             opts->intent,
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

    if (opts->cache) {
        FILE *out = fopen(opts->cache, "wb");
        if (out) {
            fprintf(out, "%s%s", LUT3D_CACHE_HEADER, cache_info);
            fwrite(iccdata.start, iccdata.len, 1, out);
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

error_exit:

    if (locked) {
        lcms2_dumb_crap = NULL;
        cmsSetLogErrorHandler(NULL);
        pthread_mutex_unlock(&lcms2_dumb_crap_lock);
    }

    if (!lut)
        mp_msg(log, MSGL_FATAL, "Error loading ICC profile.\n");

    talloc_free(tmp);
    return lut;
}

#else /* HAVE_LCMS2 */

const struct m_sub_options mp_icc_conf = {
    .opts = (m_option_t[]) { {0} },
    .size = sizeof(struct mp_icc_opts),
    .defaults = &(const struct mp_icc_opts) {0},
};

struct lut3d *mp_load_icc(struct mp_icc_opts *opts, struct mp_log *log,
                          struct mpv_global *global)
{
    mp_msg(log, MSGL_FATAL, "LCMS2 support not compiled.\n");
    return NULL;
}

#endif
