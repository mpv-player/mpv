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

#include "mpvcore/m_option.h"
#include "mpvcore/m_config.h"

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

static struct af_info* filter_list[] = {
    &af_info_dummy,
    &af_info_delay,
    &af_info_channels,
    &af_info_force,
    &af_info_volume,
    &af_info_equalizer,
    &af_info_pan,
    &af_info_surround,
    &af_info_sub,
#ifdef HAVE_SYS_MMAN_H
    &af_info_export,
#endif
    &af_info_drc,
    &af_info_extrastereo,
    &af_info_lavcac3enc,
    &af_info_lavrresample,
    &af_info_sweep,
    &af_info_hrtf,
#ifdef CONFIG_LADSPA
    &af_info_ladspa,
#endif
    &af_info_center,
    &af_info_sinesuppress,
    &af_info_karaoke,
    &af_info_scaletempo,
#ifdef CONFIG_LIBBS2B
    &af_info_bs2b,
#endif
#ifdef CONFIG_AF_LAVFI
    &af_info_lavfi,
#endif
    // Must come last, because it's the fallback format conversion filter
    &af_info_format,
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
    .legacy_hacks = true, // many filters have custom option parsing
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
        assert(arg == &((struct af_stream *)af->setup)->input);
        return AF_OK;
    }
    return AF_UNKNOWN;
}

