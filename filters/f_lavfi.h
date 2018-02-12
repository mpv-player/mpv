#pragma once

#include "frame.h"

// A wrapped libavfilter filter or filter graph.
// (to free this, free the filter itself, mp_lavfi.f)
struct mp_lavfi {
    // This mirrors the libavfilter pads according to the user specification.
    struct mp_filter *f;
};

// Create a filter with the given libavfilter graph string. The graph must
// have labels on all unconnected pads; these are exposed as pins.
//  type: if not 0, require all pads to have a compatible media type (or error)
//  bidir: if true, require exactly 2 pads, 1 input, 1 output (mp_lavfi.f will
//         have the input as pin 0, and the output as pin 1)
//  graph_opts: options for the filter graph, see mp_set_avopts() (NULL is OK)
//  graph: a libavfilter graph specification
struct mp_lavfi *mp_lavfi_create_graph(struct mp_filter *parent,
                                       enum mp_frame_type type, bool bidir,
                                       char **graph_opts, const char *graph);

// Unlike mp_lavfi_create_graph(), this creates a single filter, using explicit
// options, and without involving the libavfilter graph parser. Instead of
// a graph, it takes a filter name, and a key-value list of filter options
// (which are applied with mp_set_avopts()).
struct mp_lavfi *mp_lavfi_create_filter(struct mp_filter *parent,
                                        enum mp_frame_type type, bool bidir,
                                        char **graph_opts,
                                        const char *filter, char **filter_opts);

// Print libavfilter list for --vf/--af
void print_lavfi_help_list(struct mp_log *log, int media_type);

// Print libavfilter help for the given filter
void print_lavfi_help(struct mp_log *log, const char *name, int media_type);
