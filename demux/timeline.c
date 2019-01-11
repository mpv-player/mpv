#include "common/common.h"
#include "stream/stream.h"
#include "demux.h"

#include "timeline.h"

struct timeline *timeline_load(struct mpv_global *global, struct mp_log *log,
                               struct demuxer *demuxer)
{
    if (!demuxer->desc->load_timeline)
        return NULL;

    struct timeline *tl = talloc_ptrtype(NULL, tl);
    *tl = (struct timeline){
        .global = global,
        .log = log,
        .cancel = demuxer->cancel,
        .demuxer = demuxer,
        .format = "unknown",
    };

    demuxer->desc->load_timeline(tl);

    if (tl->num_pars)
        return tl;
    timeline_destroy(tl);
    return NULL;
}

void timeline_destroy(struct timeline *tl)
{
    if (!tl)
        return;
    for (int n = 0; n < tl->num_sources; n++) {
        struct demuxer *d = tl->sources[n];
        if (d != tl->demuxer)
            demux_free(d);
    }
    talloc_free(tl);
}
