#pragma once

#include "filter.h"

// A filter which automatically creates and uses a conversion filter based on
// the filter settings, or passes through data unchanged if no conversion is
// required.
struct mp_autoconvert {
    // f->pins[0] is input, f->pins[1] is output
    struct mp_filter *f;

    // If this is set, the callback is invoked (from the process function), and
    // further data flow is blocked until mp_autoconvert_format_change_continue()
    // is called. The idea is that you can reselect the output parameters on
    // format changes and continue filtering when ready.
    void (*on_audio_format_change)(void *opaque);
    void *on_audio_format_change_opaque;
};

// (to free this, free the filter itself, mp_autoconvert.f)
struct mp_autoconvert *mp_autoconvert_create(struct mp_filter *parent);

// Add the imgfmt as allowed video image format, and error on non-video frames.
// Each call adds to the list of allowed formats. Before the first call, all
// formats are allowed (even non-video).
// subfmt can be used to specify underlying surface formats for hardware formats,
// otherwise must be 0.
void mp_autoconvert_add_imgfmt(struct mp_autoconvert *c, int imgfmt, int subfmt);

// Add the formats supported by the hwdec interops (or essentially refine them),
// and trigger conversion if hw_subfmts mismatch. This is mostly a hack for
// D3D11/ANGLE (which supports NV12 only).
// Must be called mp_autoconvert_add_imgfmt(), and overrides them where formats
// collide.
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

// vf_d3d11vpp.c
struct mp_filter *vf_d3d11_create_outconv(struct mp_filter *parent);
