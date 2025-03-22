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
#define PY_MPV_EXTENSION_MODULE
#include "py_extend.h"

#include "core.h"
#include "client.h"

#include "common/msg_control.h"
#include "input/input.h"
#include "options/path.h"
#include "ta/ta_talloc.h"


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


/**
 * A wrapper around deconstructnode.
 */
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


static void
print_parse_error(const char *msg)
{
    // PyErr_PrintEx(0);
    // PyErr_SetString(PyExc_Exception, msg);
    PyErr_PrintEx(1);
}


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


static PyClientCtx *
get_client_context(PyObject *module)
{
    PyClientCtx *cctx = (PyClientCtx *)PyObject_GetAttrString(module, "context");
    return cctx;
}


static PyObject *
py_mpv_extension_ok(PyObject *self, PyObject *args)
{
    Py_RETURN_TRUE;
}


static PyObject *
py_mpv_printEx(PyObject *self, PyObject *args) {
    PyErr_PrintEx(0);
    Py_RETURN_NONE;
}


static PyObject *
py_mpv_handle_log(PyObject *self, PyObject *args)
{
    PyObject *mpv;
    char **args_ = talloc_array(NULL, char *, 2);
    if (!PyArg_ParseTuple(args, "Oss", &mpv, &args_[0], &args_[1])) {
        print_parse_error("Failed to parse args (script_log)\n");
        Py_RETURN_NONE;
    }

    PyClientCtx *cctx = get_client_context(mpv);
    if (cctx == NULL) {
        PyErr_PrintEx(0);
        Py_RETURN_NONE;
    }
    struct mp_log *log = cctx->log;
    Py_DECREF(cctx);

    mp_msg(log, mp_msg_find_level(args_[0]), args_[1], NULL);
    talloc_free(args_);
    Py_RETURN_NONE;
}


/**
 * @param args tuple
 *              :param str filename:
 */
static PyObject *
py_mpv_find_config_file(PyObject *self, PyObject *args)
{
    const char **fname = talloc(NULL, const char *);
    PyObject *mpv;

    if (!PyArg_ParseTuple(args, "Os", &mpv, fname)) {
        talloc_free(fname);
        print_parse_error("Failed to parse args (find_config_file)\n");
        Py_RETURN_NONE;
    }

    PyClientCtx *cctx = get_client_context(mpv);

    char *path = mp_find_config_file(cctx->ta_ctx, cctx->mpctx->global, *fname);
    talloc_free(fname);
    Py_DECREF(cctx);
    if (path) {
        PyObject* ret =  PyUnicode_FromString(path);
        talloc_free(path);
        return ret;
    } else {
        talloc_free(path);
        PyErr_SetString(PyExc_FileNotFoundError, "Not found");
        Py_RETURN_NONE;
    }
}


static PyObject *
py_mpv_input_define_section(PyObject *self, PyObject *args)
{
    void *tctx = talloc_new(NULL);

    char **nlco = talloc_array(tctx, char *, 4);
    bool *builtin = talloc(tctx, bool);
    PyObject *mpv;
    if (!PyArg_ParseTuple(args, "Osssps", &mpv, &nlco[0], &nlco[1], &nlco[2], builtin, &nlco[3])) {
        talloc_free(tctx);
        print_parse_error("Failed to parse args (mpv.mpv_input_define_section)\n");
        Py_RETURN_NONE;
    }

    PyClientCtx *cctx = get_client_context(mpv);

    mp_input_define_section(cctx->mpctx->input, nlco[0], nlco[1], nlco[2], *builtin, nlco[3]);
    talloc_free(tctx);
    Py_DECREF(cctx);
    Py_RETURN_NONE;
}


static PyObject *
py_mpv_input_enable_section(PyObject *self, PyObject *args)
{
    void *tctx = talloc_new(NULL);
    char **name = talloc(tctx, char *);
    int *flags = talloc(tctx, int);
    PyObject *mpv;

    if (!PyArg_ParseTuple(args, "Osi", &mpv, name, flags)) {
        talloc_free(tctx);
        print_parse_error("Failed to parse args (mpv.mpv_input_enable_section)\n");
        Py_RETURN_NONE;
    }

    PyClientCtx *cctx = get_client_context(mpv);

    mp_input_enable_section(cctx->mpctx->input, *name, *flags);
    talloc_free(tctx);
    Py_DECREF(cctx);
    Py_RETURN_NONE;
}

/**
 * Enables log messages
 * @param args tuple
 *              :param str log_level:
 */