static int output_control(struct af_instance* af, int cmd, void* arg)
{
    struct af_stream *s = af->setup;
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

static struct mp_audio *dummy_play(struct af_instance* af, struct mp_audio* data)
{
    return data;
}

/* Function for creating a new filter of type name.The name may
contain the commandline parameters for the filter */
static struct af_instance *af_create(struct af_stream *s, char *name,
                                     char **args)
{
    struct m_obj_desc desc;
    if (!m_obj_list_find(&desc, &af_obj_list, bstr0(name))) {
        mp_tmsg(MSGT_VFILTER, MSGL_ERR,
                "Couldn't find audio filter '%s'.\n", name);
        return NULL;
    }
    const struct af_info *info = desc.p;
    /* Make sure that the filter is not already in the list if it is
       non-reentrant */
    if (info->flags & AF_FLAGS_NOT_REENTRANT) {
        for (struct af_instance *cur = s->first; cur; cur = cur->next) {
            if (cur->info == info) {
                mp_msg(MSGT_AFILTER, MSGL_ERR, "[libaf] There can only be one "
                       "instance of the filter '%s' in each stream\n", name);
                return NULL;
            }
        }
    }

    mp_msg(MSGT_AFILTER, MSGL_V, "[libaf] Adding filter %s \n", name);

    struct af_instance *af = talloc_zero(NULL, struct af_instance);
    *af = (struct af_instance) {
        .info = info,
        .mul = 1,
    };
    struct m_config *config = m_config_from_obj_desc(af, &desc);
    if (m_config_initialize_obj(config, &desc, &af->priv, &args) < 0)
        goto error;

    // Initialize the new filter
    if (af->info->open(af) != AF_OK)
        goto error;
    if (args && af->control) {
        // Single option string for old filters
        char *opts = (char *)args; // m_config_initialize_obj did this
        assert(!af->priv);
        if (af->control(af, AF_CONTROL_COMMAND_LINE, opts) <= AF_ERROR)
            goto error;
    }

    return af;

error:
    mp_msg(MSGT_AFILTER, MSGL_ERR,
           "[libaf] Couldn't create or open audio filter '%s'\n", name);
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
    mp_msg(MSGT_AFILTER, MSGL_V, "[libaf] Removing filter %s \n",
           af->info->name);

    // Notify filter before changing anything
    af->control(af, AF_CONTROL_PRE_DESTROY, 0);

    // Detach pointers
    af->prev->next = af->next;
    af->next->prev = af->prev;

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
    mp_msg(MSGT_AFILTER, msg_level, "Audio filter chain:\n");

    struct af_instance *af = s->first;
    while (af) {
        mp_msg(MSGT_AFILTER, msg_level, "  [%s] ", af->info->name);
        if (af->data) {
            char *info = mp_audio_config_to_str(af->data);
            mp_msg(MSGT_AFILTER, msg_level, "%s", info);
            talloc_free(info);
        }
        if (af == at)
            mp_msg(MSGT_AFILTER, msg_level, " <-");
        mp_msg(MSGT_AFILTER, msg_level, "\n");

        af = af->next;
    }

    mp_msg(MSGT_AFILTER, msg_level, "  [ao] ");
    char *info = mp_audio_config_to_str(&s->output);
    mp_msg(MSGT_AFILTER, msg_level, "%s\n", info);
    talloc_free(info);
}

static int af_count_filters(struct af_stream *s)
{
    int count = 0;
    for (struct af_instance *af = s->first; af; af = af->next)
        count++;
    return count;
}

static char *af_find_conversion_filter(int srcfmt, int dstfmt)
{
    for (int n = 0; filter_list[n]; n++) {
        struct af_info *af = filter_list[n];
        if (af->test_conversion && af->test_conversion(srcfmt, dstfmt))
            return (char *)af->name;
    }
    return NULL;
}

static bool af_is_conversion_filter(struct af_instance *af)
{
    return af && af->info->test_conversion != NULL;
}

// in is what af can take as input - insert a conversion filter if the actual
// input format doesn't match what af expects.
// Returns:
//   AF_OK: must call af_reinit() or equivalent, format matches
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
    if (prev->control(prev, AF_CONTROL_FORMAT_FMT, &in.format) == AF_OK) {
        *p_af = prev;
        return AF_OK;
    }
    char *filter = af_find_conversion_filter(actual.format, in.format);
    if (!filter)
        return AF_ERROR;
    struct af_instance *new = af_prepend(s, af, filter, NULL);
    if (new == NULL)
        return AF_ERROR;
    new->auto_inserted = true;
    if (AF_OK != (rv = new->control(new, AF_CONTROL_FORMAT_FMT, &in.format)))
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
    if (prev->control(prev, AF_CONTROL_CHANNELS, &in.channels) == AF_OK) {
        *p_af = prev;
        return AF_OK;
    }
    char *filter = "lavrresample";
    struct af_instance *new = af_prepend(s, af, filter, NULL);
    if (new == NULL)
        return AF_ERROR;
    new->auto_inserted = true;
    if (AF_OK != (rv = new->control(new, AF_CONTROL_CHANNELS, &in.channels)))
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
    if (prev->control(prev, AF_CONTROL_RESAMPLE_RATE, &in.rate) == AF_OK) {
        *p_af = prev;
        return AF_OK;
    }
    char *filter = "lavrresample";
    struct af_instance *new = af_prepend(s, af, filter, NULL);
    if (new == NULL)
        return AF_ERROR;
    new->auto_inserted = true;
    if (AF_OK != (rv = new->control(new, AF_CONTROL_RESAMPLE_RATE, &in.rate)))
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
    // Start with the second filter, as the first filter is the special input
    // filter which needs no initialization.
    struct af_instance *af = s->first->next;
    int max_retry = af_count_filters(s) * 4; // up to 4 retries per filter
    int retry = 0;
    while (af) {
        if (retry >= max_retry)
            goto negotiate_error;

        // Check if this is the first filter
        struct mp_audio in = *af->prev->data;
        // Reset just in case...
        in.audio = NULL;
        in.len = 0;

        int rv = af->control(af, AF_CONTROL_REINIT, &in);
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
            mp_msg(MSGT_AFILTER, MSGL_ERR, "[libaf] Reinitialization did not "
                   "work, audio filter '%s' returned error code %i\n",
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
    mp_msg(MSGT_AFILTER, MSGL_ERR, "[libaf] Unable to correct audio format. "
           "This error should never occur, please send a bug report.\n");
    af_print_filter_chain(s, af, MSGL_ERR);
    return AF_ERROR;
}

// Uninit and remove all filters
void af_uninit(struct af_stream *s)
{
    while (s->first->next && s->first->next != s->last)
        af_remove(s, s->first->next);
}

struct af_stream *af_new(struct MPOpts *opts)
{
    struct af_stream *s = talloc_zero(NULL, struct af_stream);
    static struct af_info in = { .name = "in" };
    s->first = talloc(s, struct af_instance);
    *s->first = (struct af_instance) {
        .info = &in,
        .control = input_control,
        .play = dummy_play,
        .setup = s,
        .data = &s->input,
        .mul = 1.0,
    };
    static struct af_info out = { .name = "out" };
    s->last = talloc(s, struct af_instance);
    *s->last = (struct af_instance) {
        .info = &out,
        .control = output_control,
        .play = dummy_play,
        .setup = s,
        .data = &s->filter_output,
        .mul = 1.0,
    };
    s->first->next = s->last;
    s->last->prev = s->first;
    s->opts = opts;
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
   formate of the preferred output respectively. The function is
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
    s->input.audio  = s->output.audio  = NULL;
    s->input.len    = s->output.len    = 0;

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

    remove_auto_inserted_filters(s);

    if (af_reinit(s) != AF_OK) {
        // Something is stuffed audio out will not work
        mp_msg(MSGT_AFILTER, MSGL_ERR, "Could not create audio filter chain.\n");
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

// Filter data chunk through the filters in the list
struct mp_audio *af_play(struct af_stream *s, struct mp_audio *data)
{
    struct af_instance *af = s->first;
    // Iterate through all filters
    do {
        if (data->len <= 0)
            break;
        data = af->play(af, data);
        af = af->next;
    } while (af && data);
    return data;
}

/* Calculate the minimum output buffer size for given input data d
 * when using the RESIZE_LOCAL_BUFFER macro. The +t+1 part ensures the
 * value is >= len*mul rounded upwards to whole samples even if the
 * double 'mul' is inexact. */
int af_lencalc(double mul, struct mp_audio *d)
{
    int t = d->bps * d->nch;
    return d->len * mul + t + 1;
}

// Calculate average ratio of filter output size to input size
double af_calc_filter_multiplier(struct af_stream *s)
{
    struct af_instance *af = s->first;
    double mul = 1;
    // Iterate through all filters and calculate total multiplication factor
    do {
        mul *= af->mul;
        af = af->next;
    } while (af);

    return mul;
}

/* Calculate the total delay [bytes output] caused by the filters */
double af_calc_delay(struct af_stream *s)
{
    struct af_instance *af = s->first;
    register double delay = 0.0;
    // Iterate through all filters
    while (af) {
        delay += af->delay;
        delay *= af->mul;
        af = af->next;
    }
    return delay;
}

/* Helper function called by the macro with the same name this
   function should not be called directly */
int af_resize_local_buffer(struct af_instance *af, struct mp_audio *data)
{
    // Calculate new length
    register int len = af_lencalc(af->mul, data);
    mp_msg(MSGT_AFILTER, MSGL_V, "[libaf] Reallocating memory in module %s, "
           "old len = %i, new len = %i\n", af->info->name, af->data->len, len);
    // If there is a buffer free it
    free(af->data->audio);
    // Create new buffer and check that it is OK
    af->data->audio = malloc(len);
    if (!af->data->audio) {
        mp_msg(MSGT_AFILTER, MSGL_FATAL, "[libaf] Could not allocate memory \n");
        return AF_ERROR;
    }
    af->data->len = len;
    return AF_OK;
}

// documentation in af.h
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
