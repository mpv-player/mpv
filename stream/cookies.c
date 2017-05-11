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

#include "osdep/io.h"

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

/* Loads a file into RAM */
static char *load_file(struct mp_log *log, const char *filename, int64_t * length)
{
    int fd;
    char *buffer = NULL;

    mp_verbose(log, "Loading cookie file: %s\n", filename);

    fd = open(filename, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        mp_verbose(log, "Could not open");
        goto err_out;
    }

    *length = lseek(fd, 0, SEEK_END);

    if (*length < 0) {
        mp_verbose(log, "Could not find EOF");
        goto err_out;
    }

    if (*length > SIZE_MAX - 1) {
        mp_verbose(log, "File too big, could not malloc.");
        goto err_out;
    }

    lseek(fd, 0, SEEK_SET);

    if (!(buffer = malloc(*length + 1))) {
        mp_verbose(log, "Could not malloc.");
        goto err_out;
    }

    if (read(fd, buffer, *length) != *length) {
        mp_verbose(log, "Read is behaving funny.");
        goto err_out;
    }
    close(fd);
    buffer[*length] = 0;

    return buffer;

err_out:
    if (fd != -1) close(fd);
    free(buffer);
    return NULL;
}

/* Loads a cookies.txt file into a linked list. */
static struct cookie_list_type *load_cookies_from(void *ctx,
                                                  struct mp_log *log,
                                                  const char *filename)
{
    char *ptr, *file;
    int64_t length;

    ptr = file = load_file(log, filename, &length);
    if (!ptr)
        return NULL;

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
    free(file);
    return list;
}

// Return a cookies string as expected by lavf (libavformat/http.c). The format
// is like a Set-Cookie header (http://curl.haxx.se/rfc/cookie_spec.html),
// separated by newlines.
char *cookies_lavf(void *talloc_ctx, struct mp_log *log, char *file)
{
    void *tmp = talloc_new(NULL);
    struct cookie_list_type *list = NULL;
    if (file && file[0])
        list = load_cookies_from(tmp, log, file);

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
