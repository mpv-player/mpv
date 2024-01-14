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

static int determine_pl_log_level(struct mp_log *log)
{
    int log_level = mp_msg_level(log);
    return log_level == -1 ? PL_LOG_NONE : msg_lev_to_pl_log[log_level];
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
        .log_level  = determine_pl_log_level(log),
        .log_priv   = mp_log_new(tactx, log, "libplacebo"),
    });
}

void mppl_log_set_probing(pl_log log, bool probing)
{
    struct pl_log_params params = log->params;
    params.log_cb = probing ? log_cb_probing : log_cb;
    pl_log_update(log, &params);
}

enum pl_color_primaries mp_prim_to_pl(enum mp_csp_prim prim)
{
    switch (prim) {
    case MP_CSP_PRIM_AUTO:          return PL_COLOR_PRIM_UNKNOWN;
    case MP_CSP_PRIM_BT_601_525:    return PL_COLOR_PRIM_BT_601_525;
    case MP_CSP_PRIM_BT_601_625:    return PL_COLOR_PRIM_BT_601_625;
    case MP_CSP_PRIM_BT_709:        return PL_COLOR_PRIM_BT_709;
    case MP_CSP_PRIM_BT_2020:       return PL_COLOR_PRIM_BT_2020;
    case MP_CSP_PRIM_BT_470M:       return PL_COLOR_PRIM_BT_470M;
    case MP_CSP_PRIM_APPLE:         return PL_COLOR_PRIM_APPLE;
    case MP_CSP_PRIM_ADOBE:         return PL_COLOR_PRIM_ADOBE;
    case MP_CSP_PRIM_PRO_PHOTO:     return PL_COLOR_PRIM_PRO_PHOTO;
    case MP_CSP_PRIM_CIE_1931:      return PL_COLOR_PRIM_CIE_1931;
    case MP_CSP_PRIM_DCI_P3:        return PL_COLOR_PRIM_DCI_P3;
    case MP_CSP_PRIM_DISPLAY_P3:    return PL_COLOR_PRIM_DISPLAY_P3;
    case MP_CSP_PRIM_V_GAMUT:       return PL_COLOR_PRIM_V_GAMUT;
    case MP_CSP_PRIM_S_GAMUT:       return PL_COLOR_PRIM_S_GAMUT;
    case MP_CSP_PRIM_EBU_3213:      return PL_COLOR_PRIM_EBU_3213;
    case MP_CSP_PRIM_FILM_C:        return PL_COLOR_PRIM_FILM_C;
    case MP_CSP_PRIM_ACES_AP0:      return PL_COLOR_PRIM_ACES_AP0;
    case MP_CSP_PRIM_ACES_AP1:      return PL_COLOR_PRIM_ACES_AP1;
    case MP_CSP_PRIM_COUNT:         return PL_COLOR_PRIM_COUNT;
    }

    MP_ASSERT_UNREACHABLE();
}

enum mp_csp_prim mp_prim_from_pl(enum pl_color_primaries prim)
{
    switch (prim){
    case PL_COLOR_PRIM_UNKNOWN:     return MP_CSP_PRIM_AUTO;
    case PL_COLOR_PRIM_BT_601_525:  return MP_CSP_PRIM_BT_601_525;
    case PL_COLOR_PRIM_BT_601_625:  return MP_CSP_PRIM_BT_601_625;
    case PL_COLOR_PRIM_BT_709:      return MP_CSP_PRIM_BT_709;
    case PL_COLOR_PRIM_BT_2020:     return MP_CSP_PRIM_BT_2020;
    case PL_COLOR_PRIM_BT_470M:     return MP_CSP_PRIM_BT_470M;
    case PL_COLOR_PRIM_APPLE:       return MP_CSP_PRIM_APPLE;
    case PL_COLOR_PRIM_ADOBE:       return MP_CSP_PRIM_ADOBE;
    case PL_COLOR_PRIM_PRO_PHOTO:   return MP_CSP_PRIM_PRO_PHOTO;
    case PL_COLOR_PRIM_CIE_1931:    return MP_CSP_PRIM_CIE_1931;
    case PL_COLOR_PRIM_DCI_P3:      return MP_CSP_PRIM_DCI_P3;
    case PL_COLOR_PRIM_DISPLAY_P3:  return MP_CSP_PRIM_DISPLAY_P3;
    case PL_COLOR_PRIM_V_GAMUT:     return MP_CSP_PRIM_V_GAMUT;
    case PL_COLOR_PRIM_S_GAMUT:     return MP_CSP_PRIM_S_GAMUT;
    case PL_COLOR_PRIM_EBU_3213:    return MP_CSP_PRIM_EBU_3213;
    case PL_COLOR_PRIM_FILM_C:      return MP_CSP_PRIM_FILM_C;
    case PL_COLOR_PRIM_ACES_AP0:    return MP_CSP_PRIM_ACES_AP0;
    case PL_COLOR_PRIM_ACES_AP1:    return MP_CSP_PRIM_ACES_AP1;
    case PL_COLOR_PRIM_COUNT:       return MP_CSP_PRIM_COUNT;
    }

    MP_ASSERT_UNREACHABLE();
}

