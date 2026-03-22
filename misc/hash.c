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

#include <stdint.h>

#include <libavutil/hash.h>

#include "common/common.h"
#include "misc/hash.h"

bstr mp_hash_to_bstr(void *talloc_ctx, const uint8_t *data, size_t len, const char *algorithm)
{
    struct AVHashContext *ctx = NULL;
    mp_require(av_hash_alloc(&ctx, algorithm) == 0);
    int size = av_hash_get_size(ctx);
    uint8_t hash[AV_HASH_MAX_SIZE] = {0};

    av_hash_init(ctx);
    av_hash_update(ctx, data, len);
    av_hash_final(ctx, hash);
    av_hash_freep(&ctx);

    bstr ret = {0};
    for (int n = 0; n < size; n++)
        bstr_xappend_asprintf(talloc_ctx, &ret, "%02X", hash[n]);
    return ret;
}
