/*
 * Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>
 *
 * This file is part of libass.
 *
 * libass is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libass is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libass; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef LIBASS_ASS_H
#define LIBASS_ASS_H

#include <stdio.h>
#include <stdarg.h>
#include "ass_types.h"

#define LIBASS_VERSION 0x00908000

/*
 * A linked list of images produced by an ass renderer.
 *
 * These images have to be rendered in-order for the correct screen
 * composition.  The libass renderer clips these bitmaps to the frame size.
 * w/h can be zero, in this case the bitmap should not be rendered at all.
 * The last bitmap row is not guaranteed to be padded up to stride size,
 * e.g. in the worst case a bitmap has the size stride * (h - 1) + w.
 */
typedef struct ass_image {
    int w, h;                   // Bitmap width/height
    int stride;                 // Bitmap stride
    unsigned char *bitmap;      // 1bpp stride*h alpha buffer
                                // Note: the last row may not be padded to
                                // bitmap stride!
    uint32_t color;             // Bitmap color and alpha, RGBA
    int dst_x, dst_y;           // Bitmap placement inside the video frame

    struct ass_image *next;   // Next image, or NULL
} ASS_Image;

/*
 * Hinting type. (see ass_set_hinting below)
 *
 * FreeType's native hinter is still buggy sometimes and it is recommended
 * to use the light autohinter, ASS_HINTING_LIGHT, instead.  For best
 * compatibility with problematic fonts, disable hinting.
 */
typedef enum {
    ASS_HINTING_NONE = 0,
    ASS_HINTING_LIGHT,
    ASS_HINTING_NORMAL,
    ASS_HINTING_NATIVE
} ASS_Hinting;

/**
 * \brief Initialize the library.
 * \return library handle or NULL if failed
 */
ASS_Library *ass_library_init(void);

/**
 * \brief Finalize the library
 * \param priv library handle
 */
void ass_library_done(ASS_Library *priv);

/**
 * \brief Set private font directory.
 * It is used for saving embedded fonts and also in font lookup.
 *
 * \param priv library handle
 * \param fonts_dir private directory for font extraction
 */
void ass_set_fonts_dir(ASS_Library *priv, const char *fonts_dir);

/**
 * \brief Whether fonts should be extracted from track data.
 * \param priv library handle
 * \param extract whether to extract fonts
 */
void ass_set_extract_fonts(ASS_Library *priv, int extract);

/**
 * \brief Register style overrides with a library instance.
 * The overrides should have the form [Style.]Param=Value, e.g.
 *   SomeStyle.Font=Arial
 *   ScaledBorderAndShadow=yes
 *
 * \param priv library handle
 * \param list NULL-terminated list of strings
 */
void ass_set_style_overrides(ASS_Library *priv, char **list);

/**
 * \brief Explicitly process style overrides for a track.
 * \param track track handle
 */
void ass_process_force_style(ASS_Track *track);

/**
 * \brief Register a callback for debug/info messages.
 * If a callback is registered, it is called for every message emitted by
 * libass.  The callback receives a format string and a list of arguments,
 * to be used for the printf family of functions. Additionally, a log level
 * from 0 (FATAL errors) to 7 (verbose DEBUG) is passed.  Usually, level 5
 * should be used by applications.
 * If no callback is set, all messages level < 5 are printed to stderr,
 * prefixed with [ass].
 *
 * \param priv library handle
 * \param msg_cb pointer to callback function
 * \param data additional data, will be passed to callback
 */
void ass_set_message_cb(ASS_Library *priv, void (*msg_cb)
                        (int level, const char *fmt, va_list args, void *data),
                        void *data);

/**
 * \brief Initialize the renderer.
 * \param priv library handle
 * \return renderer handle or NULL if failed
 */
ASS_Renderer *ass_renderer_init(ASS_Library *);

/**
 * \brief Finalize the renderer.
 * \param priv renderer handle
 */
void ass_renderer_done(ASS_Renderer *priv);

/**
 * \brief Set the frame size in pixels, including margins.
 * \param priv renderer handle
 * \param w width
 * \param h height
 */
void ass_set_frame_size(ASS_Renderer *priv, int w, int h);

/**
 * \brief Set frame margins.  These values may be negative if pan-and-scan
 * is used.
 * \param priv renderer handle
 * \param t top margin
 * \param b bottom margin
 * \param l left margin
 * \param r right margin
 */
void ass_set_margins(ASS_Renderer *priv, int t, int b, int l, int r);

/**
 * \brief Whether margins should be used for placing regular events.
 * \param priv renderer handle
 * \param use whether to use the margins
 */
void ass_set_use_margins(ASS_Renderer *priv, int use);

/**
 * \brief Set aspect ratio parameters.
 * \param priv renderer handle
 * \param dar display aspect ratio (DAR), prescaled for output PAR
 * \param sar storage aspect ratio (SAR)
 */
void ass_set_aspect_ratio(ASS_Renderer *priv, double dar, double sar);

/**
 * \brief Set a fixed font scaling factor.
 * \param priv renderer handle
 * \param font_scale scaling factor, default is 1.0
 */
void ass_set_font_scale(ASS_Renderer *priv, double font_scale);

/**
 * \brief Set font hinting method.
 * \param priv renderer handle
 * \param ht hinting method
 */
void ass_set_hinting(ASS_Renderer *priv, ASS_Hinting ht);

