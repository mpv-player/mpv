/*
 * PNM image files loader
 *
 * copyleft (C) 2005-2010 Reimar Döffinger <Reimar.Doeffinger@gmx.de>
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

/**
 * \file pnm_loader.c
 * \brief PNM image files loader
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include "pnm_loader.h"

/**
 * \brief skips whitespace and comments
 * \param f file to read from
 */
static void ppm_skip(FILE *f) {
  int c, comment = 0;
  do {
    c = fgetc(f);
    if (c == '#')
      comment = 1;
    if (c == '\n')
      comment = 0;
  } while (c != EOF && (isspace(c) || comment));
  if (c != EOF)
    ungetc(c, f);
}

#define MAXDIM (16 * 1024)

uint8_t *read_pnm(FILE *f, int *width, int *height,
                  int *bytes_per_pixel, int *maxval) {
  uint8_t *data;
  int type;
  unsigned w, h, m, val, bpp;
  *width = *height = *bytes_per_pixel = *maxval = 0;
  ppm_skip(f);
  if (fgetc(f) != 'P')
    return NULL;
  type = fgetc(f);
  if (type != '5' && type != '6')
    return NULL;
  ppm_skip(f);
  if (fscanf(f, "%u", &w) != 1)
    return NULL;
  ppm_skip(f);
  if (fscanf(f, "%u", &h) != 1)
    return NULL;
  ppm_skip(f);
  if (fscanf(f, "%u", &m) != 1)
    return NULL;
  val = fgetc(f);
  if (!isspace(val))
    return NULL;
  if (w > MAXDIM || h > MAXDIM)
    return NULL;
  bpp = (m > 255) ? 2 : 1;
  if (type == '6')
    bpp *= 3;
  data = malloc(w * h * bpp);
  if (fread(data, w * bpp, h, f) != h) {
    free(data);
    return NULL;
  }
  *width  = w;
  *height = h;
  *bytes_per_pixel = bpp;
  *maxval = m;
  return data;
}
