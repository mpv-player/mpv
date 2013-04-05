/*
 * LADSPA plugin loader
 *
 * Written by Ivo van Poorten <ivop@euronet.nl>
 * Copyright (C) 2004, 2005
 *
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

/* ------------------------------------------------------------------------- */

/* Global Includes */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <inttypes.h>
#include <math.h>
#include <limits.h>

#include <dlfcn.h>
#include <ladspa.h>

/* ------------------------------------------------------------------------- */

/* Local Includes */

#include "af.h"

/* ------------------------------------------------------------------------- */

/* Filter specific data */

typedef struct af_ladspa_s
{
    int status;     /**< Status of the filter.
                     *   Either AF_OK or AF_ERROR
                     *   Because MPlayer re-inits audio filters that
                     *   _clearly_ returned AF_ERROR anyway, I use this
                     *   in play() to skip the processing and return
                     *   the data unchanged.
                     */

    int activated;  /**< 0 or 1. Activate LADSPA filters only once, even
                     *   if the buffers get resized, to avoid a stuttering
                     *   filter.
                     */

    char *file;
    char *label;

    char *myname;   /**< It's easy to have a concatenation of file and label */

    void *libhandle;
    const LADSPA_Descriptor *plugin_descriptor;

    int nports;

    int ninputs;
    int *inputs;

    int noutputs;
    int *outputs;

    int ninputcontrols;
    int *inputcontrolsmap;  /**< Map input port number [0-] to actual port */
    float *inputcontrols;

    int noutputcontrols;
    int *outputcontrolsmap;
    float *outputcontrols;

    int nch;                /**< number of channels */
    int bufsize;
    float **inbufs;
    float **outbufs;
    LADSPA_Handle *chhandles;

} af_ladspa_t;

/* ------------------------------------------------------------------------- */

static int af_open(struct af_instance *af);
static int af_ladspa_malloc_failed(char*);

/* ------------------------------------------------------------------------- */

/* Description */

struct af_info af_info_ladspa = {
    "LADSPA plugin loader",
    "ladspa",
    "Ivo van Poorten",
    "",
    AF_FLAGS_REENTRANT,
    af_open
};

/* ------------------------------------------------------------------------- */

/* By lack of a better word (in my vocabulary) this is called 'parse'.
 * Feel free to suggest an alternative.
 */

/** \brief Check for inputs, outputs and controls of a given filter.
 *
 * This function counts and checks all input, output and control ports
 * of the filter that was loaded. If it turns out to be a valid
 * filter for MPlayer use, it prints out a list of all controls and
 * the corresponding range of its value at message level  MSGL_V.
 *
 * \param setup     Current setup of the filter. Must have its
 *                  plugin_descriptor set!
 *
 * \return  Returns AF_OK if it has a valid input/output/controls
 *          configuration. Else, it returns AF_ERROR.
 */

