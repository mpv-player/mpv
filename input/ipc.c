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

#include "config.h"

#include "common/msg.h"
#include "input/input.h"
#include "misc/json.h"
#include "misc/node.h"
#include "options/m_option.h"
#include "options/options.h"
#include "options/path.h"
#include "player/client.h"

static mpv_node *mpv_node_array_get(mpv_node *src, int index)
{
    if (src->format != MPV_FORMAT_NODE_ARRAY)
        return NULL;

    if (src->u.list->num < (index + 1))
        return NULL;

    return &src->u.list->values[index];
}

static void mpv_node_array_add(void *ta_parent, mpv_node *src,  mpv_node *val)
{
    if (src->format != MPV_FORMAT_NODE_ARRAY)
        return;

    if (!src->u.list)
        src->u.list = talloc_zero(ta_parent, mpv_node_list);

    MP_TARRAY_GROW(src->u.list, src->u.list->values, src->u.list->num);

    static const struct m_option type = { .type = CONF_TYPE_NODE };
    m_option_get_node(&type, ta_parent, &src->u.list->values[src->u.list->num], val);

    src->u.list->num++;
}

static void mpv_node_array_add_string(void *ta_parent, mpv_node *src, const char *val)
{
    mpv_node val_node = {.format = MPV_FORMAT_STRING, .u.string = (char *)val};
    mpv_node_array_add(ta_parent, src, &val_node);
}

static void mpv_node_map_add(void *ta_parent, mpv_node *src, const char *key, mpv_node *val)
{
    if (src->format != MPV_FORMAT_NODE_MAP)
        return;

    if (!src->u.list)
        src->u.list = talloc_zero(ta_parent, mpv_node_list);

    MP_TARRAY_GROW(src->u.list, src->u.list->keys, src->u.list->num);
    MP_TARRAY_GROW(src->u.list, src->u.list->values, src->u.list->num);

    src->u.list->keys[src->u.list->num] = talloc_strdup(ta_parent, key);

    static const struct m_option type = { .type = CONF_TYPE_NODE };
    m_option_get_node(&type, ta_parent, &src->u.list->values[src->u.list->num], val);

    src->u.list->num++;
}

static void mpv_node_map_add_null(void *ta_parent, mpv_node *src, const char *key)
{
    mpv_node val_node = {.format = MPV_FORMAT_NONE};
    mpv_node_map_add(ta_parent, src, key, &val_node);
}

static void mpv_node_map_add_flag(void *ta_parent, mpv_node *src, const char *key, bool val)
{
    mpv_node val_node = {.format = MPV_FORMAT_FLAG, .u.flag = val};

    mpv_node_map_add(ta_parent, src, key, &val_node);
}

static void mpv_node_map_add_int64(void *ta_parent, mpv_node *src, const char *key, int64_t val)
{
    mpv_node val_node = {.format = MPV_FORMAT_INT64, .u.int64 = val};
    mpv_node_map_add(ta_parent, src, key, &val_node);
}

static void mpv_node_map_add_double(void *ta_parent, mpv_node *src, const char *key, double val)
{
    mpv_node val_node = {.format = MPV_FORMAT_DOUBLE, .u.double_ = val};
    mpv_node_map_add(ta_parent, src, key, &val_node);
}

static void mpv_node_map_add_string(void *ta_parent, mpv_node *src, const char *key, const char *val)
{
    mpv_node val_node = {.format = MPV_FORMAT_STRING, .u.string = (char*)val};
    mpv_node_map_add(ta_parent, src, key, &val_node);
}

static void mpv_event_to_node(void *ta_parent, mpv_event *event, mpv_node *dst)
{
    mpv_node_map_add_string(ta_parent, dst, "event", mpv_event_name(event->event_id));

    if (event->reply_userdata)
        mpv_node_map_add_int64(ta_parent, dst, "id", event->reply_userdata);

    if (event->error < 0)
        mpv_node_map_add_string(ta_parent, dst, "error", mpv_error_string(event->error));

    switch (event->event_id) {
    case MPV_EVENT_LOG_MESSAGE: {
        mpv_event_log_message *msg = event->data;

        mpv_node_map_add_string(ta_parent, dst, "prefix", msg->prefix);
        mpv_node_map_add_string(ta_parent, dst, "level",  msg->level);
        mpv_node_map_add_string(ta_parent, dst, "text",   msg->text);

        break;
    }

    case MPV_EVENT_CLIENT_MESSAGE: {
        mpv_event_client_message *msg = event->data;

        mpv_node args_node = {.format = MPV_FORMAT_NODE_ARRAY, .u.list = NULL};
        for (int n = 0; n < msg->num_args; n++)
            mpv_node_array_add_string(ta_parent, &args_node, msg->args[n]);
        mpv_node_map_add(ta_parent, dst, "args", &args_node);
        break;
    }

    case MPV_EVENT_PROPERTY_CHANGE: {
        mpv_event_property *prop = event->data;

        mpv_node_map_add_string(ta_parent, dst, "name", prop->name);

        switch (prop->format) {
        case MPV_FORMAT_NODE:
            mpv_node_map_add(ta_parent, dst, "data", prop->data);
            break;
        case MPV_FORMAT_DOUBLE:
            mpv_node_map_add_double(ta_parent, dst, "data", *(double *)prop->data);
            break;
        case MPV_FORMAT_FLAG:
            mpv_node_map_add_flag(ta_parent, dst, "data", *(int *)prop->data);
            break;
        case MPV_FORMAT_STRING:
            mpv_node_map_add_string(ta_parent, dst, "data", *(char **)prop->data);
            break;
        default:
            mpv_node_map_add_null(ta_parent, dst, "data");
        }
        break;
    }
    }
}

