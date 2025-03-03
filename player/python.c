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
#   include "generated/player/python/defaults.py.inc"
    },
    {"@/mpv_main_event_loop.py",
#   include "generated/player/python/mpv_main_event_loop.py.inc"
    },
    {0}
};


// Represents the global state of the python clients
typedef struct {
    PyObject_HEAD

    char                **scripts;
    size_t              script_count;
    struct mpv_handle   *client;
    struct mpv_handle   **clients;
    struct MPContext    *mpctx;
    struct mp_log       *log;
    void                *ta_ctx;
    struct stats_ctx    *stats;
} PyScriptCtx;

static PyTypeObject PyScriptCtx_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "script_ctx",
    .tp_basicsize = sizeof(PyScriptCtx),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "py script context object",
};

typedef struct {
    PyObject_HEAD

    char                *script;
    struct mpv_handle   *client;
    struct mp_log       *log;
    PyScriptCtx         *ctx;
    PyThreadState       *threadState;
    size_t              client_index;
    void                *ta_ctx;
} PyClientCtx;

static PyTypeObject PyClientCtx_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "py_client_ctx",
    .tp_basicsize = sizeof(PyClientCtx),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "py client context object",
};


// prototypes
static void makenode(void *ta_ctx, PyObject *obj, struct mpv_node *node);
static PyObject *deconstructnode(struct mpv_node *node);
static PyObject *check_error(int res);
static void *fmt_talloc(void *ta_ctx, mpv_format format);
static PyObject *unmakedata(mpv_format format, void *data);
// static void makedata(void *ta_ctx, mpv_format format, PyObject *obj, void *data);

/*
* Separation of concern
* =====================
* * Get a list of all python scripts.
* * Initialize python in it's own thread, as a single client. (call Py_Initialize)
* * Run scripts in sub interpreters. (This is where the scripts are isolated as virtual clients)
* * Run an event loop on the the mainThread created from Py_Initialize.
* * Delegate event actions to the sub interpreters.
* * Destroy all sub interpreters on MPV_EVENT_SHUTDOWN
* * Shutdown python. (call Py_Finalize)
*/
// module and type def
/* ========================================================================== */

PyThreadState *mainThread;
PyObject *PyInit_mpv(void);
PyObject *PyInit_mpvmainloop(void);

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

PyMpvObject **clients;

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

static PyScriptCtx *
get_global_context(PyObject *module)
{
    PyScriptCtx *gctx = (PyScriptCtx *)PyObject_GetAttrString(module, "context");
    return gctx;
}

static PyClientCtx *
get_client_context(PyObject *module)
{
    PyClientCtx *cctx = (PyClientCtx *)PyObject_GetAttrString(module, "context");
    return cctx;
}

static void
print_parse_error(const char *msg)
{
    // PyErr_PrintEx(0);
    // PyErr_SetString(PyExc_Exception, msg);
    PyErr_PrintEx(1);
}

/*
* args[0]: DEFAULT_TIMEOUT
* returns: PyLongObject *event_id, PyObject *data
*/
static PyObject *
mpvclient_wait_event(PyObject *mpv, PyObject *args)
{
    PyClientCtx *cctx = get_client_context(mpv);
    int *timeout = talloc(NULL, int);
    if (!PyArg_ParseTuple(args, "i", timeout)) {
        Py_DECREF(cctx);
        talloc_free(timeout);
        print_parse_error("Failed to parse args (mpv.wait_event)\n");
        Py_RETURN_NONE;
    }
    mpv_event *event = mpv_wait_event(cctx->client, *timeout);
    talloc_free(timeout);
    Py_DECREF(cctx);
    PyObject *ret = PyTuple_New(2);
    PyTuple_SetItem(ret, 0, PyLong_FromLong(event->event_id));
    if (event->event_id == MPV_EVENT_CLIENT_MESSAGE) {
        MP_INFO(cctx, "some client message\n");
        mpv_event_client_message *m = (mpv_event_client_message *)event->data;
        PyObject *data = PyTuple_New(m->num_args);
        for (int i = 0; i < m->num_args; i++) {
            PyTuple_SetItem(data, i, PyUnicode_DecodeFSDefault(m->args[i]));
        }
        PyTuple_SetItem(ret, 1, data);
        return ret;
    } else if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
        mpv_event_property *p = (mpv_event_property *)event->data;
        PyObject *data = PyTuple_New(2);
        PyTuple_SetItem(data, 0, PyUnicode_DecodeFSDefault(p->name));
        PyTuple_SetItem(data, 1, unmakedata(p->format, p->data));
        PyTuple_SetItem(ret, 1, data);
        return ret;
    }
    PyTuple_SetItem(ret, 1, Py_None);
    return ret;
}