/**
 * \brief Set line spacing. Will not be scaled with frame size.
 * \param priv renderer handle
 * \param line_spacing line spacing in pixels
 */
void ass_set_line_spacing(ASS_Renderer *priv, double line_spacing);

/**
 * \brief Set font lookup defaults.
 * \param default_font path to default font to use. Must be supplied if
 * fontconfig is disabled or unavailable.
 * \param default_family fallback font family for fontconfig, or NULL
 * \param fc whether to use fontconfig
 * \param config path to fontconfig configuration file, or NULL.  Only relevant
 * if fontconfig is used.
 * \param update whether fontconfig cache should be built/updated now.  Only
 * relevant if fontconfig is used.
 */
void ass_set_fonts(ASS_Renderer *priv, const char *default_font,
                   const char *default_family, int fc, const char *config,
                   int update);

/**
 * \brief Update/build font cache.  This needs to be called if it was
 * disabled when ass_set_fonts was set.
 *
 * \param priv renderer handle
 * \return success
 */
int ass_fonts_update(ASS_Renderer *priv);

/**
 * \brief Set hard cache limits.  Do not set, or set to zero, for reasonable
 * defaults.
 *
 * \param priv renderer handle
 * \param glyph_max maximum number of cached glyphs
 * \param bitmap_max_size maximum bitmap cache size (in MB)
 */
void ass_set_cache_limits(ASS_Renderer *priv, int glyph_max,
                          int bitmap_max_size);

/**
 * \brief Render a frame, producing a list of ASS_Image.
 * \param priv renderer handle
 * \param track subtitle track
 * \param now video timestamp in milliseconds
 * \param detect_change will be set to 1 if a change occured compared
 * to the last invocation
 */
ASS_Image *ass_render_frame(ASS_Renderer *priv, ASS_Track *track,
                            long long now, int *detect_change);


/*
 * The following functions operate on track objects and do not need
 * an ass_renderer
 */

/**
 * \brief Allocate a new empty track object.
 * \param library handle
 * \return pointer to empty track
 */
ASS_Track *ass_new_track(ASS_Library *);

/**
 * \brief Deallocate track and all its child objects (styles and events).
 * \param track track to deallocate
 */
void ass_free_track(ASS_Track *track);

/**
 * \brief Allocate new style.
 * \param track track
 * \return newly allocated style id
 */
int ass_alloc_style(ASS_Track *track);

/**
 * \brief Allocate new event.
 * \param track track
 * \return newly allocated event id
 */
int ass_alloc_event(ASS_Track *track);

/**
 * \brief Delete a style.
 * \param track track
 * \param sid style id
 * Deallocates style data. Does not modify track->n_styles.
 */
void ass_free_style(ASS_Track *track, int sid);

/**
 * \brief Delete an event.
 * \param track track
 * \param eid event id
 * Deallocates event data. Does not modify track->n_events.
 */
void ass_free_event(ASS_Track *track, int eid);

/**
 * \brief Parse a chunk of subtitle stream data.
 * \param track track
 * \param data string to parse
 * \param size length of data
 */
void ass_process_data(ASS_Track *track, char *data, int size);

/**
 * \brief Parse Codec Private section of subtitle stream.
 * \param track target track
 * \param data string to parse
 * \param size length of data
 */
void ass_process_codec_private(ASS_Track *track, char *data, int size);

/**
 * \brief Parse a chunk of subtitle stream data. In Matroska,
 * this contains exactly 1 event (or a commentary).
 * \param track track
 * \param data string to parse
 * \param size length of data
 * \param timecode starting time of the event (milliseconds)
 * \param duration duration of the event (milliseconds)
 */
void ass_process_chunk(ASS_Track *track, char *data, int size,
                       long long timecode, long long duration);

/**
 * \brief Read subtitles from file.
 * \param library library handle
 * \param fname file name
 * \param codepage encoding (iconv format)
 * \return newly allocated track
*/
ASS_Track *ass_read_file(ASS_Library *library, char *fname,
                         char *codepage);

/**
 * \brief Read subtitles from memory.
 * \param library library handle
 * \param buf pointer to subtitles text
 * \param bufsize size of buffer
 * \param codepage encoding (iconv format)
 * \return newly allocated track
*/
ASS_Track *ass_read_memory(ASS_Library *library, char *buf,
                           size_t bufsize, char *codepage);
/**
 * \brief Read styles from file into already initialized track.
 * \param fname file name
 * \param codepage encoding (iconv format)
 * \return 0 on success
 */
int ass_read_styles(ASS_Track *track, char *fname, char *codepage);

/**
 * \brief Add a memory font.
 * \param library library handle
 * \param name attachment name
 * \param data binary font data
 * \param data_size data size
*/
void ass_add_font(ASS_Library *library, char *name, char *data,
                  int data_size);

/**
 * \brief Remove all fonts stored in an ass_library object.
 * \param library library handle
 */
void ass_clear_fonts(ASS_Library *library);

/**
 * \brief Calculates timeshift from now to the start of some other subtitle
 * event, depending on movement parameter.
 * \param track subtitle track
 * \param now current time in milliseconds
 * \param movement how many events to skip from the one currently displayed
 * +2 means "the one after the next", -1 means "previous"
 * \return timeshift in milliseconds
 */
long long ass_step_sub(ASS_Track *track, long long now, int movement);

#endif /* LIBASS_ASS_H */
