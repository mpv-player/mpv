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

#ifndef MPLAYER_MATRIXVIEW_H
#define MPLAYER_MATRIXVIEW_H

#include <stdint.h>

void matrixview_init (int w, int h);
void matrixview_reshape (int w, int h);
void matrixview_draw (int w, int h, double currentTime, float frameTime,
                      uint8_t *data);
void matrixview_matrix_resize(int w, int h);
void matrixview_contrast_set(float contrast);
void matrixview_brightness_set(float brightness);

#endif /* MPLAYER_MATRIXVIEW_H */
