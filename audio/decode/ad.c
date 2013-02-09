/*
 * audio decoder interface
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#include "stream/stream.h"
#include "demux/demux.h"
#include "demux/stheader.h"
#include "ad.h"

/* Missed vorbis, mad, dshow */

extern const ad_functions_t mpcodecs_ad_mpg123;
extern const ad_functions_t mpcodecs_ad_lavc;
extern const ad_functions_t mpcodecs_ad_spdif;

const ad_functions_t * const mpcodecs_ad_drivers[] =
{
#ifdef CONFIG_MPG123
  &mpcodecs_ad_mpg123,
#endif
  &mpcodecs_ad_lavc,
  &mpcodecs_ad_spdif,
  NULL
};
