/*

dvbstream
(C) Dave Chapman <dave@dchapman.com> 2001, 2002.

The latest version can be found at http://www.linuxstb.org/dvbstream

Copyright notice:

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

// Linux includes:
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <resolv.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <values.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "config.h"

// DVB includes:

#include "stream.h"
#include "demuxer.h"

#include "../cfgparser.h"

#include "dvbin.h"
#include "dvb_defaults.h"

extern int video_id, audio_id, demuxer_type;


#define MAX_CHANNELS 8


#define min(a, b) ((a) <= (b) ? (a) : (b))

int dvbin_param_card, dvbin_param_freq, dvbin_param_srate, dvbin_param_diseqc = 0,
	dvbin_param_tone = -1, dvbin_param_vid, dvbin_param_aid, dvbin_is_active = 0;
int dvbin_param_mod, dvbin_param_gi, dvbin_param_tm, dvbin_param_bw, dvbin_param_cr;
char *dvbin_param_pol = "", *dvbin_param_inv="INVERSION_AUTO",
    *dvbin_param_type="SAT                                                                                          ",
    *dvbin_param_prog = "                                                                                           ";
dvb_history_t dvb_prev_next;

struct config dvbin_opts_conf[] = {
        {"on", &dvbin_param_on, CONF_TYPE_INT, 0, 0, 1, NULL},
        {"type", &dvbin_param_type, CONF_TYPE_STRING, 0, 0, 1, NULL},
        {"card", &dvbin_param_card, CONF_TYPE_INT, CONF_RANGE, 1, 4, NULL},
        {"freq", &dvbin_param_freq, CONF_TYPE_INT, 0, 0, 1, NULL},
        {"pol", &dvbin_param_pol, CONF_TYPE_STRING, 0, 0, 0, NULL},
        {"srate", &dvbin_param_srate, CONF_TYPE_INT, 0, 0, 1, NULL},
        {"diseqc", &dvbin_param_diseqc, CONF_TYPE_INT, CONF_RANGE, 1, 4,  NULL},
        {"tone", &dvbin_param_tone, CONF_TYPE_INT, 0, 0, 1, NULL},
        {"vid", &dvbin_param_vid, CONF_TYPE_INT, 0, 0, 1, NULL},
        {"aid", &dvbin_param_aid, CONF_TYPE_INT, 0, 0, 1, NULL},
        {"prog", &dvbin_param_prog, CONF_TYPE_STRING, 0, 0, 0, NULL},
        {"inv", &dvbin_param_inv, CONF_TYPE_STRING, 0, 0, 0, NULL},
        {"mod", &dvbin_param_mod, CONF_TYPE_INT, 0, 0, 1, NULL},
        {"gi", &dvbin_param_gi, CONF_TYPE_INT, 0, 0, 1, NULL},
        {"tm", &dvbin_param_tm, CONF_TYPE_INT, 0, 0, 1, NULL},
        {"bw", &dvbin_param_bw, CONF_TYPE_INT, 0, 0, 1, NULL},
        {"cr", &dvbin_param_cr, CONF_TYPE_INT, 0, 0, 1, NULL},
        {NULL, NULL, 0, 0, 0, 0, NULL}
};


int card=0;

extern int open_fe(int* fd_frontend, int* fd_sec);
extern int set_ts_filt(int fd, uint16_t pid, dmx_pes_type_t pestype);
extern int demux_stop(int fd);
extern void make_nonblock(int f);
extern int dvb_tune(dvb_priv_t *priv, int freq, char pol, int srate, int diseqc, int tone,
		fe_spectral_inversion_t specInv, fe_modulation_t modulation, fe_guard_interval_t guardInterval,
		fe_transmit_mode_t TransmissionMode, fe_bandwidth_t bandWidth, fe_code_rate_t HP_CodeRate);
extern char *frontenddev[4], *dvrdev[4], *secdev[4], *demuxdev[4];


dvb_channels_list *dvb_get_channels(char *filename, const char *type)
{
	dvb_channels_list  *list;
	FILE *f;
	uint8_t line[128];
	int fields, row_count;
	dvb_channel_t *ptr;
	char *tmp_lcr, *tmp_hier, *inv, *bw, *cr, *mod, *transm, *gi;
	//const char *cbl_conf = "%a[^:]:%d:%c:%d:%a[^:]:%a[^:]:%d:%d\n";
	const char *sat_conf = "%a[^:]:%d:%c:%d:%d:%d:%d:%d:%d:%d\n";
	const char *ter_conf = "%a[^:]:%d:%a[^:]:%a[^:]:%a[^:]:%a[^:]:%a[^:]:%a[^:]:%a[^:]:%a[^:]:%d:%d\n";

	list = malloc(sizeof(dvb_channels_list));
	if(list == NULL)
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "DVB_GET_CHANNELS: couldn't malloc enough memory\n");
		return NULL;
	}

	mp_msg(MSGT_DEMUX, MSGL_V, "CONFIG_READ FILE: %s, type: %s\n", filename, type);
	if((f=fopen(filename, "r"))==NULL)
	{
		mp_msg(MSGT_DEMUX, MSGL_FATAL, "CAN'T READ CONFIG FILE %s\n", filename);
		return NULL;
	}

	list->NUM_CHANNELS = 0;
	row_count = 0;
	while(! feof(f) && row_count < 512)
	{
		if( fgets(line, 128, f) == NULL ) continue;

		if(line[0] == '#')
			continue;	//comment line

		ptr =  &(list->channels[ list->NUM_CHANNELS ]);

		if(! strcmp(type, "TER"))
		{
			fields = sscanf(line, ter_conf,
				&ptr->name, &ptr->freq, &inv, &bw, &cr, tmp_lcr, &mod,
				&transm, &gi, &tmp_hier, &ptr->vpid, &ptr->apid1);

			if(! strcmp(inv, "INVERSION_ON"))
				ptr->inv = INVERSION_ON;
			else if(! strcmp(inv, "INVERSION_OFF"))
				ptr->inv = INVERSION_OFF;
			else
				ptr->inv = INVERSION_AUTO;

			if(! strcmp(bw, "BANDWIDTH_6_MHZ"))
				ptr->bw = BANDWIDTH_6_MHZ;
			else if(! strcmp(bw, "BANDWIDTH_7_MHZ"))
				ptr->bw = BANDWIDTH_7_MHZ;
			else if(! strcmp(bw, "BANDWIDTH_8_MHZ"))
				ptr->bw = BANDWIDTH_8_MHZ;


			if(! strcmp(cr, "FEC_1_2"))
				ptr->cr =FEC_1_2;
			else if(! strcmp(cr, "FEC_2_3"))
				ptr->cr =FEC_2_3;
			else if(! strcmp(cr, "FEC_3_4"))
				ptr->cr =FEC_3_4;
#ifdef HAVE_DVB_HEAD
			else if(! strcmp(cr, "FEC_4_5"))
				ptr->cr =FEC_4_5;
			else if(! strcmp(cr, "FEC_6_7"))
				ptr->cr =FEC_6_7;
			else if(! strcmp(cr, "FEC_8_9"))
				ptr->cr =FEC_8_9;
#endif
			else if(! strcmp(cr, "FEC_5_6"))
				ptr->cr =FEC_5_6;
			else if(! strcmp(cr, "FEC_7_8"))
				ptr->cr =FEC_7_8;
			else if(! strcmp(cr, "FEC_NONE"))
				ptr->cr =FEC_NONE;
			else ptr->cr =FEC_AUTO;

			if(! strcmp(mod, "QAM_128"))
				ptr->mod = QAM_128;
			else if(! strcmp(mod, "QAM_256"))
				ptr->mod = QAM_256;
			else if(! strcmp(mod, "QAM_64"))
				ptr->mod = QAM_64;
			else if(! strcmp(mod, "QAM_32"))
				ptr->mod = QAM_32;
			else if(! strcmp(mod, "QAM_16"))
				ptr->mod = QAM_16;
			else ptr->mod = QPSK;


			if(! strcmp(transm, "TRANSMISSION_MODE_2K"))
				ptr->trans = TRANSMISSION_MODE_2K;
			else if(! strcmp(transm, "TRANSMISSION_MODE_8K"))
				ptr->trans = TRANSMISSION_MODE_8K;

			if(! strcmp(gi, "GUARD_INTERVAL_1_32"))
				ptr->gi = GUARD_INTERVAL_1_32;
			else if(! strcmp(gi, "GUARD_INTERVAL_1_16"))
				ptr->gi = GUARD_INTERVAL_1_16;
			else if(! strcmp(gi, "GUARD_INTERVAL_1_8"))
				ptr->gi = GUARD_INTERVAL_1_8;
			else ptr->gi = GUARD_INTERVAL_1_4;


		}
		/*
		else if(! strcmp(type, "CBL"))
		{
			fields = sscanf(line, cbl_conf,
                                &ptr->name, &ptr->freq, &ptr->inv, &ptr->qam,
                                &ptr->fec, &ptr->mod, &ptr->vpid, &ptr->apid1);


		}
		*/
		else	//SATELLITE
		{
			fields = sscanf(line, sat_conf,
				&ptr->name, &ptr->freq, &ptr->pol, &ptr->diseqc, &ptr->srate, &ptr->vpid, &ptr->apid1,
				&ptr->tpid, &ptr->ca, &ptr->progid);
			ptr->pol = toupper(ptr->pol);
			ptr->freq *=  1000UL;
			ptr->srate *=  1000UL;
			ptr->tone = -1;
			mp_msg(MSGT_DEMUX, MSGL_V,
				"NUM_FIELDS: %d, NAME: %s, FREQ: %d, SRATE: %d, POL: %c, DISEQC: %d, TONE: %d, VPID: %d, APID1: %d, APID2: %d, TPID: %d, PROGID: %d, NUM: %d\n",
				fields, ptr->name, ptr->freq, ptr->srate, ptr->pol, ptr->diseqc, ptr->tone, ptr->vpid, ptr->apid1, ptr->apid2, ptr->tpid, ptr->progid, list->NUM_CHANNELS);
		}

		list->NUM_CHANNELS++;
		row_count++;
	}

	fclose(f);
	return list;
}


