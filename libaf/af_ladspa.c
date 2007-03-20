/* ------------------------------------------------------------------------- */

/*
 * af_ladspa.c, LADSPA plugin loader
 *
 * Written by Ivo van Poorten <ivop@euronet.nl>
 * Copyright (C) 2004, 2005
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 *
 * Changelog
 *
 * 2005-06-21   Replaced erroneous use of mp_msg by af_msg
 * 2005-05-30   Removed int16 to float conversion; leave that to af_format
 * 2004-12-23   Added to CVS
 * 2004-12-22   Cleaned up cosmetics
 *              Made conversion loops in play() more cache-friendly
 * 2004-12-20   Fixed bug for stereo effect on mono signal
 *                  (trivial >1 to >=1 change; would segfault otherwise :-) )
 *              Removed trailing whitespace and fixed warn/err messages
 *              Have CONTROL_REINIT return a proper value
 * 2004-12-13   More Doxygen comments
 * 2004-12-12   Made af_ladspa optional (updated configure, af.c, etc.)
 * 2004-12-11   Added deactivate and cleanup to uninit.
 *              Finished Doxygen comments.
 *              Moved translatable messages to help_mp-en.h
 * 2004-12-10   Added ranges to list of controls for ease of use.
 *              Fixed sig11 bug. Implemented (dummy) outputcontrols. Some
 *              perfectly normal audio processing filters also have output
 *              controls.
 * 2004-12-08   Added support for generators (no input, one output)
 *              Added support for stereo effects
 *              Added LADSPA_PATH support!
 * 2004-12-07   Fixed changing buffersize. Now it's really working, also in
 *              real-time.
 * 2004-12-06   First working version, mono-effects (1 input --> 1 output) only
 * 2004-12-05   Started, Loading of plugin/label, Check inputs/outputs/controls
 *              Due to lack of documentation, I studied the ladspa_sdk source
 *              code and the loader code of Audacity (by Dominic Mazzoni). So,
 *              certain similarities in (small) pieces of code are not
 *              coincidental :-) No C&P jobs though!
 *
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
#include "help_mp.h"

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

static int af_open(af_instance_t *af);
static int af_ladspa_malloc_failed(char*);

/* ------------------------------------------------------------------------- */

/* Description */

