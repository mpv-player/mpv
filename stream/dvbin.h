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

#if DVB_API_VERSION < 5 || DVB_API_VERSION_MINOR < 8
#error DVB support requires a non-ancient kernel
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

    bool is_on;
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
    bool cfg_full_transponder;
    int cfg_channel_switch_offset;
} dvb_opts_t;

typedef struct {
    struct mp_log *log;

    dvb_state_t *state;

    char *prog;
    int devno;

    int opts_check_time;
    dvb_opts_t *opts;
    struct m_config_cache *opts_cache;
} dvb_priv_t;


/* Keep in sync with enum fe_delivery_system. */
#define SYS_DVB__COUNT__            (SYS_DVBC_ANNEX_C + 1)


#define DELSYS_BIT(__bit)        (((unsigned int)1) << (__bit))

#define DELSYS_SET(__mask, __bit)                                       \
    (__mask) |= DELSYS_BIT((__bit))

#define DELSYS_IS_SET(__mask, __bit)                                    \
    (0 != ((__mask) & DELSYS_BIT((__bit))))


#define DELSYS_SUPP_MASK                                                \
    (                                                                   \
        DELSYS_BIT(SYS_DVBC_ANNEX_A) |                                  \
        DELSYS_BIT(SYS_DVBT) |                                          \
        DELSYS_BIT(SYS_DVBS) |                                          \
        DELSYS_BIT(SYS_DVBS2) |                                         \
        DELSYS_BIT(SYS_ATSC) |                                          \
        DELSYS_BIT(SYS_DVBC_ANNEX_B) |                                  \
        DELSYS_BIT(SYS_DVBT2) |                                         \
        DELSYS_BIT(SYS_ISDBT) |                                         \
        DELSYS_BIT(SYS_DVBC_ANNEX_C)                                    \
    )

void dvb_update_config(stream_t *);
int dvb_parse_path(stream_t *);
int dvb_set_channel(stream_t *, unsigned int, unsigned int);
dvb_state_t *dvb_get_state(stream_t *);

#endif /* MPLAYER_DVBIN_H */
