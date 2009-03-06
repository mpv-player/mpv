/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

/* system has no swab.  emulate via bswap */
#include "mpbswap.h"
#include <unistd.h>

void swab(const void *from, void *to, ssize_t n) {
  const int16_t *in = (int16_t*)from;
  int16_t *out = (int16_t*)to;
  int i;
  n /= 2;
  for (i = 0 ; i < n; i++) {
    out[i] = bswap_16(in[i]);
  }
}
