#ifndef MPV_OSDEP_IPC_H
#define MPV_OSDEP_IPC_H

#define MPV_IPC_LOCAL 0
#define MPV_IPC_TCP 1

#define LISTENER_BACKLOG 20

#define BLOCK_SIZE 4096
#define MAX_RECV 0x80000 // 512 KiB

struct mpv_ipc_ctx;
struct mpv_ipc_client_ctx;

struct mpv_ipc_ctx {
    int protocol;
    void (*handler)(struct mpv_ipc_client_ctx *ipc_client);
    int socket;
    void *pipe;
    const char *local_path;
    char tcp_host[255];
    char tcp_port[5];
    const char *tcp_auth;
    unsigned int client_index;
    struct MPContext *mpctx;
};

struct mpv_ipc_client_ctx {
    struct mpv_ipc_ctx *ipc;
    int socket;
    struct mpv_handle *client;
};

void mpv_ipc_open(struct mpv_ipc_ctx *ipc);
void *mpv_ipc_handler(void *args);
int mpv_ipc_recv_until(struct mpv_ipc_client_ctx *ipc_client,
        void *buffer, long buffer_size, void *until, long until_size);

unsigned long (*mpv_ipc_recv)(struct mpv_ipc_client_ctx *ipc_client,
        void *buffer, long length);
unsigned long (*mpv_ipc_send)(struct mpv_ipc_client_ctx *ipc_client,
        const void *message, long length);
void (*mpv_ipc_exit)(struct mpv_ipc_client_ctx *ipc);
void (*mpv_ipc_close)(struct mpv_ipc_ctx *ipc);

#ifdef _WIN32
extern void *memmem(const void *l, size_t l_len, const void *s, size_t s_len);
#endif

#endif
