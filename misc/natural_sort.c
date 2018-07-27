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

#include "misc/ctype.h"

#include "natural_sort.h"

// Comparison function for an ASCII-only "natural" sort. Case is ignored and
// numbers are ordered by value regardless of padding. Two filenames that differ
// only in the padding of numbers will be considered equal and end up in
// arbitrary order. Bytes outside of A-Z/a-z/0-9 will by sorted by byte value.
int mp_natural_sort_cmp(const char *name1, const char *name2)
{
    while (name1[0] && name2[0]) {
        if (mp_isdigit(name1[0]) && mp_isdigit(name2[0])) {
            while (name1[0] == '0')
                name1++;
            while (name2[0] == '0')
                name2++;
            const char *end1 = name1, *end2 = name2;
            while (mp_isdigit(*end1))
                end1++;
            while (mp_isdigit(*end2))
                end2++;
            // With padding stripped, a number with more digits is bigger.
            if ((end1 - name1) < (end2 - name2))
                return -1;
            if ((end1 - name1) > (end2 - name2))
                return 1;
            // Same length, lexicographical works.
            while (name1 < end1) {
                if (name1[0] < name2[0])
                    return -1;
                if (name1[0] > name2[0])
                    return 1;
                name1++;
                name2++;
            }
        } else {
            if (mp_tolower(name1[0]) < mp_tolower(name2[0]))
                return -1;
            if (mp_tolower(name1[0]) > mp_tolower(name2[0]))
                return 1;
            name1++;
            name2++;
        }
    }
    if (name2[0])
        return -1;
    if (name1[0])
        return 1;
    return 0;
}