static int af_ladspa_parse_plugin(af_ladspa_t *setup) {
    int p, i;
    const LADSPA_Descriptor *pdes = setup->plugin_descriptor;
    LADSPA_PortDescriptor d;
    LADSPA_PortRangeHint hint;

    if (!setup->libhandle)
        return AF_ERROR; /* only call parse after a succesful load */
    if (!setup->plugin_descriptor)
        return AF_ERROR; /* same as above */

    /* let's do it */

    setup->nports = pdes->PortCount;

    /* allocate memory for all inputs/outputs/controls */

    setup->inputs = calloc(setup->nports, sizeof(int));
    if (!setup->inputs) return af_ladspa_malloc_failed(setup->myname);

    setup->outputs = calloc(setup->nports, sizeof(int));
    if (!setup->outputs) return af_ladspa_malloc_failed(setup->myname);

    setup->inputcontrolsmap = calloc(setup->nports, sizeof(int));
    if (!setup->inputcontrolsmap) return af_ladspa_malloc_failed(setup->myname);

    setup->inputcontrols = calloc(setup->nports, sizeof(float));
    if (!setup->inputcontrols) return af_ladspa_malloc_failed(setup->myname);

    setup->outputcontrolsmap = calloc(setup->nports, sizeof(int));
    if (!setup->outputcontrolsmap) return af_ladspa_malloc_failed(setup->myname);

    setup->outputcontrols = calloc(setup->nports, sizeof(float));
    if (!setup->outputcontrols) return af_ladspa_malloc_failed(setup->myname);

    /* set counts to zero */

    setup->ninputs = 0;
    setup->noutputs = 0;
    setup->ninputcontrols = 0;
    setup->noutputcontrols = 0;

    /* check all ports, see what type it is and set variables according to
     * what we have found
     */

    for (p=0; p<setup->nports; p++) {
        d = pdes->PortDescriptors[p];

        if (LADSPA_IS_PORT_AUDIO(d)) {
            if (LADSPA_IS_PORT_INPUT(d)) {
                setup->inputs[setup->ninputs] = p;
                setup->ninputs++;
            } else if (LADSPA_IS_PORT_OUTPUT(d)) {
                setup->outputs[setup->noutputs] = p;
                setup->noutputs++;
            }
        }

        if (LADSPA_IS_PORT_CONTROL(d)) {
            if (LADSPA_IS_PORT_INPUT(d)) {
                setup->inputcontrolsmap[setup->ninputcontrols] = p;
                setup->ninputcontrols++;
                /* set control to zero. set values after reading the rest
                 * of the suboptions and check LADSPA_?_HINT's later.
                 */
                setup->inputcontrols[p] = 0.0f;
            } else if (LADSPA_IS_PORT_OUTPUT(d)) {
                /* read and handle these too, otherwise filters that have them
                 * will sig11
                 */
                setup->outputcontrolsmap[setup->noutputcontrols]=p;
                setup->noutputcontrols++;
                setup->outputcontrols[p] = 0.0f;
            }
        }

    }

    if (setup->ninputs == 0) {
        mp_msg(MSGT_AFILTER, MSGL_WARN, "%s: %s\n", setup->myname,
                                                _("WARNING! This LADSPA plugin has no audio inputs.\n  The incoming audio signal will be lost."));
    } else if (setup->ninputs == 1) {
        mp_msg(MSGT_AFILTER, MSGL_V, "%s: this is a mono effect\n", setup->myname);
    } else if (setup->ninputs == 2) {
        mp_msg(MSGT_AFILTER, MSGL_V, "%s: this is a stereo effect\n", setup->myname);
    } else {
        mp_msg(MSGT_AFILTER, MSGL_V, "%s: this is a %i-channel effect, "
               "support is experimental\n", setup->myname, setup->ninputs);
    }

    if (setup->noutputs == 0) {
        mp_msg(MSGT_AFILTER, MSGL_ERR, "%s: %s\n", setup->myname,
                                                _("This LADSPA plugin has no audio outputs."));
        return AF_ERROR;
    }

    if (setup->noutputs != setup->ninputs ) {
        mp_msg(MSGT_AFILTER, MSGL_ERR, "%s: %s\n", setup->myname,
                                                _("The number of audio inputs and audio outputs of the LADSPA plugin differ."));
        return AF_ERROR;
    }

    mp_msg(MSGT_AFILTER, MSGL_V, "%s: this plugin has %d input control(s)\n",
                                        setup->myname, setup->ninputcontrols);

    /* Print list of controls and its range of values it accepts */

    for (i=0; i<setup->ninputcontrols; i++) {
        p = setup->inputcontrolsmap[i];
        hint = pdes->PortRangeHints[p];
        mp_msg(MSGT_AFILTER, MSGL_V, "  --- %d %s [", i, pdes->PortNames[p]);

        if (LADSPA_IS_HINT_BOUNDED_BELOW(hint.HintDescriptor)) {
            mp_msg(MSGT_AFILTER, MSGL_V, "%0.2f , ", hint.LowerBound);
        } else {
            mp_msg(MSGT_AFILTER, MSGL_V, "... , ");
        }

        if (LADSPA_IS_HINT_BOUNDED_ABOVE(hint.HintDescriptor)) {
            mp_msg(MSGT_AFILTER, MSGL_V, "%0.2f]\n", hint.UpperBound);
        } else {
            mp_msg(MSGT_AFILTER, MSGL_V, "...]\n");
        }

    }

    return AF_OK;
}

/* ------------------------------------------------------------------------- */

/* This function might "slightly" look like dlopenLADSPA in the LADSPA SDK :-)
 * But, I changed a few things, because imho it was broken. It did not support
 * relative paths, only absolute paths that start with a /
 * I think ../../some/dir/foobar.so is just as valid. And if one wants to call
 * his library '...somename...so' he's crazy, but it should be allowed.
 * So, search the path first, try plain *filename later.
 * Also, try adding .so first! I like the recursion the SDK did, but it's
 * better the other way around. -af ladspa=cmt:amp_stereo:0.5 is easier to type
 * than -af ladspa=cmt.so:amp_stereo:0.5 :-))
 */

