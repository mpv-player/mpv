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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "stream.h"
#include "demuxer.h"
#include "help_mp.h"
#include "../m_option.h"
#include "../m_struct.h"

#include "dvbin.h"


#define MAX_CHANNELS 8
#define CHANNEL_LINE_LEN 256
#define min(a, b) ((a) <= (b) ? (a) : (b))


//TODO: CAMBIARE list_ptr e da globale a per_priv


static struct stream_priv_s
{
	char *prog;
	int card;
	char *type;
	int vid, aid;
	char *file;
}
stream_defaults =
{
	"", 1, "", 0, 0, "channels.conf"
};

#define ST_OFF(f) M_ST_OFF(struct stream_priv_s, f)

/// URL definition
static m_option_t stream_params[] = {
	{"prog", ST_OFF(prog), CONF_TYPE_STRING, 0, 0 ,0, NULL},
	{"card", ST_OFF(card), CONF_TYPE_INT, M_OPT_RANGE, 1, 4, NULL},
	{"type", ST_OFF(type), CONF_TYPE_STRING, 0, 0 ,0, NULL},
	{"vid",  ST_OFF(vid),  CONF_TYPE_INT, 0, 0 ,0, NULL},
	{"aid",  ST_OFF(aid),  CONF_TYPE_INT, 0, 0 ,0, NULL},
	{"file", ST_OFF(file), CONF_TYPE_STRING, 0, 0 ,0, NULL},

	{"hostname", 	ST_OFF(prog), CONF_TYPE_STRING, 0, 0, 0, NULL },
	{"filename", 	ST_OFF(card), CONF_TYPE_INT, M_OPT_RANGE, 1, 4, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};

static struct m_struct_st stream_opts = {
	"dvbin",
	sizeof(struct stream_priv_s),
	&stream_defaults,
	stream_params
};



m_option_t dvbin_opts_conf[] = {
	{"prog", &stream_defaults.prog, CONF_TYPE_STRING, 0, 0 ,0, NULL},
	{"card", &stream_defaults.card, CONF_TYPE_INT, M_OPT_RANGE, 1, 4, NULL},
	{"type", &stream_defaults.type, CONF_TYPE_STRING, 0, 0 ,0, NULL},
	{"vid",  &stream_defaults.vid,  CONF_TYPE_INT, 0, 0 ,0, NULL},
	{"aid",  &stream_defaults.aid,  CONF_TYPE_INT, 0, 0 ,0, NULL},
	{"file", &stream_defaults.file, CONF_TYPE_STRING, 0, 0 ,0, NULL},

	{NULL, NULL, 0, 0, 0, 0, NULL}
};




extern int dvb_set_ts_filt(int fd, uint16_t pid, dmx_pes_type_t pestype);
extern int dvb_demux_stop(int fd);
extern int dvb_get_tuner_type(dvb_priv_t *priv);

extern int dvb_tune(dvb_priv_t *priv, int freq, char pol, int srate, int diseqc, int tone,
		fe_spectral_inversion_t specInv, fe_modulation_t modulation, fe_guard_interval_t guardInterval,
		fe_transmit_mode_t TransmissionMode, fe_bandwidth_t bandWidth, fe_code_rate_t HP_CodeRate);
extern char *dvb_dvrdev[4], *dvb_demuxdev[4];

dvb_channels_list  *dvb_list_ptr = NULL;


static dvb_channels_list *dvb_get_channels(char *filename, int type)
{
	dvb_channels_list  *list;
	FILE *f;
	uint8_t line[CHANNEL_LINE_LEN];

	int fields, row_count;
	dvb_channel_t *ptr;
	char *tmp_lcr, *tmp_hier, *inv, *bw, *cr, *mod, *transm, *gi;
	const char *cbl_conf = "%a[^:]:%d:%c:%d:%a[^:]:%a[^:]:%d:%d\n";
	const char *sat_conf = "%a[^:]:%d:%c:%d:%d:%d:%d:%d:%d:%d\n";
	const char *ter_conf = "%a[^:]:%d:%a[^:]:%a[^:]:%a[^:]:%a[^:]:%a[^:]:%a[^:]:%a[^:]:%a[^:]:%d:%d\n";

	if(type != TUNER_SAT && type != TUNER_TER && type != TUNER_CBL)
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "DVB_GET_CHANNELS: wrong tuner type, exit\n");
		return 0;
	}

	list = malloc(sizeof(dvb_channels_list));
	if(list == NULL)
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "DVB_GET_CHANNELS: couldn't malloc enough memory\n");
		return NULL;
	}

	bzero(list, sizeof(dvb_channels_list));
	mp_msg(MSGT_DEMUX, MSGL_V, "CONFIG_READ FILE: %s, type: %d\n", filename, type);
	if((f=fopen(filename, "r"))==NULL)
	{
		mp_msg(MSGT_DEMUX, MSGL_FATAL, "CAN'T READ CONFIG FILE %s\n", filename);
		return NULL;
	}

	list->NUM_CHANNELS = 0;
	row_count = 0;
	while(! feof(f) && row_count < 512)
	{
		if( fgets(line, CHANNEL_LINE_LEN, f) == NULL )
			continue;

		if((line[0] == '#') || (strlen(line) == 0))
			continue;

		ptr =  &(list->channels[list->NUM_CHANNELS]);

		if(type == TUNER_TER)
		{
			fields = sscanf(line, ter_conf,
				&ptr->name, &ptr->freq, &inv, &bw, &cr, &tmp_lcr, &mod,
				&transm, &gi, &tmp_hier, &ptr->vpid, &ptr->apid1);
			/*
			mp_msg(MSGT_DEMUX, MSGL_V,
				"NUM: %d, NUM_FIELDS: %d, NAME: %s, FREQ: %d, VPID: %d, APID1: %d\n",
				list->NUM_CHANNELS, fields, ptr->name, ptr->freq, ptr->vpid, ptr->apid1);
			*/
		}
		else if(type == TUNER_CBL)
		{
			fields = sscanf(line, cbl_conf,
				&ptr->name, &ptr->freq, &inv, &ptr->srate,
				&cr, &mod, &ptr->vpid, &ptr->apid1);
			/*
			mp_msg(MSGT_DEMUX, MSGL_V,
				"NUM: %d, NUM_FIELDS: %d, NAME: %s, FREQ: %d, SRATE: %d, VPID: %d, APID1: %d\n",
				list->NUM_CHANNELS, fields, ptr->name, ptr->freq, ptr->srate, ptr->vpid, ptr->apid1);
			*/
		}
		else //SATELLITE
		{
			fields = sscanf(line, sat_conf,
				&ptr->name, &ptr->freq, &ptr->pol, &ptr->diseqc, &ptr->srate, &ptr->vpid, &ptr->apid1,
				&ptr->tpid, &ptr->ca, &ptr->progid);
			ptr->pol = toupper(ptr->pol);
			ptr->freq *=  1000UL;
			ptr->srate *=  1000UL;
			ptr->tone = -1;
			mp_msg(MSGT_DEMUX, MSGL_V,
				"NUM: %d, NUM_FIELDS: %d, NAME: %s, FREQ: %d, SRATE: %d, POL: %c, DISEQC: %d, TONE: %d, VPID: %d, APID1: %d, APID2: %d, TPID: %d, PROGID: %d\n",
				list->NUM_CHANNELS, fields, ptr->name, ptr->freq, ptr->srate, ptr->pol, ptr->diseqc, ptr->tone, ptr->vpid, ptr->apid1, ptr->apid2, ptr->tpid, ptr->progid);
		}


		if(((ptr->vpid <= 0) && (ptr->apid1 <=0)) || (ptr->freq == 0))
			continue;


		if((type == TUNER_TER) || (type == TUNER_CBL))
		{
			if(! strcmp(inv, "INVERSION_ON"))
				ptr->inv = INVERSION_ON;
			else if(! strcmp(inv, "INVERSION_OFF"))
				ptr->inv = INVERSION_OFF;
			else
				ptr->inv = INVERSION_AUTO;


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
			//else ptr->mod = QPSK;
		}


		if(type == TUNER_TER)
		{
			if(! strcmp(bw, "BANDWIDTH_6_MHZ"))
				ptr->bw = BANDWIDTH_6_MHZ;
			else if(! strcmp(bw, "BANDWIDTH_7_MHZ"))
				ptr->bw = BANDWIDTH_7_MHZ;
			else if(! strcmp(bw, "BANDWIDTH_8_MHZ"))
				ptr->bw = BANDWIDTH_8_MHZ;


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

		list->NUM_CHANNELS++;
		row_count++;
	}

	fclose(f);
	list->current = 0;
	return list;
}



