#ifndef __ASS_H__
#define __ASS_H__

#include "ass_types.h"

/// Libass "library object". Contents are private.
typedef struct ass_instance_s ass_instance_t;

/// used in ass_configure
typedef struct ass_settings_s {
	int frame_width;
	int frame_height;
	double font_size_coeff; // font size multiplier
	double line_spacing; // additional line spacing (in frame pixels)
	int top_margin; // height of top margin. Everything except toptitles is shifted down by top_margin.
	int bottom_margin; // height of bottom margin. (frame_height - top_margin - bottom_margin) is original video height.
	double aspect; // frame aspect ratio, d_width / d_height.
} ass_settings_t;

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
ass_instance_t* ass_init(void);

/**
 * \brief finalize the library
 * \param priv library handle
 */
void ass_done(ass_instance_t* priv);

/**
 * \brief configure the library
 * \param priv library handle
 * \param config struct with configuration parameters. Caller is free to reuse it after this function returns.
 */
void ass_configure(ass_instance_t* priv, const ass_settings_t* config);

/**
 * \brief start rendering a frame
 * \param priv library
 * \param track subtitle track
 * \param now video timestamp in milliseconds
 */
int ass_start_frame(ass_instance_t *priv, ass_track_t* track, long long now);

/**
 * \brief render a single event
 * uses library, track and timestamp from the previous call to ass_start_frame
 */
int ass_render_event(ass_event_t* event);

/**
 * \brief done rendering frame, give out the results
 * \return a list of images for blending
 */
ass_image_t* ass_end_frame(void); // returns linked list of images to render

/**
 * \brief render a frame, producing a list of ass_image_t
 * \param priv library
 * \param track subtitle track
 * \param now video timestamp in milliseconds
 * This function is equivalent to 
 *   ass_start_frame()
 *   for events: start <= now < end:
 *     ass_render_event()
 *   ass_end_frame()
 */
ass_image_t* ass_render_frame(ass_instance_t *priv, ass_track_t* track, long long now);


// The following functions operate on track objects and do not need an ass_instance //

/**
 * \brief allocate a new empty track object
 * \return pointer to empty track
 */
ass_track_t* ass_new_track(void);

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
 * \brief Process Codec Private section of subtitle stream
 * \param track target track
 * \param data string to parse
 * \param size length of data
 */
void ass_process_chunk(ass_track_t* track, char *data, int size);

/**
 * \brief Process a chunk of subtitle stream data. In matroska, this containes exactly 1 event (or a commentary)
 * \param track track
 * \param data string to parse
 * \param size length of data
 * \param timecode starting time of the event (milliseconds)
 * \param duration duration of the event (milliseconds)
*/
void ass_process_line(ass_track_t* track, char *data, int size, long long timecode, long long duration);

/**
 * \brief Read subtitles from file.
 * \param fname file name
 * \return newly allocated track
*/
ass_track_t* ass_read_file(char* fname);

/**
 * \brief Process embedded matroska font. Saves it to ~/.mplayer/fonts.
 * \param name attachment name
 * \param data binary font data
 * \param data_size data size
*/
void ass_process_font(const char* name, char* data, int data_size);

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