/** \brief dlopen() wrapper
 *
 * This is a wrapper around dlopen(). It tries various variations of the
 * filename (with or without the addition of the .so extension) in various
 * directories specified by the LADSPA_PATH environment variable. If all fails
 * it tries the filename directly as an absolute path to the library.
 *
 * \param filename  filename of the library to load.
 * \param flag      see dlopen(3) for a description of the flags.
 *
 * \return          returns a pointer to the loaded library on success, or
 *                  NULL if it fails to load.
 */

static void* mydlopen(const char *filename, int flag) {
    char *buf;
    const char *end, *start, *ladspapath;
    int endsinso, needslash;
    size_t filenamelen;
    void *result = NULL;

#if defined(__MINGW32__) || defined(__CYGWIN__)
    /* For Windows there's only absolute path support.
     * If you have a Windows machine, feel free to fix this.
     * (path separator, shared objects extension, et cetera). */
        mp_msg(MSGT_AFILTER, MSGL_V, "\ton windows, only absolute pathnames "
                "are supported\n");
        mp_msg(MSGT_AFILTER, MSGL_V, "\ttrying %s\n", filename);
        return dlopen(filename, flag);
#endif

    filenamelen = strlen(filename);

    endsinso = 0;
    if (filenamelen > 3)
        endsinso = (strcmp(filename+filenamelen-3, ".so") == 0);
    if (!endsinso) {
        buf=malloc(filenamelen+4);
        strcpy(buf, filename);
        strcat(buf, ".so");
        result=mydlopen(buf, flag);
        free(buf);
    }

    if (result)
        return result;

    ladspapath=getenv("LADSPA_PATH");

    if (ladspapath) {

        start=ladspapath;
        while (*start != '\0') {
            end=start;
            while ( (*end != ':') && (*end != '\0') )
                end++;

            buf=malloc(filenamelen + 2 + (end-start) );
            if (end > start)
                strncpy(buf, start, end-start);
            needslash=0;
            if (end > start)
                if (*(end-1) != '/') {
                    needslash = 1;
                    buf[end-start] = '/';
                }
            strcpy(buf+needslash+(end-start), filename);

            mp_msg(MSGT_AFILTER, MSGL_V, "\ttrying %s\n", buf);
            result=dlopen(buf, flag);

            free(buf);
            if (result)
                return result;

            start = end;
            if (*start == ':')
                start++;
        } /* end while there's still more in the path */
    } /* end if there's a ladspapath */

    /* last resort, just open it again, so the dlerror() message is correct */
    mp_msg(MSGT_AFILTER, MSGL_V, "\ttrying %s\n", filename);
    return dlopen(filename,flag);
}

/* ------------------------------------------------------------------------- */

/** \brief Load a LADSPA Plugin
 *
 * This function loads the LADSPA plugin specified by the file and label
 * that are present in the setup variable. First, it loads the library.
 * If it fails, it returns AF_ERROR. If not, it continues to look for the
 * specified label. If it finds it, it sets the plugin_descriptor inside
 * setup and returns AF_OK. If it doesn't, it returns AF_ERROR. Special case
 * is a label called 'help'. In that case, it prints a list of all available
 * labels (filters) in the library specified by file.
 *
 * \param setup     Current setup of the filter. Contains filename and label.
 *
 * \return  Either AF_ERROR or AF_OK, depending on the success of the operation.
 */