static int dvb_streaming_read(stream_t *stream, char *buffer, int size)
{
	struct pollfd pfds[1];
	int pos=0, tries, rk;
	int fd = stream->fd;
	dvb_priv_t *priv  = (dvb_priv_t *) stream->priv;

	mp_msg(MSGT_DEMUX, MSGL_V, "dvb_streaming_read(%d)\n", size);

	if(priv->retry)
		tries = 5;
	else
		tries = 1;
	while(pos < size)
	{
		pfds[0].fd = fd;
		pfds[0].events = POLLIN | POLLPRI;

		poll(pfds, 1, 500);
		rk = size - pos;
	    	if((rk = read(fd, &buffer[pos], rk)) > 0)
		{
			pos += rk;
			mp_msg(MSGT_DEMUX, MSGL_V, "ret (%d) bytes\n", pos);
		}
		else
		{
			mp_msg(MSGT_DEMUX, MSGL_ERR, "dvb_streaming_read, attempt N. %d failed with errno %d when reading %d bytes\n", tries, errno, size-pos);
			if(--tries > 0)
			{
				errno = 0;
				//reset_demuxers(priv);
				continue;
			}
			else
			{
				errno = 0;
				break;
			}
		}
	}

	if(! pos)
		mp_msg(MSGT_DEMUX, MSGL_ERR, "dvb_streaming_read, return %d bytes\n", pos);

	return pos;
}


