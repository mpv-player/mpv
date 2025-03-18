/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "py_extend.h"

// #include <assert.h>
// #include <stdio.h>
// #include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <math.h>

#include "boolobject.h"
#include "longobject.h"
#include "object.h"
#include "osdep/io.h"
#include "osdep/threads.h"

#include "mpv_talloc.h"

#include "common/common.h"
#include "common/global.h"
#include "options/m_property.h"
#include "common/msg.h"
#include "common/msg_control.h"
#include "common/stats.h"
#include "options/m_option.h"
#include "input/input.h"
#include "options/path.h"
#include "misc/bstr.h"
#include "misc/json.h"
#include "osdep/subprocess.h"
#include "osdep/timer.h"
#include "osdep/threads.h"
#include "pyerrors.h"
#include "stream/stream.h"
#include "sub/osd.h"
#include "core.h"
#include "command.h"
#include "client.h"
// #include "libmpv/client.h"
#include "ta/ta_talloc.h"

// List of builtin modules and their contents as strings.
// All these are generated from player/python/*.py
static const char *const builtin_files[][2] = {
    {"@/defaults.py",
#   include "player/python/defaults.py.inc"
    },
    {0}
};


PyTypeObject PyClientCtx_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "py_client_ctx",
    .tp_basicsize = sizeof(PyClientCtx),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "py client context object",
};


static PyObject *MpvError;

typedef struct {
    PyObject_HEAD
    char                **scripts;
    size_t              script_count;
    struct mpv_handle   *client;
    struct MPContext    *mpctx;
    struct mp_log       *log;
    struct stats_ctx    *stats;
    void                *ta_ctx;
    PyObject            *pympv_attr;
    PyObject            *pyclient;
    PyThreadState       *threadState;
    PyStatus            status;
} PyMpvObject;

static PyTypeObject PyMpv_Type;

#define PyCtxObject_Check(v)      Py_IS_TYPE(v, &PyMpv_Type)

static void
PyMpv_dealloc(PyMpvObject *self)
{
    PyObject_Free(self);
}


static PyObject *
setup(PyObject *self, PyObject *args)
{
    Py_RETURN_NONE;
}


static PyMethodDef PyMpv_methods[] = {
    {"setup", (PyCFunction)setup, METH_VARARGS,
     PyDoc_STR("Just a test method to see if extending is working.")},
    {NULL, NULL, 0, NULL}                                                 /* Sentinal */
};


static PyObject *
PyMpv_getattro(PyMpvObject *self, PyObject *name)
{
    if (self->pympv_attr != NULL) {
        PyObject *v = PyDict_GetItemWithError(self->pympv_attr, name);
        if (v != NULL) {
            Py_INCREF(v);
            return v;
        }
        else if (PyErr_Occurred()) {
            return NULL;
        }
    }
    return PyObject_GenericGetAttr((PyObject *)self, name);
}


static int
PyMpv_setattr(PyMpvObject *self, const char *name, PyObject *v)
{
    if (self->pympv_attr == NULL) {
        self->pympv_attr = PyDict_New();
        if (self->pympv_attr == NULL)
            return -1;
    }
    if (v == NULL) {
        int rv = PyDict_DelItemString(self->pympv_attr, name);
        if (rv < 0 && PyErr_ExceptionMatches(PyExc_KeyError))
            PyErr_SetString(PyExc_AttributeError,
                "delete non-existing PyMpv attribute");
        return rv;
    }
    else
        return PyDict_SetItemString(self->pympv_attr, name, v);
}


static PyTypeObject PyMpv_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "mpv.Mpv",
    .tp_basicsize = sizeof(PyMpvObject),
    .tp_dealloc = (destructor)PyMpv_dealloc,
    .tp_getattr = (getattrfunc)0,
    .tp_setattr = (setattrfunc)PyMpv_setattr,
    .tp_getattro = (getattrofunc)PyMpv_getattro,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_methods = PyMpv_methods,
};


/* ========================================================================== */

static PyObject *
load_local_pystrings(const char *string, char *module_name)
{
    PyObject *defaults = Py_CompileString(string, module_name, Py_file_input);
    if (defaults == NULL) {
        return NULL;
    }
    PyObject *defaults_mod = PyImport_ExecCodeModule(module_name, defaults);
    Py_DECREF(defaults);
    if (defaults_mod == NULL) {
        return NULL;
    }
    return defaults_mod;
}


