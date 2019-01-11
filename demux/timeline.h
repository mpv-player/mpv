#ifndef MP_TIMELINE_H_
#define MP_TIMELINE_H_

// Single segment in a timeline.
struct timeline_part {
    // (end time must match with start time of the next part)
    double start, end;
    double source_start;
    char *url;
    struct demuxer *source;
};

// Timeline formed by a single demuxer. Multiple pars are used to get tracks
// that require a separate opened demuxer, such as separate audio tracks. (For
// example, for ordered chapters there is only a single par, because all streams
// demux from the same file at a given time, while for DASH-style video+audio,
// each track would have its own timeline.)
// Note that demuxer instances must not be shared across timeline_pars. This
// would conflict in demux_timeline.c.
// "par" is short for parallel stream.
struct timeline_par {
    bstr init_fragment;
    bool dash, no_clip;

    // Segments to play, ordered by time.
    struct timeline_part *parts;
    int num_parts;

    // Which source defines the overall track list (over the full timeline).
    struct demuxer *track_layout;
};

struct timeline {
    struct mpv_global *global;
    struct mp_log *log;
    struct mp_cancel *cancel;

    const char *format;

    // main source, and all other sources (this usually only has special meaning
    // for memory management; mostly compensates for the lack of refcounting)
    struct demuxer *demuxer;
    struct demuxer **sources;
    int num_sources;

    // Description of timeline ranges, possibly multiple parallel ones.
    struct timeline_par **pars;
    int num_pars;

    struct demux_chapter *chapters;
    int num_chapters;

    // global tags, attachments, editions
    struct demuxer *meta;
};

struct timeline *timeline_load(struct mpv_global *global, struct mp_log *log,
                               struct demuxer *demuxer);
void timeline_destroy(struct timeline *tl);

#endif