af_info_t af_info_ladspa = {
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
        af_msg(AF_MSG_WARN, "%s: %s\n", setup->myname, 
                                                MSGTR_AF_LADSPA_WarnNoInputs);
    } else if (setup->ninputs == 1) {
        af_msg(AF_MSG_VERBOSE, "%s: this is a mono effect\n", setup->myname);
    } else if (setup->ninputs == 2) {
        af_msg(AF_MSG_VERBOSE, "%s: this is a stereo effect\n", setup->myname);
    } else {
        af_msg(AF_MSG_VERBOSE, "%s: this is a %i-channel effect, "
               "support is experimental\n", setup->myname, setup->ninputs);
    }

    if (setup->noutputs == 0) {
        af_msg(AF_MSG_ERROR, "%s: %s\n", setup->myname,
                                                MSGTR_AF_LADSPA_ErrNoOutputs);
        return AF_ERROR;
    }

    if (setup->noutputs != setup->ninputs ) {
        af_msg(AF_MSG_ERROR, "%s: %s\n", setup->myname,
                                                MSGTR_AF_LADSPA_ErrInOutDiff);
        return AF_ERROR;
    }

    af_msg(AF_MSG_VERBOSE, "%s: this plugin has %d input control(s)\n",
                                        setup->myname, setup->ninputcontrols);

    /* Print list of controls and its range of values it accepts */

    for (i=0; i<setup->ninputcontrols; i++) {
        p = setup->inputcontrolsmap[i];
        hint = pdes->PortRangeHints[p];
        af_msg(AF_MSG_VERBOSE, "  --- %d %s [", i, pdes->PortNames[p]);

        if (LADSPA_IS_HINT_BOUNDED_BELOW(hint.HintDescriptor)) {
            af_msg(AF_MSG_VERBOSE, "%0.2f , ", hint.LowerBound);
        } else {
            af_msg(AF_MSG_VERBOSE, "... , ");
        }

        if (LADSPA_IS_HINT_BOUNDED_ABOVE(hint.HintDescriptor)) {
            af_msg(AF_MSG_VERBOSE, "%0.2f]\n", hint.UpperBound);
        } else {
            af_msg(AF_MSG_VERBOSE, "...]\n");
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

#   ifdef WIN32         /* for windows there's only absolute path support.
                         * if you have a windows machine, feel free to fix
                         * this. (path separator, shared objects extension,
                         * et cetera).
                         */
        af_msg(AF_MSG_VERBOSE, "\ton windows, only absolute pathnames "
                "are supported\n");
        af_msg(AF_MSG_VERBOSE, "\ttrying %s\n", filename);
        return dlopen(filename, flag);
#   endif

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

            af_msg(AF_MSG_VERBOSE, "\ttrying %s\n", buf);
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
    af_msg(AF_MSG_VERBOSE, "\ttrying %s\n", filename);
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
    af_msg(AF_MSG_VERBOSE, "%s: loading ladspa plugin library %s\n",
                                                setup->myname, setup->file);

    setup->libhandle = mydlopen(setup->file, RTLD_NOW);

    if (!setup->libhandle) {
        af_msg(AF_MSG_ERROR, "%s: %s %s\n\t%s\n", setup->myname,
                    MSGTR_AF_LADSPA_ErrFailedToLoad, setup->file, dlerror() );
        return AF_ERROR;
    }

    af_msg(AF_MSG_VERBOSE, "%s: library found.\n", setup->myname);

    /* find descriptor function */
    dlerror();
    descriptor_function = (LADSPA_Descriptor_Function) dlsym (setup->libhandle,
                                                        "ladspa_descriptor");

    if (!descriptor_function) {
        af_msg(AF_MSG_ERROR, "%s: %s\n\t%s\n", setup->myname,
                                MSGTR_AF_LADSPA_ErrNoDescriptor, dlerror());
        return AF_ERROR;
    }

    /* if label == help, list all labels in library and exit */

    if (strcmp(setup->label, "help") == 0) {
        af_msg(AF_MSG_INFO, "%s: %s %s:\n", setup->myname, 
                MSGTR_AF_LADSPA_AvailableLabels, setup->file);
        for (i=0; ; i++) {
            ladspa_descriptor = descriptor_function(i);
            if (ladspa_descriptor == NULL) {
                return AF_ERROR;
            }
            af_msg(AF_MSG_INFO, "  %-16s - %s (%lu)\n",
                    ladspa_descriptor->Label,
                    ladspa_descriptor->Name,
                    ladspa_descriptor->UniqueID);
        }
    }

    af_msg(AF_MSG_VERBOSE, "%s: looking for label\n", setup->myname);

    /* find label in library */
    for (i=0; ; i++) {
        ladspa_descriptor = descriptor_function(i);
        if (ladspa_descriptor == NULL) {
            af_msg(AF_MSG_ERROR, "%s: %s\n", setup->myname,
                                            MSGTR_AF_LADSPA_ErrLabelNotFound);
            return AF_ERROR;
        }
        if (strcmp(ladspa_descriptor->Label, setup->label) == 0) {
            setup->plugin_descriptor = ladspa_descriptor;
            af_msg(AF_MSG_VERBOSE, "%s: %s found\n", setup->myname,
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
    af_msg(AF_MSG_ERROR, "%s: %s", myname, MSGTR_MemAllocFailed);
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

static int control(struct af_instance_s *af, int cmd, void *arg) {
    af_ladspa_t *setup = (af_ladspa_t*) af->setup;
    int i, r;
    float val;

    switch(cmd) {
    case AF_CONTROL_REINIT:
        af_msg(AF_MSG_VERBOSE, "%s: (re)init\n", setup->myname);

        if (!arg) return AF_ERROR;

        /* accept FLOAT, let af_format do conversion */

        af->data->rate   = ((af_data_t*)arg)->rate;
        af->data->nch    = ((af_data_t*)arg)->nch;
        af->data->format = AF_FORMAT_FLOAT_NE;
        af->data->bps    = 4;

        /* arg->len is not set here yet, so init of buffers and connecting the
         * filter, has to be done in play() :-/
         */

        return af_test_output(af, (af_data_t*)arg);
    case AF_CONTROL_COMMAND_LINE: {
        char *buf;

        af_msg(AF_MSG_VERBOSE, "%s: parse suboptions\n", setup->myname);

        /* suboption parser here!
         * format is (ladspa=)file:label:controls....
         */

        if (!arg) {
            af_msg(AF_MSG_ERROR, "%s: %s\n", setup->myname,
                                            MSGTR_AF_LADSPA_ErrNoSuboptions);
            return AF_ERROR;
        }

        buf = malloc(strlen(arg)+1);
        if (!buf) return af_ladspa_malloc_failed(setup->myname);

        /* file... */
        buf[0] = '\0';
        sscanf(arg, "%[^:]", buf);
        if (buf[0] == '\0') {
            af_msg(AF_MSG_ERROR, "%s: %s\n", setup->myname,
                                                MSGTR_AF_LADSPA_ErrNoLibFile);
            free(buf);
            return AF_ERROR;
        }
        arg += strlen(buf);
        setup->file = strdup(buf);
        if (!setup->file) return af_ladspa_malloc_failed(setup->myname);
        af_msg(AF_MSG_VERBOSE, "%s: file --> %s\n", setup->myname,
                                                        setup->file);
        if (*(char*)arg != '\0') arg++; /* read ':' */

        /* label... */
        buf[0] = '\0';
        sscanf(arg, "%[^:]", buf);
        if (buf[0] == '\0') {
            af_msg(AF_MSG_ERROR, "%s: %s\n", setup->myname,
                                                MSGTR_AF_LADSPA_ErrNoLabel);
            free(buf);
            return AF_ERROR;
        }
        arg += strlen(buf);
        setup->label = strdup(buf);
        if (!setup->label) return af_ladspa_malloc_failed(setup->myname);
        af_msg(AF_MSG_VERBOSE, "%s: label --> %s\n", setup->myname,
                                                                setup->label);
/*        if (*(char*)arg != '0') arg++; */ /* read ':' */
 
        free(buf); /* no longer needed */

        /* set new setup->myname */

        if(setup->myname) free(setup->myname);
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
            if (!arg || (*(char*)arg != ':') ) {
                af_msg(AF_MSG_ERROR, "%s: %s\n", setup->myname,
                                        MSGTR_AF_LADSPA_ErrNotEnoughControls);
                return AF_ERROR;
            }
            arg++;
            r = sscanf(arg, "%f", &val);
            if (r!=1) {
                af_msg(AF_MSG_ERROR, "%s: %s\n", setup->myname,
                                        MSGTR_AF_LADSPA_ErrNotEnoughControls);
                return AF_ERROR;
            }
            setup->inputcontrols[setup->inputcontrolsmap[i]] = val;
            arg = strchr(arg, ':');
        }

        af_msg(AF_MSG_VERBOSE, "%s: input controls: ", setup->myname);
        for(i=0; i<setup->ninputcontrols; i++) {
            af_msg(AF_MSG_VERBOSE, "%0.4f ",
                            setup->inputcontrols[setup->inputcontrolsmap[i]]);
        }
        af_msg(AF_MSG_VERBOSE, "\n");

        /* check boundaries of inputcontrols */

        af_msg(AF_MSG_VERBOSE, "%s: checking boundaries of input controls\n",
                                                                setup->myname);
        for(i=0; i<setup->ninputcontrols; i++) {
            int p = setup->inputcontrolsmap[i];
            LADSPA_PortRangeHint hint =
                                setup->plugin_descriptor->PortRangeHints[p];
            val = setup->inputcontrols[p];

            if (LADSPA_IS_HINT_BOUNDED_BELOW(hint.HintDescriptor) &&
                    val < hint.LowerBound) {
                af_msg(AF_MSG_ERROR, MSGTR_AF_LADSPA_ErrControlBelow,
                                            setup->myname, i, hint.LowerBound);
                return AF_ERROR;
            }
            if (LADSPA_IS_HINT_BOUNDED_ABOVE(hint.HintDescriptor) &&
                    val > hint.UpperBound) {
                af_msg(AF_MSG_ERROR, MSGTR_AF_LADSPA_ErrControlAbove,
                                            setup->myname, i, hint.UpperBound);
                return AF_ERROR;
            }
        }
        af_msg(AF_MSG_VERBOSE, "%s: all controls have sane values\n",
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

static void uninit(struct af_instance_s *af) {
    int i;

    if (af->data)
        free(af->data);
    if (af->setup) {
        af_ladspa_t *setup = (af_ladspa_t*) af->setup;
        const LADSPA_Descriptor *pdes = setup->plugin_descriptor;

        if (setup->myname) {
            af_msg(AF_MSG_VERBOSE, "%s: cleaning up\n", setup->myname);
            free(setup->myname);
        }

        if (setup->chhandles) {
            for(i=0; i<setup->nch; i+=setup->ninputs) {
                if (pdes->deactivate) pdes->deactivate(setup->chhandles[i]);
                if (pdes->cleanup) pdes->cleanup(setup->chhandles[i]);
            }
            free(setup->chhandles);
        }

        if (setup->file)
            free(setup->file);
        if (setup->label)
            free(setup->label);
        if (setup->inputcontrolsmap)
            free(setup->inputcontrolsmap);
        if (setup->inputcontrols)
            free(setup->inputcontrols);
        if (setup->outputcontrolsmap)
            free(setup->outputcontrolsmap);
        if (setup->outputcontrols)
            free(setup->outputcontrols);
        if (setup->inputs)
            free(setup->inputs);
        if (setup->outputs)
            free(setup->outputs);

        if (setup->inbufs) {
            for(i=0; i<setup->nch; i++) {
                if (setup->inbufs[i])
                    free(setup->inbufs[i]);
            }
            free(setup->inbufs);
        }

        if (setup->outbufs) {
            for(i=0; i<setup->nch; i++) {
                if (setup->outbufs[i])
                    free(setup->outbufs[i]);
            }
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

static af_data_t* play(struct af_instance_s *af, af_data_t *data) {
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
            af_msg(AF_MSG_DEBUG1, "%s: bufsize change; free old buffer\n",
                                                                setup->myname);

            if(setup->inbufs) {
                for(i=0; i<setup->nch; i++) {
                    if(setup->inbufs[i])
                        free(setup->inbufs[i]);
                }
                free(setup->inbufs);
            }
            if(setup->outbufs) {
                for(i=0; i<setup->nch; i++) {
                    if(setup->outbufs[i])
                        free(setup->outbufs[i]);
                }
                free(setup->outbufs);
            }
        } /* everything is freed */

        setup->bufsize = nsamples/nch;
        setup->nch = nch;

        setup->inbufs = calloc(nch, sizeof(float*));
        setup->outbufs = calloc(nch, sizeof(float*));

        af_msg(AF_MSG_DEBUG1, "%s: bufsize = %d\n",
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

            if ( (pdes->activate) && (setup->activated == 0) ) {
                pdes->activate(setup->chhandles[i]);
                setup->activated = 1;
            }

        } /* All channels/filters done! except for... */

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

static int af_open(af_instance_t *af) {

    af->control=control;
    af->uninit=uninit;
    af->play=play;
    af->mul.n=1;
    af->mul.d=1;

    af->data = calloc(1, sizeof(af_data_t));
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
