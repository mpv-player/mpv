/*
 * HTTP Cookies
 * Reads Netscape and Mozilla cookies.txt files
 *
 * Copyright (c) 2003 Dave Lambley <mplayer@davel.me.uk>
 *
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
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <inttypes.h>

#include "stream/stream.h"
#include "options/options.h"
#include "cookies.h"
#include "common/msg.h"

#define MAX_COOKIES 20

typedef struct cookie_list_type {
    char *name;
    char *value;
    char *domain;
    char *path;

    int secure;

    struct cookie_list_type *next;
} cookie_list_t;

/* Like strdup, but stops at anything <31. */
static char *col_dup(void *talloc_ctx, const char *src)
{
    int length = 0;
    while (src[length] > 31)
        length++;

    return talloc_strndup(talloc_ctx, src, length);
}

/* Finds the start of all the columns */
static int parse_line(char **ptr, char *cols[7])
{
    int col;
    cols[0] = *ptr;

    for (col = 1; col < 7; col++) {
        for (; (**ptr) > 31; (*ptr)++);
        if (**ptr == 0)
            return 0;
        (*ptr)++;
        if ((*ptr)[-1] != 9)
            return 0;
        cols[col] = (*ptr);
    }

    return 1;
}

/* Loads a cookies.txt file into a linked list. */
static struct cookie_list_type *load_cookies_from(void *ctx,
                                                  struct mpv_global *global,
                                                  struct mp_log *log,
                                                  const char *filename)
{
    mp_verbose(log, "Loading cookie file: %s\n", filename);
    bstr data = stream_read_file(filename, ctx, global, 1000000);
    if (!data.start) {
        mp_verbose(log, "Error reading\n");
        return NULL;
    }

    bstr_xappend(ctx, &data, (struct bstr){"", 1}); // null-terminate
    char *ptr = data.start;

    struct cookie_list_type *list = NULL;
    while (*ptr) {
        char *cols[7];
        if (parse_line(&ptr, cols)) {
            struct cookie_list_type *new;
            new = talloc_zero(ctx, cookie_list_t);
            new->name = col_dup(new, cols[5]);
            new->value = col_dup(new, cols[6]);
            new->path = col_dup(new, cols[2]);
            new->domain = col_dup(new, cols[0]);
            new->secure = (*(cols[3]) == 't') || (*(cols[3]) == 'T');
            new->next = list;
            list = new;
        }
    }

    return list;
}

// Return a cookies string as expected by lavf (libavformat/http.c). The format
// is like a Set-Cookie header (http://curl.haxx.se/rfc/cookie_spec.html),
// separated by newlines.
char *cookies_lavf(void *talloc_ctx,
                   struct mpv_global *global,
                   struct mp_log *log,
                   const char *file)
{
    void *tmp = talloc_new(NULL);
    struct cookie_list_type *list = NULL;
    if (file && file[0])
        list = load_cookies_from(tmp, global, log, file);

    char *res = talloc_strdup(talloc_ctx, "");

    while (list) {
        res = talloc_asprintf_append_buffer(res,
                    "%s=%s; path=%s; domain=%s; %s\n", list->name, list->value,
                    list->path, list->domain, list->secure ? "secure" : "");
        list = list->next;
    }

    talloc_free(tmp);
    return res;
}
