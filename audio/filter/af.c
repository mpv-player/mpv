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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common/common.h"
#include "common/global.h"

#include "options/m_option.h"
#include "options/m_config.h"

#include "af.h"

// Static list of filters
extern struct af_info af_info_dummy;
extern struct af_info af_info_delay;
extern struct af_info af_info_channels;
extern struct af_info af_info_format;
extern struct af_info af_info_force;
extern struct af_info af_info_volume;
extern struct af_info af_info_equalizer;
extern struct af_info af_info_pan;
extern struct af_info af_info_surround;
extern struct af_info af_info_sub;
extern struct af_info af_info_export;
extern struct af_info af_info_drc;
extern struct af_info af_info_extrastereo;
extern struct af_info af_info_lavcac3enc;
extern struct af_info af_info_lavrresample;
extern struct af_info af_info_sweep;
extern struct af_info af_info_hrtf;
extern struct af_info af_info_ladspa;
extern struct af_info af_info_center;
extern struct af_info af_info_sinesuppress;
extern struct af_info af_info_karaoke;
extern struct af_info af_info_scaletempo;
extern struct af_info af_info_bs2b;
extern struct af_info af_info_lavfi;
extern struct af_info af_info_convert24;
extern struct af_info af_info_convertsignendian;

static struct af_info* filter_list[] = {
    &af_info_dummy,
    &af_info_delay,
    &af_info_channels,
    &af_info_format,
    &af_info_volume,
    &af_info_equalizer,
    &af_info_pan,
    &af_info_surround,
    &af_info_sub,
#if HAVE_SYS_MMAN_H
    &af_info_export,
#endif
    &af_info_drc,
    &af_info_extrastereo,
    &af_info_lavcac3enc,
    &af_info_lavrresample,
    &af_info_sweep,
    &af_info_hrtf,
#if HAVE_LADSPA
    &af_info_ladspa,
#endif
    &af_info_center,
    &af_info_sinesuppress,
    &af_info_karaoke,
    &af_info_scaletempo,
#if HAVE_LIBBS2B
    &af_info_bs2b,
#endif
#if HAVE_LIBAVFILTER
    &af_info_lavfi,
#endif
    // Must come last, because they're fallback format conversion filter
    &af_info_convert24,
    &af_info_convertsignendian,
    NULL
};

static bool get_desc(struct m_obj_desc *dst, int index)
{
    if (index >= MP_ARRAY_SIZE(filter_list) - 1)
        return false;
    const struct af_info *af = filter_list[index];
    *dst = (struct m_obj_desc) {
        .name = af->name,
        .description = af->info,
        .priv_size = af->priv_size,
        .priv_defaults = af->priv_defaults,
        .options = af->options,
        .p = af,
    };
    return true;
}

const struct m_obj_list af_obj_list = {
    .get_desc = get_desc,
    .description = "audio filters",
    .aliases = {
        {"force",     "format"},
        {0}
    },
};

static bool af_config_equals(struct mp_audio *a, struct mp_audio *b)
{
    return a->format == b->format
        && mp_chmap_equals(&a->channels, &b->channels)
        && a->rate   == b->rate;
}

static void af_copy_unset_fields(struct mp_audio *dst, struct mp_audio *src)
{
    if (dst->format == AF_FORMAT_UNKNOWN)
        mp_audio_set_format(dst, src->format);
    if (dst->nch == 0)
        mp_audio_set_channels(dst, &src->channels);
    if (dst->rate == 0)
        dst->rate = src->rate;
}

static int input_control(struct af_instance* af, int cmd, void* arg)
{
    switch (cmd) {
    case AF_CONTROL_REINIT:
        assert(arg == &((struct af_stream *)af->priv)->input);
        return AF_OK;
    }
    return AF_UNKNOWN;
}

static int output_control(struct af_instance* af, int cmd, void* arg)
{
    struct af_stream *s = af->priv;
    struct mp_audio *output = &s->output;
    struct mp_audio *filter_output = &s->filter_output;

    switch (cmd) {
    case AF_CONTROL_REINIT: {
        struct mp_audio *in = arg;
        struct mp_audio orig_in = *in;

        *filter_output = *output;
        af_copy_unset_fields(filter_output, in);
        *in = *filter_output;
        return af_config_equals(in, &orig_in) ? AF_OK : AF_FALSE;
    }
    }
    return AF_UNKNOWN;
}

static int dummy_filter(struct af_instance* af, struct mp_audio* data, int f)
{
    return 0;
}

