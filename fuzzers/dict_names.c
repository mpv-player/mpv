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

#include "common.h"

static void print_names(mpv_handle *ctx, const char *kind, const char *property)
{
    mpv_node list;
    check_error(mpv_get_property(ctx, property, MPV_FORMAT_NODE, &list));
    for (int n = 0; n < list.u.list->num; n++) {
        mpv_node *entry = &list.u.list->values[n];
        if (entry->format == MPV_FORMAT_NODE_MAP) {
            for (int i = 0; i < entry->u.list->num; i++) {
                if (!strcmp(entry->u.list->keys[i], "name"))
                    printf("%s %s\n", kind, entry->u.list->values[i].u.string);
            }
        } else {
            printf("%s %s\n", kind, entry->u.string);
        }
    }
    mpv_free_node_contents(&list);
}

int main(void)
{
    mpv_handle *ctx = mpv_create();
    if (!ctx)
        return 1;
    check_error(mpv_initialize(ctx));
    print_names(ctx, "option", "options");
    print_names(ctx, "property", "property-list");
    print_names(ctx, "command", "command-list");
    print_names(ctx, "key", "input-key-list");
    print_names(ctx, "protocol", "protocol-list");
    mpv_terminate_destroy(ctx);
    return 0;
}