static PyObject *
mpv_extension_ok(PyObject *self, PyObject *args)
{
    Py_RETURN_TRUE;
}


// args: log level, varargs
static PyObject *script_log(struct mp_log *log, PyObject *args)
{
    char *level;
    char *msg;

    if (!PyArg_ParseTuple(args, "ss", &level, &msg)) {
        print_parse_error("Failed to parse args (script_log)\n");
        Py_RETURN_NONE;
    }

    mp_msg(log, mp_msg_find_level(level), msg, NULL);
    Py_RETURN_NONE;
}

static PyObject *
handle_log(PyObject *mpv, PyObject *args)
{
    PyClientCtx *cctx = get_client_context(mpv);
    struct mp_log *log = cctx->log;
    Py_DECREF(cctx);
    return script_log(log, args);
}

static PyObject *
printEx(PyObject *mpv, PyObject *args) {
    PyErr_PrintEx(0);
    Py_RETURN_NONE;
}

static PyObject *
command(PyObject *mpv, PyObject *args)
{
    PyClientCtx *cctx = get_client_context(mpv);
    struct mpv_node *cmd = NULL;
    makenode(cctx->ta_ctx, PyTuple_GetItem(args, 0), cmd);
    struct mpv_node *result = talloc(cctx->ta_ctx, struct mpv_node);
    if (!PyObject_IsTrue(check_error(mpv_command_node(cctx->client, cmd, result)))) {
        mp_msg(cctx->log, mp_msg_find_level("error"), "failed to run node command\n");
        Py_DECREF(cctx);
        Py_RETURN_NONE;
    }
    Py_DECREF(cctx);
    return deconstructnode(result);
}

// args: string
static PyObject *
command_string(PyObject* mpv, PyObject* args)
{
    PyClientCtx *cctx = get_client_context(mpv);

    const char **s = talloc(cctx->ta_ctx, const char *);

    if (!PyArg_ParseTuple(args, "s", s)) {
        talloc_free(s);
        Py_DECREF(cctx);
        print_parse_error("Failed to parse args (mpv.command_string)\n");
        Py_RETURN_NONE;
    }

    int res = mpv_command_string(cctx->client, *s);
    talloc_free(s);
    Py_DECREF(cctx);
    return check_error(res);
}

static PyObject *
commandv(PyObject *mpv, PyObject *args)
{
    Py_ssize_t arg_length = PyTuple_Size(args);
    const char **argv = talloc_array(NULL, const char *, arg_length + 1);
    for (Py_ssize_t i = 0; i < arg_length; i++) {
        argv[i] = talloc_strdup(argv, PyUnicode_AsUTF8(PyTuple_GetItem(args, i)));
    }
    argv[arg_length] = NULL;
    PyClientCtx *cctx = get_client_context(mpv);
    int ret = mpv_command(cctx->client, argv);
    Py_DECREF(cctx);
    talloc_free(argv);
    return check_error(ret);
}

// args: string -> string
static PyObject*
find_config_file(PyObject* mpv, PyObject* args)
{
    PyClientCtx *cctx = get_client_context(mpv);

    const char **fname = talloc(NULL, const char *);

    if (!PyArg_ParseTuple(args, "s", fname)) {
        talloc_free(fname);
        Py_DECREF(cctx);
        print_parse_error("Failed to parse args (find_config_file)\n");
        Py_RETURN_NONE;
    }

    char *path = mp_find_config_file(cctx->ta_ctx, cctx->ctx->mpctx->global, *fname);
    talloc_free(fname);
    Py_DECREF(cctx);
    if (path) {
        PyObject* ret =  PyUnicode_FromString(path);
        talloc_free(path);
        return ret;
    } else {
        talloc_free(path);
        PyErr_SetString(PyExc_FileNotFoundError, "Not found");
        return NULL;
    }
}