static int af_ladspa_load_plugin(af_ladspa_t *setup) {
    const LADSPA_Descriptor *ladspa_descriptor;
    LADSPA_Descriptor_Function descriptor_function;
    int i;

    /* load library */
    mp_msg(MSGT_AFILTER, MSGL_V, "%s: loading ladspa plugin library %s\n",
                                                setup->myname, setup->file);

    setup->libhandle = mydlopen(setup->file, RTLD_NOW);

    if (!setup->libhandle) {
        mp_msg(MSGT_AFILTER, MSGL_ERR, "%s: %s %s\n\t%s\n", setup->myname,
                    _("failed to load"), setup->file, dlerror() );
        return AF_ERROR;
    }

    mp_msg(MSGT_AFILTER, MSGL_V, "%s: library found.\n", setup->myname);

    /* find descriptor function */
    dlerror();
    descriptor_function = (LADSPA_Descriptor_Function) dlsym (setup->libhandle,
                                                        "ladspa_descriptor");

    if (!descriptor_function) {
        mp_msg(MSGT_AFILTER, MSGL_ERR, "%s: %s\n\t%s\n", setup->myname,
                                _("Couldn't find ladspa_descriptor() function in the specified library file."), dlerror());
        return AF_ERROR;
    }

    /* if label == help, list all labels in library and exit */

    if (strcmp(setup->label, "help") == 0) {
        mp_msg(MSGT_AFILTER, MSGL_INFO, "%s: %s %s:\n", setup->myname,
                _("available labels in"), setup->file);
        for (i=0; ; i++) {
            ladspa_descriptor = descriptor_function(i);
            if (ladspa_descriptor == NULL) {
                return AF_ERROR;
            }
            mp_msg(MSGT_AFILTER, MSGL_INFO, "  %-16s - %s (%lu)\n",
                    ladspa_descriptor->Label,
                    ladspa_descriptor->Name,
                    ladspa_descriptor->UniqueID);
        }
    }

    mp_msg(MSGT_AFILTER, MSGL_V, "%s: looking for label\n", setup->myname);

    /* find label in library */
    for (i=0; ; i++) {
        ladspa_descriptor = descriptor_function(i);
        if (ladspa_descriptor == NULL) {
            mp_msg(MSGT_AFILTER, MSGL_ERR, "%s: %s\n", setup->myname,
                                            _("Couldn't find label in plugin library."));
            return AF_ERROR;
        }
        if (strcmp(ladspa_descriptor->Label, setup->label) == 0) {
            setup->plugin_descriptor = ladspa_descriptor;
            mp_msg(MSGT_AFILTER, MSGL_V, "%s: %s found\n", setup->myname,
                                                                setup->label);
            return AF_OK;
        }
    }

    return AF_OK;
}

/* ------------------------------------------------------------------------- */

/** \brief Print a malloc() failed error message.
 *
 * Generic function which can be called if a call to malloc(), calloc(),
 * strdup(), et cetera, failed. It prints a message to the console and
 * returns AF_ERROR.
 *
 * \return  AF_ERROR
 */

static int af_ladspa_malloc_failed(char *myname) {
    mp_msg(MSGT_AFILTER, MSGL_ERR, "%s: %s", myname, "Memory allocation failed.\n");
    return AF_ERROR;
}

/* ------------------------------------------------------------------------- */

/** \brief Controls the filter.
 *
 * Control the behaviour of the filter.
 *
 * Commands:
 * CONTROL_REINIT   Sets the af structure with proper values for number
 *                  of channels, rate, format, et cetera.
 * CONTROL_COMMAND_LINE     Parses the suboptions given to this filter
 *                          through arg. It first parses the filename and
 *                          the label. After that, it loads the filter
 *                          and finds out its proprties. Then in continues
 *                          parsing the controls given on the commandline,
 *                          if any are needed.
 *
 * \param af    Audio filter instance
 * \param cmd   The command to execute
 * \param arg   Arguments to the command
 *
 * \return      Either AF_ERROR or AF_OK, depending on the succes of the
 *              operation.
 */

