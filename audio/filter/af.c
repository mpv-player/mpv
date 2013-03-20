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

#include "af.h"

// Static list of filters
extern struct af_info af_info_dummy;
extern struct af_info af_info_delay;
extern struct af_info af_info_channels;
extern struct af_info af_info_format;
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
    NULL
};

// CPU speed
int *af_cpu_speed = NULL;

/* Find a filter in the static list of filters using it's name. This
   function is used internally */
static struct af_info *af_find(char *name)
{
    int i = 0;
    while (filter_list[i]) {
        if (!strcmp(filter_list[i]->name, name))
            return filter_list[i];
        i++;
    }
    mp_msg(MSGT_AFILTER, MSGL_ERR, "Couldn't find audio filter '%s'\n", name);
    return NULL;
}

/* Find filter in the dynamic filter list using it's name This
   function is used for finding already initialized filters */
struct af_instance *af_get(struct af_stream *s, char *name)
{
    struct af_instance *af = s->first;
    // Find the filter
    while (af != NULL) {
        if (!strcmp(af->info->name, name))
            return af;
        af = af->next;
    }
    return NULL;
}

/* Function for creating a new filter of type name.The name may
contain the commandline parameters for the filter */
static struct af_instance *af_create(struct af_stream *s,
                                     const char *name_with_cmd)
{
    char *name = strdup(name_with_cmd);
    char *cmdline = name;

    // Allocate space for the new filter and reset all pointers
    struct af_instance *new = malloc(sizeof(struct af_instance));
    if (!name || !new) {
        mp_msg(MSGT_AFILTER, MSGL_ERR, "[libaf] Could not allocate memory\n");
        goto err_out;
    }
    memset(new, 0, sizeof(struct af_instance));

    // Check for commandline parameters
    char *skip = strstr(cmdline, "=");
    if (skip) {
        *skip = '\0'; // for name
        cmdline = skip + 1;
    } else {
        cmdline = NULL;
    }

    // Find filter from name
    if (NULL == (new->info = af_find(name)))
        goto err_out;

    /* Make sure that the filter is not already in the list if it is
       non-reentrant */
    if (new->info->flags & AF_FLAGS_NOT_REENTRANT) {
        if (af_get(s, name)) {
            mp_msg(MSGT_AFILTER, MSGL_ERR, "[libaf] There can only be one "
                   "instance of the filter '%s' in each stream\n", name);
            goto err_out;
        }
    }

    mp_msg(MSGT_AFILTER, MSGL_V, "[libaf] Adding filter %s \n", name);

    // Initialize the new filter
    if (AF_OK == new->info->open(new) &&
        AF_ERROR < new->control(new, AF_CONTROL_POST_CREATE, &s->cfg))
    {
        if (cmdline) {
            if (AF_ERROR >= new->control(new, AF_CONTROL_COMMAND_LINE, cmdline))
                goto err_out;
        }
        free(name);
        return new;
    }

err_out:
    free(new);
    mp_msg(MSGT_AFILTER, MSGL_ERR,
           "[libaf] Couldn't create or open audio filter '%s'\n", name);
    free(name);
    return NULL;
}

/* Create and insert a new filter of type name before the filter in the
   argument. This function can be called during runtime, the return
   value is the new filter */
static struct af_instance *af_prepend(struct af_stream *s,
                                      struct af_instance *af,
                                      const char *name)
{
    // Create the new filter and make sure it is OK
    struct af_instance *new = af_create(s, name);
    if (!new)
        return NULL;
    // Update pointers
    new->next = af;
    if (af) {
        new->prev = af->prev;
        af->prev = new;
    } else
        s->last = new;
    if (new->prev)
        new->prev->next = new;
    else
        s->first = new;
    return new;
}

/* Create and insert a new filter of type name after the filter in the
   argument. This function can be called during runtime, the return
   value is the new filter */
static struct af_instance *af_append(struct af_stream *s,
                                     struct af_instance *af,
                                     const char *name)
{
    // Create the new filter and make sure it is OK
    struct af_instance *new = af_create(s, name);
    if (!new)
        return NULL;
    // Update pointers
    new->prev = af;
    if (af) {
        new->next = af->next;
        af->next = new;
    } else
        s->first = new;
    if (new->next)
        new->next->prev = new;
    else
        s->last = new;
    return new;
}

