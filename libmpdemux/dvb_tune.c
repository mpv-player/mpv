/* dvbtune - tune.c

   Copyright (C) Dave Chapman 2001,2002

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
   Or, point your browser to http://www.gnu.org/copyleft/gpl.html

*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <error.h>
#include "config.h"

#ifdef HAVE_DVB_HEAD
	#include <linux/dvb/dmx.h>
	#include <linux/dvb/frontend.h>
	char* frontenddev[4]={"/dev/dvb/adapter0/frontend0","/dev/dvb/adapter1/frontend0","/dev/dvb/adapter2/frontend0","/dev/dvb/adapter3/frontend0"};
	char* dvrdev[4]={"/dev/dvb/adapter0/dvr0","/dev/dvb/adapter1/dvr0","/dev/dvb/adapter2/dvr0","/dev/dvb/adapter3/dvr0"};
	char* demuxdev[4]={"/dev/dvb/adapter0/demux0","/dev/dvb/adapter1/demux0","/dev/dvb/adapter2/demux0","/dev/dvb/adapter3/demux0"};
	char* secdev[4]={"","","",""};	//UNUSED, ONLY FOR UNIFORMITY
#else
	#include <ost/dmx.h>
	#include <ost/sec.h>
	#include <ost/frontend.h>
	char* frontenddev[4]={"/dev/ost/frontend0","/dev/ost/frontend1","/dev/ost/frontend2","/dev/ost/frontend3"};
	char* dvrdev[4]={"/dev/ost/dvr0","/dev/ost/dvr1","/dev/ost/dvr2","/dev/ost/dvr3"};
	char* secdev[4]={"/dev/ost/sec0","/dev/ost/sec1","/dev/ost/sec2","/dev/ost/sec3"};
	char* demuxdev[4]={"/dev/ost/demux0","/dev/ost/demux1","/dev/ost/demux2","/dev/ost/demux3"};
#endif

#include "dvbin.h"
#include "dvb_defaults.h"
#include "../mp_msg.h"


extern int card;

int open_fe(int* fd_frontend, int* fd_sec)
{
	if((*fd_frontend = open(frontenddev[card], O_RDWR)) < 0)
	{
		perror("ERROR IN OPENING FRONTEND DEVICE: ");
		return -1;
	}
#ifdef HAVE_DVB_HEAD
    	fd_sec=0;
#else
	if (fd_sec != 0)
	{
      		if((*fd_sec = open(secdev[card], O_RDWR)) < 0)
      		{
          		perror("ERROR IN OPENING SEC DEVICE: ");
          		return -1;
      		}
    	}
#endif
    	return 1;
}



int set_ts_filt(int fd, uint16_t pid, dmx_pes_type_t pestype)
{
	int i;
	struct dmx_pes_filter_params pesFilterParams;

	pesFilterParams.pid     = pid;
	pesFilterParams.input   = DMX_IN_FRONTEND;
	pesFilterParams.output  = DMX_OUT_TS_TAP;
#ifdef HAVE_DVB_HEAD
	pesFilterParams.pes_type = pestype;
#else
	pesFilterParams.pesType = pestype;
#endif

	//pesFilterParams.pesType = pestype;

	pesFilterParams.flags   = DMX_IMMEDIATE_START;

	if ((i = ioctl(fd, DMX_SET_PES_FILTER, &pesFilterParams)) < 0)
	{
		mp_msg(MSGT_DEMUX, MSGL_DBG2, "ERROR IN SETTING DMX_FILTER %i: ", pid);
	}

	mp_msg(MSGT_DEMUX, MSGL_V, "SET PES FILTER ON PID %d, RESULT: %d\n", pid, i );
	return 1;
}


int demux_stop(int fd)
{
	int i;
	i = ioctl(fd, DMX_STOP);

	mp_msg(MSGT_DEMUX, MSGL_DBG2, "STOPPING FD: %d, RESULT: %d\n", fd, i);

	return (i==0);
}



void make_nonblock(int f)
{
	int oldflags;

	if ((oldflags=fcntl(f, F_GETFL, 0)) < 0)
	{
		perror("ERROR IN F_GETFL");
	}

	oldflags|=O_NONBLOCK;
	if (fcntl(f, F_SETFL, oldflags) < 0)
	{
		perror("F_SETFL");
	}
}


static int tune_it(int fd_frontend, int fd_sec, unsigned int freq, unsigned int srate, char pol, int tone,
	fe_spectral_inversion_t specInv, unsigned int diseqc, fe_modulation_t modulation, fe_code_rate_t HP_CodeRate,
	fe_transmit_mode_t TransmissionMode, fe_guard_interval_t guardInterval, fe_bandwidth_t bandwidth);


//int dvb_tune(dvb_priv_t *priv, int freq, char pol, int srate, int diseqc, int tone)
dvb_tune(dvb_priv_t *priv, int freq, char pol, int srate, int diseqc, int tone,
		fe_spectral_inversion_t specInv, fe_modulation_t modulation, fe_guard_interval_t guardInterval,
		fe_transmit_mode_t TransmissionMode, fe_bandwidth_t bandWidth, fe_code_rate_t HP_CodeRate)
{
	mp_msg(MSGT_DEMUX, MSGL_DBG2, "dvb_tune con Freq: %lu, pol: %c, srate: %lu, diseqc %d, tone %d\n", freq, pol, srate, diseqc, tone);
	/* INPUT: frequency, polarization, srate */
	if(freq > 100000000)
	{
		if(open_fe(&(priv->fe_fd), 0))
		{
		      //tune_it(fd_frontend, 0, freq, 0, 0, tone, specInv, diseqc,modulation,HP_CodeRate,TransmissionMode,guardInterval,bandWidth);
			tune_it(priv->fe_fd, 0, freq, 0, 0, tone, specInv, diseqc, modulation, HP_CodeRate, TransmissionMode, guardInterval, bandWidth);

			close(priv->fe_fd);
		}
		else
			return 0;
	}
	else if ((freq != 0) && (pol != 0) && (srate != 0))
	{
		if (open_fe(&(priv->fe_fd), &(priv->sec_fd)))
		{
			tune_it(priv->fe_fd, priv->sec_fd, freq, srate, pol, tone, specInv, diseqc, modulation, HP_CodeRate, TransmissionMode, guardInterval, bandWidth);
			close(priv->fe_fd);
			close(priv->sec_fd);
		}
		else
			return 0;
	}

	priv->channel.freq = freq;
	priv->channel.srate = srate;
	priv->channel.pol = pol;
	priv->channel.diseqc = diseqc;
	priv->channel.tone = tone;
	priv->channel.inv = specInv;
	priv->channel.mod = modulation;
	priv->channel.gi = guardInterval;
	priv->channel.trans = TransmissionMode;
	priv->channel.bw = bandWidth;
	priv->channel.cr = HP_CodeRate;

	return 1;
}





