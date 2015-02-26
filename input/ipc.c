/*
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

#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include "config.h"

#include "osdep/io.h"
#include "osdep/threads.h"

#include "common/common.h"
#include "common/global.h"
#include "common/msg.h"
#include "input/input.h"
#include "libmpv/client.h"
#include "misc/bstr.h"
#include "misc/json.h"
#include "options/m_option.h"
#include "options/options.h"
#include "options/path.h"
#include "player/client.h"

struct mp_ipc_ctx {
    struct mp_log *log;
    struct mp_client_api *client_api;
    const char *path;

    pthread_t thread;
    int death_pipe[2];
};

struct client_arg {
    struct mp_log *log;
    struct mpv_handle *client;

    char *client_name;
    int client_fd;
    bool close_client_fd;

    bool writable;
};

static mpv_node *mpv_node_map_get(mpv_node *src, const char *key)
{
    if (src->format != MPV_FORMAT_NODE_MAP)
        return NULL;

    for (int i = 0; i < src->u.list->num; i++)
        if (!strcmp(key, src->u.list->keys[i]))
            return &src->u.list->values[i];

    return NULL;
}

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

static char *json_encode_event(mpv_event *event)
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
static char *json_execute_command(struct client_arg *arg, void *ta_parent,
                                  char *src)
{
    int rc;
    const char *cmd = NULL;

    mpv_node msg_node;
    mpv_node reply_node = {.format = MPV_FORMAT_NODE_MAP, .u.list = NULL};

    rc = json_parse(ta_parent, &msg_node, &src, 3);
    if (rc < 0) {
        MP_ERR(arg, "malformed JSON received\n");
        rc = MPV_ERROR_INVALID_PARAMETER;
        goto error;
    }

    if (msg_node.format != MPV_FORMAT_NODE_MAP) {
        rc = MPV_ERROR_INVALID_PARAMETER;
        goto error;
    }

    mpv_node *cmd_node = mpv_node_map_get(&msg_node, "command");
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
        const char *client_name = mpv_client_name(arg->client);
        mpv_node_map_add_string(ta_parent, &reply_node, "data", client_name);
        rc = MPV_ERROR_SUCCESS;
    } else if (!strcmp("get_time_us", cmd)) {
        int64_t time_us = mpv_get_time_us(arg->client);
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

        rc = mpv_get_property(arg->client, cmd_node->u.list->values[1].u.string,
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

        char *result = mpv_get_property_string(arg->client,
                                        cmd_node->u.list->values[1].u.string);
        if (!result) {
            mpv_node_map_add_null(ta_parent, &reply_node, "data");
        } else {
            mpv_node_map_add_string(ta_parent, &reply_node, "data", result);
            mpv_free(result);
        }
    } else if (!strcmp("set_property", cmd)) {
        if (cmd_node->u.list->num != 3) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        if (cmd_node->u.list->values[1].format != MPV_FORMAT_STRING) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        rc = mpv_set_property(arg->client, cmd_node->u.list->values[1].u.string,
                              MPV_FORMAT_NODE, &cmd_node->u.list->values[2]);
    } else if (!strcmp("set_property_string", cmd)) {
        if (cmd_node->u.list->num != 3) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        if (cmd_node->u.list->values[1].format != MPV_FORMAT_STRING) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        if (cmd_node->u.list->values[2].format != MPV_FORMAT_STRING) {
            rc = MPV_ERROR_INVALID_PARAMETER;
            goto error;
        }

        rc = mpv_set_property_string(arg->client,
                                     cmd_node->u.list->values[1].u.string,
                                     cmd_node->u.list->values[2].u.string);
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

        rc = mpv_observe_property(arg->client,
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

        rc = mpv_observe_property(arg->client,
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

        rc = mpv_unobserve_property(arg->client,
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

        rc = mpv_request_log_messages(arg->client,
                                      cmd_node->u.list->values[1].u.string);
    } else if (!strcmp("suspend", cmd)) {
        mpv_suspend(arg->client);
        rc = MPV_ERROR_SUCCESS;
    } else if (!strcmp("resume", cmd)) {
        mpv_resume(arg->client);
        rc = MPV_ERROR_SUCCESS;
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
                mpv_request_event(arg->client, n, enable);
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
            rc = mpv_request_event(arg->client, event, enable);
        }
    } else {
        mpv_node result_node;

        rc = mpv_command_node(arg->client, cmd_node, &result_node);
        if (rc >= 0)
            mpv_node_map_add(ta_parent, &reply_node, "data", &result_node);
    }

error:
    mpv_node_map_add_string(ta_parent, &reply_node, "error", mpv_error_string(rc));

    char *output = talloc_strdup(ta_parent, "");
    json_write(&output, &reply_node);
    output = ta_talloc_strdup_append(output, "\n");

    return output;
}

static char *text_execute_command(struct client_arg *arg, void *tmp, char *src)
{
    mpv_command_string(arg->client, src);

    return NULL;
}

static int ipc_write_str(struct client_arg *client, const char *buf)
{
    size_t count = strlen(buf);
    while (count > 0) {
        ssize_t rc = write(client->client_fd, buf, count);
        if (rc <= 0) {
            if (rc == 0)
                return -1;

            if (errno == EBADF) {
                client->writable = false;
                return 0;
            }

            if (errno == EINTR)
                continue;

            if (errno == EAGAIN)
                return 0;

            return rc;
        }

        count -= rc;
        buf   += rc;
    }

    return 0;
}

static void *client_thread(void *p)
{
    pthread_detach(pthread_self());

    int rc;

    struct client_arg *arg = p;
    bstr client_msg = { talloc_strdup(NULL, ""), 0 };

    mpthread_set_name(arg->client_name);

    int pipe_fd = mpv_get_wakeup_pipe(arg->client);
    if (pipe_fd < 0) {
        MP_ERR(arg, "Could not get wakeup pipe\n");
        goto done;
    }

    MP_INFO(arg, "Client connected\n");

    struct pollfd fds[2] = {
        {.events = POLLIN, .fd = pipe_fd},
        {.events = POLLIN, .fd = arg->client_fd},
    };

    fcntl(arg->client_fd, F_SETFL, fcntl(arg->client_fd, F_GETFL, 0) | O_NONBLOCK);
    mpv_suspend(arg->client);

    while (1) {
        rc = poll(fds, 2, 0);
        if (rc == 0) {
            mpv_resume(arg->client);
            rc = poll(fds, 2, -1);
            mpv_suspend(arg->client);
        }
        if (rc < 0) {
            MP_ERR(arg, "Poll error\n");
            continue;
        }

        if (fds[0].revents & POLLIN) {
            char discard[100];
            read(pipe_fd, discard, sizeof(discard));

            while (1) {
                mpv_event *event = mpv_wait_event(arg->client, 0);

                if (event->event_id == MPV_EVENT_NONE)
                    break;

                if (event->event_id == MPV_EVENT_SHUTDOWN)
                    goto done;

                if (!arg->writable)
                    continue;

                char *event_msg = json_encode_event(event);
                if (!event_msg) {
                    MP_ERR(arg, "Encoding error\n");
                    goto done;
                }

                rc = ipc_write_str(arg, event_msg);
                talloc_free(event_msg);
                if (rc < 0) {
                    MP_ERR(arg, "Write error (%s)\n", mp_strerror(errno));
                    goto done;
                }
            }
        }

        if (fds[1].revents & (POLLIN | POLLHUP)) {
            while (1) {
                char buf[128];
                bstr append = { buf, 0 };

                ssize_t bytes = read(arg->client_fd, buf, sizeof(buf));
                if (bytes < 0) {
                    if (errno == EAGAIN)
                        break;

                    MP_ERR(arg, "Read error (%s)\n", mp_strerror(errno));
                    goto done;
                }

                if (bytes == 0) {
                    MP_INFO(arg, "Client disconnected\n");
                    goto done;
                }

                append.len = bytes;

                bstr_xappend(NULL, &client_msg, append);

                while (bstrchr(client_msg, '\n') != -1) {
                    void *tmp = talloc_new(NULL);
                    bstr rest;
                    bstr line = bstr_getline(client_msg, &rest);
                    char *line0 = bstrto0(tmp, line);
                    talloc_steal(tmp, client_msg.start);
                    client_msg = bstrdup(NULL, rest);

                    json_skip_whitespace(&line0);

                    char *reply_msg = NULL;
                    if (line0[0] == '\0' || line0[0] == '#') {
                        // skip
                    } else if (line0[0] == '{') {
                        reply_msg = json_execute_command(arg, tmp, line0);
                    } else {
                        reply_msg = text_execute_command(arg, tmp, line0);
                    }

                    if (reply_msg && arg->writable) {
                        rc = ipc_write_str(arg, reply_msg);
                        if (rc < 0) {
                            MP_ERR(arg, "Write error (%s)\n", mp_strerror(errno));
                            talloc_free(tmp);
                            goto done;
                        }
                    }

                    talloc_free(tmp);
                }
            }
        }
    }

done:
    if (client_msg.len > 0)
        MP_WARN(arg, "Ignoring unterminated command on disconnect.\n");
    talloc_free(client_msg.start);
    if (arg->close_client_fd)
        close(arg->client_fd);
    mpv_detach_destroy(arg->client);
    talloc_free(arg);
    return NULL;
}

static void ipc_start_client(struct mp_ipc_ctx *ctx, struct client_arg *client)
{
    client->client = mp_new_client(ctx->client_api, client->client_name),
    client->log    = mp_client_get_log(client->client);

    pthread_t client_thr;
    if (pthread_create(&client_thr, NULL, client_thread, client)) {
        mpv_detach_destroy(client->client);
        if (client->close_client_fd)
            close(client->client_fd);
        talloc_free(client);
    }
}

static void ipc_start_client_json(struct mp_ipc_ctx *ctx, int id, int fd)
{
    struct client_arg *client = talloc_ptrtype(NULL, client);
    *client = (struct client_arg){
        .client_name = talloc_asprintf(client, "ipc-%d", id),
        .client_fd   = fd,
        .close_client_fd = true,

        .writable = true,
    };

    ipc_start_client(ctx, client);
}

static void ipc_start_client_text(struct mp_ipc_ctx *ctx, const char *path)
{
    int mode = O_RDONLY;
    int client_fd = -1;
    bool close_client_fd = true;
    bool writable = false;

    if (strcmp(path, "/dev/stdin") == 0) { // for symmetry with Linux
        client_fd = STDIN_FILENO;
        close_client_fd = false;
    } else if (strncmp(path, "fd://", 5) == 0) {
        char *end = NULL;
        client_fd = strtol(path + 5, &end, 0);
        if (!end || end == path + 5 || end[0]) {
            MP_ERR(ctx, "Invalid FD: %s\n", path);
            return;
        }
        close_client_fd = false;
        writable = true; // maybe
    } else {
        // Use RDWR for FIFOs to ensure they stay open over multiple accesses.
        struct stat st;
        if (stat(path, &st) == 0 && S_ISFIFO(st.st_mode))
            mode = O_RDWR;
        client_fd = open(path, mode);
    }
    if (client_fd < 0) {
        MP_ERR(ctx, "Could not open '%s'\n", path);
        return;
    }

    struct client_arg *client = talloc_ptrtype(NULL, client);
    *client = (struct client_arg){
        .client_name = "input-file",
        .client_fd   = client_fd,
        .close_client_fd = close_client_fd,
        .writable = writable,
    };

    ipc_start_client(ctx, client);
}

static void *ipc_thread(void *p)
{
    int rc;

    int ipc_fd;
    struct sockaddr_un ipc_un;

    struct mp_ipc_ctx *arg = p;

    mpthread_set_name("ipc socket listener");

    MP_INFO(arg, "Starting IPC master\n");

    ipc_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ipc_fd < 0) {
        MP_ERR(arg, "Could not create IPC socket\n");
        goto done;
    }

#if HAVE_FCHMOD
    fchmod(ipc_fd, 0600);
#endif

    size_t path_len = strlen(arg->path);
    if (path_len >= sizeof(ipc_un.sun_path) - 1) {
        MP_ERR(arg, "Could not create IPC socket\n");
        goto done;
    }

    ipc_un.sun_family = AF_UNIX,
    strncpy(ipc_un.sun_path, arg->path, sizeof(ipc_un.sun_path));

    unlink(ipc_un.sun_path);

    if (ipc_un.sun_path[0] == '@') {
        ipc_un.sun_path[0] = '\0';
        path_len--;
    }

    size_t addr_len = offsetof(struct sockaddr_un, sun_path) + 1 + path_len;
    rc = bind(ipc_fd, (struct sockaddr *) &ipc_un, addr_len);
    if (rc < 0) {
        MP_ERR(arg, "Could not bind IPC socket\n");
        goto done;
    }

    rc = listen(ipc_fd, 10);
    if (rc < 0) {
        MP_ERR(arg, "Could not listen on IPC socket\n");
        goto done;
    }

    int client_num = 0;

    struct pollfd fds[2] = {
        {.events = POLLIN, .fd = arg->death_pipe[0]},
        {.events = POLLIN, .fd = ipc_fd},
    };

    while (1) {
        rc = poll(fds, 2, -1);
        if (rc < 0) {
            MP_ERR(arg, "Poll error\n");
            continue;
        }

        if (fds[0].revents & POLLIN)
            goto done;

        if (fds[1].revents & POLLIN) {
            int client_fd = accept(ipc_fd, NULL, NULL);
            if (client_fd < 0) {
                MP_ERR(arg, "Could not accept IPC client\n");
                goto done;
            }

            ipc_start_client_json(arg, client_num++, client_fd);
        }
    }

done:
    if (ipc_fd >= 0)
        close(ipc_fd);

    return NULL;
}

struct mp_ipc_ctx *mp_init_ipc(struct mp_client_api *client_api,
                               struct mpv_global *global)
{
    struct MPOpts *opts = global->opts;

    struct mp_ipc_ctx *arg = talloc_ptrtype(NULL, arg);
    *arg = (struct mp_ipc_ctx){
        .log        = mp_log_new(arg, global->log, "ipc"),
        .client_api = client_api,
        .path       = mp_get_user_path(arg, global, opts->ipc_path),
        .death_pipe = {-1, -1},
    };
    char *input_file = mp_get_user_path(arg, global, opts->input_file);

    if (input_file && *input_file)
        ipc_start_client_text(arg, input_file);

    if (!opts->ipc_path || !*opts->ipc_path)
        goto out;

    if (mp_make_wakeup_pipe(arg->death_pipe) < 0)
        goto out;

    if (pthread_create(&arg->thread, NULL, ipc_thread, arg))
        goto out;

    return arg;

out:
    close(arg->death_pipe[0]);
    close(arg->death_pipe[1]);
    talloc_free(arg);
    return NULL;
}

void mp_uninit_ipc(struct mp_ipc_ctx *arg)
{
    if (!arg)
        return;

    write(arg->death_pipe[1], &(char){0}, 1);
    pthread_join(arg->thread, NULL);

    close(arg->death_pipe[0]);
    close(arg->death_pipe[1]);
    talloc_free(arg);
}