static int control(struct af_instance *af, int cmd, void *arg) {
    af_ladspa_t *setup = (af_ladspa_t*) af->setup;
    int i, r;
    float val;

    switch(cmd) {
    case AF_CONTROL_REINIT:
        mp_msg(MSGT_AFILTER, MSGL_V, "%s: (re)init\n", setup->myname);

        if (!arg) return AF_ERROR;

        /* accept FLOAT, let af_format do conversion */

        mp_audio_copy_config(af->data, (struct mp_audio*)arg);
        mp_audio_set_format(af->data, AF_FORMAT_FLOAT_NE);

        /* arg->len is not set here yet, so init of buffers and connecting the
         * filter, has to be done in play() :-/
         */

        return af_test_output(af, (struct mp_audio*)arg);
    case AF_CONTROL_COMMAND_LINE: {
        char *buf;
        char *line = arg;

        mp_msg(MSGT_AFILTER, MSGL_V, "%s: parse suboptions\n", setup->myname);

        /* suboption parser here!
         * format is (ladspa=)file:label:controls....
         */

        if (!line) {
            mp_msg(MSGT_AFILTER, MSGL_ERR, "%s: %s\n", setup->myname,
                                            _("No suboptions specified."));
            return AF_ERROR;
        }

        buf = malloc(strlen(line)+1);
        if (!buf) return af_ladspa_malloc_failed(setup->myname);

        /* file... */
        buf[0] = '\0';
        sscanf(line, "%[^:]", buf);
        if (buf[0] == '\0') {
            mp_msg(MSGT_AFILTER, MSGL_ERR, "%s: %s\n", setup->myname,
                                                _("No library file specified."));
            free(buf);
            return AF_ERROR;
        }
        line += strlen(buf);
        setup->file = strdup(buf);
        if (!setup->file) return af_ladspa_malloc_failed(setup->myname);
        mp_msg(MSGT_AFILTER, MSGL_V, "%s: file --> %s\n", setup->myname,
                                                        setup->file);
        if (*line != '\0') line++; /* read ':' */

        /* label... */
        buf[0] = '\0';
        sscanf(line, "%[^:]", buf);
        if (buf[0] == '\0') {
            mp_msg(MSGT_AFILTER, MSGL_ERR, "%s: %s\n", setup->myname,
                                                _("No filter label specified."));
            free(buf);
            return AF_ERROR;
        }
        line += strlen(buf);
        setup->label = strdup(buf);
        if (!setup->label) return af_ladspa_malloc_failed(setup->myname);
        mp_msg(MSGT_AFILTER, MSGL_V, "%s: label --> %s\n", setup->myname,
                                                                setup->label);
/*        if (*line != '0') line++; */ /* read ':' */

        free(buf); /* no longer needed */

        /* set new setup->myname */

        free(setup->myname);
        setup->myname = calloc(strlen(af_info_ladspa.name)+strlen(setup->file)+
                                                    strlen(setup->label)+6, 1);
        snprintf(setup->myname, strlen(af_info_ladspa.name)+
                strlen(setup->file)+strlen(setup->label)+6, "%s: (%s:%s)",
                            af_info_ladspa.name, setup->file, setup->label);

        /* load plugin :) */

        if ( af_ladspa_load_plugin(setup) != AF_OK )
            return AF_ERROR;

        /* see what inputs, outputs and controls this plugin has */
        if ( af_ladspa_parse_plugin(setup) != AF_OK )
            return AF_ERROR;

        /* ninputcontrols is set by now, read control values from arg */

        for(i=0; i<setup->ninputcontrols; i++) {
            if (!line || *line != ':') {
                mp_msg(MSGT_AFILTER, MSGL_ERR, "%s: %s\n", setup->myname,
                                        _("Not enough controls specified on the command line."));
                return AF_ERROR;
            }
            line++;
            r = sscanf(line, "%f", &val);
            if (r!=1) {
                mp_msg(MSGT_AFILTER, MSGL_ERR, "%s: %s\n", setup->myname,
                                        _("Not enough controls specified on the command line."));
                return AF_ERROR;
            }
            setup->inputcontrols[setup->inputcontrolsmap[i]] = val;
            line = strchr(line, ':');
        }

        mp_msg(MSGT_AFILTER, MSGL_V, "%s: input controls: ", setup->myname);
        for(i=0; i<setup->ninputcontrols; i++) {
            mp_msg(MSGT_AFILTER, MSGL_V, "%0.4f ",
                            setup->inputcontrols[setup->inputcontrolsmap[i]]);
        }
        mp_msg(MSGT_AFILTER, MSGL_V, "\n");

        /* check boundaries of inputcontrols */

        mp_msg(MSGT_AFILTER, MSGL_V, "%s: checking boundaries of input controls\n",
                                                                setup->myname);
        for(i=0; i<setup->ninputcontrols; i++) {
            int p = setup->inputcontrolsmap[i];
            LADSPA_PortRangeHint hint =
                                setup->plugin_descriptor->PortRangeHints[p];
            val = setup->inputcontrols[p];

            if (LADSPA_IS_HINT_BOUNDED_BELOW(hint.HintDescriptor) &&
                    val < hint.LowerBound) {
                mp_tmsg(MSGT_AFILTER, MSGL_ERR, "%s: Input control #%d is below lower boundary of %0.4f.\n",
                                            setup->myname, i, hint.LowerBound);
                return AF_ERROR;
            }
            if (LADSPA_IS_HINT_BOUNDED_ABOVE(hint.HintDescriptor) &&
                    val > hint.UpperBound) {
                mp_tmsg(MSGT_AFILTER, MSGL_ERR, "%s: Input control #%d is above upper boundary of %0.4f.\n",
                                            setup->myname, i, hint.UpperBound);
                return AF_ERROR;
            }
        }
        mp_msg(MSGT_AFILTER, MSGL_V, "%s: all controls have sane values\n",
                                                                setup->myname);

        /* All is well! */
        setup->status = AF_OK;

        return AF_OK; }
    }

    return AF_UNKNOWN;
}