#ifndef HAVE_DVB_HEAD
static int OSTSelftest(int fd)
{
    int ans;

    if ((ans = ioctl(fd, FE_SELFTEST,0) < 0))
    {
        mp_msg(MSGT_DEMUX, MSGL_ERR, "FE SELF TEST: ");
        return -1;
    }

    return 0;
}

static int OSTSetPowerState(int fd, uint32_t state)
{
    int ans;

    if ((ans = ioctl(fd,FE_SET_POWER_STATE,state) < 0))
    {
        mp_msg(MSGT_DEMUX, MSGL_ERR, "OST SET POWER STATE: ");
        return -1;
    }

    return 0;
}

static int OSTGetPowerState(int fd, uint32_t *state)
{
    int ans;

    if ((ans = ioctl(fd,FE_GET_POWER_STATE,state) < 0))
    {
        mp_msg(MSGT_DEMUX, MSGL_ERR, "OST GET POWER STATE: ");
        return -1;
    }

    switch(*state)
    {
	case FE_POWER_ON:
    	    mp_msg(MSGT_DEMUX, MSGL_V, "POWER ON (%d)\n",*state);
    	    break;
	case FE_POWER_STANDBY:
    	    mp_msg(MSGT_DEMUX, MSGL_V, "POWER STANDBY (%d)\n",*state);
    	    break;
	case FE_POWER_SUSPEND:
    	    mp_msg(MSGT_DEMUX, MSGL_V, "POWER SUSPEND (%d)\n",*state);
    	    break;
	case FE_POWER_OFF:
    	    mp_msg(MSGT_DEMUX, MSGL_V, "POWER OFF (%d)\n",*state);
    	    break;
	default:
    	    mp_msg(MSGT_DEMUX, MSGL_V, "unknown (%d)\n",*state);
    	break;
    }

    return 0;
}


