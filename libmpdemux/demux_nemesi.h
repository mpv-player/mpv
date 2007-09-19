/*
 *  Copyright (C) 2007 Alessandro Molina <amol.wrk@gmail.com>
 *   based on previous Live555 RTP support.
 *
 *  MPlayer is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  MPlayer is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with MPlayer; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


#ifndef DEMUX_NEMESI_H
#define DEMUX_NEMESI_H

#include <stdlib.h>
#include <stdio.h>
#include "stream/stream.h"
#include "demuxer.h"


/** 
 * Open the RTP demuxer
 */
demuxer_t* demux_open_rtp(demuxer_t* demuxer);

/**
 * Read from the RTP demuxer
 */
int demux_rtp_fill_buffer(demuxer_t *demux, demux_stream_t* ds);

/**
 * Close the RTP demuxer
 */
void demux_close_rtp(demuxer_t* demuxer);

#endif /* DEMUX_NEMESI_H */