/* ------------------------------------------------------------------------- */

/** \brief Uninitialise the LADSPA Plugin Loader filter.
 *
 * This function deactivates the plugin(s), cleans up, frees all allocated
 * memory and exits.
 *
 * \return  No return value.
 */

static void uninit(struct af_instance *af) {
    int i;

    free(af->data);
    if (af->setup) {
        af_ladspa_t *setup = (af_ladspa_t*) af->setup;
        const LADSPA_Descriptor *pdes = setup->plugin_descriptor;

        if (setup->myname) {
            mp_msg(MSGT_AFILTER, MSGL_V, "%s: cleaning up\n", setup->myname);
            free(setup->myname);
        }

        if (setup->chhandles) {
            for(i=0; i<setup->nch; i+=setup->ninputs) {
                if (pdes->deactivate) pdes->deactivate(setup->chhandles[i]);
                if (pdes->cleanup) pdes->cleanup(setup->chhandles[i]);
            }
            free(setup->chhandles);
        }

        free(setup->file);
        free(setup->label);
        free(setup->inputcontrolsmap);
        free(setup->inputcontrols);
        free(setup->outputcontrolsmap);
        free(setup->outputcontrols);
        free(setup->inputs);
        free(setup->outputs);

        if (setup->inbufs) {
            for(i=0; i<setup->nch; i++)
                free(setup->inbufs[i]);
            free(setup->inbufs);
        }

        if (setup->outbufs) {
            for(i=0; i<setup->nch; i++)
                free(setup->outbufs[i]);
            free(setup->outbufs);
        }

        if (setup->libhandle)
            dlclose(setup->libhandle);

        free(setup);
        setup = NULL;
    }
}

/* ------------------------------------------------------------------------- */

/** \brief Process chunk of audio data through the selected LADSPA Plugin.
 *
 * \param af    Pointer to audio filter instance
 * \param data  Pointer to chunk of audio data
 *
 * \return      Either AF_ERROR or AF_OK
 */

static struct mp_audio* play(struct af_instance *af, struct mp_audio *data) {
    af_ladspa_t *setup = af->setup;
    const LADSPA_Descriptor *pdes = setup->plugin_descriptor;
    float *audio = (float*)data->audio;
    int nsamples = data->len/4; /* /4 because it's 32-bit float */
    int nch = data->nch;
    int rate = data->rate;
    int i, p;

    if (setup->status !=AF_OK)
        return data;

    /* See if it's the first call. If so, setup inbufs/outbufs, instantiate
     * plugin, connect ports and activate plugin
     */

    /* 2004-12-07: Also check if the buffersize has to be changed!
     *             data->len is not constant per se! re-init buffers.
     */

