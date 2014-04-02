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

#include <ctype.h>

#include "core.h"
#include "talloc.h"
#include "common/msg.h"
#include "options/options.h"
#include "osdep/ipc.h"
#include "client.h"
#include "libmpv/client.h"

#include "slave.h"

#define MAX_COMMAND_LEN 0x40000
#define MAX_ARGS 2
#define MAX_ARG_LEN 0x10000

struct slave_handler_ctx {
    char *command;
    char *arg[MAX_ARGS];
    char *response;
};

#define COMMAND(C) (memcmp(command, C, strlen(C)) == 0)

static char *get_arg_ptr(char *command, int n)
{
    char *ws = 0;
    char *arg_ptr = command;
    for (unsigned long i = 0; i <= n; i++)
    {
        ws = strchr(arg_ptr, ' ');
        arg_ptr = ws + 1;
    }
    return arg_ptr;
}

#define GET_ARG_PTR(N) get_arg_ptr(command, N)

static bool arg(char *command, int n)
{
    bool is_arg = false;
    if (get_arg_ptr(command, n) - 1)
        is_arg = true;
    return is_arg;
}

#define ARG(N) arg(command, N)

static char *get_raw_arg(char *command, char *arg, int n)
{
    char *arg_ptr = get_arg_ptr(command, n);
    char *is_end = strchr(arg_ptr, ' ');
    if (!is_end)
        is_end = arg_ptr + strlen(arg_ptr);
    strncpy(arg, arg_ptr, is_end - arg_ptr);
    return arg;
}

#define CHAR_FROM_HEX(C) (isdigit(C) ? C - '0' : tolower(C) - 'a' + 10)

static char *decode_arg(struct slave_handler_ctx *ctx, char *arg, int argn)
{
    talloc_free(ctx->arg[argn]);
    ctx->arg[argn] = talloc_size(ctx, strlen(arg) + 1);
    char *a_ptr = arg;
    char *buf = ctx->arg[argn];
    char *b_ptr = buf;
    while (*a_ptr)
    {
        if (*a_ptr == '%')
        {
            if (a_ptr[1] && a_ptr[2])
            {
                *b_ptr++ = CHAR_FROM_HEX(a_ptr[1]) << 4 |
                    CHAR_FROM_HEX(a_ptr[2]);
                a_ptr += 2;
            }
        }
        else
        {
            *b_ptr++ = *a_ptr;
        }
        a_ptr++;
    }
    *b_ptr = '\0';

    return buf;
}

#define GET_ARG(N) ({ \
    char arg[MAX_ARG_LEN]; \
    memset(arg, 0, MAX_ARG_LEN); \
    decode_arg(ctx, get_raw_arg(command, arg, N), N); \
})

static void response(struct slave_handler_ctx *ctx,
        struct mpv_ipc_client_ctx *ipc_client, const char *message)
{
    ctx->response = talloc_size(ctx, strlen(message) + 2);
    strcpy(ctx->response, message);
    strcat(ctx->response, "\r\n");
    mpv_ipc_send(ipc_client, ctx->response, strlen(ctx->response));
    talloc_free(ctx->response);
}

#define RESPONSE(M) ({ \
    response(ctx, ipc_client, M); \
    continue; \
})

#define RESPONSE_ERROR_CODE(E) ({ \
    char error_string[11]; \
    sprintf(error_string, "%d", E); \
    RESPONSE(error_string); \
})

