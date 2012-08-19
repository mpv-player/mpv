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

#ifndef MPLAYER_INPUT_H
#define MPLAYER_INPUT_H

#include <stdbool.h>

// All command IDs
enum mp_command_type {
    MP_CMD_SEEK,
    MP_CMD_AUDIO_DELAY,
    MP_CMD_QUIT,
    MP_CMD_PAUSE,
    MP_CMD_GRAB_FRAMES, // deprecated: was a no-op command for years
    MP_CMD_PLAYLIST_NEXT,
    MP_CMD_PLAYLIST_PREV,
    MP_CMD_SUB_DELAY,
    MP_CMD_OSD,
    MP_CMD_VOLUME,
    MP_CMD_MIXER_USEMASTER,
    MP_CMD_CONTRAST,
    MP_CMD_BRIGHTNESS,
    MP_CMD_HUE,
    MP_CMD_SATURATION,
    MP_CMD_FRAMEDROPPING,
    MP_CMD_TV_STEP_CHANNEL,
    MP_CMD_TV_STEP_NORM,
    MP_CMD_TV_STEP_CHANNEL_LIST,
    MP_CMD_VO_FULLSCREEN,
    MP_CMD_SUB_POS,
    MP_CMD_SCREENSHOT,
    MP_CMD_PANSCAN,
    MP_CMD_MUTE,
    MP_CMD_LOADFILE,
    MP_CMD_LOADLIST,
    MP_CMD_PLAYLIST_CLEAR,
    MP_CMD_VF_CHANGE_RECTANGLE,
    MP_CMD_GAMMA,
    MP_CMD_SUB_VISIBILITY,
    MP_CMD_VOBSUB_LANG, // deprecated: combined with SUB_SELECT
    MP_CMD_GET_TIME_LENGTH,
    MP_CMD_GET_PERCENT_POS,
    MP_CMD_SUB_STEP,
    MP_CMD_TV_SET_CHANNEL,
    MP_CMD_EDL_MARK,
    MP_CMD_SUB_ALIGNMENT,
    MP_CMD_TV_LAST_CHANNEL,
    MP_CMD_OSD_SHOW_TEXT,
    MP_CMD_TV_SET_FREQ,
    MP_CMD_TV_SET_NORM,
    MP_CMD_TV_SET_BRIGHTNESS,
    MP_CMD_TV_SET_CONTRAST,
    MP_CMD_TV_SET_HUE,
    MP_CMD_TV_SET_SATURATION,
    MP_CMD_GET_VO_FULLSCREEN,
    MP_CMD_GET_SUB_VISIBILITY,
    MP_CMD_SUB_FORCED_ONLY,
    MP_CMD_VO_ONTOP,
    MP_CMD_SUB_SELECT,
    MP_CMD_VO_ROOTWIN,
    MP_CMD_SWITCH_VSYNC,
    MP_CMD_SWITCH_RATIO,
    MP_CMD_FRAME_STEP,
    MP_CMD_SPEED_INCR,
    MP_CMD_SPEED_MULT,
    MP_CMD_SPEED_SET,
    MP_CMD_RUN,
    MP_CMD_SWITCH_AUDIO,
    MP_CMD_GET_TIME_POS,
    MP_CMD_SUB_LOAD,
    MP_CMD_KEYDOWN_EVENTS,
    MP_CMD_VO_BORDER,
    MP_CMD_SET_PROPERTY,
    MP_CMD_SET_PROPERTY_OSD,
    MP_CMD_GET_PROPERTY,
    MP_CMD_OSD_SHOW_PROPERTY_TEXT,
    MP_CMD_OSD_SHOW_PROGRESSION,
    MP_CMD_SEEK_CHAPTER,
    MP_CMD_GET_FILENAME,
    MP_CMD_GET_VIDEO_CODEC,
    MP_CMD_GET_VIDEO_BITRATE,
    MP_CMD_GET_VIDEO_RESOLUTION,
    MP_CMD_GET_AUDIO_CODEC,
    MP_CMD_GET_AUDIO_BITRATE,
    MP_CMD_GET_AUDIO_SAMPLES,
    MP_CMD_GET_META_TITLE,
    MP_CMD_GET_META_ARTIST,
    MP_CMD_GET_META_ALBUM,
    MP_CMD_GET_META_YEAR,
    MP_CMD_GET_META_COMMENT,
    MP_CMD_GET_META_TRACK,
    MP_CMD_GET_META_GENRE,
    MP_CMD_RADIO_STEP_CHANNEL,
    MP_CMD_RADIO_SET_CHANNEL,
    MP_CMD_RADIO_SET_FREQ,
    MP_CMD_SET_MOUSE_POS,
    MP_CMD_STEP_PROPERTY,
    MP_CMD_STEP_PROPERTY_OSD,
    MP_CMD_RADIO_STEP_FREQ,
    MP_CMD_TV_STEP_FREQ,
    MP_CMD_LOOP,
    MP_CMD_BALANCE,
    MP_CMD_SUB_SCALE,
    MP_CMD_TV_START_SCAN,
    MP_CMD_SWITCH_ANGLE,
    MP_CMD_ASS_USE_MARGINS,
    MP_CMD_SWITCH_TITLE,
    MP_CMD_STOP,

