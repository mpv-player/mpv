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

#include <ctype.h>
#include <string.h>

#include "mpv_talloc.h"

#include "common/common.h"
#include "misc/bstr.h"
#include "misc/language.h"
#include "misc/path_utils.h"
#include "misc/string_similarity.h"

static bool is_suffix_token(const char *tkn)
{
    return mp_language_is_suffix_token(tkn);
}

char *mp_normalize_base_name(void *ta_ctx, const char *path)
{
    struct bstr base = bstr0(mp_basename(path));
    base = bstr_strip_ext(base);
    char *tmpbuf = talloc_strndup(ta_ctx, base.start, base.len);
    for (int i = 0; tmpbuf[i]; i++)
        tmpbuf[i] = tolower((unsigned char)tmpbuf[i]);
    char **tokens = NULL;
    int ntok = 0;
    char *p = tmpbuf;
    while (*p) {
        while (*p && !isalnum((unsigned char)*p)) p++;
        if (!*p) break;
        char *start = p;
        while (*p && isalnum((unsigned char)*p)) p++;
        char save = *p; *p = '\0';
        MP_TARRAY_APPEND(ta_ctx, tokens, ntok, talloc_strdup(ta_ctx, start));
        *p = save;
    }
    while (ntok > 0 && is_suffix_token(tokens[ntok - 1]))
        ntok--;
    char *out = talloc_strdup(ta_ctx, "");
    for (int i = 0; i < ntok; i++)
        out = talloc_asprintf_append_buffer(out, "%s", tokens[i]);
    if (!out[0])
        out = talloc_strdup(ta_ctx, tmpbuf);
    return out;
}

int mp_levenshtein_dist(const char *a, const char *b)
{
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (la == 0) return lb;
    if (lb == 0) return la;
    int *prev = talloc_array(NULL, int, lb + 1);
    int *curr = talloc_array(NULL, int, lb + 1);
    for (int j = 0; j <= lb; j++) prev[j] = j;
    for (int i = 1; i <= la; i++) {
        curr[0] = i;
        for (int j = 1; j <= lb; j++) {
            int cost = a[i - 1] == b[j - 1] ? 0 : 1;
            int del = prev[j] + 1;
            int ins = curr[j - 1] + 1;
            int sub = prev[j - 1] + cost;
            int m = del < ins ? del : ins;
            curr[j] = m < sub ? m : sub;
        }
        int *tmpv = prev; prev = curr; curr = tmpv;
    }
    int d = prev[lb];
    talloc_free(prev);
    talloc_free(curr);
    return d;
}

double mp_similarity_ratio(const char *a, const char *b)
{
    int la = (int)strlen(a), lb = (int)strlen(b);
    int m = la > lb ? la : lb;
    if (m == 0) return 1.0;
    int d = mp_levenshtein_dist(a, b);
    return 1.0 - (double)d / (double)m;
}
