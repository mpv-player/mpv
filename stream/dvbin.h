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

#define SLOF (11700 * 1000UL)
#define LOF1 (9750 * 1000UL)
#define LOF2 (10600 * 1000UL)

#include <inttypes.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/version.h>

#undef DVB_ATSC
#if defined(DVB_API_VERSION_MINOR)

/* kernel headers >=2.6.28 have version 5.
 *
 * Version 5 is also called S2API, it adds support for tuning to S2 channels
 * and is extensible for future delivery systems. Old API is deprecated.
 * StreamID-implementation only supported since API >=5.2.
 */

#if (DVB_API_VERSION == 5 && DVB_API_VERSION_MINOR >= 2)
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
    int freq, srate, diseqc, tone;
    char pol;
    int tpid, dpid1, dpid2, progid, ca, pids[DMX_FILTER_SIZE], pids_cnt;
    bool is_dvb_s2;
    int stream_id;
    int service_id;
    fe_spectral_inversion_t inv;
    fe_modulation_t mod;
    fe_transmit_mode_t trans;
    fe_bandwidth_t bw;
    fe_guard_interval_t gi;
    fe_code_rate_t cr, cr_lp;
    fe_hierarchy_t hier;
} dvb_channel_t;

typedef struct {
    uint16_t NUM_CHANNELS;
    uint16_t current;
    dvb_channel_t *channels;
} dvb_channels_list;

typedef struct {
    int type;
    dvb_channels_list *list;
    char *name;
    int devno;
} dvb_card_config_t;

typedef struct {
    int count;
    dvb_card_config_t *cards;
    void *priv;
} dvb_config_t;

typedef struct dvb_params {
    struct mp_log *log;
    int fd;
    int card;
    int fe_fd;
    int sec_fd;
    int demux_fd[3], demux_fds[DMX_FILTER_SIZE], demux_fds_cnt;
    int dvr_fd;

    dvb_config_t *config;
    dvb_channels_list *list;
    int tuner_type;
    int is_on;
    int retry;
    int timeout;
    int last_freq;

    char *cfg_prog;
    int cfg_card;
    int cfg_timeout;
    char *cfg_file;

    int cfg_full_transponder;
} dvb_priv_t;

#define TUNER_SAT       1
#define TUNER_TER       2
#define TUNER_CBL       3
#define TUNER_ATSC      4

int dvb_step_channel(stream_t *, int);
int dvb_set_channel(stream_t *, int, int);
dvb_config_t *dvb_get_config(stream_t *);
void dvb_free_config(dvb_config_t *config);

#endif /* MPLAYER_DVBIN_H */
