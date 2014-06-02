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
#include <winsock2.h>
#include <ws2tcpip.h>

#include "player/client.h"
#include "player/core.h"
#include "talloc.h"
#include "common/msg.h"
#include "osdep/threads.h"

#include "osdep/ipc.h"

static void *mpv_ipc_pipe_listener(void *args)
{
    pthread_detach(pthread_self());

    struct mpv_ipc_ctx *ipc = (struct mpv_ipc_ctx*)args;
    struct MPContext *mpctx = ipc->mpctx;

    while(1)
    {
        bool connected = ConnectNamedPipe(ipc->pipe, NULL);

        if (connected)
        {
            struct mpv_ipc_client_ctx *ipc_client = talloc(NULL,
                    struct mpv_ipc_client_ctx);

            char client_name[15];
            snprintf(client_name, sizeof(client_name), "ipc_%d",
                    ipc->client_index++);

            *ipc_client = (struct mpv_ipc_client_ctx) {
                .ipc = ipc,
                .client = mp_new_client(mpctx->clients, client_name)
            };

            pthread_t thread;
            if (pthread_create(&thread, NULL, mpv_ipc_handler,
                        (void*)ipc_client))
                talloc_free(ipc_client);
        }
    }

    return 0;
}

static unsigned long mpv_ipc_pipe_recv(struct mpv_ipc_client_ctx *ipc_client,
        void *buffer, long length)
{
    unsigned long bytes_read;
    ReadFile(ipc_client->ipc->pipe, buffer, length, &bytes_read, NULL);
    return bytes_read;
}

static unsigned long mpv_ipc_pipe_send(struct mpv_ipc_client_ctx *ipc_client,
        const void *message, long length)
{
    unsigned long bytes_written;
    WriteFile(ipc_client->ipc->pipe, message, length, &bytes_written, NULL);
    return bytes_written;
}

static void mpv_ipc_pipe_exit(struct mpv_ipc_client_ctx *ipc_client)
{
    FlushFileBuffers(ipc_client->ipc->pipe);
    DisconnectNamedPipe(ipc_client->ipc->pipe);
    mpv_destroy(ipc_client->client);
    talloc_free(ipc_client);
}

static void mpv_ipc_pipe_close(struct mpv_ipc_ctx *ipc)
{
    CloseHandle(ipc->pipe);
}

static void *mpv_ipc_socket_listener(void *args)
{
    pthread_detach(pthread_self());

    struct mpv_ipc_ctx *ipc = (struct mpv_ipc_ctx*)args;
    struct MPContext *mpctx = ipc->mpctx;

    int client_socket;
    struct sockaddr_storage client_addr;
    int sin_size = sizeof(client_addr);

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
    closesocket(ipc_client->socket);
    mpv_destroy(ipc_client->client);
    talloc_free(ipc_client);
}

static void mpv_ipc_socket_close(struct mpv_ipc_ctx *ipc)
{
    closesocket(ipc->socket);
    WSACleanup();
}

void mpv_ipc_open(struct mpv_ipc_ctx *ipc)
{
    struct MPContext *mpctx = ipc->mpctx;

    ipc->client_index = 0;

    if (ipc->protocol == MPV_IPC_LOCAL)
    {
        mpv_ipc_recv = &mpv_ipc_pipe_recv;
        mpv_ipc_send = &mpv_ipc_pipe_send;
        mpv_ipc_exit = &mpv_ipc_pipe_exit;
        mpv_ipc_close = &mpv_ipc_pipe_close;

        char *name = talloc_size(NULL, strlen(ipc->local_path) + 9);
        memset(name, 0, strlen(ipc->local_path) + 9);
        strcpy(name, "\\\\.\\pipe\\");
        strcat(name, ipc->local_path);

        ipc->pipe = CreateNamedPipe(name, PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES, BLOCK_SIZE, BLOCK_SIZE, 0, NULL);

        talloc_free(name);

        if (ipc->pipe != INVALID_HANDLE_VALUE)
        {
            pthread_t thread;
            pthread_create(&thread, NULL, mpv_ipc_pipe_listener,
                    (void*)ipc);
        }
        else
        {
            MP_ERR(mpctx, "ipc_local: CreateNamedPipe failed (%lu).\n",
                    GetLastError());
        }
    }

    else if (ipc->protocol == MPV_IPC_TCP)
    {
        mpv_ipc_recv = &mpv_ipc_socket_recv;
        mpv_ipc_send = &mpv_ipc_socket_send;
        mpv_ipc_exit = &mpv_ipc_socket_exit;
        mpv_ipc_close = &mpv_ipc_socket_close;

        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);

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

            const char yes = 1;
            setsockopt(ipc->socket, SOL_SOCKET, SO_REUSEADDR, &yes,
                    sizeof(char));

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

// Simple memmem since it's not in the windows' standard library:
void *memmem(const void *l, size_t l_len, const void *s, size_t s_len)
{
	register char *cur;
    register char *last;
	const char *cl = (const char*)l;
	const char *cs = (const char*)s;

	if (l_len == 0 || s_len == 0)
		return NULL;

	if (l_len < s_len)
		return NULL;

	if (s_len == 1)
		return memchr(l, (int)*cs, l_len);

	last = (char*)cl + l_len - s_len;

	for (cur = (char*)cl; cur <= last; cur++)
		if (cur[0] == cs[0] && memcmp(cur, cs, s_len) == 0)
			return cur;

	return NULL;
}
