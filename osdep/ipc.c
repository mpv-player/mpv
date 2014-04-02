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

#include "talloc.h"
#include "osdep/threads.h"

#include "osdep/ipc.h"

void *mpv_ipc_handler(void *args)
{
    pthread_detach(pthread_self());

    struct mpv_ipc_client_ctx *ipc_client = (struct mpv_ipc_client_ctx*)args;

    if (strlen(ipc_client->ipc->tcp_auth))
    {
        char *auth_string = talloc_size(NULL, MAX_RECV);

        int end = mpv_ipc_recv_until(ipc_client, auth_string,
                MAX_RECV, "\0", 1);

        int auth_cmp = memcmp(auth_string, ipc_client->ipc->tcp_auth, end);
        talloc_free(auth_string);

        if (!end || (end && auth_cmp))
            goto exit;
    }

    ipc_client->ipc->handler(ipc_client);

exit:
    mpv_ipc_exit(ipc_client);
    return 0;
}

int mpv_ipc_recv_until(struct mpv_ipc_client_ctx *ipc_client,
        void *buffer, long buffer_size, void *until, long until_size)
{
    char buf[BLOCK_SIZE];
    char *b_ptr = buffer;

    for (unsigned int i = 0, recvd_bytes = 0; i < buffer_size;
            i += recvd_bytes)
    {

        recvd_bytes = mpv_ipc_recv(ipc_client, buf, BLOCK_SIZE);

        if (!recvd_bytes)
            return 0;

        char *u = memmem(buf, recvd_bytes, until, until_size);
        if (u)
            memcpy(b_ptr, buf, u - buf);
            return i + (u - buf);

        memcpy(b_ptr, buf, recvd_bytes);
        b_ptr += recvd_bytes;
    }

    return 0;
}