/**
 * @param args:
 *              :param int event_id:
 *              :param int enable:
 */
static PyObject *
request_event_(struct mpv_handle *client, PyObject *args)
{
    int *args_ = talloc_array(NULL, int, 2);
    if (!PyArg_ParseTuple(args, "ii", &args_[0], &args_[1])) {
        talloc_free(args_);
        print_parse_error("Failed to parse args (request_event_)\n");
        Py_RETURN_NONE;
    }

    int err = mpv_request_event(client, args_[0], args_[1]);
    talloc_free(args_);

    return check_error(err);
}

// args: int, int
static PyObject *
request_event_mpv(PyObject* mpv, PyObject* args)
{
    PyClientCtx *cctx = get_client_context(mpv);

    PyObject *ret = request_event_(cctx->client, args);
    Py_DECREF(cctx);

    return ret;
}

// args: string
static PyObject *
enable_messages(PyObject* mpv, PyObject* args)
{
    PyClientCtx *cctx = get_client_context(mpv);

    const char *level;

    if (!PyArg_ParseTuple(args, "s", &level)) {
        return NULL;
    }

    int res = mpv_request_log_messages(cctx->client, level);
    Py_DECREF(cctx);
    if (res == MPV_ERROR_INVALID_PARAMETER) {
        PyErr_SetString(PyExc_Exception, "Invalid Log Error");
        return NULL;
    }
    return check_error(res);
}


/**
 * @param args tuple
 *             :param str property_name:
 *             :param int mpv_format:
 *             :param typing.Any data:
 */
static PyObject *
set_property(PyObject* mpv, PyObject* args)
{
    PyClientCtx *cctx = get_client_context(mpv);
    void *tctx = talloc_new(cctx->ta_ctx);

    char **name = talloc(tctx, char *);
    mpv_format *format = talloc(tctx, mpv_format);
    PyObject *value;
    if (!PyArg_ParseTuple(args, "siO", name, format, &value)) {
        talloc_free(tctx);
        Py_DECREF(cctx);
        print_parse_error("Failed to parse args (mpv.set_property)\n");
        Py_RETURN_NONE;
    }

    int res;
    void *data = fmt_talloc(tctx, *format);

    switch (*format) {
        case MPV_FORMAT_STRING:
        case MPV_FORMAT_OSD_STRING:
            *(char **)data = talloc_strdup(data, PyUnicode_AsUTF8(value));
            break;
        case MPV_FORMAT_FLAG:
            *(int *)data = PyLong_AsLong(value);
            break;
        case MPV_FORMAT_INT64:
            *(int64_t *)data = PyLong_AsLongLong(value);
            break;
        case MPV_FORMAT_DOUBLE:
            *(double *)data = PyFloat_AsDouble(value);
            break;
        case MPV_FORMAT_NODE:
            makenode(tctx, value, (struct mpv_node *)data);
            break;
        default:
            // TODO: raise Exception
            talloc_free(tctx);
            Py_DECREF(cctx);
            Py_RETURN_NONE;
    }
    res = mpv_set_property(cctx->client, *name, *format, data);
    talloc_free(tctx);
    Py_DECREF(cctx);
    return check_error(res);
}


// args: string
static PyObject*
del_property(PyObject* mpv, PyObject* args)
{
    PyClientCtx *cctx = get_client_context(mpv);

    const char **p = talloc(cctx->ta_ctx, const char *);
    if (!PyArg_ParseTuple(args, "s", p)) {
        talloc_free(p);
        print_parse_error("Failed to parse args (mpv.del_property)\n");
        Py_RETURN_NONE;
    }

    int res = mpv_del_property(cctx->client, *p);

    talloc_free(p);
    Py_DECREF(cctx);
    return check_error(res);
}

/**
 * @param args tuple
 *             :param str property_name:
 *             :param int mpv_format:
 */