    /// DVB commands
    MP_CMD_DVB_SET_CHANNEL = 5101,

    /// Audio Filter commands
    MP_CMD_AF_SWITCH,
    MP_CMD_AF_ADD,
    MP_CMD_AF_DEL,
    MP_CMD_AF_CLR,
    MP_CMD_AF_CMDLINE,

    MP_CMD_SHOW_CHAPTERS,
    MP_CMD_SHOW_TRACKS,

    /// Video output commands
    MP_CMD_VO_CMDLINE,
};

// The arg types
#define MP_CMD_ARG_INT 1
#define MP_CMD_ARG_FLOAT 2
#define MP_CMD_ARG_STRING 3

#define MP_CMD_MAX_ARGS 10

// Error codes for the drivers

// An error occurred but we can continue
#define MP_INPUT_ERROR -1
// A fatal error occurred, this driver should be removed
#define MP_INPUT_DEAD -2
// No input was available
#define MP_INPUT_NOTHING -3
//! Input will be available if you try again
#define MP_INPUT_RETRY -4
// Key FIFO was full - release events may be lost, zero button-down status
#define MP_INPUT_RELEASE_ALL -5

enum mp_input_section_flags {
    // If a key binding is not defined in the current section, search the
    // default section for it ("default" refers to bindings with no section
    // specified, not to the default input.conf aka builtin key bindings)
    MP_INPUT_NO_DEFAULT_SECTION = 1,
};

struct input_ctx;

struct mp_cmd_arg {
    int type;
    bool optional;
    union {
        int i;
        float f;
        char *s;
    } v;
};

typedef struct mp_cmd {
    int id;
    char *name;
    struct mp_cmd_arg args[MP_CMD_MAX_ARGS];
    int nargs;
    int pausing;
    struct mp_cmd *queue_next;
} mp_cmd_t;


// Executing this command will abort playback (play something else, or quit).
bool mp_input_is_abort_cmd(int cmd_id);

/* Add a new command input source.
 * "fd" is a file descriptor (use a negative value if you don't use any fd)
 * "select" tells whether to use select() on the fd to determine when to
 * try reading.
 * "read_func" is optional. If NULL a default function which reads data
 * directly from the fd will be used. It must return either text data
 * or one of the MP_INPUT error codes above.
 * "close_func" will be called when closing. Can be NULL. Its return value
 * is ignored (it's only there to allow using standard close() as the func).
 */
int mp_input_add_cmd_fd(struct input_ctx *ictx, int fd, int select,
                        int read_func(int fd, char *dest, int size),
                        int close_func(int fd));

// This removes a cmd driver, you usually don't need to use it.
void mp_input_rm_cmd_fd(struct input_ctx *ictx, int fd);

/* The args are similar to the cmd version above, except you must give
 * a read_func, and it should return key codes (ASCII plus keycodes.h).
 */
int mp_input_add_key_fd(struct input_ctx *ictx, int fd, int select,
                        int read_func(void *ctx, int fd),
                        int close_func(int fd), void *ctx);

// Feed a keypress (alternative to being returned from read_func above)
void mp_input_feed_key(struct input_ctx *ictx, int code);

// As for the cmd one you usually don't need this function.
void mp_input_rm_key_fd(struct input_ctx *ictx, int fd);

// Get input key from its name.
int mp_input_get_key_from_name(const char *name);

// Add a command to the command queue.
int mp_input_queue_cmd(struct input_ctx *ictx, struct mp_cmd *cmd);

/* Return next available command, or sleep up to "time" ms if none is
 * available. If "peek_only" is true return a reference to the command
 * but leave it queued.
 */
struct mp_cmd *mp_input_get_cmd(struct input_ctx *ictx, int time,
                                int peek_only);

/* Parse text and return corresponding struct mp_cmd. */
struct mp_cmd *mp_input_parse_cmd(char *str);

// After getting a command from mp_input_get_cmd you need to free it using this
// function
void mp_cmd_free(struct mp_cmd *cmd);

// This creates a copy of a command (used by the auto repeat stuff).
struct mp_cmd *mp_cmd_clone(struct mp_cmd *cmd);

// Set current input section
// flags is a bitfield of enum mp_input_section_flags values
void mp_input_set_section(struct input_ctx *ictx, char *name, int flags);

// Get current input section
char *mp_input_get_section(struct input_ctx *ictx);

// Initialize the input system
struct input_conf;
struct input_ctx *mp_input_init(struct input_conf *input_conf);

void mp_input_uninit(struct input_ctx *ictx);

struct m_config;
void mp_input_register_options(struct m_config *cfg);

// Wake up sleeping input loop from another thread.
void mp_input_wakeup(struct input_ctx *ictx);

// Interruptible usleep:  (used by libmpdemux)
int mp_input_check_interrupt(struct input_ctx *ictx, int time);

extern int async_quit_request;

#endif /* MPLAYER_INPUT_H */
