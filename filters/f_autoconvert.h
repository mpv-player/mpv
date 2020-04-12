#pragma once

#include "filter.h"
#include "video/sws_utils.h"

struct mp_image;
struct mp_image_params;

// A filter which automatically creates and uses a conversion filter based on
// the filter settings, or passes through data unchanged if no conversion is
// required.
struct mp_autoconvert {
    // f->pins[0] is input, f->pins[1] is output
    struct mp_filter *f;

    enum mp_sws_scaler force_scaler;

    // If this is set, the callback is invoked (from the process function), and
    // further data flow is blocked until mp_autoconvert_format_change_continue()
    // is called. The idea is that you can reselect the output parameters on
    // format changes and continue filtering when ready.
    void (*on_audio_format_change)(void *opaque);
    void *on_audio_format_change_opaque;
};

// (to free this, free the filter itself, mp_autoconvert.f)
struct mp_autoconvert *mp_autoconvert_create(struct mp_filter *parent);

// Require that output frames have the following params set.
// This implicitly clears the image format list, and calls
// mp_autoconvert_add_imgfmt() with the values in *p.
// Idempotent on subsequent calls (no reinit forced if parameters don't change).
// Mixing this with other format-altering calls has undefined effects.
void mp_autoconvert_set_target_image_params(struct mp_autoconvert *c,
                                            struct mp_image_params *p);

// Add the imgfmt as allowed video image format, and error on non-video frames.
// Each call adds to the list of allowed formats. Before the first call, all
// formats are allowed (even non-video).
// subfmt can be used to specify underlying surface formats for hardware formats,
// otherwise must be 0. (Mismatches lead to conversion errors.)
void mp_autoconvert_add_imgfmt(struct mp_autoconvert *c, int imgfmt, int subfmt);

// Add all sw image formats. The effect is that hardware video image formats are
// disallowed. The semantics are the same as calling mp_autoconvert_add_imgfmt()
// for each sw format that exists.
// No need to do this if you add sw formats with mp_autoconvert_add_imgfmt(),
// as the normal semantics will exclude other formats (including hw ones).
void mp_autoconvert_add_all_sw_imgfmts(struct mp_autoconvert *c);

// Approximate test for whether the input would be accepted for conversion
// according to the current settings. If false is returned, conversion will
// definitely fail; if true is returned, it might succeed, but with no hard
// guarantee. This is mainly intended for better error reporting to the user.
// The result is "approximate" because it could still fail at runtime.
// The mp_image is not mutated.
// This function is relatively slow.
// Accepting mp_image instead of any mp_frame is the result of laziness.
bool mp_autoconvert_probe_input_video(struct mp_autoconvert *c,
                                      struct mp_image *img);

// This is pointless.
struct mp_hwdec_devices;
void mp_autoconvert_add_vo_hwdec_subfmts(struct mp_autoconvert *c,
                                         struct mp_hwdec_devices *devs);

// Add afmt (an AF_FORMAT_* value) as allowed audio format.
// See mp_autoconvert_add_imgfmt() for other remarks.
void mp_autoconvert_add_afmt(struct mp_autoconvert *c, int afmt);

// Add allowed audio channel configuration.
struct mp_chmap;
void mp_autoconvert_add_chmap(struct mp_autoconvert *c, struct mp_chmap *chmap);

// Add allowed audio sample rate.
void mp_autoconvert_add_srate(struct mp_autoconvert *c, int rate);

// Reset set of allowed formats back to initial state. (This does not flush
// any frames or remove currently active filters, although to get reasonable
// behavior, you need to readd all previously allowed formats, or reset the
// filter.)
void mp_autoconvert_clear(struct mp_autoconvert *c);

// See mp_autoconvert.on_audio_format_change.
void mp_autoconvert_format_change_continue(struct mp_autoconvert *c);