char *mp_json_encode_event(mpv_event *event)
{
    void *ta_parent = talloc_new(NULL);
    mpv_node event_node = {.format = MPV_FORMAT_NODE_MAP, .u.list = NULL};

    mpv_event_to_node(ta_parent, event, &event_node);

    char *output = talloc_strdup(NULL, "");
    json_write(&output, &event_node);
    output = ta_talloc_strdup_append(output, "\n");

    talloc_free(ta_parent);

    return output;
}

// Function is allowed to modify src[n].
static char *json_execute_command(struct mpv_handle *client, void *ta_parent,
                                  char *src)
{
    int rc;
    const char *cmd = NULL;
    struct mp_log *log = mp_client_get_log(client);

    mpv_node msg_node;
    mpv_node reply_node = {.format = MPV_FORMAT_NODE_MAP, .u.list = NULL};
    mpv_node *reqid_node = NULL;

    rc = json_parse(ta_parent, &msg_node, &src, 50);
    if (rc < 0) {
        mp_err(log, "malformed JSON received: '%s'\n", src);
        rc = MPV_ERROR_INVALID_PARAMETER;
        goto error;
    }

    if (msg_node.format != MPV_FORMAT_NODE_MAP) {
        rc = MPV_ERROR_INVALID_PARAMETER;
        goto error;
    }

    reqid_node = node_map_get(&msg_node, "request_id");

    mpv_node *cmd_node = node_map_get(&msg_node, "command");
    if (!cmd_node ||
        (cmd_node->format != MPV_FORMAT_NODE_ARRAY) ||
        !cmd_node->u.list->num)
    {
        rc = MPV_ERROR_INVALID_PARAMETER;
        goto error;
    }

    mpv_node *cmd_str_node = mpv_node_array_get(cmd_node, 0);
    if (!cmd_str_node || (cmd_str_node->format != MPV_FORMAT_STRING)) {
        rc = MPV_ERROR_INVALID_PARAMETER;
        goto error;
    }

    cmd = cmd_str_node->u.string;

    if (!strcmp("client_name", cmd)) {
        const char *client_name = mpv_client_name(client);
        mpv_node_map_add_string(ta_parent, &reply_node, "data", client_name);
        rc = MPV_ERROR_SUCCESS;
    } else if (!strcmp("get_time_us", cmd)) {
        int64_t time_us = mpv_get_time_us(client);
        mpv_node_map_add_int64(ta_parent, &reply_node, "data", time_us);
        rc = MPV_ERROR_SUCCESS;
    } else if (!strcmp("get_version", cmd)) {
        int64_t ver = mpv_client_api_version();
        mpv_node_map_add_int64(ta_parent, &reply_node, "data", ver);
        rc = MPV_ERROR_SUCCESS;
    } else if (!strcmp("get_property", cmd)) {
        mpv_node result_node;

        if (cmd_node->u.list->num != 2) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        if (cmd_node->u.list->values[1].format != MPV_FORMAT_STRING) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        rc = mpv_get_property(client, cmd_node->u.list->values[1].u.string,
                              MPV_FORMAT_NODE, &result_node);
        if (rc >= 0) {
            mpv_node_map_add(ta_parent, &reply_node, "data", &result_node);
            mpv_free_node_contents(&result_node);
        }
    } else if (!strcmp("get_property_string", cmd)) {
        if (cmd_node->u.list->num != 2) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        if (cmd_node->u.list->values[1].format != MPV_FORMAT_STRING) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        char *result = mpv_get_property_string(client,
                                        cmd_node->u.list->values[1].u.string);
        if (result) {
            mpv_node_map_add_string(ta_parent, &reply_node, "data", result);
            mpv_free(result);
        } else {
            mpv_node_map_add_null(ta_parent, &reply_node, "data");
        }
    } else if (!strcmp("set_property", cmd) ||
        !strcmp("set_property_string", cmd))
    {
        if (cmd_node->u.list->num != 3) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        if (cmd_node->u.list->values[1].format != MPV_FORMAT_STRING) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        rc = mpv_set_property(client, cmd_node->u.list->values[1].u.string,
                              MPV_FORMAT_NODE, &cmd_node->u.list->values[2]);
    } else if (!strcmp("observe_property", cmd)) {
        if (cmd_node->u.list->num != 3) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        if (cmd_node->u.list->values[1].format != MPV_FORMAT_INT64) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        if (cmd_node->u.list->values[2].format != MPV_FORMAT_STRING) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        rc = mpv_observe_property(client,
                                  cmd_node->u.list->values[1].u.int64,
                                  cmd_node->u.list->values[2].u.string,
                                  MPV_FORMAT_NODE);
    } else if (!strcmp("observe_property_string", cmd)) {
        if (cmd_node->u.list->num != 3) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        if (cmd_node->u.list->values[1].format != MPV_FORMAT_INT64) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        if (cmd_node->u.list->values[2].format != MPV_FORMAT_STRING) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        rc = mpv_observe_property(client,
                                  cmd_node->u.list->values[1].u.int64,
                                  cmd_node->u.list->values[2].u.string,
                                  MPV_FORMAT_STRING);
    } else if (!strcmp("unobserve_property", cmd)) {
        if (cmd_node->u.list->num != 2) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        if (cmd_node->u.list->values[1].format != MPV_FORMAT_INT64) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        rc = mpv_unobserve_property(client,
                                    cmd_node->u.list->values[1].u.int64);
    } else if (!strcmp("request_log_messages", cmd)) {
        if (cmd_node->u.list->num != 2) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        if (cmd_node->u.list->values[1].format != MPV_FORMAT_STRING) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        rc = mpv_request_log_messages(client,
                                      cmd_node->u.list->values[1].u.string);
    } else if (!strcmp("enable_event", cmd) ||
               !strcmp("disable_event", cmd))
    {
        bool enable = !strcmp("enable_event", cmd);

        if (cmd_node->u.list->num != 2) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        if (cmd_node->u.list->values[1].format != MPV_FORMAT_STRING) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        char *name = cmd_node->u.list->values[1].u.string;
        if (strcmp(name, "all") == 0) {
            for (int n = 0; n < 64; n++)
                mpv_request_event(client, n, enable);
            rc = MPV_ERROR_SUCCESS;
        } else {
            int event = -1;
            for (int n = 0; n < 64; n++) {
                const char *evname = mpv_event_name(n);
                if (evname && strcmp(evname, name) == 0)
                    event = n;
            }
            if (event < 0) {
                rc = MPV_ERROR_INVALID_PARAMETER;
                goto error;
            }
            rc = mpv_request_event(client, event, enable);
        }
    } else {
        mpv_node result_node;

        rc = mpv_command_node(client, cmd_node, &result_node);
        if (rc >= 0)
            mpv_node_map_add(ta_parent, &reply_node, "data", &result_node);
    }

error:
    /* If the request contains a "request_id", copy it back into the response.
     * This makes it easier on the requester to match up the IPC results with
     * the original requests.
     */
    if (reqid_node) {
        mpv_node_map_add(ta_parent, &reply_node, "request_id", reqid_node);
    }

    mpv_node_map_add_string(ta_parent, &reply_node, "error", mpv_error_string(rc));

    char *output = talloc_strdup(ta_parent, "");
    json_write(&output, &reply_node);
    output = ta_talloc_strdup_append(output, "\n");

    return output;
}

static char *text_execute_command(struct mpv_handle *client, void *tmp, char *src)
{
    mpv_command_string(client, src);

    return NULL;
}

char *mp_ipc_consume_next_command(struct mpv_handle *client, void *ctx, bstr *buf)
{
    void *tmp = talloc_new(NULL);

    bstr rest;
    bstr line = bstr_getline(*buf, &rest);
    char *line0 = bstrto0(tmp, line);
    talloc_steal(tmp, buf->start);
    *buf = bstrdup(NULL, rest);

    json_skip_whitespace(&line0);

    char *reply_msg = NULL;
    if (line0[0] == '\0' || line0[0] == '#') {
        // skip
    } else if (line0[0] == '{') {
        reply_msg = json_execute_command(client, tmp, line0);
    } else {
        reply_msg = text_execute_command(client, tmp, line0);
    }

    talloc_steal(ctx, reply_msg);
    talloc_free(tmp);
    return reply_msg;
}
