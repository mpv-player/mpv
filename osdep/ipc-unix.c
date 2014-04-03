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

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>

#include "player/core.h"
#include "player/client.h"
#include "talloc.h"
#include "common/msg.h"
#include "osdep/threads.h"

#include "osdep/ipc.h"

static void *mpv_ipc_socket_listener(void *args)
{
    pthread_detach(pthread_self());

    struct mpv_ipc_ctx *ipc = (struct mpv_ipc_ctx*)args;
    struct MPContext *mpctx = ipc->mpctx;

    int client_socket;
    struct sockaddr_storage client_addr;
    socklen_t sin_size = sizeof(client_addr);

    listen(ipc->socket, LISTENER_BACKLOG);

    while(1)
    {
        client_socket = accept(ipc->socket, (struct sockaddr*)&client_addr,
                &sin_size);

        struct mpv_ipc_client_ctx *ipc_client = talloc(NULL,
                struct mpv_ipc_client_ctx);

        char client_name[15];
        snprintf(client_name, sizeof(client_name), "ipc_%d",
                ipc->client_index++);

        *ipc_client = (struct mpv_ipc_client_ctx) {
            .ipc = ipc,
            .socket = client_socket,
            .client = mp_new_client(mpctx->clients, client_name)
        };

        pthread_t thread;
        if (pthread_create(&thread, NULL, mpv_ipc_handler, (void*)ipc_client))
            talloc_free(ipc_client);
    }

    return 0;
}

static unsigned long mpv_ipc_socket_recv(struct mpv_ipc_client_ctx *ipc_client,
        void *buffer, long length)
{
    return recv(ipc_client->socket, buffer, length, 0);
}

static unsigned long mpv_ipc_socket_send(struct mpv_ipc_client_ctx *ipc_client,
        const void *message, long length)
{
    return send(ipc_client->socket, message, length, 0);
}

static void mpv_ipc_socket_exit(struct mpv_ipc_client_ctx *ipc_client)
{
    close(ipc_client->socket);
    mpv_destroy(ipc_client->client);
    talloc_free(ipc_client);
}

static void mpv_ipc_socket_close(struct mpv_ipc_ctx *ipc)
{
    close(ipc->socket);
    if (ipc->protocol == MPV_IPC_LOCAL)
        unlink(ipc->local_path);
}

void mpv_ipc_open(struct mpv_ipc_ctx *ipc)
{
    struct MPContext *mpctx = ipc->mpctx;

    mpv_ipc_recv = &mpv_ipc_socket_recv;
    mpv_ipc_send = &mpv_ipc_socket_send;
    mpv_ipc_exit = &mpv_ipc_socket_exit;
    mpv_ipc_close = &mpv_ipc_socket_close;

    ipc->client_index = 0;

    if (ipc->protocol == MPV_IPC_LOCAL)
    {
        ipc->socket = socket(AF_UNIX, SOCK_STREAM, 0);

        struct sockaddr_un local;

        local.sun_family = AF_UNIX;
        strcpy(local.sun_path, ipc->local_path);
        unlink(local.sun_path);

        if (!bind(ipc->socket, (struct sockaddr*)&local,
                    strlen(local.sun_path) + sizeof(local.sun_family)))
        {
            pthread_t thread;
            pthread_create(&thread, NULL, mpv_ipc_socket_listener, (void*)ipc);
        }
        else
        {
            MP_ERR(mpctx, "ipc_local: bind error.\n");
        }
    }

    else if (ipc->protocol == MPV_IPC_TCP)
    {
        int status;
        struct addrinfo hints;
        struct addrinfo *res, *ai;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        if (!(status = getaddrinfo(ipc->tcp_host, ipc->tcp_port,
                    &hints, &res)))
        {
            for (ai = res; ai != NULL; ai = ai->ai_next)
                if (ai->ai_family == AF_INET)
                    break;

            ipc->socket = socket(ai->ai_family, ai->ai_socktype,
                    ai->ai_protocol);

            int yes = 1;
            setsockopt(ipc->socket, SOL_SOCKET, SO_REUSEADDR, &yes,
                    sizeof(int));

            if (!bind(ipc->socket, ai->ai_addr, ai->ai_addrlen))
            {
                pthread_t thread;
                pthread_create(&thread, NULL, mpv_ipc_socket_listener,
                        (void*)ipc);
            }
            else
            {
                MP_ERR(mpctx, "ipc_tcp: bind error.\n");
            }

            freeaddrinfo(ai);
        }
        else
        {
            MP_ERR(mpctx, "ipc_tcp: getaddrinfo error: %s\n",
                    gai_strerror(status));
        }
    }
}