/* Function for creating a new filter of type name.The name may
contain the commandline parameters for the filter */
static struct af_instance *af_create(struct af_stream *s, char *name,
                                     char **args)
{
    struct m_obj_desc desc;
    if (!m_obj_list_find(&desc, &af_obj_list, bstr0(name))) {
        MP_ERR(s, "Couldn't find audio filter '%s'.\n", name);
        return NULL;
    }
    const struct af_info *info = desc.p;
    /* Make sure that the filter is not already in the list if it is
       non-reentrant */
    if (info->flags & AF_FLAGS_NOT_REENTRANT) {
        for (struct af_instance *cur = s->first; cur; cur = cur->next) {
            if (cur->info == info) {
                MP_ERR(s, "There can only be one "
                       "instance of the filter '%s' in each stream\n", name);
                return NULL;
            }
        }
    }

    MP_VERBOSE(s, "Adding filter %s \n", name);

    struct af_instance *af = talloc_zero(NULL, struct af_instance);
    *af = (struct af_instance) {
        .info = info,
        .mul = 1,
        .data = talloc_zero(af, struct mp_audio),
        .log = mp_log_new(af, s->log, name),
        .metadata = s->metadata,
        .replaygain_data = s->replaygain_data,
    };
    struct m_config *config = m_config_from_obj_desc(af, s->log, &desc);
    if (m_config_apply_defaults(config, name, s->opts->af_defs) < 0)
        goto error;
    if (m_config_set_obj_params(config, args) < 0)
        goto error;
    af->priv = config->optstruct;

    // Initialize the new filter
    if (af->info->open(af) != AF_OK)
        goto error;

    return af;

error:
    MP_ERR(s, "Couldn't create or open audio filter '%s'\n", name);
    talloc_free(af);
    return NULL;
}

/* Create and insert a new filter of type name before the filter in the
   argument. This function can be called during runtime, the return
   value is the new filter */
static struct af_instance *af_prepend(struct af_stream *s,
                                      struct af_instance *af,
                                      char *name, char **args)
{
    if (!af)
        af = s->last;
    if (af == s->first)
        af = s->first->next;
    // Create the new filter and make sure it is OK
    struct af_instance *new = af_create(s, name, args);
    if (!new)
        return NULL;
    // Update pointers
    new->next = af;
    new->prev = af->prev;
    af->prev = new;
    new->prev->next = new;
    return new;
}

/* Create and insert a new filter of type name after the filter in the
   argument. This function can be called during runtime, the return
   value is the new filter */
static struct af_instance *af_append(struct af_stream *s,
                                     struct af_instance *af,
                                     char *name, char **args)
{
    if (!af)
        af = s->first;
    if (af == s->last)
        af = s->last->prev;
    // Create the new filter and make sure it is OK
    struct af_instance *new = af_create(s, name, args);
    if (!new)
        return NULL;
    // Update pointers
    new->prev = af;
    new->next = af->next;
    af->next = new;
    new->next->prev = new;
    return new;
}

// Uninit and remove the filter "af"
static void af_remove(struct af_stream *s, struct af_instance *af)
{
    if (!af)
        return;

    if (af == s->first || af == s->last)
        return;

    // Print friendly message
    MP_VERBOSE(s, "Removing filter %s \n", af->info->name);

    // Detach pointers
    af->prev->next = af->next;
    af->next->prev = af->prev;

    if (af->uninit)
        af->uninit(af);
    talloc_free(af);
}

static void remove_auto_inserted_filters(struct af_stream *s)
{
repeat:
    for (struct af_instance *af = s->first; af; af = af->next) {
        if (af->auto_inserted) {
            af_remove(s, af);
            goto repeat;
        }
    }
}

static void af_print_filter_chain(struct af_stream *s, struct af_instance *at,
                                  int msg_level)
{
    MP_MSG(s, msg_level, "Audio filter chain:\n");

    struct af_instance *af = s->first;
    while (af) {
        MP_MSG(s, msg_level, "  [%s] ", af->info->name);
        if (af->data) {
            char *info = mp_audio_config_to_str(af->data);
            MP_MSG(s, msg_level, "%s", info);
            talloc_free(info);
        }
        if (af == at)
            MP_MSG(s, msg_level, " <-");
        MP_MSG(s, msg_level, "\n");

        af = af->next;
    }

    MP_MSG(s, msg_level, "  [ao] ");
    char *info = mp_audio_config_to_str(&s->output);
    MP_MSG(s, msg_level, "%s\n", info);
    talloc_free(info);
}

static int af_count_filters(struct af_stream *s)
{
    int count = 0;
    for (struct af_instance *af = s->first; af; af = af->next)
        count++;
    return count;
}