static long getmsec()
{
	struct timeval tv;
	gettimeofday(&tv, (struct timezone*) NULL);
	return(tv.tv_sec%1000000)*1000 + tv.tv_usec/1000;
}



int dvb_streaming_read(int fd, char *buffer, unsigned int size, dvb_priv_t *priv)
{
	struct pollfd pfds[1];
	uint32_t ok = 0, pos = 0, tot = 0, rk, d, r, m;

	mp_msg(MSGT_DEMUX, MSGL_DBG2, "dvb_streaming_read(%u)\n", fd);

	while(pos < size)
	{
	    ok = 0;
	    tot = 0;
	    //int m = min((size-pos), 188);
	    m = size - pos;
	    d = (int) (m / 188);
	    r = m % 188;

	    m = d * 188;
	    m = (m ? m : r);

	    pfds[0].fd = fd;
	    pfds[0].events = POLLIN | POLLPRI;

	    mp_msg(MSGT_DEMUX, MSGL_DBG2, "DEVICE: %d, DVR: %d, PIPE: %d <-> %d\n", fd, priv->dvr_fd, priv->input, priv->output);

	    poll(pfds, 1, 500);
	    if((rk = read(fd, &buffer[pos], m)) > 0)
	    	pos += rk;
	}

	return pos;
}