static void slave_handler(struct mpv_ipc_client_ctx *ipc_client)
{
    struct slave_handler_ctx *ctx = talloc(NULL, struct slave_handler_ctx);
    ctx->command = talloc_size(ctx, MAX_COMMAND_LEN);
    char *command = ctx->command;

    while (1)
    {
        memset(command, 0, MAX_COMMAND_LEN);

        int end = mpv_ipc_recv_until(ipc_client, command,
                MAX_RECV, "\r\n", 2);

        if (!end)
            break;

        // Client API:

        if (COMMAND("client_api_version"))
        {
            char client_api_version[15];
            sprintf(client_api_version, "0x%lX", mpv_client_api_version());
            RESPONSE(client_api_version);
        }

        if (COMMAND("error_string") && ARG(0))
            RESPONSE(mpv_error_string(atoi(GET_ARG(0))));

        if (COMMAND("client_name"))
            RESPONSE(mpv_client_name(ipc_client->client));

        if (COMMAND("suspend"))
        {
            mpv_suspend(ipc_client->client);
            RESPONSE("OK");
        }

        if (COMMAND("resume"))
        {
            mpv_resume(ipc_client->client);
            RESPONSE("OK");
        }

        if (COMMAND("get_time_us"))
        {
            char time_us[20];
            sprintf(time_us, "%lu", mpv_get_time_us(ipc_client->client));
            RESPONSE(time_us);
        }

        if (COMMAND("set_option") && ARG(0) && ARG(1))
        {
            char *name = GET_ARG(0);
            char *data = GET_ARG(1);

            int error = mpv_set_option_string(ipc_client->client, name, data);

            RESPONSE_ERROR_CODE(error);
        }

        if (COMMAND("command") && ARG(0))
        {
            char *args = GET_ARG(0);

            int error = mpv_command_string(ipc_client->client, args);

            RESPONSE_ERROR_CODE(error);
        }

        if (COMMAND("set_property") && ARG(0) && ARG(1))
        {
            char *name = GET_ARG(0);
            char *data = GET_ARG(1);

            int error = mpv_set_property_string(ipc_client->client, name,
                    data);

            RESPONSE_ERROR_CODE(error);
        }

        if (COMMAND("get_property") && ARG(0))
        {
            char *name = GET_ARG(0);

            char *property = mpv_get_property_string(ipc_client->client, name);

            if (property)
                RESPONSE(property);
            else
                RESPONSE("NULL");
        }

        RESPONSE("ERR invalid command");
    }

    talloc_free(ctx);
}

static const struct slave_opts slave_opts_def = {
    .protocol = 0,
    .path = ".mpv_slave",
    .host = "localhost",
    .port = 17771,
    .auth = "",
};

#define OPT_BASE_STRUCT struct slave_opts
const struct m_sub_options slave_opts_conf = {
    .opts = (m_option_t[]) {
        OPT_CHOICE("protocol", protocol, 0,
            ({"local", MPV_IPC_LOCAL},
             {"tcp", MPV_IPC_TCP})),
        OPT_STRING("path", path, 0),
        OPT_STRING("host", host, 0),
        OPT_INTRANGE("port", port, 0, 1, 65535),
        OPT_STRING("auth", auth, 0),
        {0}
    },
    .size = sizeof(struct slave_opts),
    .defaults = &slave_opts_def,
};

struct slave_ctx {
    struct MPContext *mpctx;
    struct mpv_ipc_ctx *ipc;
};

void mpv_slave_enable(struct MPContext *mpctx)
{
    struct slave_opts *opts = mpctx->opts->slave_opts;

    mpctx->slave_ctx = talloc(mpctx, struct slave_ctx);
    struct slave_ctx *ctx = mpctx->slave_ctx;
    ctx->ipc = talloc(ctx, struct mpv_ipc_ctx);
    struct mpv_ipc_ctx *ipc = ctx->ipc;
    ipc->mpctx = mpctx;

    if (opts->protocol == MPV_IPC_LOCAL)
    {
        ipc->protocol = MPV_IPC_LOCAL;
        ipc->local_path = opts->path;

        MP_INFO(mpctx, "Enabling slave on %s\n",
                ipc->local_path);
    }

    else if (opts->protocol == MPV_IPC_TCP)
    {
        ipc->protocol = MPV_IPC_TCP;

        strcpy(ipc->tcp_host, opts->host);

        char port[6];
        sprintf(port, "%u", opts->port);
        strcpy(ipc->tcp_port, port);

        MP_INFO(mpctx, "Enabling slave on %s:%s\n",
                ipc->tcp_host, ipc->tcp_port);
    }

    ipc->tcp_auth = opts->auth;

    ipc->handler = &slave_handler;

    mpv_ipc_open(ipc);

}

void mpv_slave_disable(struct MPContext *mpctx)
{
    struct slave_ctx *ctx = mpctx->slave_ctx;
    talloc_free(ctx);
    mpv_ipc_close(mpctx->slave_ctx->ipc);
}