// Finds the first conversion filter on the way from srcfmt to dstfmt.
// Conversions form a DAG: each node is a format/filter pair, and possible
// conversions are edges. We search the DAG for the shortest path.
// Some cases visit the same filter multiple times, but with different formats
// (like u24le->s8), so one node per format or filter separate is not enough.
// Returns the filter and dest. format for the first conversion step.
// (So we know what conversion filter with what format to insert next.)
static char *af_find_conversion_filter(int srcfmt, int *dstfmt)
{
#define MAX_NODES (64 * 32)
    int num_fmt = 0, num_filt = 0;
    for (int n = 0; filter_list[n]; n++)
        num_filt = n + 1;
    for (int n = 0; af_fmtstr_table[n].format; n++)
        num_fmt = n + 1;

    int num_nodes = num_fmt * num_filt;
    assert(num_nodes < MAX_NODES);

    bool visited[MAX_NODES] = {0};
    unsigned char distance[MAX_NODES];
    short previous[MAX_NODES] = {0};
    for (int n = 0; n < num_nodes; n++) {
        distance[n] = 255;
        if (af_fmtstr_table[n % num_fmt].format == srcfmt)
            distance[n] = 0;
    }

    while (1) {
        int next = -1;
        for (int n = 0; n < num_nodes; n++) {
            if (!visited[n] && (next < 0 || (distance[n] < distance[next])))
                next = n;
        }
        if (next < 0 || distance[next] == 255)
            return NULL;
        visited[next] = true;

        int fmt = next % num_fmt;
        if (af_fmtstr_table[fmt].format == *dstfmt) {
            // Best match found
            for (int cur = next; cur >= 0; cur = previous[cur] - 1) {
                if (distance[cur] == 1) {
                    *dstfmt = af_fmtstr_table[cur % num_fmt].format;
                    return (char *)filter_list[cur / num_fmt]->name;
                }
            }
            return NULL;
        }

        for (int n = 0; filter_list[n]; n++) {
            struct af_info *af = filter_list[n];
            if (!af->test_conversion)
                continue;
            for (int i = 0; af_fmtstr_table[i].format; i++) {
                if (i != fmt && af->test_conversion(af_fmtstr_table[fmt].format,
                                                    af_fmtstr_table[i].format))
                {
                    int other = n * num_fmt + i;
                    int ndist = distance[next] + 1;
                    if (ndist < distance[other]) {
                        distance[other] = ndist;
                        previous[other] = next + 1;
                    }
                }
            }
        }
    }
    assert(0);
#undef MAX_NODES
}

static bool af_is_conversion_filter(struct af_instance *af)
{
    return af && af->info->test_conversion != NULL;
}

// in is what af can take as input - insert a conversion filter if the actual
// input format doesn't match what af expects.
// Returns:
//   AF_OK: must call af_reinit() or equivalent, format matches (or is closer)
//   AF_FALSE: nothing was changed, format matches
//   else: error
static int af_fix_format_conversion(struct af_stream *s,
                                    struct af_instance **p_af,
                                    struct mp_audio in)
{
    int rv;
    struct af_instance *af = *p_af;
    struct af_instance *prev = af->prev;
    struct mp_audio actual = *prev->data;
    if (actual.format == in.format)
        return AF_FALSE;
    int dstfmt = in.format;
    char *filter = af_find_conversion_filter(actual.format, &dstfmt);
    if (!filter)
        return AF_ERROR;
    if (strcmp(filter, prev->info->name) == 0) {
        if (prev->control(prev, AF_CONTROL_SET_FORMAT, &dstfmt) == AF_OK) {
            *p_af = prev;
            return AF_OK;
        }
    }
    struct af_instance *new = af_prepend(s, af, filter, NULL);
    if (new == NULL)
        return AF_ERROR;
    new->auto_inserted = true;
    if (AF_OK != (rv = new->control(new, AF_CONTROL_SET_FORMAT, &dstfmt)))
        return rv;
    *p_af = new;
    return AF_OK;
}

// same as af_fix_format_conversion - only wrt. channels
static int af_fix_channels(struct af_stream *s, struct af_instance **p_af,
                           struct mp_audio in)
{
    int rv;
    struct af_instance *af = *p_af;
    struct af_instance *prev = af->prev;
    struct mp_audio actual = *prev->data;
    if (mp_chmap_equals(&actual.channels, &in.channels))
        return AF_FALSE;
    if (prev->control(prev, AF_CONTROL_SET_CHANNELS, &in.channels) == AF_OK) {
        *p_af = prev;
        return AF_OK;
    }
    char *filter = "lavrresample";
    struct af_instance *new = af_prepend(s, af, filter, NULL);
    if (new == NULL)
        return AF_ERROR;
    new->auto_inserted = true;
    if (AF_OK != (rv = new->control(new, AF_CONTROL_SET_CHANNELS, &in.channels)))
        return rv;
    *p_af = new;
    return AF_OK;
}

