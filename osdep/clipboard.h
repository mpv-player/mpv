/*
 * Clipboard access routines
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

#include "player/core.h"
#include "video/mp_image.h"

enum m_clipboard_type {
    CLIPBOARD_UNKNOWN,
    CLIPBOARD_TEXT,
    CLIPBOARD_URL,
    CLIPBOARD_PATH,
    CLIPBOARD_PATHS,
    CLIPBOARD_IMAGE,
};

struct m_clipboard_item {
    enum m_clipboard_type type;
    union {
        char *string;
        char **string_list;
        struct mp_image *image;
    };
};

struct clipboard_state {
#if HAVE_COCOA
    long changeCount;
#endif
};

enum m_clipboard_return {
    CLIPBOARD_OK = 1,
    CLIPBOARD_NONE = 0,
    CLIPBOARD_FAILED = -1,
};

int m_clipboard_get(struct MPContext *ctx, struct m_clipboard_item *item);
int m_clipboard_set(struct MPContext *ctx, const struct m_clipboard_item *item);
bool m_clipboard_poll(struct MPContext *ctx);

static inline void m_clipboard_item_free(struct m_clipboard_item *item) {
    switch (item->type) {
    case CLIPBOARD_TEXT:
    case CLIPBOARD_PATH:
    case CLIPBOARD_URL:
        talloc_free(item->string);
        break;
    case CLIPBOARD_PATHS:
        talloc_free(item->string_list);
        break;
    case CLIPBOARD_IMAGE:
        talloc_free(item->image);
        break;
    default:
        break;
    }
}

static inline struct clipboard_state *m_clipboard_new(struct MPContext *ctx) {
    return talloc_zero(ctx, struct clipboard_state);
}
