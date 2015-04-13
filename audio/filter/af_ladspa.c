/*
 * LADSPA plugin loader
 *
 * Written by Ivo van Poorten <ivop@euronet.nl>
 * Copyright (C) 2004, 2005
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
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

#define _(x) (x)

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
    char *controls;

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

#define OPT_BASE_STRUCT af_ladspa_t
const struct af_info af_info_ladspa = {
    .info = "LADSPA plugin loader",
    .name = "ladspa",
    .open = af_open,
    .priv_size = sizeof(af_ladspa_t),
    .options = (const struct m_option[]) {
        OPT_STRING("file", file, 0),
        OPT_STRING("label", label, 0),
        OPT_STRING("controls", controls, 0),
        {0}
    },
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

static int af_ladspa_parse_plugin(struct af_instance *af) {
    af_ladspa_t *setup = af->priv;
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
        MP_WARN(af, "%s: %s\n", setup->myname,
                                                _("WARNING! This LADSPA plugin has no audio inputs.\n  The incoming audio signal will be lost."));
    } else if (setup->ninputs == 1) {
        MP_VERBOSE(af, "%s: this is a mono effect\n", setup->myname);
    } else if (setup->ninputs == 2) {
        MP_VERBOSE(af, "%s: this is a stereo effect\n", setup->myname);
    } else {
        MP_VERBOSE(af, "%s: this is a %i-channel effect, "
               "support is experimental\n", setup->myname, setup->ninputs);
    }

    if (setup->noutputs == 0) {
        MP_ERR(af, "%s: %s\n", setup->myname,
                                                _("This LADSPA plugin has no audio outputs."));
        return AF_ERROR;
    }

    if (setup->noutputs != setup->ninputs ) {
        MP_ERR(af, "%s: %s\n", setup->myname,
                                                _("The number of audio inputs and audio outputs of the LADSPA plugin differ."));
        return AF_ERROR;
    }

    MP_VERBOSE(af, "%s: this plugin has %d input control(s)\n",
                                        setup->myname, setup->ninputcontrols);

    /* Print list of controls and its range of values it accepts */

    for (i=0; i<setup->ninputcontrols; i++) {
        p = setup->inputcontrolsmap[i];
        hint = pdes->PortRangeHints[p];
        MP_VERBOSE(af, "  --- %d %s [", i, pdes->PortNames[p]);

        if (LADSPA_IS_HINT_BOUNDED_BELOW(hint.HintDescriptor)) {
            MP_VERBOSE(af, "%0.2f , ", hint.LowerBound);
        } else {
            MP_VERBOSE(af, "... , ");
        }

        if (LADSPA_IS_HINT_BOUNDED_ABOVE(hint.HintDescriptor)) {
            MP_VERBOSE(af, "%0.2f]\n", hint.UpperBound);
        } else {
            MP_VERBOSE(af, "...]\n");
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
    int endsinso;
    size_t filenamelen;
    void *result = NULL;

#if defined(__MINGW32__) || defined(__CYGWIN__)
    /* For Windows there's only absolute path support.
     * If you have a Windows machine, feel free to fix this.
     * (path separator, shared objects extension, et cetera). */
        MP_VERBOSE(af, "\ton windows, only absolute pathnames "
                "are supported\n");
        MP_VERBOSE(af, "\ttrying %s\n", filename);
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
            int needslash;
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

static int af_ladspa_load_plugin(struct af_instance *af) {
    af_ladspa_t *setup = af->priv;
    const LADSPA_Descriptor *ladspa_descriptor;
    LADSPA_Descriptor_Function descriptor_function;
    int i;

    /* load library */
    MP_VERBOSE(af, "%s: loading ladspa plugin library %s\n",
                                                setup->myname, setup->file);

    setup->libhandle = mydlopen(setup->file, RTLD_NOW);

    if (!setup->libhandle) {
        MP_ERR(af, "%s: %s %s\n\t%s\n", setup->myname,
                    _("failed to load"), setup->file, dlerror() );
        return AF_ERROR;
    }

    MP_VERBOSE(af, "%s: library found.\n", setup->myname);

    /* find descriptor function */
    dlerror();
    descriptor_function = (LADSPA_Descriptor_Function) dlsym (setup->libhandle,
                                                        "ladspa_descriptor");

    if (!descriptor_function) {
        MP_ERR(af, "%s: %s\n\t%s\n", setup->myname,
                                _("Couldn't find ladspa_descriptor() function in the specified library file."), dlerror());
        return AF_ERROR;
    }

    /* if label == help, list all labels in library and exit */

    if (strcmp(setup->label, "help") == 0) {
        MP_INFO(af, "%s: %s %s:\n", setup->myname,
                _("available labels in"), setup->file);
        for (i=0; ; i++) {
            ladspa_descriptor = descriptor_function(i);
            if (ladspa_descriptor == NULL) {
                return AF_ERROR;
            }
            MP_INFO(af, "  %-16s - %s (%lu)\n",
                    ladspa_descriptor->Label,
                    ladspa_descriptor->Name,
                    ladspa_descriptor->UniqueID);
        }
    }

    MP_VERBOSE(af, "%s: looking for label\n", setup->myname);

    /* find label in library */
    for (i=0; ; i++) {
        ladspa_descriptor = descriptor_function(i);
        if (ladspa_descriptor == NULL) {
            MP_ERR(af, "%s: %s\n", setup->myname,
                                            _("Couldn't find label in plugin library."));
            return AF_ERROR;
        }
        if (strcmp(ladspa_descriptor->Label, setup->label) == 0) {
            setup->plugin_descriptor = ladspa_descriptor;
            MP_VERBOSE(af, "%s: %s found\n", setup->myname,
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
 * \param af    Audio filter instance
 * \param cmd   The command to execute
 * \param arg   Arguments to the command
 *
 * \return      Either AF_ERROR or AF_OK, depending on the succes of the
 *              operation.
 */

static int control(struct af_instance *af, int cmd, void *arg) {
    af_ladspa_t *setup = (af_ladspa_t*) af->priv;

    switch(cmd) {
    case AF_CONTROL_REINIT:
        MP_VERBOSE(af, "%s: (re)init\n", setup->myname);

        if (!arg) return AF_ERROR;

        /* accept FLOAT, let af_format do conversion */

        mp_audio_copy_config(af->data, (struct mp_audio*)arg);
        mp_audio_set_format(af->data, AF_FORMAT_FLOAT);

        return af_test_output(af, (struct mp_audio*)arg);
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
    if (af->priv) {
        af_ladspa_t *setup = (af_ladspa_t*) af->priv;
        const LADSPA_Descriptor *pdes = setup->plugin_descriptor;

        if (setup->myname) {
            MP_VERBOSE(af, "%s: cleaning up\n", setup->myname);
            free(setup->myname);
        }

        if (setup->chhandles) {
            for (int i = 0; i < setup->nch; i+=setup->ninputs) {
                if (pdes->deactivate) pdes->deactivate(setup->chhandles[i]);
                if (pdes->cleanup) pdes->cleanup(setup->chhandles[i]);
            }
            free(setup->chhandles);
        }

        free(setup->inputcontrolsmap);
        free(setup->inputcontrols);
        free(setup->outputcontrolsmap);
        free(setup->outputcontrols);
        free(setup->inputs);
        free(setup->outputs);

        if (setup->inbufs) {
            for(int i = 0; i < setup->nch; i++)
                free(setup->inbufs[i]);
            free(setup->inbufs);
        }

        if (setup->outbufs) {
            for (int i = 0; i < setup->nch; i++)
                free(setup->outbufs[i]);
            free(setup->outbufs);
        }

        if (setup->libhandle)
            dlclose(setup->libhandle);
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

static int filter_frame(struct af_instance *af, struct mp_audio *data)
{
    if (!data)
        return 0;
    af_ladspa_t *setup = af->priv;
    const LADSPA_Descriptor *pdes = setup->plugin_descriptor;
    float *audio = (float*)data->planes[0];
    int nsamples = data->samples*data->nch;
    int nch = data->nch;
    int rate = data->rate;
    int i, p;

    if (setup->status !=AF_OK) {
        talloc_free(data);
        return -1;
    }
    if (af_make_writeable(af, data) < 0) {
        talloc_free(data);
        return -1;
    }

    /* See if it's the first call. If so, setup inbufs/outbufs, instantiate
     * plugin, connect ports and activate plugin
     */

    if ( (setup->bufsize != nsamples/nch) || (setup->nch != nch) ) {

        /* if setup->nch==0, it's the first call, if not, something has
         * changed and all previous mallocs have to be freed
         */

        if (setup->nch != 0) {
            MP_TRACE(af, "%s: bufsize change; free old buffer\n",
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

        MP_TRACE(af, "%s: bufsize = %d\n",
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

    af_add_output_frame(af, data);
    return 0;
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
    af->filter_frame = filter_frame;

    af_ladspa_t *setup = af->priv;

    setup->status = AF_ERROR; /* will be set to AF_OK if
                                                   * all went OK and filter()
                                                   * should proceed.
                                                   */

    setup->myname = strdup(af_info_ladspa.name);
    if (!setup->myname)
        return af_ladspa_malloc_failed((char*)af_info_ladspa.name);

    if (!setup->file || !setup->file[0]) {
        MP_ERR(af, "%s: %s\n", setup->myname,
                                            _("No library file specified."));
        uninit(af);
        return AF_ERROR;
    }
    if (!setup->label || !setup->label[0]) {
        MP_ERR(af, "%s: %s\n", setup->myname,
                                            _("No filter label specified."));
        uninit(af);
        return AF_ERROR;
    }

    free(setup->myname);
    setup->myname = calloc(strlen(af_info_ladspa.name)+strlen(setup->file)+
                                                strlen(setup->label)+6, 1);
    snprintf(setup->myname, strlen(af_info_ladspa.name)+
            strlen(setup->file)+strlen(setup->label)+6, "%s: (%s:%s)",
                        af_info_ladspa.name, setup->file, setup->label);

    /* load plugin :) */

    if ( af_ladspa_load_plugin(af) != AF_OK )
        return AF_ERROR;

    /* see what inputs, outputs and controls this plugin has */
    if ( af_ladspa_parse_plugin(af) != AF_OK )
        return AF_ERROR;

    /* ninputcontrols is set by now, read control values from arg */

    float val;
    char *line = setup->controls;
    for (int i = 0; i < setup->ninputcontrols; i++) {
        if (!line || (i != 0 && *line != ',')) {
            MP_ERR(af, "%s: %s\n", setup->myname,
                                    _("Not enough controls specified on the command line."));
            return AF_ERROR;
        }
        if (i != 0)
            line++;
        if (sscanf(line, "%f", &val) != 1) {
            MP_ERR(af, "%s: %s\n", setup->myname,
                                    _("Not enough controls specified on the command line."));
            return AF_ERROR;
        }
        setup->inputcontrols[setup->inputcontrolsmap[i]] = val;
        line = strchr(line, ',');
    }

    MP_VERBOSE(af, "%s: input controls: ", setup->myname);
    for (int i = 0; i < setup->ninputcontrols; i++) {
        MP_VERBOSE(af, "%0.4f ",
                        setup->inputcontrols[setup->inputcontrolsmap[i]]);
    }
    MP_VERBOSE(af, "\n");

    /* check boundaries of inputcontrols */

    MP_VERBOSE(af, "%s: checking boundaries of input controls\n",
                                                            setup->myname);
    for (int i = 0; i < setup->ninputcontrols; i++) {
        int p = setup->inputcontrolsmap[i];
        LADSPA_PortRangeHint hint =
                            setup->plugin_descriptor->PortRangeHints[p];
        val = setup->inputcontrols[p];

        if (LADSPA_IS_HINT_BOUNDED_BELOW(hint.HintDescriptor) &&
                val < hint.LowerBound) {
            MP_ERR(af, "%s: Input control #%d is below lower boundary of %0.4f.\n",
                                        setup->myname, i, hint.LowerBound);
            return AF_ERROR;
        }
        if (LADSPA_IS_HINT_BOUNDED_ABOVE(hint.HintDescriptor) &&
                val > hint.UpperBound) {
            MP_ERR(af, "%s: Input control #%d is above upper boundary of %0.4f.\n",
                                        setup->myname, i, hint.UpperBound);
            return AF_ERROR;
        }
    }
    MP_VERBOSE(af, "%s: all controls have sane values\n",
                                                            setup->myname);

    /* All is well! */
    setup->status = AF_OK;

    return AF_OK;
}

/* ------------------------------------------------------------------------- */
