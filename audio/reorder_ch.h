/*
 * common functions for reordering audio channels
 *
 * Copyright (C) 2007 Ulion <ulion A gmail P com>
 *
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

#ifndef MPLAYER_REORDER_CH_H
#define MPLAYER_REORDER_CH_H

#include <inttypes.h>

void reorder_to_planar(void *restrict out, const void *restrict in,
                       size_t size, size_t nchan, size_t nmemb);
void reorder_to_packed(uint8_t *out, uint8_t **in,
                       size_t size, size_t nchan, size_t nmemb);

#endif /* MPLAYER_REORDER_CH_H */