dvb_history_t *dvb_step_channel(dvb_priv_t *priv, int dir, dvb_history_t *h)
{
    //int new_freq, new_srate, new_diseqc, new_tone, new_vpid, new_apid;
    //char new_pol;
    int new_current;
    dvb_channel_t *next;
    dvb_channels_list *list;

    if(priv == NULL)
    {
    	mp_msg(MSGT_DEMUX, MSGL_ERR, "dvb_step_channel: PRIV NULL PTR, quit\n");
    	return 0;
    }

    list = priv->list;
    if(list == NULL)
    {
    	mp_msg(MSGT_DEMUX, MSGL_ERR, "dvb_step_channel: LIST NULL PTR, quit\n");
    	return 0;
    }

    mp_msg(MSGT_DEMUX, MSGL_V, "DVB_STEP_CHANNEL dir %d\n", dir);

    if(dir == DVB_CHANNEL_HIGHER)
    {
    	if(list->current == list->NUM_CHANNELS)
    		return 0;

    	new_current = list->current + 1;
    	next = &(list->channels[new_current]);
    }
    else
    {
    	if(list->current == 0)
    		return 0;

    	new_current = list->current - 1;
    	next = &(list->channels[new_current]);

    }

    demux_stop(priv->demux_fd[0]);
    demux_stop(priv->demux_fd[1]);

    h->prev = list->current;
    h->next = new_current;

    list->current = new_current;

    return h;
}


extern char *get_path(char *);

