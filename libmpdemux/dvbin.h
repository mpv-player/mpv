
#ifndef DVBIN_H
#define DVBIN_H

#ifdef HAVE_DVB_HEAD
	#include <linux/dvb/dmx.h>
	#include <linux/dvb/frontend.h>
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
	#define fe_sec_voltage_t SecVoltage
	#define dmx_pes_filter_params dmxPesFilterParams
	#define dmx_sct_filter_params dmxSctFilterParams
	#define dmx_pes_type_t dmxPesType_t
#endif



#define DVB_CHANNEL_LOWER -1
#define DVB_CHANNEL_HIGHER 1


typedef struct
{
    int next, prev;
} dvb_history_t;

typedef struct {
	char 				*name;
	int 				freq, srate, diseqc, tone;
	char 				pol;
	int 				vpid, apid1, apid2, tpid, dpid1, dpid2, progid, ca;
	fe_spectral_inversion_t 	inv;
	fe_modulation_t 		mod;
	fe_transmit_mode_t 		trans;
	fe_bandwidth_t 			bw;
	fe_guard_interval_t 		gi;
	fe_code_rate_t 			cr;
} dvb_channel_t;


typedef struct {
	uint16_t NUM_CHANNELS;
	uint16_t current;
	dvb_channel_t channels[512];
} dvb_channels_list;



typedef struct {
    int fe_fd;
    int sec_fd;
    int demux_fd[3];
    int dvr_fd;
    int input;
    int output;
    int discard;

    dvb_channel_t channel;
    dvb_channels_list *list;
} dvb_priv_t;


extern dvb_history_t *dvb_step_channel(dvb_priv_t*, int, dvb_history_t*);

extern dvb_channels_list *dvb_get_channels(char *, const char *);
extern dvb_history_t dvb_prev_next;




#ifndef DVB_T_LOCATION
    #ifndef UK
    #warning No DVB-T country defined in dvb_defaults.h, defaulting to UK
    #endif

    /* UNITED KINGDOM settings */
    #define DVB_T_LOCATION              "in United Kingdom"
    #define BANDWIDTH_DEFAULT           BANDWIDTH_8_MHZ
    #define HP_CODERATE_DEFAULT         FEC_2_3
    #define CONSTELLATION_DEFAULT       QAM_64
    #define TRANSMISSION_MODE_DEFAULT   TRANSMISSION_MODE_2K
    #define GUARD_INTERVAL_DEFAULT      GUARD_INTERVAL_1_32
    #define HIERARCHY_DEFAULT           HIERARCHY_NONE
#endif

#define HIERARCHY_DEFAULT           HIERARCHY_NONE
#define LP_CODERATE_DEFAULT (0)



#endif
