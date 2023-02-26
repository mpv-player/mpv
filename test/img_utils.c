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

#include <assert.h>
#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>

#include "common/common.h"
#include "img_utils.h"
#include "video/img_format.h"
#include "video/fmt-conversion.h"

int imgfmts[IMGFMT_AVPIXFMT_END - IMGFMT_AVPIXFMT_START + 100];
int num_imgfmts;

static enum AVPixelFormat pixfmt_unsup[100];
static int num_pixfmt_unsup;

static int cmp_imgfmt_name(const void *a, const void *b)
{
    char *name_a = mp_imgfmt_to_name(*(int *)a);
    char *name_b = mp_imgfmt_to_name(*(int *)b);

    return strcmp(name_a, name_b);
}

void init_imgfmts_list(void)
{
    const AVPixFmtDescriptor *avd = av_pix_fmt_desc_next(NULL);
    for (; avd; avd = av_pix_fmt_desc_next(avd)) {
        enum AVPixelFormat fmt = av_pix_fmt_desc_get_id(avd);
        int mpfmt = pixfmt2imgfmt(fmt);
        if (!mpfmt) {
            assert(num_pixfmt_unsup < MP_ARRAY_SIZE(pixfmt_unsup));
            pixfmt_unsup[num_pixfmt_unsup++] = fmt;
        }
    }

    for (int fmt = IMGFMT_START; fmt <= IMGFMT_END; fmt++) {
        struct mp_imgfmt_desc d = mp_imgfmt_get_desc(fmt);
        enum AVPixelFormat pixfmt = imgfmt2pixfmt(fmt);
        if (d.id || pixfmt != AV_PIX_FMT_NONE) {
            assert(num_imgfmts < MP_ARRAY_SIZE(imgfmts)); // enlarge that array
            imgfmts[num_imgfmts++] = fmt;
        }
    }

    qsort(imgfmts, num_imgfmts, sizeof(imgfmts[0]), cmp_imgfmt_name);
}
