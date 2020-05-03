#include "common/common.h"
#include "utils.h"

static const int pl_log_to_msg_lev[PL_LOG_ALL+1] = {
    [PL_LOG_FATAL] = MSGL_FATAL,
    [PL_LOG_ERR]   = MSGL_ERR,
    [PL_LOG_WARN]  = MSGL_WARN,
    [PL_LOG_INFO]  = MSGL_V,
    [PL_LOG_DEBUG] = MSGL_DEBUG,
    [PL_LOG_TRACE] = MSGL_TRACE,
};

static const enum pl_log_level msg_lev_to_pl_log[MSGL_MAX+1] = {
    [MSGL_FATAL]   = PL_LOG_FATAL,
    [MSGL_ERR]     = PL_LOG_ERR,
    [MSGL_WARN]    = PL_LOG_WARN,
    [MSGL_INFO]    = PL_LOG_WARN,
    [MSGL_STATUS]  = PL_LOG_WARN,
    [MSGL_V]       = PL_LOG_INFO,
    [MSGL_DEBUG]   = PL_LOG_DEBUG,
    [MSGL_TRACE]   = PL_LOG_TRACE,
    [MSGL_MAX]     = PL_LOG_ALL,
};

// translates log levels while probing
static const enum pl_log_level probing_map(enum pl_log_level level)
{
    switch (level) {
    case PL_LOG_FATAL:
    case PL_LOG_ERR:
    case PL_LOG_WARN:
        return PL_LOG_INFO;

    default:
        return level;
    }
}

static void log_cb(void *priv, enum pl_log_level level, const char *msg)
{
    struct mp_log *log = priv;
    mp_msg(log, pl_log_to_msg_lev[level], "%s\n", msg);
}

static void log_cb_probing(void *priv, enum pl_log_level level, const char *msg)
{
    struct mp_log *log = priv;
    mp_msg(log, pl_log_to_msg_lev[probing_map(level)], "%s\n", msg);
}

void mppl_ctx_set_log(struct pl_context *ctx, struct mp_log *log, bool probing)
{
    assert(log);

    pl_context_update(ctx, &(struct pl_context_params) {
        .log_cb      = probing ? log_cb_probing : log_cb,
        .log_level   = msg_lev_to_pl_log[mp_msg_level(log)],
        .log_priv    = log,
    });
}
