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
 */

#ifndef MP_JSON_H
#define MP_JSON_H

// We reuse mpv_node.
#include "libmpv/client.h"

int json_parse(void *ta_parent, struct mpv_node *dst, char **src, int max_depth);
void json_skip_whitespace(char **src);
int json_write(char **s, struct mpv_node *src);

#endif