static int af_fix_rate(struct af_stream *s, struct af_instance **p_af,
                       struct mp_audio in)
{
    int rv;
    struct af_instance *af = *p_af;
    struct af_instance *prev = af->prev;
    struct mp_audio actual = *prev->data;
    if (actual.rate == in.rate)
        return AF_FALSE;
    if (prev->control(prev, AF_CONTROL_SET_RESAMPLE_RATE, &in.rate) == AF_OK) {
        *p_af = prev;
        return AF_OK;
    }
    char *filter = "lavrresample";
    struct af_instance *new = af_prepend(s, af, filter, NULL);
    if (new == NULL)
        return AF_ERROR;
    new->auto_inserted = true;
    if (AF_OK != (rv = new->control(new, AF_CONTROL_SET_RESAMPLE_RATE, &in.rate)))
        return rv;
    *p_af = new;
    return AF_OK;
}

// Return AF_OK on success or AF_ERROR on failure.
// Warning:
// A failed af_reinit() leaves the audio chain behind in a useless, broken
// state (for example, format filters that were tentatively inserted stay
// inserted).
// In that case, you should always rebuild the filter chain, or abort.
static int af_reinit(struct af_stream *s)
{
    remove_auto_inserted_filters(s);
    // Start with the second filter, as the first filter is the special input
    // filter which needs no initialization.
    struct af_instance *af = s->first->next;
    // Up to 7 retries per filter (channel, rate, 5x format conversions)
    int max_retry = af_count_filters(s) * 7;
    int retry = 0;
    while (af) {
        if (retry >= max_retry)
            goto negotiate_error;

        // Check if this is the first filter
        struct mp_audio in = *af->prev->data;
        // Reset just in case...
        mp_audio_set_null_data(&in);

        int rv = af->control(af, AF_CONTROL_REINIT, &in);
        if (rv == AF_OK && !mp_audio_config_equals(&in, af->prev->data))
            rv = AF_FALSE; // conversion filter needed
        switch (rv) {
        case AF_OK:
            af = af->next;
            break;
        case AF_FALSE: { // Configuration filter is needed
            if (af_fix_channels(s, &af, in) == AF_OK) {
                retry++;
                continue;
            }
            if (af_fix_rate(s, &af, in) == AF_OK) {
                retry++;
                continue;
            }
            // Do this last, to prevent "format->lavrresample" being added to
            // the filter chain when output formats not supported by
            // af_lavrresample are in use.
            if (af_fix_format_conversion(s, &af, in) == AF_OK) {
                retry++;
                continue;
            }
            goto negotiate_error;
        }
        case AF_DETACH: { // Filter is redundant and wants to be unloaded
            struct af_instance *aft = af->prev; // never NULL
            af_remove(s, af);
            af = aft->next;
            break;
        }
        default:
            MP_ERR(s, "Reinitialization did not work, "
                   "audio filter '%s' returned error code %i\n",
                   af->info->name, rv);
            af_print_filter_chain(s, af, MSGL_ERR);
            return AF_ERROR;
        }
    }

    af_print_filter_chain(s, NULL, MSGL_V);

    /* Set previously unset fields in s->output to those of the filter chain
     * output. This is used to make the output format fixed, and even if you
     * insert new filters or change the input format, the output format won't
     * change. (Audio outputs generally can't change format at runtime.) */
    af_copy_unset_fields(&s->output, &s->filter_output);
    return af_config_equals(&s->output, &s->filter_output) ? AF_OK : AF_ERROR;

negotiate_error:
    MP_ERR(s, "Unable to convert audio input format to output format.\n");
    af_print_filter_chain(s, af, MSGL_ERR);
    return AF_ERROR;
}

// Uninit and remove all filters
void af_uninit(struct af_stream *s)
{
    while (s->first->next && s->first->next != s->last)
        af_remove(s, s->first->next);
}

