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

#ifndef MP_JSON_H
#define MP_JSON_H

#define MAX_JSON_DEPTH 50

struct bstr;
struct mpv_node;

int json_parse(void *ta_parent, struct mpv_node *dst, char **src, int max_depth);
int json_append(struct bstr *b, const struct mpv_node *src, int indent);
void json_skip_whitespace(char **src);
int json_write(char **s, struct mpv_node *src);
int json_write_pretty(char **s, struct mpv_node *src);

#endif