static PyObject *
py_mpv_enable_messages(PyObject *self, PyObject *args)
{
    const char *level;
    PyObject *mpv;

    if (!PyArg_ParseTuple(args, "Os", &mpv, &level)) {
        print_parse_error("Failed to parse args (mpv.enable_messages)\n");
        Py_RETURN_NONE;
    }

    PyClientCtx *cctx = get_client_context(mpv);

    int res = mpv_request_log_messages(cctx->client, level);
    Py_DECREF(cctx);
    if (res == MPV_ERROR_INVALID_PARAMETER) {
        PyErr_SetString(PyExc_Exception, "Invalid Log Error");
        Py_RETURN_NONE;
    }
    return check_error(res);
}


/**
 * @param args tuple
 *             :param str property_name:
 *             :param int mpv_format:
 */
static PyObject *
py_mpv_get_property(PyObject *self, PyObject *args)
{
    const char **name = talloc(NULL, const char *);
    mpv_format *format = talloc(NULL, mpv_format);
    PyObject *mpv;

    if (!PyArg_ParseTuple(args, "Osi", &mpv, name, format)) {
        talloc_free(name);
        talloc_free(format);
        print_parse_error("Failed to parse args (mpv.get_property)\n");
        Py_RETURN_NONE;
    }

    PyClientCtx *cctx = get_client_context(mpv);

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
 *             :param typing.Any data:
 */
static PyObject *
py_mpv_set_property(PyObject *self, PyObject *args)
{
    void *tctx = talloc_new(NULL);
    char **name = talloc(tctx, char *);
    mpv_format *format = talloc(tctx, mpv_format);
    PyObject *value;
    PyObject *mpv;

    if (!PyArg_ParseTuple(args, "OsiO", &mpv, name, format, &value)) {
        talloc_free(tctx);
        print_parse_error("Failed to parse args (mpv.set_property)\n");
        Py_RETURN_NONE;
    }

    PyClientCtx *cctx = get_client_context(mpv);

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


/**
 * @param args tuple
 *              :param str property_name:
 */
static PyObject *
py_mpv_del_property(PyObject *self, PyObject *args)
{
    const char **p = talloc(NULL, const char *);
    PyObject *mpv;

    if (!PyArg_ParseTuple(args, "Os", &mpv, p)) {
        talloc_free(p);
        print_parse_error("Failed to parse args (mpv.del_property)\n");
        Py_RETURN_NONE;
    }

    PyClientCtx *cctx = get_client_context(mpv);

    int res = mpv_del_property(cctx->client, *p);

    talloc_free(p);
    Py_DECREF(cctx);
    return check_error(res);
}


/**
 * @param args tuple
 *             :param str property_name:
 *             :param int mpv_format:
 *             :param int reply_userdata:
 */
static PyObject *
py_mpv_observe_property(PyObject *self, PyObject *args)
{
    void *tctx = talloc_new(NULL);
    const char **name = talloc(tctx, const char *);
    mpv_format *format = talloc(tctx, mpv_format);
    uint64_t *reply_userdata = talloc(tctx, uint64_t);
    PyObject *mpv;

    if (!PyArg_ParseTuple(args, "OKsi", &mpv, reply_userdata, name, format)) {
        talloc_free(tctx);
        print_parse_error("Failed to parse args (mpv.observe_property)\n");
        Py_RETURN_NONE;
    }

    PyClientCtx *ctx = get_client_context(mpv);

    int err = mpv_observe_property(ctx->client, *reply_userdata, *name, *format);
    talloc_free(tctx);
    Py_DECREF(ctx);
    return check_error(err);
}


static PyObject *
py_mpv_unobserve_property(PyObject *self, PyObject *args)
{
    uint64_t *reply_userdata = talloc(NULL, uint64_t);
    PyObject *mpv;

    if (!PyArg_ParseTuple(args, "OK", &mpv, reply_userdata)) {
        talloc_free(reply_userdata);
        print_parse_error("Failed to parse args (mpv.unobserve_property)\n");
        Py_RETURN_NONE;
    }

    PyClientCtx *ctx = get_client_context(mpv);

    int err = mpv_unobserve_property(ctx->client, *reply_userdata);

    talloc_free(reply_userdata);
    Py_DECREF(ctx);

    return check_error(err);
}


static PyObject *
py_mpv_command(PyObject *self, PyObject *args)
{
    PyClientCtx *cctx = get_client_context(PyTuple_GetItem(args, 0));
    struct mpv_node *cmd = NULL;
    makenode(cctx->ta_ctx, PyTuple_GetItem(args, 1), cmd);
    struct mpv_node *result = talloc(cctx->ta_ctx, struct mpv_node);
    if (!PyObject_IsTrue(check_error(mpv_command_node(cctx->client, cmd, result)))) {
        mp_msg(cctx->log, mp_msg_find_level("error"), "failed to run node command\n");
        Py_DECREF(cctx);
        Py_RETURN_NONE;
    }
    Py_DECREF(cctx);
    return deconstructnode(result);
}


/**
 * @param args tuple
 *              :param PyObject* mpv:
 *              :param str *args:
 */
static PyObject *
py_mpv_commandv(PyObject *self, PyObject *args)
{
    Py_ssize_t arg_length = PyTuple_Size(args);
    const char **argv = talloc_array(NULL, const char *, arg_length);
    for (Py_ssize_t i = 0; i < (arg_length - 1); i++) {
        argv[i] = talloc_strdup(argv, PyUnicode_AsUTF8(PyTuple_GetItem(args, i + 1)));
    }
    argv[arg_length - 1] = NULL;
    PyClientCtx *cctx = get_client_context(PyTuple_GetItem(args, 0));
    int ret = mpv_command(cctx->client, argv);
    Py_DECREF(cctx);
    talloc_free(argv);
    return check_error(ret);
}


// args: string
static PyObject *
py_mpv_command_string(PyObject *self, PyObject *args)
{
    const char **s = talloc(NULL, const char *);
    PyObject *mpv;

    if (!PyArg_ParseTuple(args, "Os", &mpv, s)) {
        talloc_free(s);
        print_parse_error("Failed to parse args (mpv.command_string)\n");
        Py_RETURN_NONE;
    }

    PyClientCtx *cctx = get_client_context(mpv);

    int res = mpv_command_string(cctx->client, *s);
    talloc_free(s);
    Py_DECREF(cctx);
    return check_error(res);
}


/**
 * @param args:
 *              :param int event_id:
 *              :param int enable:
 */
static PyObject *
py_mpv_request_event(PyObject *self, PyObject *args)
{
    int *args_ = talloc_array(NULL, int, 2);
    PyObject *mpv;

    if (!PyArg_ParseTuple(args, "Oii", &mpv, &args_[0], &args_[1])) {
        talloc_free(args_);
        print_parse_error("Failed to parse args (mpv.request_event)\n");
        Py_RETURN_NONE;
    }

    PyClientCtx *cctx = get_client_context(mpv);

    int err = mpv_request_event(cctx->client, args_[0], args_[1]);
    talloc_free(args_);
    Py_DECREF(cctx);

    return check_error(err);
}


/*
* args[0]: DEFAULT_TIMEOUT
* returns: PyLongObject *event_id, PyObject *data
*/
static PyObject *
py_mpv_wait_event(PyObject *self, PyObject *args)
{
    int *timeout = talloc(NULL, int);
    PyObject *mpv;

    if (!PyArg_ParseTuple(args, "Oi", &mpv, timeout)) {
        talloc_free(timeout);
        print_parse_error("Failed to parse args (mpv.wait_event)\n");
        Py_RETURN_NONE;
    }

    PyClientCtx *cctx = get_client_context(mpv);

    mpv_event *event = mpv_wait_event(cctx->client, *timeout);

    talloc_free(timeout);
    Py_DECREF(cctx);

    PyObject *ret = PyTuple_New(2);
    PyTuple_SetItem(ret, 0, PyLong_FromLong(event->event_id));
    if (event->event_id == MPV_EVENT_CLIENT_MESSAGE) {
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


static PyObject *py_mpv_run_event_loop(PyObject *self, PyObject *args)
{
    PyObject *mpv = PyTuple_GetItem(args, 0);
    PyClientCtx *cctx = get_client_context(mpv);
    mpv_handle *client = cctx->client;
    Py_DECREF(cctx);

    while (true) {
        mpv_event *event = mpv_wait_event(client, -1);
        if (event->event_id == MPV_EVENT_SHUTDOWN) {
            break;
        } else {
            PyObject *event_data = PyTuple_New(2);
            PyTuple_SetItem(event_data, 0, PyLong_FromLong(event->event_id));
            if (event->event_id == MPV_EVENT_CLIENT_MESSAGE) {
                mpv_event_client_message *m = (mpv_event_client_message *)event->data;
                PyObject *data = PyTuple_New(m->num_args);
                for (int i = 0; i < m->num_args; i++) {
                    PyTuple_SetItem(data, i, PyUnicode_DecodeFSDefault(m->args[i]));
                }
                PyTuple_SetItem(event_data, 1, data);
            } else if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
                mpv_event_property *p = (mpv_event_property *)event->data;
                PyObject *data = PyTuple_New(2);
                PyTuple_SetItem(data, 0, PyUnicode_DecodeFSDefault(p->name));
                PyTuple_SetItem(data, 1, unmakedata(p->format, p->data));
                PyTuple_SetItem(event_data, 1, data);
            } else PyTuple_SetItem(event_data, 1, Py_None);

            PyObject *handler = PyObject_GetAttrString(mpv, "handle_event");
            if (PyObject_CallOneArg(handler, event_data) == Py_True)
                break;
            Py_DECREF(event_data);
            Py_DECREF(handler);
        }
    }
    Py_RETURN_NONE;
}


static PyMethodDef py_mpv_methods[] = {
    {"extension_ok", (PyCFunction)py_mpv_extension_ok, METH_VARARGS,             /* METH_VARARGS | METH_KEYWORDS (PyObject *self, PyObject *args, PyObject **kwargs) */
     PyDoc_STR("Just a test method to see if extending is working.")},
    {"run_event_loop", (PyCFunction)py_mpv_run_event_loop, METH_VARARGS,
     PyDoc_STR("mpv holds here to listen for events.")},
    {"handle_log", (PyCFunction)py_mpv_handle_log, METH_VARARGS,
     PyDoc_STR("handles log records emitted from python thread.")},
    {"printEx", (PyCFunction)py_mpv_printEx, METH_VARARGS,
     PyDoc_STR("")},
    {"find_config_file", (PyCFunction)py_mpv_find_config_file, METH_VARARGS,
     PyDoc_STR("")},
    {"request_event", (PyCFunction)py_mpv_request_event, METH_VARARGS,
     PyDoc_STR("")},
    {"enable_messages", (PyCFunction)py_mpv_enable_messages, METH_VARARGS,
     PyDoc_STR("")},
    {"get_property", (PyCFunction)py_mpv_get_property, METH_VARARGS,
     PyDoc_STR("")},
    {"set_property", (PyCFunction)py_mpv_set_property, METH_VARARGS,
     PyDoc_STR("")},
    {"del_property", (PyCFunction)py_mpv_del_property, METH_VARARGS,
     PyDoc_STR("")},
    {"observe_property", (PyCFunction)py_mpv_observe_property, METH_VARARGS,
     PyDoc_STR("")},
    {"unobserve_property", (PyCFunction)py_mpv_unobserve_property, METH_VARARGS,
     PyDoc_STR("")},
    {"mpv_input_define_section", (PyCFunction)py_mpv_input_define_section, METH_VARARGS,
     PyDoc_STR("Given input section description, defines the input section.")},
    {"mpv_input_enable_section", (PyCFunction)py_mpv_input_enable_section, METH_VARARGS,
     PyDoc_STR("Given name of an input section, enables the input section.")},
    {"commandv", (PyCFunction)py_mpv_commandv, METH_VARARGS,
     PyDoc_STR("runs mpv_command given command name and args.")},
    {"command_string", (PyCFunction)py_mpv_command_string, METH_VARARGS,
     PyDoc_STR("runs mpv_command_string given a string as the only argument.")},
    {"command", (PyCFunction)py_mpv_command, METH_VARARGS,
     PyDoc_STR("runs mpv_command_node given py structure(s, as in list) convertible to mpv_node as the only argument.")},
    {"wait_event", (PyCFunction)py_mpv_wait_event, METH_VARARGS,
     PyDoc_STR("Listens for mpv_event and returns event_id and event_data")},
    {NULL, NULL, 0, NULL}                                                     /* Sentinel */
};


static PyTypeObject PyMpv_Type;
static PyObject *MpvError;


typedef struct {
    PyObject_HEAD

    PyObject *pympv_attr;
} PyMpvObject;


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
    {NULL, NULL, 0, NULL}                                                 /* Sentinel */
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


static int
py_mpv_exec(PyObject *m)
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


static struct PyModuleDef_Slot py_mpv_slots[] = {
    {Py_mod_exec, py_mpv_exec},
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
    {0, NULL}
};


static int py_mpv_m_clear(PyObject *self)
{
    PyObject_Free(self);
    return 0;
}


static struct PyModuleDef py_mpv_module_def = {
    PyModuleDef_HEAD_INIT,
    "mpv",
    PyDoc_STR("Extension module container (Python exposed) C function wrappers for controlling mpv (with Python)."),
    0,
    py_mpv_methods,
    py_mpv_slots,
    NULL,
    py_mpv_m_clear,
    NULL
};


PyMODINIT_FUNC PyInit_mpv(void)
{
    return PyModuleDef_Init(&py_mpv_module_def);
}