struct af_stream *af_new(struct mpv_global *global)
{
    struct af_stream *s = talloc_zero(NULL, struct af_stream);
    static struct af_info in = { .name = "in" };
    s->first = talloc(s, struct af_instance);
    *s->first = (struct af_instance) {
        .info = &in,
        .control = input_control,
        .filter = dummy_filter,
        .priv = s,
        .data = &s->input,
        .mul = 1.0,
    };
    static struct af_info out = { .name = "out" };
    s->last = talloc(s, struct af_instance);
    *s->last = (struct af_instance) {
        .info = &out,
        .control = output_control,
        .filter = dummy_filter,
        .priv = s,
        .data = &s->filter_output,
        .mul = 1.0,
    };
    s->first->next = s->last;
    s->last->prev = s->first;
    s->opts = global->opts;
    s->log = mp_log_new(s, global->log, "!af");
    return s;
}

void af_destroy(struct af_stream *s)
{
    af_uninit(s);
    talloc_free(s);
}

/* Initialize the stream "s". This function creates a new filter list
   if necessary according to the values set in input and output. Input
   and output should contain the format of the current movie and the
   format of the preferred output respectively. The function is
   reentrant i.e. if called with an already initialized stream the
   stream will be reinitialized.
   If one of the prefered output parameters is 0 the one that needs
   no conversion is used (i.e. the output format in the last filter).
   The return value is 0 if success and -1 if failure */
int af_init(struct af_stream *s)
{
    // Sanity check
    if (!s)
        return -1;

    // Precaution in case caller is misbehaving
    mp_audio_set_null_data(&s->input);
    mp_audio_set_null_data(&s->output);

    // Check if this is the first call
    if (s->first->next == s->last) {
        // Add all filters in the list (if there are any)
        struct m_obj_settings *list = s->opts->af_settings;
        for (int i = 0; list && list[i].name; i++) {
            if (!af_prepend(s, s->last, list[i].name, list[i].attribs)) {
                af_uninit(s);
                return -1;
            }
        }
    }

    if (af_reinit(s) != AF_OK) {
        // Something is stuffed audio out will not work
        MP_ERR(s, "Could not create audio filter chain.\n");
        af_uninit(s);
        return -1;
    }
    return 0;
}

/* Add filter during execution. This function adds the filter "name"
   to the stream s. The filter will be inserted somewhere nice in the
   list of filters. The return value is a pointer to the new filter,
   If the filter couldn't be added the return value is NULL. */
struct af_instance *af_add(struct af_stream *s, char *name, char **args)
{
    struct af_instance *new;
    // Sanity check
    if (!s || !s->first || !name)
        return NULL;
    // Insert the filter somewhere nice
    if (af_is_conversion_filter(s->first->next))
        new = af_append(s, s->first->next, name, args);
    else
        new = af_prepend(s, s->first->next, name, args);
    if (!new)
        return NULL;

    // Reinitalize the filter list
    if (af_reinit(s) != AF_OK) {
        af_uninit(s);
        af_init(s);
        return NULL;
    }
    return new;
}

/* Filter data chunk through the filters in the list.
 * On success, *data is set to the filtered data/format.
 * Warning: input audio data will be overwritten.
 */
int af_filter(struct af_stream *s, struct mp_audio *data, int flags)
{
    struct af_instance *af = s->first;
    assert(mp_audio_config_equals(af->data, data));
    // Iterate through all filters
    while (af) {
        int r = af->filter(af, data, flags);
        if (r < 0)
            return r;
        assert(mp_audio_config_equals(af->data, data));
        af = af->next;
    }
    return 0;
}

// Calculate average ratio of filter output samples to input samples.
// e.g: num_output_samples = mul * num_input_samples
double af_calc_filter_multiplier(struct af_stream *s)
{
    struct af_instance *af = s->first;
    double mul = 1;
    do {
        mul *= af->mul;
        af = af->next;
    } while (af);

    return mul;
}

/* Calculate the total delay [seconds of output] caused by the filters */
double af_calc_delay(struct af_stream *s)
{
    struct af_instance *af = s->first;
    double delay = 0.0;
    while (af) {
        delay += af->delay;
        af = af->next;
    }
    return delay;
}

/* Send control to all filters, starting with the last until one accepts the
 * command with AF_OK. Return the accepting filter. */
struct af_instance *af_control_any_rev(struct af_stream *s, int cmd, void *arg)
{
    int res = AF_UNKNOWN;
    struct af_instance *filt = s->last;
    while (filt) {
        res = filt->control(filt, cmd, arg);
        if (res == AF_OK)
            return filt;
        filt = filt->prev;
    }
    return NULL;
}

/* Send control to all filters. Never stop, even if a filter returns AF_OK. */
void af_control_all(struct af_stream *s, int cmd, void *arg)
{
    for (struct af_instance *af = s->first; af; af = af->next)
        af->control(af, cmd, arg);
}