// Uninit and remove the filter "af"
void af_remove(struct af_stream *s, struct af_instance *af)
{
    if (!af)
        return;

    // Print friendly message
    mp_msg(MSGT_AFILTER, MSGL_V, "[libaf] Removing filter %s \n",
           af->info->name);

    // Notify filter before changing anything
    af->control(af, AF_CONTROL_PRE_DESTROY, 0);

    // Detach pointers
    if (af->prev)
        af->prev->next = af->next;
    else
        s->first = af->next;
    if (af->next)
        af->next->prev = af->prev;
    else
        s->last = af->prev;

    // Uninitialize af and free memory
    af->uninit(af);
    free(af);
}

static void print_fmt(struct mp_audio *d)
{
    if (d) {
        mp_msg(MSGT_AFILTER, MSGL_V, "%dHz/%dch/%s", d->rate, d->nch,
               af_fmt2str_short(d->format));
    } else
        mp_msg(MSGT_AFILTER, MSGL_V, "(?)");
}

static void af_print_filter_chain(struct af_stream *s)
{
    mp_msg(MSGT_AFILTER, MSGL_V, "Audio filter chain:\n");

    mp_msg(MSGT_AFILTER, MSGL_V, "  [in] ");
    print_fmt(&s->input);
    mp_msg(MSGT_AFILTER, MSGL_V, "\n");

    struct af_instance *af = s->first;
    while (af) {
        mp_msg(MSGT_AFILTER, MSGL_V, "  [%s] ", af->info->name);
        print_fmt(af->data);
        mp_msg(MSGT_AFILTER, MSGL_V, "\n");

        af = af->next;
    }

    mp_msg(MSGT_AFILTER, MSGL_V, "  [out] ");
    print_fmt(&s->output);
    mp_msg(MSGT_AFILTER, MSGL_V, "\n");
}

// Warning:
// A failed af_reinit() leaves the audio chain behind in a useless, broken
// state (for example, format filters that were tentatively inserted stay
// inserted).
// In that case, you should always rebuild the filter chain, or abort.
int af_reinit(struct af_stream *s, struct af_instance *af)
{
    do {
        int rv = 0; // Return value

        // Check if there are any filters left in the list
        if (NULL == af) {
            if (!(af = af_append(s, s->first, "dummy")))
                return AF_UNKNOWN;
            else
                return AF_ERROR;
        }

        // Check if this is the first filter
        struct mp_audio in = af->prev ? *(af->prev->data) : s->input;
        // Reset just in case...
        in.audio = NULL;
        in.len = 0;

        rv = af->control(af, AF_CONTROL_REINIT, &in);
        switch (rv) {
        case AF_OK:
            af = af->next;
            break;
        case AF_FALSE: { // Configuration filter is needed
            // Do auto insertion only if force is not specified
            if ((AF_INIT_TYPE_MASK & s->cfg.force) != AF_INIT_FORCE) {
                struct af_instance *new = NULL;
                // Insert channels filter
                if ((af->prev ? af->prev->data->nch : s->input.nch) != in.nch) {
                    // Create channels filter
                    if (NULL == (new = af_prepend(s, af, "channels")))
                        return AF_ERROR;
                    // Set number of output channels
                    if (AF_OK !=
                        (rv = new->control(new, AF_CONTROL_CHANNELS, &in.nch)))
                        return rv;
                    // Initialize channels filter
                    in = new->prev ? (*new->prev->data) : s->input;
                    if (AF_OK != (rv = new->control(new, AF_CONTROL_REINIT, &in)))
                        return rv;
                }
                // Insert format filter
                if ((af->prev ? af->prev->data->format : s->input.format) !=
                    in.format)
                {
                    // Create format filter
                    if (NULL == (new = af_prepend(s, af, "format")))
                        return AF_ERROR;
                    // Set output bits per sample
                    in.format |= af_bits2fmt(in.bps * 8);
                    if (AF_OK !=
                        (rv = new->control(new, AF_CONTROL_FORMAT_FMT, &in.format)))
                        return rv;
                    // Initialize format filter
                    in = new->prev ? (*new->prev->data) : s->input;
                    if (AF_OK != (rv = new->control(new, AF_CONTROL_REINIT, &in)))
                        return rv;
                }
                if (!new) { // Should _never_ happen
                    mp_msg(
                        MSGT_AFILTER, MSGL_ERR,
                        "[libaf] Unable to correct audio format. "
                        "This error should never occur, please send a bug report.\n");
                    return AF_ERROR;
                }
                af = new->next;
            } else {
                mp_msg(
                    MSGT_AFILTER, MSGL_ERR,
                    "[libaf] Automatic filter insertion disabled "
                    "but formats do not match. Giving up.\n");
                return AF_ERROR;
            }
            break;
        }
        case AF_DETACH: { // Filter is redundant and wants to be unloaded
            // Do auto remove only if force is not specified
            if ((AF_INIT_TYPE_MASK & s->cfg.force) != AF_INIT_FORCE) {
                struct af_instance *aft = af->prev;
                af_remove(s, af);
                if (aft)
                    af = aft->next;
                else
                    af = s->first;  // Restart configuration
            }
            break;
        }
        default:
            mp_msg(MSGT_AFILTER, MSGL_ERR, "[libaf] Reinitialization did not "
                   "work, audio filter '%s' returned error code %i\n",
                   af->info->name, rv);
            return AF_ERROR;
        }
    } while (af);

    af_print_filter_chain(s);

    return AF_OK;
}

