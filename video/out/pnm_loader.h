/*
 * PNM image files loader
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
 *
 * You can alternatively redistribute this file and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifndef MPLAYER_PNM_LOADER_H
#define MPLAYER_PNM_LOADER_H

#include <stdio.h>
#include <stdint.h>

/**
 * Read a "portable anymap" image.
 * Supports raw PGM (P5) and PNM (P6).
 *
 * @param[in]  f                input stream.
 * @param[out] width            width of the loaded image.
 * @param[out] height           height of the loaded image.
 * @param[out] bytes_per_pixel  format of the loaded image.
 * @param[out] maxval           maximum pixel value; possible values are:
 *                              1 for  8 bits gray,
 *                              2 for 16 bits gray,
 *                              3 for  8 bits per component RGB,
 *                              6 for 16 bits per component RGB.
 * @return                      a newly allocated array of
 *                              width*height*bytes_per_pixel bytes,
 *                              or NULL in case of error.
 */
uint8_t *read_pnm(FILE *f, int *width, int *height,
                  int *bytes_per_pixel, int *maxval);

#endif /* MPLAYER_PNM_LOADER_H */