static int SecGetStatus (int fd, struct secStatus *state)
{
    int ans;

    if ((ans = ioctl(fd, SEC_GET_STATUS, state) < 0))
    {
        mp_msg(MSGT_DEMUX, MSGL_ERR, ("SEC GET STATUS: "));
        return -1;
    }

    switch (state->busMode)
    {
	case SEC_BUS_IDLE:
    	    mp_msg(MSGT_DEMUX, MSGL_V, "SEC BUS MODE:  IDLE (%d)\n",state->busMode);
    	    break;
	case SEC_BUS_BUSY:
    	    mp_msg(MSGT_DEMUX, MSGL_V, "SEC BUS MODE:  BUSY (%d)\n",state->busMode);
	    break;
        case SEC_BUS_OFF:
	    mp_msg(MSGT_DEMUX, MSGL_V, "SEC BUS MODE:  OFF  (%d)\n",state->busMode);
    	    break;
        case SEC_BUS_OVERLOAD:
	    mp_msg(MSGT_DEMUX, MSGL_V, "SEC BUS MODE:  OVERLOAD (%d)\n",state->busMode);
    	    break;
	default:
    	    mp_msg(MSGT_DEMUX, MSGL_V, "SEC BUS MODE:  unknown  (%d)\n",state->busMode);
            break;
    }

    switch (state->selVolt)
    {
	case SEC_VOLTAGE_OFF:
		mp_msg(MSGT_DEMUX, MSGL_V, "SEC VOLTAGE:  OFF (%d)\n",state->selVolt);
		break;
	case SEC_VOLTAGE_LT:
		mp_msg(MSGT_DEMUX, MSGL_V, "SEC VOLTAGE:  LT  (%d)\n",state->selVolt);
		break;
	case SEC_VOLTAGE_13:
		mp_msg(MSGT_DEMUX, MSGL_V, "SEC VOLTAGE:  13  (%d)\n",state->selVolt);
		break;
	case SEC_VOLTAGE_13_5:
		mp_msg(MSGT_DEMUX, MSGL_V, "SEC VOLTAGE:  13.5 (%d)\n",state->selVolt);
		break;
	case SEC_VOLTAGE_18:
		mp_msg(MSGT_DEMUX, MSGL_V, "SEC VOLTAGE:  18 (%d)\n",state->selVolt);
		break;
	case SEC_VOLTAGE_18_5:
		mp_msg(MSGT_DEMUX, MSGL_V, "SEC VOLTAGE:  18.5 (%d)\n",state->selVolt);
		break;
	default:
		mp_msg(MSGT_DEMUX, MSGL_V, "SEC VOLTAGE:  unknown (%d)\n",state->selVolt);
		break;
    }

 	mp_msg(MSGT_DEMUX, MSGL_V, "SEC CONT TONE: %s\n", (state->contTone == SEC_TONE_ON ? "ON" : "OFF"));
    return 0;
}

#endif

static void print_status(fe_status_t festatus)
{
	mp_msg(MSGT_DEMUX, MSGL_V, "FE_STATUS:");
	if (festatus & FE_HAS_SIGNAL) mp_msg(MSGT_DEMUX, MSGL_V," FE_HAS_SIGNAL");
#ifdef HAVE_DVB_HEAD
	if (festatus & FE_TIMEDOUT) mp_msg(MSGT_DEMUX, MSGL_V, " FE_TIMEDOUT");
#else
	if (festatus & FE_HAS_POWER) mp_msg(MSGT_DEMUX, MSGL_V, " FE_HAS_POWER");
	if (festatus & FE_SPECTRUM_INV) mp_msg(MSGT_DEMUX, MSGL_V, " FE_SPECTRUM_INV");
	if (festatus & FE_TUNER_HAS_LOCK) mp_msg(MSGT_DEMUX, MSGL_V, " FE_TUNER_HAS_LOCK");
#endif
	if (festatus & FE_HAS_LOCK) mp_msg(MSGT_DEMUX, MSGL_V, " FE_HAS_LOCK");
	if (festatus & FE_HAS_CARRIER) mp_msg(MSGT_DEMUX, MSGL_V, " FE_HAS_CARRIER");
	if (festatus & FE_HAS_VITERBI) mp_msg(MSGT_DEMUX, MSGL_V, " FE_HAS_VITERBI");
	if (festatus & FE_HAS_SYNC) mp_msg(MSGT_DEMUX, MSGL_V, " FE_HAS_SYNC");
	mp_msg(MSGT_DEMUX, MSGL_V, "\n");
}


