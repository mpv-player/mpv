#pragma once

#include "options/m_option.h"
#include "video/mp_image.h"

#include "filter.h"

enum mp_output_chain_type {
    MP_OUTPUT_CHAIN_VIDEO = 1,      // --vf
};

// A classic single-media filter chain, which reflects --vf and --af.
// It manages the user-specified filter chain, and VO/AO output conversions.
// Also handles some automatic filtering (auto rotation and such).
struct mp_output_chain {
    // This filter will have 1 input (from decoder) and 1 output (to VO/AO).
    struct mp_filter *f;

    bool got_input_eof;
    bool got_output_eof;

    // The filter chain output could not be converted to any format the output
    // supports.
    bool failed_output_conversion;

    // Set if any formats in the chain changed. The user can reset the flag.
    // For implementing change notifications out input/output_params.
    bool reconfig_happened;

    // --- for type==MP_OUTPUT_CHAIN_VIDEO
    struct mp_image_params input_params;
    struct mp_image_params output_params;
    double container_fps;
};

// (free by freeing mp_output_chain.f)
struct mp_output_chain *mp_output_chain_create(struct mp_filter *parent,
                                               enum mp_output_chain_type type);

// Set the VO, which will be used to determine basic capabilities like format
// and rotation support, and to init hardware filtering things.
// For type==MP_OUTPUT_CHAIN_VIDEO only.
struct vo;
void mp_output_chain_set_vo(struct mp_output_chain *p, struct vo *vo);

// Send a command to the filter with the target label.
bool mp_output_chain_command(struct mp_output_chain *p, const char *target,
                             struct mp_filter_command *cmd);

// Perform a seek reset _and_ reset all filter failure states, so that future
// filtering continues normally.
void mp_output_chain_reset_harder(struct mp_output_chain *p);

// Try to exchange the filter list. If creation of any filter fails, roll
// back the changes, and return false.
struct m_obj_settings;
bool mp_output_chain_update_filters(struct mp_output_chain *p,
                                    struct m_obj_settings *list);