static PyObject *
get_property(PyObject* mpv, PyObject* args)
{
    PyClientCtx *cctx = get_client_context(mpv);
    const char **name = talloc(NULL, const char *);
    mpv_format *format = talloc(NULL, mpv_format);
    if (!PyArg_ParseTuple(args, "si", name, format)) {
        talloc_free(name);
        talloc_free(format);
        Py_DECREF(cctx);
        print_parse_error("Failed to parse args (mpv.get_property)\n");
        Py_RETURN_NONE;
    }

    if (*format == MPV_FORMAT_NONE) {
        talloc_free(name);
        talloc_free(format);
        Py_DECREF(cctx);
        Py_RETURN_NONE;
    }

    void *out = fmt_talloc(NULL, *format);
    int err;
    if (*format == MPV_FORMAT_STRING || *format == MPV_FORMAT_OSD_STRING) {
        err = mpv_get_property(cctx->client, *name, *format, &out);
    } else {
        err = mpv_get_property(cctx->client, *name, *format, out);
    }
    talloc_free(name);
    Py_DECREF(cctx);
    if (err >= 0) {
        PyObject *ret = unmakedata(*format, out);
        talloc_free(out);
        talloc_free(format);
        return ret;
    }
    talloc_free(out);
    talloc_free(format);
    return check_error(err);
}

/**
 * @param args tuple
 *             :param str property_name:
 *             :param int mpv_format:
 *             :param int reply_userdata:
 */
static PyObject *
observe_property(PyObject *mpv, PyObject *args)
{
    PyScriptCtx *ctx = get_global_context(mpv);
    void *tctx = talloc_new(ctx->ta_ctx);
    const char **name = talloc(tctx, const char *);
    mpv_format *format = talloc(tctx, mpv_format);
    uint64_t *reply_userdata = talloc(tctx, uint64_t);
    if (!PyArg_ParseTuple(args, "siK", name, format, reply_userdata)) {
        talloc_free(tctx);
        Py_DECREF(ctx);
        print_parse_error("Failed to parse args (mpv.observe_property)\n");
        Py_RETURN_NONE;
    }
    int err = mpv_observe_property(ctx->client, *reply_userdata, *name, *format);
    talloc_free(tctx);
    Py_DECREF(ctx);
    return check_error(err);
}

static PyObject *
unobserve_property(PyObject *mpv, PyObject *args)
{
    PyScriptCtx *ctx = get_global_context(mpv);
    uint64_t reply_userdata = 0;
    int err = mpv_unobserve_property(ctx->client, reply_userdata);
    Py_DECREF(ctx);
    return check_error(err);
}

static PyObject *
mpv_input_define_section(PyObject *mpv, PyObject *args)
{
    PyClientCtx *cctx = get_client_context(mpv);

    void *tctx = talloc_new(cctx->ta_ctx);

    char **nlco = talloc_array(tctx, char *, 4);
    bool *builtin = talloc(tctx, bool);
    if (!PyArg_ParseTuple(args, "sssps", &nlco[0], &nlco[1], &nlco[2], builtin, &nlco[3])) {
        talloc_free(tctx);
        Py_DECREF(cctx);
        print_parse_error("Failed to parse args (mpv.mpv_input_define_section)\n");
        Py_RETURN_NONE;
    }

    mp_input_define_section(cctx->ctx->mpctx->input, nlco[0], nlco[1], nlco[2], *builtin, nlco[3]);
    talloc_free(tctx);
    Py_DECREF(cctx);
    Py_RETURN_NONE;
}


static PyObject *
mpv_input_enable_section(PyObject *mpv, PyObject *args)
{
    PyClientCtx *cctx = get_client_context(mpv);
    void *tctx = talloc_new(cctx->ta_ctx);
    char **name = talloc(tctx, char *);
    int *flags = talloc(tctx, int);
    if (!PyArg_ParseTuple(args, "si", name, flags)) {
        talloc_free(tctx);
        Py_DECREF(cctx);
        print_parse_error("Failed to parse args (mpv.mpv_input_enable_section)\n");
        Py_RETURN_NONE;
    }
    mp_input_enable_section(cctx->ctx->mpctx->input, *name, *flags);
    talloc_free(tctx);
    Py_DECREF(cctx);
    Py_RETURN_NONE;
}