#ifdef HAVE_DVB_HEAD
static int check_status(int fd_frontend,struct dvb_frontend_parameters* feparams,int tone)
{
	int i,res;
	int32_t strength;
	fe_status_t festatus;
	struct dvb_frontend_event event;
	struct dvb_frontend_info fe_info;
	struct pollfd pfd[1];

	if (ioctl(fd_frontend,FE_SET_FRONTEND,feparams) < 0)
	{
	mp_msg(MSGT_DEMUX, MSGL_ERR, "ERROR tuning channel\n");
	return -1;
	}

	pfd[0].fd = fd_frontend;
	pfd[0].events = POLLIN;

	event.status=0;
	while (((event.status & FE_TIMEDOUT)==0) && ((event.status & FE_HAS_LOCK)==0))
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "polling....\n");
		if (poll(pfd,1,10000))
		{
			if (pfd[0].revents & POLLIN)
			{
				mp_msg(MSGT_DEMUX, MSGL_V, "Getting frontend event\n");
				if ( ioctl(fd_frontend, FE_GET_EVENT, &event) < 0)
				{
					mp_msg(MSGT_DEMUX, MSGL_ERR, "FE_GET_EVENT");
					return -1;
				}
			}
			print_status(event.status);
		}
	}

	if (event.status & FE_HAS_LOCK)
	{
		switch(fe_info.type)
		{
			case FE_OFDM:
			mp_msg(MSGT_DEMUX, MSGL_V, "Event:  Frequency: %d\n",event.parameters.frequency);
			break;
			case FE_QPSK:
			mp_msg(MSGT_DEMUX, MSGL_V, "Event:  Frequency: %d\n",(unsigned int)((event.parameters.frequency)+(tone==SEC_TONE_OFF ? LOF1 : LOF2)));
			mp_msg(MSGT_DEMUX, MSGL_V, "        SymbolRate: %d\n",event.parameters.u.qpsk.symbol_rate);
			mp_msg(MSGT_DEMUX, MSGL_V, "        FEC_inner:  %d\n",event.parameters.u.qpsk.fec_inner);
			mp_msg(MSGT_DEMUX, MSGL_V, "\n");
			break;
			case FE_QAM:
			mp_msg(MSGT_DEMUX, MSGL_V, "Event:  Frequency: %d\n",event.parameters.frequency);
			mp_msg(MSGT_DEMUX, MSGL_V, "        SymbolRate: %d\n",event.parameters.u.qpsk.symbol_rate);
			mp_msg(MSGT_DEMUX, MSGL_V, "        FEC_inner:  %d\n",event.parameters.u.qpsk.fec_inner);
			break;
			default:
			break;
		}

		strength=0;
		ioctl(fd_frontend,FE_READ_BER,&strength);
		mp_msg(MSGT_DEMUX, MSGL_V, "Bit error rate: %d\n",strength);

		strength=0;
		ioctl(fd_frontend,FE_READ_SIGNAL_STRENGTH,&strength);
		mp_msg(MSGT_DEMUX, MSGL_V, "Signal strength: %d\n",strength);

		strength=0;
		ioctl(fd_frontend,FE_READ_SNR,&strength);
		mp_msg(MSGT_DEMUX, MSGL_V, "SNR: %d\n",strength);

		festatus=0;
		ioctl(fd_frontend,FE_READ_STATUS,&festatus);
		print_status(festatus);
	}
	else
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "Not able to lock to the signal on the given frequency\n");
		return -1;
	}
	return 0;
}

