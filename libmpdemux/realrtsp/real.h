/*
 * This file was ported to MPlayer from xine CVS real.h,v 1.2 2002/12/24 01:30:22
 */

/*
 * Copyright (C) 2002 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 *
 * special functions for real streams.
 * adopted from joschkas real tools.
 *
 */
 
#ifndef HAVE_REAL_H
#define HAVE_REAL_H

#include "rmff.h"
#include "rtsp.h"

/*
 * calculates response and checksum of a given challenge
 * (RealChallenge1 in rtsp). See implementation for details.
 */
void real_calc_response_and_checksum (char *response, char *chksum, char *challenge);
int real_get_rdt_chunk(rtsp_t *rtsp_session, char *buffer);
rmff_header_t *real_parse_sdp(char *data, char *stream_rules, uint32_t bandwidth);
rmff_header_t *real_setup_and_get_header(rtsp_t *rtsp_session, uint32_t bandwidth);

#endif

