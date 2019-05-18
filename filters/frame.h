#pragma once

#include <stdbool.h>

enum mp_frame_type {
    MP_FRAME_NONE = 0,  // NULL, placeholder, no frame available (_not_ EOF)
    MP_FRAME_VIDEO,     // struct mp_image*
    MP_FRAME_AUDIO,     // struct mp_aframe*
    MP_FRAME_PACKET,    // struct demux_packet*
    MP_FRAME_EOF,       // NULL, signals end of stream (but frames after it can
                        // resume filtering!)
};

const char *mp_frame_type_str(enum mp_frame_type t);

// Generic container for a piece of data, such as a video frame, or a collection
// of audio samples. Wraps an actual media-specific frame data types in a
// generic way. Also can be an empty frame for signaling (MP_FRAME_EOF and
// possibly others).
// This struct is usually allocated on the stack and can be copied by value.
// You need to consider that the underlying pointer is ref-counted, and that
// the _unref/_ref functions must be used accordingly.
struct mp_frame {
    enum mp_frame_type type;
    void *data;
};

// Return whether the frame contains actual data (audio, video, ...). If false,
// it's either signaling, or MP_FRAME_NONE.
bool mp_frame_is_data(struct mp_frame frame);

// Return whether the frame is for signaling (data flow commands like
// MP_FRAME_EOF). If false, it's either data (mp_frame_is_data()), or
// MP_FRAME_NONE.
bool mp_frame_is_signaling(struct mp_frame frame);

// Unreferences any frame data, and sets *frame to MP_FRAME_NONE. (It does
// _not_ deallocate the memory block the parameter points to, only frame->data.)
void mp_frame_unref(struct mp_frame *frame);

// Return a new reference to the given frame. The caller owns the returned
// frame. On failure returns a MP_FRAME_NONE.
struct mp_frame mp_frame_ref(struct mp_frame frame);

double mp_frame_get_pts(struct mp_frame frame);
void mp_frame_set_pts(struct mp_frame frame, double pts);

// Estimation of total size in bytes. This is for buffering purposes.
int mp_frame_approx_size(struct mp_frame frame);

struct AVFrame;
struct AVRational;
struct AVFrame *mp_frame_to_av(struct mp_frame frame, struct AVRational *tb);
struct mp_frame mp_frame_from_av(enum mp_frame_type type, struct AVFrame *frame,
                                 struct AVRational *tb);

#define MAKE_FRAME(type, frame) ((struct mp_frame){(type), (frame)})
#define MP_NO_FRAME MAKE_FRAME(0, 0)
#define MP_EOF_FRAME MAKE_FRAME(MP_FRAME_EOF, 0)