    if ( (setup->bufsize != nsamples/nch) || (setup->nch != nch) ) {

        /* if setup->nch==0, it's the first call, if not, something has
         * changed and all previous mallocs have to be freed
         */

        if (setup->nch != 0) {
            mp_msg(MSGT_AFILTER, MSGL_DBG3, "%s: bufsize change; free old buffer\n",
                                                                setup->myname);

            if(setup->inbufs) {
                for(i=0; i<setup->nch; i++)
                    free(setup->inbufs[i]);
                free(setup->inbufs);
            }
            if(setup->outbufs) {
                for(i=0; i<setup->nch; i++)
                    free(setup->outbufs[i]);
                free(setup->outbufs);
            }
        } /* everything is freed */

        setup->bufsize = nsamples/nch;
        setup->nch = nch;

        setup->inbufs = calloc(nch, sizeof(float*));
        setup->outbufs = calloc(nch, sizeof(float*));

        mp_msg(MSGT_AFILTER, MSGL_DBG3, "%s: bufsize = %d\n",
                                        setup->myname, setup->bufsize);

        for(i=0; i<nch; i++) {
            setup->inbufs[i] = calloc(setup->bufsize, sizeof(float));
            setup->outbufs[i] = calloc(setup->bufsize, sizeof(float));
        }

        /* only on the first call, there are no handles. */

        if (!setup->chhandles) {
            setup->chhandles = calloc(nch, sizeof(LADSPA_Handle));

            /* create handles
             * for stereo effects, create one handle for two channels
             */

            for(i=0; i<nch; i++) {

                if (i % setup->ninputs) { /* stereo effect */
                    /* copy the handle from previous channel */
                    setup->chhandles[i] = setup->chhandles[i-1];
                    continue;
                }

                setup->chhandles[i] = pdes->instantiate(pdes, rate);
            }
        }

        /* connect input/output ports for each channel/filter instance
         *
         * always (re)connect ports
         */

        for(i=0; i<nch; i++) {
            pdes->connect_port(setup->chhandles[i],
                               setup->inputs[i % setup->ninputs],
                               setup->inbufs[i]);
            pdes->connect_port(setup->chhandles[i],
                               setup->outputs[i % setup->ninputs],
                               setup->outbufs[i]);

            /* connect (input) controls */

            for (p=0; p<setup->nports; p++) {
                LADSPA_PortDescriptor d = pdes->PortDescriptors[p];
                if (LADSPA_IS_PORT_CONTROL(d)) {
                    if (LADSPA_IS_PORT_INPUT(d)) {
                        pdes->connect_port(setup->chhandles[i], p,
                                                &(setup->inputcontrols[p]) );
                    } else {
                        pdes->connect_port(setup->chhandles[i], p,
                                                &(setup->outputcontrols[p]) );
                    }
                }
            }

            /* Activate filter (if it isn't already :) ) */

            if (pdes->activate && !setup->activated && i % setup->ninputs == 0)
                pdes->activate(setup->chhandles[i]);

        } /* All channels/filters done! except for... */
        setup->activated = 1;

        /* Stereo effect with one channel left. Use same buffer for left
         * and right. connect it to the second port.
         */

        for (p = i; p % setup->ninputs; p++) {
            pdes->connect_port(setup->chhandles[i-1],
                               setup->inputs[p % setup->ninputs],
                               setup->inbufs[i-1]);
            pdes->connect_port(setup->chhandles[i-1],
                               setup->outputs[p % setup->ninputs],
                               setup->outbufs[i-1]);
        } /* done! */

    } /* setup for first call/change of bufsize is done.
       * normal playing routine follows...
       */

    /* Right now, I use a separate input and output buffer.
     * I could change this to in-place processing (inbuf==outbuf), but some
     * ladspa filters are broken and are not able to handle that. This seems
     * fast enough, so unless somebody complains, it stays this way :)
     */

    /* Fill inbufs */

    for (p=0; p<setup->bufsize; p++) {
        for (i=0; i<nch; i++) {
            setup->inbufs[i][p] = audio[p*nch + i];
        }
    }

    /* Run filter(s) */

    for (i=0; i<nch; i+=setup->ninputs) {
        pdes->run(setup->chhandles[i], setup->bufsize);
    }

    /* Extract outbufs */

    for (p=0; p<setup->bufsize; p++) {
        for (i=0; i<nch; i++) {
            audio[p*nch + i] = setup->outbufs[i][p];
        }
    }

    /* done */

    return data;
}

/* ------------------------------------------------------------------------- */

/** \brief Open LADSPA Plugin Loader Filter
 *
 * \param af    Audio Filter instance
 *
 * \return      Either AF_ERROR or AF_OK
 */

static int af_open(struct af_instance *af) {

    af->control=control;
    af->uninit=uninit;
    af->play=play;
    af->mul=1;

    af->data = calloc(1, sizeof(struct mp_audio));
    if (af->data == NULL)
        return af_ladspa_malloc_failed((char*)af_info_ladspa.name);

    af->setup = calloc(1, sizeof(af_ladspa_t));
    if (af->setup == NULL) {
        free(af->data);
        af->data=NULL;
        return af_ladspa_malloc_failed((char*)af_info_ladspa.name);
    }

    ((af_ladspa_t*)af->setup)->status = AF_ERROR; /* will be set to AF_OK if
                                                   * all went OK and play()
                                                   * should proceed.
                                                   */

    ((af_ladspa_t*)af->setup)->myname = strdup(af_info_ladspa.name);
    if (!((af_ladspa_t*)af->setup)->myname)
        return af_ladspa_malloc_failed((char*)af_info_ladspa.name);

    return AF_OK;
}

/* ------------------------------------------------------------------------- */