static int reset_demuxers(dvb_priv_t *priv)
{
	dvb_channel_t *channel;
	dvb_channels_list *list = priv->list;

	channel = &(list->channels[list->current]);

	if(priv->is_on)	//the fds are already open and we have to stop the demuxers
	{
		dvb_demux_stop(priv->demux_fd[0]);
		dvb_demux_stop(priv->demux_fd[1]);
	}

	if(channel->vpid)
  	  	if(! dvb_set_ts_filt(priv->demux_fd[0], channel->vpid, DMX_PES_VIDEO))
			return 0;
	//dvb_demux_start(priv->demux_fd[0]);

	if(channel->apid1)
		if(! dvb_set_ts_filt(priv->demux_fd[1], channel->apid1, DMX_PES_AUDIO))
			return 0;

	printf("RESET DEMUXERS SUCCEDED, errno=%d\n\n\n", errno);
}


int dvb_set_channel(dvb_priv_t *priv, int n)
{
	dvb_channels_list *list;
	dvb_channel_t *channel;
	int do_tuning;
	stream_t *stream  = (stream_t*) priv->stream;
	char buf[4096];

	if(priv->is_on)	//the fds are already open and we have to stop the demuxers
	{
		dvb_demux_stop(priv->demux_fd[0]);
		dvb_demux_stop(priv->demux_fd[1]);
		priv->retry = 0;
		while(stream_read(stream, buf, 4096));	//empty both the stream's and driver's buffer
	}

	priv->retry = 1;
	mp_msg(MSGT_DEMUX, MSGL_V, "DVB_SET_CHANNEL: channel %d\n", n);
	list = priv->list;
	if(list == NULL)
	{
		mp_msg(MSGT_DEMUX, MSGL_ERR, "dvb_set_channel: LIST NULL PTR, quit\n");
		return 0;
	}

	if((n > list->NUM_CHANNELS) || (n < 0))
	{
		mp_msg(MSGT_DEMUX, MSGL_ERR, "dvb_set_channel: INVALID CHANNEL NUMBER: %d, abort\n", n);
		return 0;
	}

	list->current = n;
	channel = &(list->channels[list->current]);
	mp_msg(MSGT_DEMUX, MSGL_V, "DVB_SET_CHANNEL: new channel name=%s\n", channel->name);

	switch(priv->tuner_type)
	{
		case TUNER_SAT:
			sprintf(priv->new_tuning, "%d|%09d|%09d|%d|%c", priv->card, channel->freq, channel->srate, channel->diseqc, channel->pol);
			break;

		case TUNER_TER:
			sprintf(priv->new_tuning, "%d|%09d|%d|%d|%d|%d|%d|%d", priv->card, channel->freq, channel->inv,
				channel->bw, channel->cr, channel->mod, channel->trans, channel->gi);
		  break;

		case TUNER_CBL:
			sprintf(priv->new_tuning, "%d|%09d|%d|%d|%d|%d", priv->card, channel->freq, channel->inv, channel->srate,
				channel->cr, channel->mod);
		break;
	}



	if(strcmp(priv->prev_tuning, priv->new_tuning))
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "DIFFERENT TUNING THAN THE PREVIOUS: %s  -> %s\n", priv->prev_tuning, priv->new_tuning);
		strcpy(priv->prev_tuning, priv->new_tuning);
		do_tuning = 1;
	}
	else
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "SAME TUNING, NO TUNING\n");
		do_tuning = 0;
	}

	stream->eof=1;
	stream_reset(stream);


	if(do_tuning)
		if (! dvb_tune(priv, channel->freq, channel->pol, channel->srate, channel->diseqc, channel->tone,
			channel->inv, channel->mod, channel->gi, channel->trans, channel->bw, channel->cr))
			return 0;


	priv->is_on = 1;

	//sets demux filters and restart the stream
	if(channel->vpid)
		if(! dvb_set_ts_filt(priv->demux_fd[0], channel->vpid, DMX_PES_VIDEO))
			return 0;
	//dvb_demux_start(priv->demux_fd[0]);

	if(channel->apid1)
		if(! dvb_set_ts_filt(priv->demux_fd[1], channel->apid1, DMX_PES_AUDIO))
			return 0;
	//dvb_demux_start(priv->demux_fd[1]);

	return 1;
}



