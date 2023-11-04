#include "common/common.h"
#include "utils.h"

#include <libplacebo/utils/dolbyvision.h>

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

pl_log mppl_log_create(void *tactx, struct mp_log *log)
{
    return pl_log_create(PL_API_VER, &(struct pl_log_params) {
        .log_cb     = log_cb,
        .log_level  = msg_lev_to_pl_log[mp_msg_level(log)],
        .log_priv   = mp_log_new(tactx, log, "libplacebo"),
    });
}

void mppl_log_set_probing(pl_log log, bool probing)
{
    struct pl_log_params params = log->params;
    params.log_cb = probing ? log_cb_probing : log_cb;
    pl_log_update(log, &params);
}

enum pl_alpha_mode mp_alpha_to_pl(enum mp_alpha_type alpha)
{
    switch (alpha) {
    case MP_ALPHA_AUTO:             return PL_ALPHA_UNKNOWN;
    case MP_ALPHA_STRAIGHT:         return PL_ALPHA_INDEPENDENT;
    case MP_ALPHA_PREMUL:           return PL_ALPHA_PREMULTIPLIED;
    }

    MP_ASSERT_UNREACHABLE();
}

enum pl_chroma_location mp_chroma_to_pl(enum mp_chroma_location chroma)
{
    switch (chroma) {
    case MP_CHROMA_AUTO:            return PL_CHROMA_UNKNOWN;
    case MP_CHROMA_TOPLEFT:         return PL_CHROMA_TOP_LEFT;
    case MP_CHROMA_LEFT:            return PL_CHROMA_LEFT;
    case MP_CHROMA_CENTER:          return PL_CHROMA_CENTER;
    case MP_CHROMA_COUNT:           return PL_CHROMA_COUNT;
    }

    MP_ASSERT_UNREACHABLE();
}

void mp_map_dovi_metadata_to_pl(struct mp_image *mpi,
                                struct pl_frame *frame)
{
#ifdef PL_HAVE_LAV_DOLBY_VISION
    if (mpi->dovi) {
        const AVDOVIMetadata *metadata = (AVDOVIMetadata *) mpi->dovi->data;
        const AVDOVIRpuDataHeader *header = av_dovi_get_header(metadata);

        if (header->disable_residual_flag) {
            // Only automatically map DoVi RPUs that don't require an EL
            struct pl_dovi_metadata *dovi = talloc_ptrtype(mpi, dovi);
            pl_frame_map_avdovi_metadata(frame, dovi, metadata);
        }
    }

#if defined(PL_HAVE_LIBDOVI)
    if (mpi->dovi_buf)
        pl_hdr_metadata_from_dovi_rpu(&frame->color.hdr, mpi->dovi_buf->data,
                                      mpi->dovi_buf->size);
#endif

#endif // PL_HAVE_LAV_DOLBY_VISION
}