enum pl_color_transfer mp_trc_to_pl(enum mp_csp_trc trc)
{
    switch (trc) {
    case MP_CSP_TRC_AUTO:           return PL_COLOR_TRC_UNKNOWN;
    case MP_CSP_TRC_BT_1886:        return PL_COLOR_TRC_BT_1886;
    case MP_CSP_TRC_SRGB:           return PL_COLOR_TRC_SRGB;
    case MP_CSP_TRC_LINEAR:         return PL_COLOR_TRC_LINEAR;
    case MP_CSP_TRC_GAMMA18:        return PL_COLOR_TRC_GAMMA18;
    case MP_CSP_TRC_GAMMA20:        return PL_COLOR_TRC_GAMMA20;
    case MP_CSP_TRC_GAMMA22:        return PL_COLOR_TRC_GAMMA22;
    case MP_CSP_TRC_GAMMA24:        return PL_COLOR_TRC_GAMMA24;
    case MP_CSP_TRC_GAMMA26:        return PL_COLOR_TRC_GAMMA26;
    case MP_CSP_TRC_GAMMA28:        return PL_COLOR_TRC_GAMMA28;
    case MP_CSP_TRC_PRO_PHOTO:      return PL_COLOR_TRC_PRO_PHOTO;
    case MP_CSP_TRC_PQ:             return PL_COLOR_TRC_PQ;
    case MP_CSP_TRC_HLG:            return PL_COLOR_TRC_HLG;
    case MP_CSP_TRC_V_LOG:          return PL_COLOR_TRC_V_LOG;
    case MP_CSP_TRC_S_LOG1:         return PL_COLOR_TRC_S_LOG1;
    case MP_CSP_TRC_S_LOG2:         return PL_COLOR_TRC_S_LOG2;
    case MP_CSP_TRC_ST428:          return PL_COLOR_TRC_ST428;
    case MP_CSP_TRC_COUNT:          return PL_COLOR_TRC_COUNT;
    }

    MP_ASSERT_UNREACHABLE();
}

enum mp_csp_trc mp_trc_from_pl(enum pl_color_transfer trc)
{
    switch (trc){
    case PL_COLOR_TRC_UNKNOWN: return MP_CSP_TRC_AUTO;
    case PL_COLOR_TRC_BT_1886: return MP_CSP_TRC_BT_1886;
    case PL_COLOR_TRC_SRGB: return MP_CSP_TRC_SRGB;
    case PL_COLOR_TRC_LINEAR: return MP_CSP_TRC_LINEAR;
    case PL_COLOR_TRC_GAMMA18: return MP_CSP_TRC_GAMMA18;
    case PL_COLOR_TRC_GAMMA20: return MP_CSP_TRC_GAMMA20;
    case PL_COLOR_TRC_GAMMA22: return MP_CSP_TRC_GAMMA22;
    case PL_COLOR_TRC_GAMMA24: return MP_CSP_TRC_GAMMA24;
    case PL_COLOR_TRC_GAMMA26: return MP_CSP_TRC_GAMMA26;
    case PL_COLOR_TRC_GAMMA28: return MP_CSP_TRC_GAMMA28;
    case PL_COLOR_TRC_PRO_PHOTO: return MP_CSP_TRC_PRO_PHOTO;
    case PL_COLOR_TRC_PQ: return MP_CSP_TRC_PQ;
    case PL_COLOR_TRC_HLG: return MP_CSP_TRC_HLG;
    case PL_COLOR_TRC_V_LOG: return MP_CSP_TRC_V_LOG;
    case PL_COLOR_TRC_S_LOG1: return MP_CSP_TRC_S_LOG1;
    case PL_COLOR_TRC_S_LOG2: return MP_CSP_TRC_S_LOG2;
    case PL_COLOR_TRC_ST428: return MP_CSP_TRC_ST428;
    case PL_COLOR_TRC_COUNT: return MP_CSP_TRC_COUNT;
    }

    MP_ASSERT_UNREACHABLE();
}

enum pl_color_system mp_csp_to_pl(enum mp_csp csp)
{
    switch (csp) {
    case MP_CSP_AUTO:               return PL_COLOR_SYSTEM_UNKNOWN;
    case MP_CSP_BT_601:             return PL_COLOR_SYSTEM_BT_601;
    case MP_CSP_BT_709:             return PL_COLOR_SYSTEM_BT_709;
    case MP_CSP_SMPTE_240M:         return PL_COLOR_SYSTEM_SMPTE_240M;
    case MP_CSP_BT_2020_NC:         return PL_COLOR_SYSTEM_BT_2020_NC;
    case MP_CSP_BT_2020_C:          return PL_COLOR_SYSTEM_BT_2020_C;
    case MP_CSP_RGB:                return PL_COLOR_SYSTEM_RGB;
    case MP_CSP_XYZ:                return PL_COLOR_SYSTEM_XYZ;
    case MP_CSP_YCGCO:              return PL_COLOR_SYSTEM_YCGCO;
    case MP_CSP_COUNT:              return PL_COLOR_SYSTEM_COUNT;
    }

    MP_ASSERT_UNREACHABLE();
}

enum pl_color_levels mp_levels_to_pl(enum mp_csp_levels levels)
{
    switch (levels) {
    case MP_CSP_LEVELS_AUTO:        return PL_COLOR_LEVELS_UNKNOWN;
    case MP_CSP_LEVELS_TV:          return PL_COLOR_LEVELS_TV;
    case MP_CSP_LEVELS_PC:          return PL_COLOR_LEVELS_PC;
    case MP_CSP_LEVELS_COUNT:       return PL_COLOR_LEVELS_COUNT;
    }

    MP_ASSERT_UNREACHABLE();
}

enum mp_csp_levels mp_levels_from_pl(enum pl_color_levels levels)
{
    switch (levels){
    case PL_COLOR_LEVELS_UNKNOWN:   return MP_CSP_LEVELS_AUTO;
    case PL_COLOR_LEVELS_TV:        return MP_CSP_LEVELS_TV;
    case PL_COLOR_LEVELS_PC:        return MP_CSP_LEVELS_PC;
    case PL_COLOR_LEVELS_COUNT:     return MP_CSP_LEVELS_COUNT;
    }

    MP_ASSERT_UNREACHABLE();
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