int dvb_step_channel(dvb_priv_t *priv, int dir)
{
	int new_current;
	dvb_channels_list *list;

	mp_msg(MSGT_DEMUX, MSGL_V, "DVB_STEP_CHANNEL dir %d\n", dir);

	if(priv == NULL)
	{
		mp_msg(MSGT_DEMUX, MSGL_ERR, "dvb_step_channel: NULL priv_ptr, quit\n");
		return 0;
	}

	list = priv->list;
	if(list == NULL)
	{
		mp_msg(MSGT_DEMUX, MSGL_ERR, "dvb_step_channel: NULL list_ptr, quit\n");
		return 0;
	}


	if(dir == DVB_CHANNEL_HIGHER)
	{
		if(list->current == list->NUM_CHANNELS-1)
			return 0;

		new_current = list->current + 1;
	}
	else
	{
		if(list->current == 0)
			return 0;

		new_current = list->current - 1;
	}

	return dvb_set_channel(priv, new_current);
}




extern char *get_path(char *);

static void dvbin_close(stream_t *stream)
{
	dvb_priv_t *priv  = (dvb_priv_t *) stream->priv;

	close(priv->dvr_fd);
	close(priv->demux_fd[0]);
	close(priv->demux_fd[1]);
	priv->is_on = 0;
	priv->stream = NULL;
	if(dvb_list_ptr)
		free(dvb_list_ptr);

	dvb_list_ptr = NULL;
}


