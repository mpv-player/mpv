/*
 *  Copyright (C) 2006 Benjamin Zores
 *   Set of helper routines for stream metadata and properties retrieval.
 *
 *   This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef _METADATA_H_
#define _METADATA_H_

typedef enum metadata_s metadata_t;
enum metadata_s {
  /* common info */
  META_NAME = 0,

  /* video stream properties */
  META_VIDEO_CODEC,
  META_VIDEO_BITRATE,
  META_VIDEO_RESOLUTION,

  /* audio stream properties */
  META_AUDIO_CODEC,
  META_AUDIO_BITRATE,
  META_AUDIO_SAMPLES,

  /* ID3 tags and other stream infos */
  META_INFO_TITLE,
  META_INFO_ARTIST,
  META_INFO_ALBUM,
  META_INFO_YEAR,
  META_INFO_COMMENT,
  META_INFO_TRACK,
  META_INFO_GENRE
};

char *get_metadata (metadata_t type);

#endif /* _METADATA_H_ */