static PyMethodDef Mpv_methods[] = {
    {"extension_ok", (PyCFunction)mpv_extension_ok, METH_VARARGS,             /* METH_VARARGS | METH_KEYWORDS (PyObject *self, PyObject *args, PyObject **kwargs) */
     PyDoc_STR("Just a test method to see if extending is working.")},
    {"handle_log", (PyCFunction)handle_log, METH_VARARGS,
     PyDoc_STR("handles log records emitted from python thread.")},
    {"printEx", (PyCFunction)printEx, METH_VARARGS,
     PyDoc_STR("")},
    {"find_config_file", (PyCFunction)find_config_file, METH_VARARGS,
     PyDoc_STR("")},
    {"request_event", (PyCFunction)request_event_mpv, METH_VARARGS,
     PyDoc_STR("")},
    {"enable_messages", (PyCFunction)enable_messages, METH_VARARGS,
     PyDoc_STR("")},
    {"set_property", (PyCFunction)set_property, METH_VARARGS,
     PyDoc_STR("")},
    {"del_property", (PyCFunction)del_property, METH_VARARGS,
     PyDoc_STR("")},
    {"get_property", (PyCFunction)get_property, METH_VARARGS,
     PyDoc_STR("")},
    {"observe_property", (PyCFunction)observe_property, METH_VARARGS,
     PyDoc_STR("")},
    {"unobserve_property", (PyCFunction)unobserve_property, METH_VARARGS,
     PyDoc_STR("")},
    {"mpv_input_define_section", (PyCFunction)mpv_input_define_section, METH_VARARGS,
     PyDoc_STR("")},
    {"mpv_input_enable_section", (PyCFunction)mpv_input_enable_section, METH_VARARGS,
     PyDoc_STR("")},
    {"commandv", (PyCFunction)commandv, METH_VARARGS,
     PyDoc_STR("runs mpv_command given command name and args.")},
    {"command_string", (PyCFunction)command_string, METH_VARARGS,
     PyDoc_STR("runs mpv_command_string given a string as the only argument.")},
    {"command", (PyCFunction)command, METH_VARARGS,
     PyDoc_STR("runs mpv_command_node given py structure(s, as in list) convertible to mpv_node as the only argument.")},
    {"wait_event", (PyCFunction)mpvclient_wait_event, METH_VARARGS,
     PyDoc_STR("Listens for mpv_event and returns event_id and event_data")},
    {NULL, NULL, 0, NULL}                                                     /* Sentinal */
};


static int
pympv_exec(PyObject *m)
{
    if (PyType_Ready(&PyMpv_Type) < 0)
        return -1;

    if (MpvError == NULL) {
        MpvError = PyErr_NewException("mpv.error", NULL, NULL);
        if (MpvError == NULL)
            return -1;
    }
    int rc = PyModule_AddType(m, (PyTypeObject *)MpvError);
    if (rc < 0)
        return -1;

    if (PyModule_AddType(m, &PyMpv_Type) < 0)
        return -1;

    return 0;
}


static struct PyModuleDef_Slot pympv_slots[] = {
    {Py_mod_exec, pympv_exec},
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
    {0, NULL}
};


// mpv python module
static struct PyModuleDef mpv_module_def = {
    PyModuleDef_HEAD_INIT,
    "mpv",
    NULL,
    0,
    Mpv_methods,
    pympv_slots,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC PyInit_mpv(void)
{
    return PyModuleDef_Init(&mpv_module_def);
}

static PyMethodDef MpvMainLoop_methods[] = {

    {NULL, NULL, 0, NULL}
};


static int
mpvmainloop_exec(PyObject *m)
{
    if (PyType_Ready(&PyScriptCtx_Type) < 0)
        return -1;

    if (PyModule_AddType(m, &PyScriptCtx_Type) < 0)
        return -1;

    return 0;
}


static struct PyModuleDef_Slot mpvmainloop_slots[] = {
    {Py_mod_exec, mpvmainloop_exec},
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
    {0, NULL}
};


// mpv python module
static struct PyModuleDef mpv_main_loop_module_def = {
    PyModuleDef_HEAD_INIT,
    "mpvmainloop",
    NULL,
    0,
    MpvMainLoop_methods,
    mpvmainloop_slots,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC PyInit_mpvmainloop(void)
{
    return PyModuleDef_Init(&mpv_main_loop_module_def);
}


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
load_script(char *script_name, PyObject *defaults, const char *client_name)
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
        talloc_free(pathname);
        return NULL;
    }
    PyObject *client_mod = PyImport_ExecCodeModuleEx(client_name, client, *pathname);
    talloc_free(pathname);
    if (client_mod == NULL) {
        return NULL;
    }
    return client_mod;
}