static int dvb_streaming_start(dvb_priv_t *priv, struct stream_priv_s *opts, int tuner_type)
{
	int pids[MAX_CHANNELS], pestypes[MAX_CHANNELS], npids = 0, i;
	dvb_channel_t *channel = NULL;
	stream_t *stream  = (stream_t*) priv->stream;


	mp_msg(MSGT_DEMUX, MSGL_INFO, "code taken from dvbstream for mplayer v0.4pre1 - (C) Dave Chapman 2001\n");
	mp_msg(MSGT_DEMUX, MSGL_INFO, "Released under the GPL.\n");
	mp_msg(MSGT_DEMUX, MSGL_INFO, "Latest version available from http://www.linuxstb.org/\n");
	mp_msg(MSGT_DEMUX, MSGL_V, 	  "PROG: %s, CARD: %d, VID: %d, AID: %d, TYPE: %s, FILE: %s\n",
	    opts->prog, opts->card, opts->vid, opts->aid,  opts->type, opts->file);

	priv->is_on = 0;

	if(strlen(opts->prog))
	{
		if(dvb_list_ptr != NULL)
		{
			i = 0;
			while((channel == NULL) && i < dvb_list_ptr->NUM_CHANNELS)
			{
				if(! strcmp(dvb_list_ptr->channels[i].name, opts->prog))
					channel = &(dvb_list_ptr->channels[i]);

				i++;
			}

			if(channel != NULL)
			{
				dvb_list_ptr->current = i-1;
				mp_msg(MSGT_DEMUX, MSGL_V, "PROGRAM NUMBER %d: name=%s, vid=%d, aid=%d, freq=%lu, srate=%lu, pol=%c, diseqc: %d, tone: %d\n", i-1,
					channel->name, channel->vpid, channel->apid1,
					channel->freq, channel->srate, channel->pol, channel->diseqc, channel->tone);
			}
			else if(opts->prog)
			{
				mp_msg(MSGT_DEMUX, MSGL_ERR, "\n\nDVBIN: no such channel \"%s\"\n\n", opts->prog);
				return 0;
			}
		}
		else
		{
			mp_msg(MSGT_DEMUX, MSGL_ERR, "DVBIN: chanel %s requested, but no channel list supplied %s\n", opts->prog);
			return 0;
		}
	}



	if(opts->vid > 0)
	{
	    pids[npids] =  opts->vid;
	}
	else if(channel != NULL)
	{
	    pids[npids] =  channel->vpid;
	}
	pestypes[npids] = DMX_PES_VIDEO;
	npids++;

	if(opts->aid > 0)
	{
	    pids[npids] = opts->aid;
	}
	else if(channel != NULL)
	{
	    pids[npids] = channel->apid1;
	}
	pestypes[npids] = DMX_PES_AUDIO;
	npids++;



	priv->demux_fd[0] = open(dvb_demuxdev[priv->card], O_RDWR);
	if(priv->demux_fd[0] < 0)
	{
		mp_msg(MSGT_DEMUX, MSGL_ERR, "ERROR OPENING DEMUX 0: %d\n", errno);
		return 0;
	}

	priv->demux_fd[1] = open(dvb_demuxdev[priv->card], O_RDWR);
	if(priv->demux_fd[1] < 0)
	{
		mp_msg(MSGT_DEMUX, MSGL_ERR, "ERROR OPENING DEMUX 1: %d\n", errno);
		return 0;
	}


	priv->dvr_fd = open(dvb_dvrdev[priv->card], O_RDONLY| O_NONBLOCK);
	if(priv->dvr_fd < 0)
	{
	  mp_msg(MSGT_DEMUX, MSGL_ERR, "ERROR OPENING DVR DEVICE %s: %d\n", dvb_dvrdev[priv->card], errno);
	  return 0;
	}


	strcpy(priv->prev_tuning, "");
	if(!dvb_set_channel(priv, dvb_list_ptr->current))
	{
		mp_msg(MSGT_DEMUX, MSGL_ERR, "ERROR, COULDN'T SET CHANNEL  %i: ", dvb_list_ptr->current);
		dvbin_close(stream);
		return 0;
	}

	stream->fd = priv->dvr_fd;

	mp_msg(MSGT_DEMUX, MSGL_V,  "SUCCESSFUL EXIT from dvb_streaming_start\n");

	return 1;
}




