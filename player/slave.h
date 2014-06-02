#ifndef MPV_SLAVE_H
#define MPV_SLAVE_H

struct slave_opts {
    int protocol;
    char *path;
    char *host;
    int port;
    char *auth;
};

extern const struct m_sub_options slave_opts_conf;

struct MPContext;

void mpv_slave_enable(struct MPContext *mpctx);
void mpv_slave_disable(struct MPContext *mpctx);

#endif
