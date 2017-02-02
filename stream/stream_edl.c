// Dummy stream implementation to enable demux_edl, which is in turn a
// dummy demuxer implementation to enable tl_edl.

#include "stream.h"

static int s_open (struct stream *stream)
{
    stream->demuxer = "edl";
    stream->allow_caching = false;

    return STREAM_OK;
}

const stream_info_t stream_info_edl = {
    .name = "edl",
    .open = s_open,
    .protocols = (const char*const[]){"edl", NULL},
};
