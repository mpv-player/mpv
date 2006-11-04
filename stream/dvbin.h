/* Imported from the dvbstream project
 *
 * Modified for use with MPlayer, for details see the changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 */

#ifndef DVBIN_H
#define DVBIN_H

#include "stream.h"

#define SLOF (11700*1000UL)
#define LOF1 (9750*1000UL)
#define LOF2 (10600*1000UL)

#ifdef HAVE_DVB_HEAD
	#include <linux/dvb/dmx.h>
	#include <linux/dvb/frontend.h>
	#include <linux/dvb/version.h>
#else
	#include <ost/dmx.h>
	#include <ost/sec.h>
	#include <ost/frontend.h>
	#define fe_status_t FrontendStatus
	#define fe_spectral_inversion_t SpectralInversion
	#define fe_modulation_t Modulation
	#define fe_code_rate_t CodeRate
	#define fe_transmit_mode_t TransmitMode
	#define fe_guard_interval_t GuardInterval
	#define fe_bandwidth_t BandWidth
	#define fe_hierarchy_t Hierarchy
	#define fe_sec_voltage_t SecVoltage
	#define dmx_pes_filter_params dmxPesFilterParams
	#define dmx_sct_filter_params dmxSctFilterParams
	#define dmx_pes_type_t dmxPesType_t
#endif

#undef DVB_ATSC
#if defined(DVB_API_VERSION_MINOR)
#if DVB_API_VERSION == 3 && DVB_API_VERSION_MINOR >= 1
#define DVB_ATSC 1
#endif
#endif


#define DVB_CHANNEL_LOWER -1
#define DVB_CHANNEL_HIGHER 1

#include "inttypes.h"

#ifndef DMX_FILTER_SIZE
#define DMX_FILTER_SIZE 16
#endif

typedef struct {
	char 				*name;
	int 				freq, srate, diseqc, tone;
	char 				pol;
	int 				tpid, dpid1, dpid2, progid, ca, pids[DMX_FILTER_SIZE], pids_cnt;
	fe_spectral_inversion_t 	inv;
	fe_modulation_t 		mod;
	fe_transmit_mode_t 		trans;
	fe_bandwidth_t 			bw;
	fe_guard_interval_t 		gi;
	fe_code_rate_t 			cr, cr_lp;
	fe_hierarchy_t			hier;
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

typedef struct {
	int card;
	int fe_fd;
	int sec_fd;
	int demux_fd[3], demux_fds[DMX_FILTER_SIZE], demux_fds_cnt;
	int dvr_fd;

	dvb_config_t *config;
	dvb_channels_list *list;
	int tuner_type;
	int is_on;
	stream_t *stream;
	int retry;
	int timeout;
	int last_freq;
} dvb_priv_t;


#define TUNER_SAT	1
#define TUNER_TER	2
#define TUNER_CBL	3
#define TUNER_ATSC	4

extern int dvb_step_channel(dvb_priv_t *, int);
extern int dvb_set_channel(dvb_priv_t *, int, int);
extern dvb_config_t *dvb_get_config(void);

#endif