// Uninit and remove all filters
void af_uninit(struct af_stream *s)
{
    while (s->first)
        af_remove(s, s->first);
}

/**
 * Extend the filter chain so we get the required output format at the end.
 * \return AF_ERROR on error, AF_OK if successful.
 */
static int fixup_output_format(struct af_stream *s)
{
    struct af_instance *af = NULL;
    // Check number of output channels fix if not OK
    // If needed always inserted last -> easy to screw up other filters
    if (s->output.nch && s->last->data->nch != s->output.nch) {
        if (!strcmp(s->last->info->name, "format"))
            af = af_prepend(s, s->last, "channels");
        else
            af = af_append(s, s->last, "channels");
        // Init the new filter
        if (!af ||
            (AF_OK != af->control(af, AF_CONTROL_CHANNELS, &(s->output.nch))))
            return AF_ERROR;
        if (AF_OK != af_reinit(s, af))
            return AF_ERROR;
    }

    // Check output format fix if not OK
    if (s->output.format != AF_FORMAT_UNKNOWN &&
        s->last->data->format != s->output.format) {
        if (strcmp(s->last->info->name, "format"))
            af = af_append(s, s->last, "format");
        else
            af = s->last;
        // Init the new filter
        s->output.format |= af_bits2fmt(s->output.bps * 8);
        if (!af ||
            (AF_OK != af->control(af, AF_CONTROL_FORMAT_FMT, &(s->output.format))))
            return AF_ERROR;
        if (AF_OK != af_reinit(s, af))
            return AF_ERROR;
    }

    // Re init again just in case
    if (AF_OK != af_reinit(s, s->first))
        return AF_ERROR;

    if (s->output.format == AF_FORMAT_UNKNOWN)
        s->output.format = s->last->data->format;
    if (!s->output.nch)
        s->output.nch = s->last->data->nch;
    if (!s->output.rate)
        s->output.rate = s->last->data->rate;
    if ((s->last->data->format != s->output.format) ||
        (s->last->data->nch != s->output.nch) ||
        (s->last->data->rate != s->output.rate))
        return AF_ERROR;
    return AF_OK;
}

/**
 * Automatic downmix to stereo in case the codec does not implement it.
 */
