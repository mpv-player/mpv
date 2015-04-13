#ifndef MP_TIMELINE_H_
#define MP_TIMELINE_H_

struct timeline_part {
    double start;
    double source_start;
    struct demuxer *source;
};

struct timeline {
    struct mpv_global *global;
    struct mp_log *log;
    struct mp_cancel *cancel;

    // main source
    struct demuxer *demuxer;

    // All referenced files. The source file must be at sources[0].
    struct demuxer **sources;
    int num_sources;

    // Segments to play, ordered by time. parts[num_parts] must be valid; its
    // start field sets the duration, and source must be NULL.
    struct timeline_part *parts;
    int num_parts;

    struct demux_chapter *chapters;
    int num_chapters;

    // Which source defines the overall track list (over the full timeline).
    struct demuxer *track_layout;
};

struct timeline *timeline_load(struct mpv_global *global, struct mp_log *log,
                               struct demuxer *demuxer);
void timeline_destroy(struct timeline *tl);

#endif