#else

static int check_status(int fd_frontend,FrontendParameters* feparams,int tone)
{
	int i,res;
	int32_t strength;
	fe_status_t festatus;
	FrontendEvent event;
	FrontendInfo fe_info;
	struct pollfd pfd[1];

	i = 0; res = -1;
	while ((i < 3) && (res < 0))
	{
		if (ioctl(fd_frontend,FE_SET_FRONTEND,feparams) < 0)
		{
			mp_msg(MSGT_DEMUX, MSGL_ERR, "ERROR tuning channel\n");
			return -1;
		}

		pfd[0].fd = fd_frontend;
		pfd[0].events = POLLIN;

		if (poll(pfd,1,10000))
		{
			if (pfd[0].revents & POLLIN)
			{
				mp_msg(MSGT_DEMUX, MSGL_V, "Getting frontend event\n");
				if ( ioctl(fd_frontend, FE_GET_EVENT, &event) < 0)
				{
					mp_msg(MSGT_DEMUX, MSGL_ERR, "FE_GET_EVENT");
					return -1;
				}
				mp_msg(MSGT_DEMUX, MSGL_V, "Received ");
				switch(event.type)
				{
					case FE_UNEXPECTED_EV:
					mp_msg(MSGT_DEMUX, MSGL_V, "unexpected event\n");
					res = -1;
					break;

					case FE_FAILURE_EV:
					mp_msg(MSGT_DEMUX, MSGL_V, "failure event\n");
					res = -1;
					break;

					case FE_COMPLETION_EV:
					mp_msg(MSGT_DEMUX, MSGL_V, "completion event\n");
					res = 0;
					break;
				}
			}
			i++;
		}
	}

	if (res > 0)
	switch (event.type)
	{
		case FE_UNEXPECTED_EV: mp_msg(MSGT_DEMUX, MSGL_V, "FE_UNEXPECTED_EV\n");
			break;
		case FE_COMPLETION_EV: mp_msg(MSGT_DEMUX, MSGL_V, "FE_COMPLETION_EV\n");
			break;
		case FE_FAILURE_EV: mp_msg(MSGT_DEMUX, MSGL_V, "FE_FAILURE_EV\n");
			break;
	}

	if (event.type == FE_COMPLETION_EV)
	{
		switch(fe_info.type)
		{
			case FE_OFDM:
			mp_msg(MSGT_DEMUX, MSGL_V, "Event:  Frequency: %d\n",event.u.completionEvent.Frequency);
			break;

			case FE_QPSK:
			mp_msg(MSGT_DEMUX, MSGL_V, "Event:  Frequency: %d\n",(unsigned int)((event.u.completionEvent.Frequency)+(tone==SEC_TONE_OFF ? LOF1 : LOF2)));
			mp_msg(MSGT_DEMUX, MSGL_V, "        SymbolRate: %d\n",event.u.completionEvent.u.qpsk.SymbolRate);
			mp_msg(MSGT_DEMUX, MSGL_V, "        FEC_inner:  %d\n",event.u.completionEvent.u.qpsk.FEC_inner);
			mp_msg(MSGT_DEMUX, MSGL_V, "\n");
			break;

			case FE_QAM:
			mp_msg(MSGT_DEMUX, MSGL_V, "Event:  Frequency: %d\n",event.u.completionEvent.Frequency);
			mp_msg(MSGT_DEMUX, MSGL_V, "        SymbolRate: %d\n",event.u.completionEvent.u.qpsk.SymbolRate);
			mp_msg(MSGT_DEMUX, MSGL_V, "        FEC_inner:  %d\n",event.u.completionEvent.u.qpsk.FEC_inner);
			break;

			default:
			break;
		}

		strength=0;
		ioctl(fd_frontend,FE_READ_BER,&strength);
		mp_msg(MSGT_DEMUX, MSGL_V, "Bit error rate: %d\n",strength);

		strength=0;
		ioctl(fd_frontend,FE_READ_SIGNAL_STRENGTH,&strength);
		mp_msg(MSGT_DEMUX, MSGL_V, "Signal strength: %d\n",strength);

		strength=0;
		ioctl(fd_frontend,FE_READ_SNR,&strength);
		mp_msg(MSGT_DEMUX, MSGL_V, "SNR: %d\n",strength);

		festatus=0;
		ioctl(fd_frontend,FE_READ_STATUS,&festatus);

		mp_msg(MSGT_DEMUX, MSGL_V, "FE_STATUS:");
		if (festatus & FE_HAS_POWER) mp_msg(MSGT_DEMUX, MSGL_V, " FE_HAS_POWER");
		if (festatus & FE_HAS_SIGNAL) mp_msg(MSGT_DEMUX, MSGL_V, " FE_HAS_SIGNAL");
		if (festatus & FE_SPECTRUM_INV) mp_msg(MSGT_DEMUX, MSGL_V, " FE_SPECTRUM_INV");
		if (festatus & FE_HAS_LOCK) mp_msg(MSGT_DEMUX, MSGL_V, " FE_HAS_LOCK");
		if (festatus & FE_HAS_CARRIER) mp_msg(MSGT_DEMUX, MSGL_V, " FE_HAS_CARRIER");
		if (festatus & FE_HAS_VITERBI) mp_msg(MSGT_DEMUX, MSGL_V, " FE_HAS_VITERBI");
		if (festatus & FE_HAS_SYNC) mp_msg(MSGT_DEMUX, MSGL_V, " FE_HAS_SYNC");
		if (festatus & FE_TUNER_HAS_LOCK) mp_msg(MSGT_DEMUX, MSGL_V, " FE_TUNER_HAS_LOCK");
		mp_msg(MSGT_DEMUX, MSGL_V, "\n");
	}
	else
	{
		mp_msg(MSGT_DEMUX, MSGL_V, "Not able to lock to the signal on the given frequency\n");
		return -1;
	}
	return 0;
}
#endif