static void
end_interpreter(PyClientCtx *client_ctx)
{
    PyErr_PrintEx(0);
    Py_EndInterpreter(client_ctx->threadState);
    talloc_free(client_ctx->ta_ctx);
    PyThreadState_Swap(NULL);
}

/**********************************************************************
 *  Main mp.* scripting APIs and helpers
 *********************************************************************/

static PyObject* check_error(int err)
{
    if (err >= 0) {
        Py_RETURN_TRUE;
    }
    const char *errstr = mpv_error_string(err);
    printf("%s\n", errstr);
    PyErr_SetString(PyExc_Exception, errstr);
    // PyErr_PrintEx(0); // clearing it out lets python to continue (or use: PyErr_Clear())
    Py_RETURN_NONE;
}

static void makenode(void *ta_ctx, PyObject *obj, struct mpv_node *node) {
    if (obj == Py_None) {
        node->format = MPV_FORMAT_NONE;
    }
    else if (PyBool_Check(obj)) {
        node->format = MPV_FORMAT_FLAG;
        node->u.flag = (int) PyObject_IsTrue(obj);
    }
    else if (PyLong_Check(obj)) {
        node->format = MPV_FORMAT_INT64;
        node->u.int64 = (int64_t) PyLong_AsLongLong(obj);
    }
    else if (PyFloat_Check(obj)) {
        node->format = MPV_FORMAT_DOUBLE;
        node->u.double_ = PyFloat_AsDouble(obj);
    }
    else if (PyUnicode_Check(obj)) {
        node->format = MPV_FORMAT_STRING;
        node->u.string = talloc_strdup(ta_ctx, (char *)PyUnicode_AsUTF8(obj));
    }
    else if (PyList_Check(obj)) {
        node->format = MPV_FORMAT_NODE_ARRAY;
        node->u.list = talloc(ta_ctx, struct mpv_node_list);
        int l = (int) PyList_Size(obj);
        node->u.list->num = l;
        node->u.list->keys = NULL;
        node->u.list->values = talloc_array(ta_ctx, struct mpv_node, l);
        for (int i = 0; i < l; i++) {
            PyObject *child = PyList_GetItem(obj, i);
            makenode(ta_ctx, child, &node->u.list->values[i]);
        }
    }
    else if (PyDict_Check(obj)) {
        node->format = MPV_FORMAT_NODE_MAP;
        node->u.list = talloc(ta_ctx, struct mpv_node_list);
        int l = (int) PyDict_Size(obj);
        node->u.list->num = l;
        node->u.list->keys = talloc_array(ta_ctx, char *, l);
        node->u.list->values = talloc_array(ta_ctx, struct mpv_node, l);

        PyObject *key, *value;
        Py_ssize_t pos = 0;
        while (PyDict_Next(obj, &pos, &key, &value)) {
            if (!PyUnicode_Check(key)) {
                PyErr_Format(PyExc_TypeError, "node keys must be 'str'");
            }
            int i = (int) pos;
            node->u.list->keys[i] = talloc_strdup(ta_ctx, (char *)PyUnicode_AsUTF8(key));
            makenode(ta_ctx, value, &node->u.list->values[i]);
        }
    }
}


