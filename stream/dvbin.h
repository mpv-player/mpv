/* Imported from the dvbstream project
 *
 * Modified for use with MPlayer, for details see the changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 */

#ifndef MPLAYER_DVBIN_H
#define MPLAYER_DVBIN_H

#include "config.h"
#include "stream.h"

#if !HAVE_GPL
#error GPL only
#endif

#define SLOF (11700 * 1000UL)
#define LOF1 (9750 * 1000UL)
#define LOF2 (10600 * 1000UL)

#include <inttypes.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/version.h>

#define MAX_ADAPTERS 16
#define MAX_FRONTENDS 8

#undef DVB_ATSC
#if defined(DVB_API_VERSION_MINOR)

/* kernel headers >=2.6.28 have version 5.
 *
 * Version 5 is also called S2API, it adds support for tuning to S2 channels
 * and is extensible for future delivery systems. Old API is deprecated.
 * StreamID-implementation only supported since API >=5.2.
 * At least DTV_ENUM_DELSYS requires 5.5.
 */

#if (DVB_API_VERSION == 5 && DVB_API_VERSION_MINOR >= 5)
#define DVB_USE_S2API 1

// This had a different name until API 5.8.
#ifndef DTV_STREAM_ID
#define DTV_STREAM_ID DTV_ISDBS_TS_ID
#endif
#endif

// This is only defined, for convenience, since API 5.8.
#ifndef NO_STREAM_ID_FILTER
#define NO_STREAM_ID_FILTER (~0U)
#endif

#if (DVB_API_VERSION == 3 && DVB_API_VERSION_MINOR >= 1) || DVB_API_VERSION == 5
#define DVB_ATSC 1
#endif

#endif

#define DVB_CHANNEL_LOWER -1
#define DVB_CHANNEL_HIGHER 1

#ifndef DMX_FILTER_SIZE
#define DMX_FILTER_SIZE 32
#endif

typedef struct {
    char *name;
    unsigned int freq, srate, diseqc;
    char pol;
    unsigned int tpid, dpid1, dpid2, progid, ca, pids[DMX_FILTER_SIZE], pids_cnt;
    bool is_dvb_x2; /* Used only in dvb_get_channels() and parse_vdr_par_string(), use delsys. */
    unsigned int frontend;
    unsigned int delsys;
    unsigned int stream_id;
    unsigned int service_id;
    fe_spectral_inversion_t inv;
    fe_modulation_t mod;
    fe_transmit_mode_t trans;
    fe_bandwidth_t bw;
    fe_guard_interval_t gi;
    fe_code_rate_t cr, cr_lp;
    fe_hierarchy_t hier;
} dvb_channel_t;

typedef struct {
    unsigned int NUM_CHANNELS;
    unsigned int current;
    dvb_channel_t *channels;
} dvb_channels_list_t;

typedef struct {
    int devno;
    unsigned int delsys_mask[MAX_FRONTENDS];
    dvb_channels_list_t *list;
} dvb_adapter_config_t;

typedef struct {
    unsigned int adapters_count;
    dvb_adapter_config_t *adapters;
    unsigned int cur_adapter;
    unsigned int cur_frontend;

    int fe_fd;
    int dvr_fd;
    int demux_fd[3], demux_fds[DMX_FILTER_SIZE], demux_fds_cnt;

    int is_on;
    int retry;
    unsigned int last_freq;
    bool switching_channel;
    bool stream_used;
} dvb_state_t;

typedef struct {
    char *cfg_prog;
    int cfg_devno;
    int cfg_timeout;
    char *cfg_file;
    int cfg_full_transponder;
    int cfg_channel_switch_offset;
} dvb_opts_t;

typedef struct {
    struct mp_log *log;

    dvb_state_t *state;

    char *prog;
    int devno;

    dvb_opts_t *opts;
    struct m_config_cache *opts_cache;
} dvb_priv_t;


/* Keep in sync with enum fe_delivery_system. */
#ifndef DVB_USE_S2API
#    define SYS_DVBC_ANNEX_A        1
#    define SYS_DVBC_ANNEX_B        1
#    define SYS_DVBT                3
#    define SYS_DVBS                5
#    define SYS_DVBS2               6
#    define SYS_ATSC                11
#    define SYS_DVBT2               16
#    define SYS_DVBC_ANNEX_C        18
#endif
#define SYS_DVB__COUNT__            (SYS_DVBC_ANNEX_C + 1)


#define DELSYS_BIT(__bit)        (((unsigned int)1) << (__bit))

#define DELSYS_SET(__mask, __bit)                                       \
    (__mask) |= DELSYS_BIT((__bit))

#define DELSYS_IS_SET(__mask, __bit)                                    \
    (0 != ((__mask) & DELSYS_BIT((__bit))))


#ifdef DVB_ATSC
#define DELSYS_SUPP_MASK                                                \
    (                                                                   \
        DELSYS_BIT(SYS_DVBC_ANNEX_A) |                                  \
        DELSYS_BIT(SYS_DVBT) |                                          \
        DELSYS_BIT(SYS_DVBS) |                                          \
        DELSYS_BIT(SYS_DVBS2) |                                         \
        DELSYS_BIT(SYS_ATSC) |                                          \
        DELSYS_BIT(SYS_DVBC_ANNEX_B) |                                  \
        DELSYS_BIT(SYS_DVBT2) |                                         \
        DELSYS_BIT(SYS_DVBC_ANNEX_C)                                    \
    )
#else
#define DELSYS_SUPP_MASK                                                \
    (                                                                   \
        DELSYS_BIT(SYS_DVBC_ANNEX_A) |                                  \
        DELSYS_BIT(SYS_DVBT) |                                          \
        DELSYS_BIT(SYS_DVBS) |                                          \
        DELSYS_BIT(SYS_DVBS2) |                                         \
        DELSYS_BIT(SYS_DVBT2) |                                         \
        DELSYS_BIT(SYS_DVBC_ANNEX_C)                                    \
    )
#endif

void dvb_update_config(stream_t *);
int dvb_parse_path(stream_t *);
int dvb_set_channel(stream_t *, unsigned int, unsigned int);
dvb_state_t *dvb_get_state(stream_t *);
void dvb_free_state(dvb_state_t *);

#endif /* MPLAYER_DVBIN_H */