#ifdef HAVE_DVB_HEAD

struct diseqc_cmd {
   struct dvb_diseqc_master_cmd cmd;
   uint32_t wait;
};

static void diseqc_send_msg(int fd, fe_sec_voltage_t v, struct diseqc_cmd *cmd,
		     fe_sec_tone_mode_t t, fe_sec_mini_cmd_t b)
{
   ioctl(fd, FE_SET_TONE, SEC_TONE_OFF);
   ioctl(fd, FE_SET_VOLTAGE, v);
   usleep(15 * 1000);
   ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &cmd->cmd);
   usleep(cmd->wait * 1000);
   usleep(15 * 1000);
   ioctl(fd, FE_DISEQC_SEND_BURST, b);
   usleep(15 * 1000);
   ioctl(fd, FE_SET_TONE, t);
}




/* digital satellite equipment control,
 * specification is available from http://www.eutelsat.com/
 */
static int head_diseqc(int secfd, int sat_no, int pol, int hi_lo)
{
   struct diseqc_cmd cmd =  { {{0xe0, 0x10, 0x38, 0xf0, 0x00, 0x00}, 4}, 0 };

   /* param: high nibble: reset bits, low nibble set bits,
    * bits are: option, position, polarizaion, band
    */
   cmd.cmd.msg[3] =
       0xf0 | (((sat_no * 4) & 0x0f) | (hi_lo ? 1 : 0) | (pol ? 0 : 2));

   diseqc_send_msg(secfd, pol ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18,
		   &cmd, hi_lo ? SEC_TONE_ON : SEC_TONE_OFF,
		   (sat_no / 4) % 2 ? SEC_MINI_B : SEC_MINI_A);

   return 1;
}

#endif