dvb_channels_list  *list_ptr = NULL;

int dvb_streaming_start(stream_t *stream)
{
	int pids[MAX_CHANNELS];
	int pestypes[MAX_CHANNELS];
	int npids, i;
	char *filename, type[80];
	unsigned long freq = 0;
	char pol = 0;
	unsigned long srate = 0;
	int diseqc = 0, old_diseqc = 0;
	int tone = -1;

	dvb_priv_t *priv;
	dvb_channel_t *channel = NULL;
	fe_spectral_inversion_t 	specInv			=	INVERSION_AUTO;
	fe_modulation_t 		modulation		=	CONSTELLATION_DEFAULT;
	fe_transmit_mode_t 		TransmissionMode 	=	TRANSMISSION_MODE_DEFAULT;
	fe_bandwidth_t 			bandWidth		=	BANDWIDTH_DEFAULT;
	fe_guard_interval_t 		guardInterval		=	GUARD_INTERVAL_DEFAULT;
	fe_code_rate_t 			HP_CodeRate		=	HP_CODERATE_DEFAULT;


	stream->priv = (dvb_priv_t*) malloc(sizeof(dvb_priv_t));
	if(stream->priv ==  NULL)
	    return 0;
	priv = (dvb_priv_t*) stream->priv;

	if(!strncmp(dvbin_param_type, "CBL", 3))
	    strncpy(type, "CBL", 3);
	else if(!strncmp(dvbin_param_type, "TER", 3))
	    strncpy(type, "TER", 3);
	else
	    strncpy(type, "SAT", 3);


	filename = get_path("channels.conf");

	if(list_ptr == NULL)
	{
		if(filename)
		{
			if((list_ptr = dvb_get_channels(filename, type)) == NULL)
				mp_msg(MSGT_DEMUX, MSGL_WARN, "EMPTY CHANNELS LIST!\n");
			else
			{
				priv->list = list_ptr;
				priv->list->current = 0;
			}
		}
		else
		{
			list_ptr = NULL;
			mp_msg(MSGT_DEMUX, MSGL_WARN, "NO CHANNELS FILE FOUND!\n");
		}
	}


	mp_msg(MSGT_DEMUX, MSGL_INFO, "code taken from dvbstream for mplayer v0.4pre1 - (C) Dave Chapman 2001\n");
	mp_msg(MSGT_DEMUX, MSGL_INFO, "Released under the GPL.\n");
	mp_msg(MSGT_DEMUX, MSGL_INFO, "Latest version available from http://www.linuxstb.org/\n");
	mp_msg(MSGT_DEMUX, MSGL_V, "ON: %d, CARD: %d, FREQ: %d, SRATE: %d, POL: %s, VID: %d, AID: %d\n", dvbin_param_on,
	    dvbin_param_card, dvbin_param_freq, dvbin_param_srate, dvbin_param_pol, dvbin_param_vid, dvbin_param_aid);

	npids = 0;

	if((dvb_prev_next.next > -1) && (dvb_prev_next.prev > -1) && (list_ptr != NULL))	//We are after a channel stepping
	{
	    list_ptr->current = dvb_prev_next.next;
	    channel = &(list_ptr->channels[dvb_prev_next.next]);
	    mp_msg(MSGT_DEMUX, MSGL_V, "PROGRAM NUMBER %d: name=%s, vid=%d, aid=%d, freq=%lu, srate=%lu, pol=%c, diseqc: %d, tone: %d\n", dvb_prev_next.next,
		    channel->name, channel->vpid, channel->apid1,
		    channel->freq, channel->srate, channel->pol, channel->diseqc, channel->tone);


	    if((dvb_prev_next.prev >= 0) && (dvb_prev_next.prev < list_ptr->NUM_CHANNELS))
	    {
		dvb_channel_t *tmp = &(list_ptr->channels[dvb_prev_next.prev]);
		old_diseqc = tmp->diseqc;
	    }
	}
	else if(list_ptr != NULL && strlen(dvbin_param_prog))
	{
	    i = 0;
	    while((channel == NULL) && i < list_ptr->NUM_CHANNELS)
	    {
		if(! strcmp(list_ptr->channels[i].name, dvbin_param_prog))
		    channel = &(list_ptr->channels[i]);

		i++;
	    }
	    if(channel != NULL)
	    {
		list_ptr->current = i-1;
	    	mp_msg(MSGT_DEMUX, MSGL_V, "PROGRAM NUMBER %d: name=%s, vid=%d, aid=%d, freq=%lu, srate=%lu, pol=%c, diseqc: %d, tone: %d\n", i-1,
		    channel->name, channel->vpid, channel->apid1,
		    channel->freq, channel->srate, channel->pol, channel->diseqc, channel->tone);

	    }
	}


	if(dvbin_param_vid > 0)
	{
	    pids[npids] = priv->channel.vpid = dvbin_param_vid;
	}
	else if(channel != NULL)
	{
	    pids[npids] = priv->channel.vpid = channel->vpid;
	}
	pestypes[npids] = DMX_PES_VIDEO;
	npids++;

	if(dvbin_param_aid > 0)
	{
	    pids[npids] = priv->channel.apid1 = dvbin_param_aid;
	}
	else if(channel != NULL)
	{
	    pids[npids] = priv->channel.vpid = channel->apid1;
	}
	pestypes[npids] = DMX_PES_AUDIO;
	npids++;



	if(dvbin_param_freq)
	    freq  = dvbin_param_freq  * 1000UL;
	else  if(channel != NULL)
	    freq = channel->freq;


	if(dvbin_param_srate)
	    srate = dvbin_param_srate * 1000UL;
	else  if(channel != NULL)
	    srate = channel->srate;

	if((1<= dvbin_param_diseqc)  && (dvbin_param_diseqc <= 4))
	    diseqc = dvbin_param_diseqc;
	else
	    if(channel != NULL)
		if(channel->diseqc != old_diseqc)
		    diseqc = channel->diseqc;
		else
		    diseqc = 0;
	    else
		diseqc = 0;
	mp_msg(MSGT_DEMUX, MSGL_INFO, "DISEQC: %d\n", diseqc);

	if((dvbin_param_tone == 0) || (dvbin_param_tone == 1))
	    tone = dvbin_param_tone;
	else
	    if(channel != NULL)
		tone = channel->tone;
	    else
		tone = -1;

	if(! strcmp(dvbin_param_pol, "V")) pol = 'V';
	else if(! strcmp(dvbin_param_pol, "H")) pol = 'H';
	else if(channel != NULL) pol = channel->pol;
	else pol='V';
	pol = toupper(pol);


	if(!strcmp(dvbin_param_inv, "INVERSION_ON"))
		specInv = INVERSION_ON;
	else if(!strcmp(dvbin_param_inv, "INVERSION_OFF"))
		specInv = INVERSION_OFF;
	else if(!strcmp(dvbin_param_inv, "INVERSION_AUTO"))
		specInv = INVERSION_AUTO;
	else if(channel != NULL)
		specInv = channel->inv;
	else
		specInv = INVERSION_AUTO;


	if(dvbin_param_mod)
	{
		switch(dvbin_param_mod)
		{
			case 16:  modulation=QAM_16; break;
			case 32:  modulation=QAM_32; break;
			case 64:  modulation=QAM_64; break;
			case 128: modulation=QAM_128; break;
			case 256: modulation=QAM_256; break;
			default:
				mp_msg(MSGT_DEMUX, MSGL_ERR, "Invalid QAM rate: %s\n", dvbin_param_mod);
				modulation=CONSTELLATION_DEFAULT;
		}
	}
	else  if(channel != NULL)
	    	modulation = channel->mod;
	else
		modulation=CONSTELLATION_DEFAULT;


	if(dvbin_param_gi)
	{
		switch(dvbin_param_gi)
		{
			case 32:  guardInterval=GUARD_INTERVAL_1_32; break;
			case 16:  guardInterval=GUARD_INTERVAL_1_16; break;
			case 8:   guardInterval=GUARD_INTERVAL_1_8; break;
			case 4:   guardInterval=GUARD_INTERVAL_1_4; break;
			default:
				mp_msg(MSGT_DEMUX, MSGL_ERR, "Invalid Guard Interval: %s\n", dvbin_param_gi);
				guardInterval=GUARD_INTERVAL_DEFAULT;
		}
	}
	else  if(channel != NULL)
	    	guardInterval = channel->gi;
	else
		guardInterval=GUARD_INTERVAL_DEFAULT;

	if(dvbin_param_tm)
	{
		switch(dvbin_param_tm)
		{
			case 8:   TransmissionMode=TRANSMISSION_MODE_8K; break;
			case 2:   TransmissionMode=TRANSMISSION_MODE_2K; break;
			default:
				TransmissionMode=TRANSMISSION_MODE_DEFAULT;
				mp_msg(MSGT_DEMUX, MSGL_ERR, "Invalid Transmission Mode: %s\n", dvbin_param_tm);
		}
	}
	else  if(channel != NULL)
	    	TransmissionMode = channel->trans;
	else
		TransmissionMode=TRANSMISSION_MODE_DEFAULT;


	if(dvbin_param_bw)
	{
		switch(dvbin_param_bw)
		{
			case 8:   bandWidth=BANDWIDTH_8_MHZ; break;
			case 7:   bandWidth=BANDWIDTH_7_MHZ; break;
			case 6:   bandWidth=BANDWIDTH_6_MHZ; break;
			default:
				mp_msg(MSGT_DEMUX, MSGL_ERR, "Invalid DVB-T bandwidth: %s\n", dvbin_param_bw);
			       	bandWidth=BANDWIDTH_DEFAULT;
		}
	}
	else  if(channel != NULL)
	    	bandWidth = channel->bw;
	else
		bandWidth=BANDWIDTH_DEFAULT;


	if(dvbin_param_cr)
	{
		switch(dvbin_param_cr)
		{
			case -1: HP_CodeRate=FEC_AUTO; break;
			case 12: HP_CodeRate=FEC_1_2; break;
			case 23: HP_CodeRate=FEC_2_3; break;
			case 34: HP_CodeRate=FEC_3_4; break;
			case 56: HP_CodeRate=FEC_5_6; break;
			case 78: HP_CodeRate=FEC_7_8; break;
			default:
				mp_msg(MSGT_DEMUX, MSGL_ERR, "Invalid Code Rate: %s\n", dvbin_param_cr);
				HP_CodeRate=HP_CODERATE_DEFAULT;
		}
	}
	else  if(channel != NULL)
	    	HP_CodeRate = channel->cr;
	else
		HP_CodeRate=HP_CODERATE_DEFAULT;



	card = dvbin_param_card - 1;
	if((card < 0) || (card > 4))
	    card = 0;


	dvbin_param_on = 1;

	mp_msg(MSGT_DEMUX, MSGL_V,  "CARD: %d, FREQ: %d, POL: %c, SRATE: %d, DISEQC: %d, TONE: %d, VPID: %d, APID: %d\n", card, freq, pol, srate, diseqc, tone, pids[0], pids[1]);

	priv->channel.freq = freq;
	priv->channel.srate = srate;
	priv->channel.diseqc = diseqc;
	priv->channel.pol = pol;
	priv->channel.tone = tone;
	priv->channel.inv = specInv;
	priv->channel.mod = modulation;
	priv->channel.gi = guardInterval;
	priv->channel.trans = TransmissionMode;
	priv->channel.bw = bandWidth;
	priv->channel.cr = HP_CodeRate;

	if(freq && pol && srate)
		if (! dvb_tune(priv, freq, pol, srate, diseqc, tone, specInv, modulation, guardInterval, TransmissionMode, bandWidth, HP_CodeRate))
			return 0;

	for (i=0; i < npids; i++)
	{
		if((priv->demux_fd[i] = open(demuxdev[card], O_RDWR)) < 0)
		{
	  		mp_msg(MSGT_DEMUX, MSGL_ERR, "ERROR OPENING DEMUX %i: ", i);
	  		return -1;
		}
	}

	if((priv->dvr_fd = open(dvrdev[card], O_RDONLY| O_NONBLOCK)) < 0)
	{
		mp_msg(MSGT_DEMUX, MSGL_ERR, "DVR DEVICE: ");
		return -1;
	}


	/* Now we set the filters */
	for (i=0; i< npids; i++)
	{
	    set_ts_filt(priv->demux_fd[i], pids[i], pestypes[i]);
	    //make_nonblock(fd[i]);
	}


	stream->fd = priv->dvr_fd;

	dvbin_is_active = 1;

	mp_msg(MSGT_DEMUX, MSGL_DBG2,  "ESCO da dvb_streaming_start(s)\n");

	return 1;
}


int dvbin_close(dvb_priv_t *priv)
{
	//close(priv->dvr_fd);
	close(priv->demux_fd[0]);
	close(priv->demux_fd[1]);
}