static PyObject *
load_script(const char *script_name, PyObject *defaults, const char *client_name)
{
    PyObject *mpv = PyObject_GetAttrString(defaults, "mpv");

    const char **pathname = talloc(NULL, const char *);
    PyObject *args = PyObject_CallMethod(mpv, "compile_script", "s", script_name);
    if (args == NULL) {
        return NULL;
    }
    PyObject *client = PyTuple_GetItem(args, 1);
    *(const char **)pathname = talloc_strdup(pathname, PyUnicode_AsUTF8(PyTuple_GetItem(args, 0)));
    Py_INCREF(client);
    Py_DECREF(args);
    Py_DECREF(mpv);
    if (client == NULL) {
        Py_DECREF(client);
        talloc_free(pathname);
        return NULL;
    }
    PyObject *client_mod = PyImport_ExecCodeModuleEx(client_name, client, *pathname);
    Py_DECREF(client);
    talloc_free(pathname);
    if (client_mod == NULL) {
        return NULL;
    }
    return client_mod;
}

static void
end_interpreter(PyClientCtx *client_ctx)
{
    // PyErr_PrintEx(0);
    Py_EndInterpreter(client_ctx->threadState);
    talloc_free(client_ctx->ta_ctx);
}


static int run_client(PyClientCtx *cctx)
{
    // extension module mpv
    PyObject *client_extension = PyImport_ImportModule("mpv");

    PyObject *filename = PyUnicode_DecodeFSDefault(cctx->filename);
    PyModule_AddObject(client_extension, "filename", filename);

    if (PyModule_AddObject(client_extension, "context", (PyObject *)cctx) < 0) {
        MP_ERR(cctx, "%s.\n", "cound not set up context for the module mpv\n");
        // end_interpreter(cctx);
        return -1;
    };

    // defaults.py
    PyObject *defaults = load_local_pystrings(builtin_files[0][1], "mpvclient");

    if (defaults == NULL) {
        MP_ERR(cctx, "failed to load defaults (AKA. mpvclient) module.\n");
        // end_interpreter(cctx);
        return -1;
    }

    PyObject *client_name = PyObject_GetAttrString(defaults, "client_name");

    PyObject *os = PyImport_ImportModule("os");
    PyObject *path = PyObject_GetAttrString(os, "path");
    if (PyObject_CallMethod(path, "exists", "s", cctx->filename) == Py_False) {
        MP_ERR(cctx, "%s does not exists.\n", cctx->filename);
        // end_interpreter(cctx);
        return -1;
    }

    const char **cname = talloc(cctx->ta_ctx, const char *);
    *(const char **)cname = talloc_strdup(cname, PyUnicode_AsUTF8(client_name));
    PyObject *client = load_script(cctx->filename, defaults, *cname);

    Py_DECREF(client_extension);
    Py_DECREF(defaults);
    Py_DECREF(client_name);
    Py_DECREF(os);
    Py_DECREF(path);

    if (client == NULL) {
        MP_ERR(cctx, "could not load client. discarding: %s.\n", *cname);
        // end_interpreter(cctx);
        return -1;
    }

    if (PyObject_HasAttrString(client, "mpv") == 0) {
        MP_ERR(cctx, "illegal client. does not have an 'mpv' instance (use: from mpvclient import mpv). discarding: %s.\n", *cname);
        // end_interpreter(cctx);
        return -1;
    }

    PyObject *mpv = PyObject_GetAttrString(client, "mpv");
    PyObject_CallMethod(mpv, "run", NULL);

    // end_interpreter(cctx);
    // PyThreadState *threadState = PyThreadState_Swap(NULL);
    // PyThreadState_Swap(threadState);
    // Py_EndInterpreter(threadState);
    return 0;
}


/************************************************************************************************/
static int s_load_python(struct mp_script_args *args)
{
    int ret = 0;

    PyInterpreterConfig config = {
        .use_main_obmalloc = 0,
        .allow_fork = 0,
        .allow_exec = 0,
        .allow_threads = 1,
        .allow_daemon_threads = 0,
        .check_multi_interp_extensions = 1,
        .gil = PyInterpreterConfig_OWN_GIL,
    };
    PyThreadState *threadState = NULL;
    Py_NewInterpreterFromConfig(&threadState, &config);

    PyThreadState_Swap(threadState);

    PyClientCtx *ctx = PyObject_New(PyClientCtx, &PyClientCtx_Type);
    ctx->filename = args->filename;
    ctx->path = args->path;
    ctx->client = args->client;
    ctx->mpctx = args->mpctx;
    ctx->log = args->log;
    ctx->ta_ctx = talloc_new(NULL);

    ret = run_client(ctx);
    return ret;
}


// main export of this file
const struct mp_scripting mp_scripting_py = {
    .name = "python",
    .file_ext = "py",
    .load = s_load_python,
};