static int dvb_open(stream_t *stream, int mode, void *opts, int *file_format)
{
	// I don't force  the file format bacause, although it's almost always TS,
	// there are some providers that stream an IP multicast with M$ Mpeg4 inside
	struct stream_priv_s* p = (struct stream_priv_s*)opts;
	char *name = NULL, *filename;
	dvb_priv_t *priv;
	int tuner_type;



	if(mode != STREAM_READ)
		return STREAM_UNSUPORTED;

	stream->priv = (dvb_priv_t*) malloc(sizeof(dvb_priv_t));
	if(stream->priv ==  NULL)
		return STREAM_ERROR;

	priv = (dvb_priv_t *)stream->priv;
	priv->stream = stream;

	name = malloc(sizeof(char)*128);

	if(name == NULL)
	{
		mp_msg(MSGT_DEMUX, MSGL_ERR, "COULDN'T MALLOC SOME TMP MEMORY, EXIT!\n");
		return STREAM_ERROR;
	}

	priv->card = p->card - 1;

	if(!strncmp(p->type, "CBL", 3))
	{
		tuner_type = TUNER_CBL;
	}
	else if(!strncmp(p->type, "TER", 3))
	{
		tuner_type = TUNER_TER;
	}
	else if(!strncmp(p->type, "SAT", 3))
	{
		tuner_type = TUNER_SAT;
	}
	else
	{
		int t = dvb_get_tuner_type(priv);

		if((t==TUNER_SAT) || (t==TUNER_TER) || (t==TUNER_CBL))
		{
			tuner_type = t;
		}
	}

	priv->tuner_type = tuner_type;

	mp_msg(MSGT_DEMUX, MSGL_V, "OPEN_DVB: prog=%s, card=%d, type=%d, vid=%d, aid=%d, file=%s\n",
		p->prog, priv->card+1, priv->tuner_type, p->vid, p->aid, p->file);

	if(dvb_list_ptr == NULL)
	{
		filename = get_path(p->file);
		if(filename)
		{
			if((dvb_list_ptr = dvb_get_channels(filename, tuner_type)) == NULL)
				mp_msg(MSGT_DEMUX, MSGL_ERR, "EMPTY CHANNELS LIST FROM FILE %s!\n", filename);
			else
			{
				priv->list = dvb_list_ptr;
			}
		}
		else
		{
			dvb_list_ptr = NULL;
			mp_msg(MSGT_DEMUX, MSGL_WARN, "NO CHANNELS FILE FOUND!\n");
		}
	}
	else
		priv->list = dvb_list_ptr;


	if(! strcmp(p->prog, ""))
	{
		if(dvb_list_ptr != NULL)
		{
			dvb_channel_t *channel;

			channel = &(dvb_list_ptr->channels[dvb_list_ptr->current]);
			p->prog = channel->name;
		}
	}


	if(! dvb_streaming_start(priv, p, tuner_type))
	{
		free(stream->priv);
		stream->priv = NULL;
		return STREAM_ERROR;
	}

	stream->type = STREAMTYPE_DVB;
	stream->fill_buffer = dvb_streaming_read;
	stream->close = dvbin_close;
	m_struct_free(&stream_opts, opts);

    return STREAM_OK;
}



stream_info_t stream_info_dvb = {
	"Dvb Input",
	"dvbin",
	"Nico",
	"based on the code from ??? (probably Arpi)",
	dvb_open, 			
	{ "dvb", NULL },
	&stream_opts,
	1 				// Urls are an option string
};