static PyObject *
deconstructnode(struct mpv_node *node)
{
    if (node->format == MPV_FORMAT_NONE) {
        Py_RETURN_NONE;
    }
    else if (node->format == MPV_FORMAT_FLAG) {
        if (node->u.flag == 1) {
            Py_RETURN_TRUE;
        }
        Py_RETURN_FALSE;
    }
    else if (node->format == MPV_FORMAT_INT64) {
        return PyLong_FromLongLong(node->u.int64);
    }
    else if (node->format == MPV_FORMAT_DOUBLE) {
        return PyFloat_FromDouble(node->u.double_);
    }
    else if (node->format == MPV_FORMAT_STRING) {
        return PyUnicode_FromString(node->u.string);
    }
    else if (node->format == MPV_FORMAT_NODE_ARRAY) {
        PyObject *lnode = PyList_New(node->u.list->num);
        for (int i = 0; i < node->u.list->num; i++) {
            PyList_SetItem(lnode, i, deconstructnode(&node->u.list->values[i]));
        }
        return lnode;
    }
    else if (node->format == MPV_FORMAT_NODE_MAP) {
        PyObject *dnode = PyDict_New();
        for (int i = 0; i < node->u.list->num; i++) {
            PyDict_SetItemString(dnode, node->u.list->keys[i], deconstructnode(&node->u.list->values[i]));
        }
        return dnode;
    }
    Py_RETURN_NONE;
}

static void *
fmt_talloc(void *ta_ctx, mpv_format format)
{
    switch (format) {
        case MPV_FORMAT_STRING:
        case MPV_FORMAT_OSD_STRING:
            return talloc(ta_ctx, char *);
        case MPV_FORMAT_FLAG:
            return talloc(ta_ctx, int);
        case MPV_FORMAT_INT64:
            return talloc(ta_ctx, int64_t);
        case MPV_FORMAT_DOUBLE:
            return talloc(ta_ctx, double);
        case MPV_FORMAT_NODE:
            return talloc(ta_ctx, struct mpv_node);
        default:
            // TODO: raise Exception
            return NULL;
    }
}

static PyObject *
unmakedata(mpv_format format, void *data)
{
    PyObject *ret;
    switch (format) {
        case MPV_FORMAT_STRING:
        case MPV_FORMAT_OSD_STRING:
            ret = PyUnicode_DecodeFSDefault((char *)data);
            break;
        case MPV_FORMAT_FLAG:
            ret = PyLong_FromLong(*(int *)data);
            break;
        case MPV_FORMAT_INT64:
            ret = PyLong_FromLongLong(*(int64_t *)data);
            break;
        case MPV_FORMAT_DOUBLE:
            ret = PyFloat_FromDouble(*(double *)data);
            break;
        case MPV_FORMAT_NODE:
            ret = deconstructnode((struct mpv_node *)data);
            break;
        default:
            // TODO: raise Exception
            Py_RETURN_NONE;
    }
    return ret;
}

static MP_THREAD_VOID run_thread(void *p)
{
    PyClientCtx *cctx = p;
    PyScriptCtx *sctx = cctx->ctx;
    PyInterpreterConfig config = {
        .use_main_obmalloc = 0,
        .allow_fork = 0,
        .allow_exec = 0,
        .allow_threads = 1,
        .allow_daemon_threads = 0,
        .check_multi_interp_extensions = 1,
        .gil = PyInterpreterConfig_OWN_GIL,
    };
    Py_NewInterpreterFromConfig(&cctx->threadState, &config);

    PyThreadState_Swap(cctx->threadState);

    // extension module mpv
    PyObject *client_extension = PyImport_ImportModule("mpv");
    if (PyModule_AddObject(client_extension, "context", (PyObject *)cctx) < 0) {
        mp_msg(sctx->log, mp_msg_find_level("error"), "%s.\n", "cound not set up context for the module mpv\n");
        end_interpreter(cctx);
        MP_THREAD_RETURN();
    };

    PyObject *filename = PyUnicode_DecodeFSDefault(cctx->script);
    PyModule_AddObject(client_extension, "filename", filename);

    // defaults.py
    PyObject *defaults = load_local_pystrings(builtin_files[0][1], "mpvclient");

    if (defaults == NULL) {
        mp_msg(sctx->log, mp_msg_find_level("error"), "failed to load defaults (AKA. mpvclient) module.\n");
        end_interpreter(cctx);
        MP_THREAD_RETURN();
    }

    PyObject *client_name = PyObject_GetAttrString(defaults, "client_name");

    PyObject *os = PyImport_ImportModule("os");
    PyObject *path = PyObject_GetAttrString(os, "path");
    if (PyObject_CallMethod(path, "exists", "s", cctx->script) == Py_False) {
        mp_msg(sctx->log, mp_msg_find_level("error"), "%s does not exists.\n", cctx->script);
        end_interpreter(cctx);
        MP_THREAD_RETURN();
    }

    const char **cname = talloc(sctx->ta_ctx, const char *);
    *(const char **)cname = talloc_strdup(cname, PyUnicode_AsUTF8(client_name));
    PyObject *client = load_script(cctx->script, defaults, *cname);

    Py_DECREF(client_extension);
    Py_DECREF(defaults);
    Py_DECREF(client_name);
    Py_DECREF(os);
    Py_DECREF(path);

    if (client == NULL) {
        mp_msg(sctx->log, mp_msg_find_level("error"), "could not load client. discarding: %s.\n", *cname);
        end_interpreter(cctx);
        MP_THREAD_RETURN();
    }


    if (PyObject_HasAttrString(client, "mpv") == 0) {
        mp_msg(sctx->log, mp_msg_find_level("error"), "illegal client. does not have an 'mpv' instance (use: from mpvclient import mpv). discarding: %s.\n", *cname);
        end_interpreter(cctx);
        MP_THREAD_RETURN();
    }

    PyObject *mpv = PyObject_GetAttrString(client, "mpv");
    Py_DECREF(client);
    PyObject *index = PyLong_FromSize_t(cctx->client_index);
    PyObject_SetAttrString(mpv, "index", index);
    Py_DECREF(index);
    PyObject_CallMethod(mpv, "flush", NULL);

    PyObject_CallMethod(mpv, "run", NULL);

    end_interpreter(cctx);
    MP_THREAD_RETURN();
}


