// -*- c-basic-offset: 8; indent-tabs-mode: t -*-
// vim:ts=8:sw=8:noet:ai:
/*
  Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef __ASS_H__
#define __ASS_H__

#include "ass_types.h"

/// Libass renderer object. Contents are private.
typedef struct ass_renderer_s ass_renderer_t;

/// a linked list of images produced by ass renderer
typedef struct ass_image_s {
	int w, h; // bitmap width/height
	int stride; // bitmap stride
	unsigned char* bitmap; // 1bpp stride*h alpha buffer
	uint32_t color; // RGBA
	int dst_x, dst_y; // bitmap placement inside the video frame

	struct ass_image_s* next; // linked list
} ass_image_t;

/**
 * \brief initialize the library
 * \return library handle or NULL if failed
 */
ass_library_t* ass_library_init(void);

/**
 * \brief finalize the library
 * \param priv library handle
 */
void ass_library_done(ass_library_t*);

/**
 * \brief set private font directory
 * It is used for saving embedded fonts and also in font lookup.
 */
void ass_set_fonts_dir(ass_library_t* priv, const char* fonts_dir);

void ass_set_extract_fonts(ass_library_t* priv, int extract);

void ass_set_style_overrides(ass_library_t* priv, char** list);

/**
 * \brief initialize the renderer
 * \param priv library handle
 * \return renderer handle or NULL if failed
 */
ass_renderer_t* ass_renderer_init(ass_library_t*);

/**
 * \brief finalize the renderer
 * \param priv renderer handle
 */
void ass_renderer_done(ass_renderer_t* priv);

void ass_set_frame_size(ass_renderer_t* priv, int w, int h);
void ass_set_margins(ass_renderer_t* priv, int t, int b, int l, int r);
void ass_set_use_margins(ass_renderer_t* priv, int use);
void ass_set_aspect_ratio(ass_renderer_t* priv, double ar);
void ass_set_font_scale(ass_renderer_t* priv, double font_scale);

/**
 * \brief set font lookup defaults
 */
int  ass_set_fonts(ass_renderer_t* priv, const char* default_font, const char* default_family);

/**
 * \brief render a frame, producing a list of ass_image_t
 * \param priv library
 * \param track subtitle track
 * \param now video timestamp in milliseconds
 */
ass_image_t* ass_render_frame(ass_renderer_t *priv, ass_track_t* track, long long now, int* detect_change);


// The following functions operate on track objects and do not need an ass_renderer //

/**
 * \brief allocate a new empty track object
 * \return pointer to empty track
 */
ass_track_t* ass_new_track(ass_library_t*);

/**
 * \brief deallocate track and all its child objects (styles and events)
 * \param track track to deallocate
 */
void ass_free_track(ass_track_t* track);

/**
 * \brief allocate new style
 * \param track track
 * \return newly allocated style id
 */
int ass_alloc_style(ass_track_t* track);

/**
 * \brief allocate new event
 * \param track track
 * \return newly allocated event id
 */
int ass_alloc_event(ass_track_t* track);

/**
 * \brief delete a style
 * \param track track
 * \param sid style id
 * Deallocates style data. Does not modify track->n_styles.
 */
void ass_free_style(ass_track_t* track, int sid);

/**
 * \brief delete an event
 * \param track track
 * \param eid event id
 * Deallocates event data. Does not modify track->n_events.
 */
void ass_free_event(ass_track_t* track, int eid);

/**
 * \brief Process Codec Private section of subtitle stream
 * \param track target track
 * \param data string to parse
 * \param size length of data
 */
void ass_process_codec_private(ass_track_t* track, char *data, int size);

/**
 * \brief Process a chunk of subtitle stream data. In matroska, this containes exactly 1 event (or a commentary)
 * \param track track
 * \param data string to parse
 * \param size length of data
 * \param timecode starting time of the event (milliseconds)
 * \param duration duration of the event (milliseconds)
*/
void ass_process_chunk(ass_track_t* track, char *data, int size, long long timecode, long long duration);

/**
 * \brief Read subtitles from file.
 * \param fname file name
 * \return newly allocated track
*/
ass_track_t* ass_read_file(ass_library_t* library, char* fname, char* codepage);

/**
 * \brief Read subtitles from memory.
 * \param library libass library object
 * \param buf pointer to subtitles text
 * \param bufsize size of buffer
 * \param codepage recode buffer contents from given codepage
 * \return newly allocated track
*/ 
ass_track_t* ass_read_memory(ass_library_t* library, char* buf, size_t bufsize, char* codepage);
/**
 * \brief read styles from file into already initialized track
 * \return 0 on success
 */
int ass_read_styles(ass_track_t* track, char* fname, char* codepage);

/**
 * \brief Add a memory font.
 * \param name attachment name
 * \param data binary font data
 * \param data_size data size
*/
void ass_add_font(ass_library_t* library, char* name, char* data, int data_size);

/**
 * \brief Calculates timeshift from now to the start of some other subtitle event, depending on movement parameter
 * \param track subtitle track
 * \param now current time, ms
 * \param movement how many events to skip from the one currently displayed
 * +2 means "the one after the next", -1 means "previous"
 * \return timeshift, ms
 */
long long ass_step_sub(ass_track_t* track, long long now, int movement);

#endif