static void af_downmix(struct af_stream *s)
{
    static const char *const downmix_strs[AF_NCH + 1] = {
        /*                FL       FR       RL       RR          FC          LF         AL      AR */
        [3] = "pan=2:" "0.6:0:" "0:0.6:"                     "0.4:0.4",
        [4] = "pan=2:" "0.6:0:" "0:0.6:" "0.4:0:"  "0:0.4",
        [5] = "pan=2:" "0.5:0:" "0:0.5:" "0.2:0:"  "0:0.2:"  "0.3:0.3",
        [6] = "pan=2:" "0.4:0:" "0:0.4:" "0.2:0:"  "0:0.2:"  "0.3:0.3:"   "0.1:0.1",
        [7] = "pan=2:" "0.4:0:" "0:0.4:" "0.2:0:"  "0:0.2:"  "0.3:0.3:"   "0.1:0:"    "0:0.1",
        [8] = "pan=2:" "0.4:0:" "0:0.4:" "0.15:0:" "0:0.15:" "0.25:0.25:" "0.1:0.1:"  "0.1:0:" "0:0.1",
    };
    const char *af_pan_str = downmix_strs[s->input.nch];

    if (af_pan_str)
        af_append(s, s->first, af_pan_str);
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
    struct MPOpts *opts = s->opts;
    int i = 0;

    // Sanity check
    if (!s)
        return -1;

    // Precaution in case caller is misbehaving
    s->input.audio  = s->output.audio  = NULL;
    s->input.len    = s->output.len    = 0;

    // Figure out how fast the machine is
    if (AF_INIT_AUTO == (AF_INIT_TYPE_MASK & s->cfg.force))
        s->cfg.force = (s->cfg.force & ~AF_INIT_TYPE_MASK) | AF_INIT_TYPE;

    // Check if this is the first call
    if (!s->first) {
        // Append a downmix pan filter at the beginning of the chain if needed
        if (s->input.nch != opts->audio_output_channels
            && opts->audio_output_channels == 2)
            af_downmix(s);
        // Add all filters in the list (if there are any)
        if (s->cfg.list) {
            while (s->cfg.list[i]) {
                if (!af_append(s, s->last, s->cfg.list[i++]))
                    return -1;
            }
        }
    }

    // If we do not have any filters otherwise
    // add dummy to make automatic format conversion work
    if (!s->first && !af_append(s, s->first, "dummy"))
        return -1;

    // Init filters
    if (AF_OK != af_reinit(s, s->first))
        return -1;

    // make sure the chain is not empty and valid (e.g. because of AF_DETACH)
    if (!s->first) {
        if (!af_append(s, s->first, "dummy") || AF_OK != af_reinit(s, s->first))
            return -1;
    }

    // Check output format
    if ((AF_INIT_TYPE_MASK & s->cfg.force) != AF_INIT_FORCE) {
        struct af_instance *af = NULL; // New filter
        // Check output frequency if not OK fix with resample
        if (s->output.rate && s->last->data->rate != s->output.rate) {
            // try to find a filter that can change samplrate
            af = af_control_any_rev(s, AF_CONTROL_RESAMPLE_RATE | AF_CONTROL_SET,
                                    &(s->output.rate));
            if (!af) {
                char *resampler = "lavrresample";
                if ((AF_INIT_TYPE_MASK & s->cfg.force) == AF_INIT_SLOW) {
                    if (!strcmp(s->first->info->name, "format"))
                        af = af_append(s, s->first, resampler);
                    else
                        af = af_prepend(s, s->first, resampler);
                } else {
                    if (!strcmp(s->last->info->name, "format"))
                        af = af_prepend(s, s->last, resampler);
                    else
                        af = af_append(s, s->last, resampler);
                }
                // Init the new filter
                if (!af)
                    return -1;
                if (af->control(af, AF_CONTROL_RESAMPLE_RATE | AF_CONTROL_SET,
                                &(s->output.rate)) != AF_OK)
                    return -1;
            }
            if (AF_OK != af_reinit(s, af))
                return -1;
        }
        if (AF_OK != fixup_output_format(s)) {
            // Something is stuffed audio out will not work
            mp_msg(
                MSGT_AFILTER, MSGL_ERR,
                "[libaf] Unable to setup filter system can not"
                " meet sound-card demands, please send a bug report. \n");
            af_uninit(s);
            return -1;
        }
    }
    return 0;
}

/* Add filter during execution. This function adds the filter "name"
   to the stream s. The filter will be inserted somewhere nice in the
   list of filters. The return value is a pointer to the new filter,
   If the filter couldn't be added the return value is NULL. */
struct af_instance *af_add(struct af_stream *s, char *name)
{
    struct af_instance *new;
    // Sanity check
    if (!s || !s->first || !name)
        return NULL;
    // Insert the filter somewhere nice
    if (!strcmp(s->first->info->name, "format"))
        new = af_append(s, s->first, name);
    else
        new = af_prepend(s, s->first, name);
    if (!new)
        return NULL;

    // Reinitalize the filter list
    if (AF_OK != af_reinit(s, s->first) ||
        AF_OK != fixup_output_format(s)) {
        while (s->first)
            af_remove(s, s->first);
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

void af_help(void)
{
    int i = 0;
    mp_msg(MSGT_AFILTER, MSGL_INFO, "Available audio filters:\n");
    while (filter_list[i]) {
        if (filter_list[i]->comment && filter_list[i]->comment[0]) {
            mp_msg(MSGT_AFILTER, MSGL_INFO, "  %-15s: %s (%s)\n",
                   filter_list[i]->name, filter_list[i]->info,
                   filter_list[i]->comment);
        } else {
            mp_msg(MSGT_AFILTER, MSGL_INFO, "  %-15s: %s\n",
                   filter_list[i]->name,
                   filter_list[i]->info);
        }
        i++;
    }
}

void af_fix_parameters(struct mp_audio *data)
{
    if (data->nch < 0 || data->nch > AF_NCH) {
        mp_msg(MSGT_AFILTER, MSGL_ERR,
               "Invalid number of channels %i, assuming 2.\n", data->nch);
        data->nch = 2;
    }
    data->bps = af_fmt2bits(data->format) / 8;
}