static int
init_python_clients(PyScriptCtx *sctx)
{
    for (size_t i = 0; i < sctx->script_count; i++) {
        PyClientCtx *cctx = PyObject_New(PyClientCtx, &PyClientCtx_Type);
        cctx->ctx = sctx;
        cctx->script = sctx->scripts[i];
        cctx->client_index = i;
        cctx->ta_ctx = talloc_new(sctx->ta_ctx);
        cctx->log = sctx->log;
        cctx->threadState = NULL;
        cctx->client = sctx->clients[i];
        MP_INFO(cctx, "cctx client_name: %s\n", mpv_client_name(cctx->client));
        MP_INFO(cctx, "sctx client_name: %s\n", mpv_client_name(sctx->client));
        pthread_t thread;
        pthread_create(&thread, NULL, run_thread, cctx);
    }

    while (true) {
        mpv_event *event = mpv_wait_event(sctx->client, -1);
        if (event->event_id == MPV_EVENT_SHUTDOWN) {
            break;
        } else if (event->event_id == MPV_EVENT_CLIENT_MESSAGE) {
            MP_INFO(sctx, "(python client) some client message\n");
        }
    }

    PyErr_PrintEx(0);
    Py_Finalize();
    return 0;
}


/************************************************************************************************/
// Main Entrypoint (We want only one call here.)
static int s_load_python(struct mp_script_args *args)
{
    int ret = 0;
    MP_INFO(args, "client_name[0]: %s\n", args->client_names[0]);
    if (args->script_count == 0)
        return ret;

    if (PyImport_AppendInittab("mpv", PyInit_mpv) == -1) {
        mp_msg(args->log, mp_msg_find_level("error"), "could not extend in-built modules table\n");
        return -1;
    }

    if (PyImport_AppendInittab("mpvmainloop", PyInit_mpvmainloop) == -1) {
        mp_msg(args->log, mp_msg_find_level("error"), "could not extend in-built modules table\n");
        return -1;
    }

    Py_Initialize();

    PyScriptCtx *ctx = PyObject_New(PyScriptCtx, &PyScriptCtx_Type);
    ctx->client = args->client;
    ctx->mpctx = args->mpctx;
    ctx->log = args->log;
    ctx->scripts = args->py_scripts;
    ctx->script_count = args->script_count;
    ctx->clients = args->clients;
    ctx->ta_ctx = talloc_new(NULL);

    ret = init_python_clients(ctx);

    // PyErr_PrintEx(0);
    // Py_Finalize(); // closes the sub interpreters, no need for doing it manually
    return 0;
}


// main export of this file, used by cplayer to load js scripts
const struct mp_scripting mp_scripting_py = {
    .name = "python",
    .file_ext = "py",
    .load = s_load_python,
};