static int tune_it(int fd_frontend, int fd_sec, unsigned int freq, unsigned int srate, char pol, int tone,
	fe_spectral_inversion_t specInv, unsigned int diseqc, fe_modulation_t modulation, fe_code_rate_t HP_CodeRate,
	fe_transmit_mode_t TransmissionMode, fe_guard_interval_t guardInterval, fe_bandwidth_t bandwidth)
{
  int res;
#ifdef HAVE_DVB_HEAD
  struct dvb_frontend_parameters feparams;
  struct dvb_frontend_info fe_info;
  fe_sec_voltage_t voltage;
#else
  FrontendParameters feparams;
  FrontendInfo fe_info;
  secVoltage voltage;
  struct secStatus sec_state;
#endif


  mp_msg(MSGT_DEMUX, MSGL_V,  "TUNE_IT, fd_frontend %d, fd_sec %d, freq %lu, srate %lu, pol %c, tone %i, specInv, diseqc %u, fe_modulation_t modulation,fe_code_rate_t HP_CodeRate, fe_transmit_mode_t TransmissionMode,fe_guard_interval_t guardInterval, fe_bandwidth_t bandwidth\n",
	    fd_frontend, fd_sec, freq, srate, pol, tone, diseqc);


  if ( (res = ioctl(fd_frontend,FE_GET_INFO, &fe_info) < 0))
  {
  	mp_msg(MSGT_DEMUX, MSGL_ERR, "FE_GET_INFO: ");
	return -1;
  }


#ifdef HAVE_DVB_HEAD
  mp_msg(MSGT_DEMUX, MSGL_V, "Using DVB card \"%s\"\n",fe_info.name);
#endif

  switch(fe_info.type)
  {
    case FE_OFDM:
#ifdef HAVE_DVB_HEAD
      if (freq < 1000000) freq*=1000UL;
      feparams.frequency=freq;
      feparams.inversion=INVERSION_OFF;
      feparams.u.ofdm.bandwidth=bandwidth;
      feparams.u.ofdm.code_rate_HP=HP_CodeRate;
      feparams.u.ofdm.code_rate_LP=LP_CODERATE_DEFAULT;
      feparams.u.ofdm.constellation=modulation;
      feparams.u.ofdm.transmission_mode=TransmissionMode;
      feparams.u.ofdm.guard_interval=guardInterval;
      feparams.u.ofdm.hierarchy_information=HIERARCHY_DEFAULT;
#else
      if (freq < 1000000) freq*=1000UL;
      feparams.Frequency=freq;
      feparams.Inversion=INVERSION_OFF;
      feparams.u.ofdm.bandWidth=bandwidth;
      feparams.u.ofdm.HP_CodeRate=HP_CodeRate;
      feparams.u.ofdm.LP_CodeRate=LP_CODERATE_DEFAULT;
      feparams.u.ofdm.Constellation=modulation;
      feparams.u.ofdm.TransmissionMode=TransmissionMode;
      feparams.u.ofdm.guardInterval=guardInterval;
      feparams.u.ofdm.HierarchyInformation=HIERARCHY_DEFAULT;
#endif
      mp_msg(MSGT_DEMUX, MSGL_V, "tuning DVB-T (%s) to %d Hz\n",DVB_T_LOCATION,freq);
      break;
    case FE_QPSK:
#ifdef HAVE_DVB_HEAD
      mp_msg(MSGT_DEMUX, MSGL_V, "tuning DVB-S to L-Band:%d, Pol:%c Srate=%d, 22kHz=%s\n",feparams.frequency,pol,srate,tone == SEC_TONE_ON ? "on" : "off");
#else
      mp_msg(MSGT_DEMUX, MSGL_V, "tuning DVB-S to L-Band:%d, Pol:%c Srate=%d, 22kHz=%s\n",feparams.Frequency,pol,srate,tone == SEC_TONE_ON ? "on" : "off");
#endif
      if ((pol=='h') || (pol=='H'))
      {
        voltage = SEC_VOLTAGE_18;
      }
      else
      {
        voltage = SEC_VOLTAGE_13;
      }
#ifdef HAVE_DVB_HEAD
      if (ioctl(fd_frontend,FE_SET_VOLTAGE,voltage) < 0)
      {
#else
      if (ioctl(fd_sec,SEC_SET_VOLTAGE,voltage) < 0)
      {
#endif
         mp_msg(MSGT_DEMUX, MSGL_ERR, "ERROR setting voltage\n");
      }

      if (freq > 2200000)
      {
        // this must be an absolute frequency
        if (freq < SLOF)
	{
#ifdef HAVE_DVB_HEAD
          feparams.frequency=(freq-LOF1);
#else
          feparams.Frequency=(freq-LOF1);
#endif
          if (tone < 0) tone = SEC_TONE_OFF;
        }
	else
	{
#ifdef HAVE_DVB_HEAD
          feparams.frequency=(freq-LOF2);
#else
          feparams.Frequency=(freq-LOF2);
#endif
          if (tone < 0) tone = SEC_TONE_ON;
        }
      }
      else
      {
        // this is an L-Band frequency
#ifdef HAVE_DVB_HEAD
       feparams.frequency=freq;
#else
       feparams.Frequency=freq;
#endif
      }

#ifdef HAVE_DVB_HEAD
      feparams.inversion=specInv;
      feparams.u.qpsk.symbol_rate=srate;
      feparams.u.qpsk.fec_inner=FEC_AUTO;
#else
      feparams.Inversion=specInv;
      feparams.u.qpsk.SymbolRate=srate;
      feparams.u.qpsk.FEC_inner=FEC_AUTO;
#endif

#ifdef HAVE_DVB_HEAD
      if (ioctl(fd_frontend, FE_SET_TONE,tone) < 0)
      {
         mp_msg(MSGT_DEMUX, MSGL_ERR, "ERROR setting tone\n");
      }
#else
      if (ioctl(fd_sec, SEC_SET_TONE,tone) < 0)
      {
         mp_msg(MSGT_DEMUX, MSGL_ERR, "ERROR setting tone\n");
      }
#endif

#ifdef HAVE_DVB_HEAD
  //#warning DISEQC is unimplemented for HAVE_DVB_HEAD
  if(diseqc > 0)
  {
    int ipol = (pol == 'V' ? 1 : 0);
    int hiband = (freq >= SLOF);

   if(head_diseqc(fd_frontend, diseqc-1, ipol, hiband))
   {
	mp_msg(MSGT_DEMUX, MSGL_V, "DISEQC SETTING SUCCEDED\n");
   }
   else
   {
	mp_msg(MSGT_DEMUX, MSGL_V, "DISEQC SETTING FAILED\n");
   }
  }
#else
      if (diseqc > 0)
      {
        struct secCommand scmd;
        struct secCmdSequence scmds;

        scmds.continuousTone = tone;
        scmds.voltage = voltage;
        /*
        scmds.miniCommand = toneBurst ? SEC_MINI_B : SEC_MINI_A;
        */
        scmds.miniCommand = SEC_MINI_NONE;

        scmd.type = 0;
        scmds.numCommands = 1;
        scmds.commands = &scmd;

        scmd.u.diseqc.addr = 0x10;
        scmd.u.diseqc.cmd = 0x38;
        scmd.u.diseqc.numParams = 1;
        scmd.u.diseqc.params[0] = 0xf0 |
                                  (((diseqc - 1) << 2) & 0x0c) |
                                  (voltage==SEC_VOLTAGE_18 ? 0x02 : 0) |
                                  (tone==SEC_TONE_ON ? 0x01 : 0);

        if (ioctl(fd_sec,SEC_SEND_SEQUENCE,&scmds) < 0)
	{
          mp_msg(MSGT_DEMUX, MSGL_ERR, "Error sending DisEqC");
          return -1;
        }
      }
#endif
      break;
    case FE_QAM:
      mp_msg(MSGT_DEMUX, MSGL_V, "tuning DVB-C to %d, srate=%d\n",freq,srate);
#ifdef HAVE_DVB_HEAD
      feparams.frequency=freq;
      feparams.inversion=INVERSION_OFF;
      feparams.u.qam.symbol_rate = srate;
      feparams.u.qam.fec_inner = FEC_AUTO;
      feparams.u.qam.modulation = QAM_64;
#else
      feparams.Frequency=freq;
      feparams.Inversion=INVERSION_OFF;
      feparams.u.qam.SymbolRate = srate;
      feparams.u.qam.FEC_inner = FEC_AUTO;
      feparams.u.qam.QAM = QAM_64;
#endif
      break;
    default:
      mp_msg(MSGT_DEMUX, MSGL_V, "Unknown FE type. Aborting\n");
      exit(-1);
  }
  usleep(100000);

#ifndef HAVE_DVB_HEAD
  if (fd_sec) SecGetStatus(fd_sec, &sec_state);
#endif

  return(check_status(fd_frontend,&feparams,tone));
}
