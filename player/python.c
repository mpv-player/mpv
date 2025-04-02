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

#include "common/msg.h"
#include "core.h"
#include "client.h"
#include "ta/ta_talloc.h"

// List of builtin modules and their contents as strings.
// All these are generated from player/python/*.py
static const char *const builtin_files[][2] = {
    {"@/defaults.py",  // internal_name: mpvclient
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
    PyObject *args = PyObject_CallMethod(mpv, "compile_script", "ss", script_name, client_name);
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
    talloc_free(client_ctx->ta_ctx);
    Py_EndInterpreter(client_ctx->threadState);
}


static int run_client(PyClientCtx *cctx)
{
    PyObject *defaults = load_local_pystrings(builtin_files[0][1], "mpvclient");

    if (defaults == NULL) {
        PyErr_PrintEx(1);
        MP_ERR(cctx, "Failed to load defaults (AKA. mpvclient) module.\n");
        end_interpreter(cctx);
        return -1;
    }

    PyObject *os = PyImport_ImportModule("os");
    PyObject *path = PyObject_GetAttrString(os, "path");
    if (PyObject_CallMethod(path, "exists", "s", cctx->filename) == Py_False) {
        MP_ERR(cctx, "%s does not exists.\n", cctx->filename);
        end_interpreter(cctx);
        return 0;
    }

    PyObject *mpv = PyObject_GetAttrString(defaults, "mpv");
    PyObject_SetAttrString(mpv, "context", (PyObject *)cctx);

    const char *cname = mpv_client_name(cctx->client);
    PyObject *client = load_script(cctx->filename, defaults, cname);

    Py_DECREF(defaults);
    Py_DECREF(os);
    Py_DECREF(path);

    if (client == NULL) {
        PyErr_PrintEx(1);
        MP_ERR(cctx, "Could not load client. discarding: %s.\n", cname);
        end_interpreter(cctx);
        return 0;
    }

    if (PyObject_HasAttrString(client, "mpv") == 0) {
        MP_ERR(cctx, "illegal client. does not have an 'mpv' instance (use: from mpvclient import mpv). discarding: %s.\n", cname);
        end_interpreter(cctx);
        return 0;
    }

    PyObject *client_mpv = PyObject_GetAttrString(client, "mpv");
    PyObject *Mpv = PyObject_GetAttrString(defaults, "Mpv");

    int isins = PyObject_IsInstance(client_mpv, Mpv);
    if (isins == 0) {
        MP_ERR(cctx, "illegal client. 'mpv' instance is not an instance of mpvclient.Mpv (use: from mpvclient import mpv). discarding: %s.\n", cname);
        end_interpreter(cctx);
        return 0;
    } else if (isins == -1) {
        end_interpreter(cctx);
        return -1;
    }
    Py_DECREF(client_mpv);
    Py_DECREF(Mpv);

    PyObject_CallMethod(mpv, "run", NULL);

    end_interpreter(cctx);
    return 0;
}


/************************************************************************************************/
static int s_load_python(struct mp_script_args *args)
{
    int ret = 0;

    if (!args->mpctx->opts->enable_python) {
        MP_WARN(args, "%s\n", "Python has NOT been initialized. Be sure to set option 'enable-python' to 'yes'");
        return ret;
    }

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
    ctx->threadState = threadState;
    ctx->ta_ctx = talloc_new(NULL);

    ret = run_client(ctx);

    if (ret == -1) PyErr_PrintEx(1);

    return ret;
}


// main export of this file
const struct mp_scripting mp_scripting_py = {
    .name = "python",
    .file_ext = "py",
    .load = s_load_python,
};
