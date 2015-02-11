/* dvbtune - tune.c

   Copyright (C) Dave Chapman 2001,2002

   Modified for use with MPlayer, for details see the changelog at
   http://svn.mplayerhq.hu/mplayer/trunk/
   $Id$

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
#include <sys/ioctl.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>

#include "config.h"
#include "osdep/io.h"
#include "dvbin.h"
#include "dvb_tune.h"
#include "common/msg.h"

int dvb_get_tuner_type(int fe_fd, struct mp_log *log)
{
    struct dvb_frontend_info fe_info;
    int res = ioctl(fe_fd, FE_GET_INFO, &fe_info);
    if (res < 0) {
        mp_err(log, "FE_GET_INFO error: %d, FD: %d\n\n", errno, fe_fd);
        return 0;
    }

    switch (fe_info.type) {
    case FE_OFDM:
        mp_verbose(log, "TUNER TYPE SEEMS TO BE DVB-T\n");
        return TUNER_TER;

    case FE_QPSK:
        mp_verbose(log, "TUNER TYPE SEEMS TO BE DVB-S\n");
        return TUNER_SAT;

    case FE_QAM:
        mp_verbose(log, "TUNER TYPE SEEMS TO BE DVB-C\n");
        return TUNER_CBL;

#ifdef DVB_ATSC
    case FE_ATSC:
        mp_verbose(log, "TUNER TYPE SEEMS TO BE DVB-ATSC\n");
        return TUNER_ATSC;
#endif
    default:
        mp_err(log, "UNKNOWN TUNER TYPE\n");
        return 0;
    }

}

int dvb_open_devices(dvb_priv_t *priv, int n, int demux_cnt)
{
    int i;
    char frontend_dev[32], dvr_dev[32], demux_dev[32];

    sprintf(frontend_dev, "/dev/dvb/adapter%d/frontend0", n);
    sprintf(dvr_dev, "/dev/dvb/adapter%d/dvr0", n);
    sprintf(demux_dev, "/dev/dvb/adapter%d/demux0", n);
    priv->fe_fd = open(frontend_dev, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (priv->fe_fd < 0) {
        MP_ERR(priv, "ERROR OPENING FRONTEND DEVICE %s: ERRNO %d\n",
               frontend_dev, errno);
        return 0;
    }
    priv->demux_fds_cnt = 0;
    MP_VERBOSE(priv, "DVB_OPEN_DEVICES(%d)\n", demux_cnt);
    for (i = 0; i < demux_cnt; i++) {
        priv->demux_fds[i] = open(demux_dev, O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (priv->demux_fds[i] < 0) {
            MP_ERR(priv, "ERROR OPENING DEMUX 0: %d\n", errno);
            return 0;
        } else {
            MP_VERBOSE(priv, "OPEN(%d), file %s: FD=%d, CNT=%d\n", i, demux_dev,
                       priv->demux_fds[i], priv->demux_fds_cnt);
            priv->demux_fds_cnt++;
        }
    }


    priv->dvr_fd = open(dvr_dev, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (priv->dvr_fd < 0) {
        MP_ERR(priv, "ERROR OPENING DVR DEVICE %s: %d\n", dvr_dev, errno);
        return 0;
    }

    return 1;
}


int dvb_fix_demuxes(dvb_priv_t *priv, int cnt)
{
    int i;
    char demux_dev[32];

    sprintf(demux_dev, "/dev/dvb/adapter%d/demux0", priv->card);
    MP_VERBOSE(priv, "FIX %d -> %d\n", priv->demux_fds_cnt, cnt);
    if (priv->demux_fds_cnt >= cnt) {
        for (i = priv->demux_fds_cnt - 1; i >= cnt; i--) {
            MP_VERBOSE(priv, "FIX, CLOSE fd(%d): %d\n", i, priv->demux_fds[i]);
            close(priv->demux_fds[i]);
        }
        priv->demux_fds_cnt = cnt;
    } else if (priv->demux_fds_cnt < cnt) {
        for (i = priv->demux_fds_cnt; i < cnt; i++) {
            priv->demux_fds[i] = open(demux_dev,
                                      O_RDWR | O_NONBLOCK | O_CLOEXEC);
            MP_VERBOSE(priv, "FIX, OPEN fd(%d): %d\n", i, priv->demux_fds[i]);
            if (priv->demux_fds[i] < 0) {
                MP_ERR(priv, "ERROR OPENING DEMUX 0: %d\n", errno);
                return 0;
            } else
                priv->demux_fds_cnt++;
        }
    }

    return 1;
}

int dvb_set_ts_filt(dvb_priv_t *priv, int fd, uint16_t pid,
                    dmx_pes_type_t pestype)
{
    int i;
    struct dmx_pes_filter_params pesFilterParams;

    pesFilterParams.pid     = pid;
    pesFilterParams.input   = DMX_IN_FRONTEND;
    pesFilterParams.output  = DMX_OUT_TS_TAP;
    pesFilterParams.pes_type = pestype;
    pesFilterParams.flags   = DMX_IMMEDIATE_START;

    {
        int buffersize = 64 * 1024;
        if (ioctl(fd, DMX_SET_BUFFER_SIZE, buffersize) < 0)
            MP_ERR(priv, "ERROR IN DMX_SET_BUFFER_SIZE %i for fd %d: ERRNO: %d\n",
                   pid, fd, errno);
    }

    errno = 0;
    if ((i = ioctl(fd, DMX_SET_PES_FILTER, &pesFilterParams)) < 0) {
        MP_ERR(priv, "ERROR IN SETTING DMX_FILTER %i for fd %d: ERRNO: %d\n",
               pid, fd, errno);
        return 0;
    }

    MP_VERBOSE(priv, "SET PES FILTER ON PID %d to fd %d, RESULT: %d, ERRNO: %d\n",
               pid, fd, i, errno);
    return 1;
}

int dvb_get_pmt_pid(dvb_priv_t *priv, int card, int service_id)
{
    /* We need special filters on the demux,
       so open one locally, and close also here. */
    char demux_dev[32];
    sprintf(demux_dev, "/dev/dvb/adapter%d/demux0", card);

    struct dmx_sct_filter_params fparams;

    memset(&fparams, 0, sizeof(fparams));
    fparams.pid = 0;
    fparams.filter.filter[0] = 0x00;
    fparams.filter.mask[0] = 0xff;
    fparams.timeout = 0;
    fparams.flags = DMX_IMMEDIATE_START | DMX_CHECK_CRC;

    int pat_fd;
    if ((pat_fd = open(demux_dev, O_RDWR)) < 0) {
        MP_ERR(priv, "Opening PAT DEMUX failed, error: %d", errno);
        return -1;
    }

    if (ioctl(pat_fd, DMX_SET_FILTER, &fparams) == -1) {
        MP_ERR(priv, "ioctl DMX_SET_FILTER failed, error: %d", errno);
        close(pat_fd);
        return -1;
    }

    int bytes_read;
    int section_length;
    unsigned char buft[4096];
    unsigned char *bufptr = buft;

    int pmt_pid = -1;

    bool pat_read = false;
    while (!pat_read) {
        if (((bytes_read =
                  read(pat_fd, bufptr,
                       sizeof(buft))) < 0) && errno == EOVERFLOW)
            bytes_read = read(pat_fd, bufptr, sizeof(buft));
        if (bytes_read < 0) {
            MP_ERR(priv, "PAT: read_sections: read error: %d", errno);
            close(pat_fd);
            return -1;
        }

        section_length = ((bufptr[1] & 0x0f) << 8) | bufptr[2];
        if (bytes_read != section_length + 3)
            continue;

        bufptr += 8;
        section_length -= 8;

        /* assumes one section contains the whole pat */
        pat_read = true;
        while (section_length > 0) {
            int this_service_id = (bufptr[0] << 8) | bufptr[1];
            if (this_service_id == service_id) {
                pmt_pid = ((bufptr[2] & 0x1f) << 8) | bufptr[3];
                section_length = 0;
            }
            bufptr += 4;
            section_length -= 4;
        }
    }
    close(pat_fd);

    return pmt_pid;
}

int dvb_demux_stop(int fd)
{
    return ioctl(fd, DMX_STOP) == 0;
}

int dvb_demux_start(int fd)
{
    return ioctl(fd, DMX_START) == 0;
}

static void print_status(dvb_priv_t *priv, fe_status_t festatus)
{
    MP_VERBOSE(priv, "FE_STATUS:");
    if (festatus & FE_HAS_SIGNAL)
        MP_VERBOSE(priv, " FE_HAS_SIGNAL");
    if (festatus & FE_TIMEDOUT)
        MP_VERBOSE(priv, " FE_TIMEDOUT");
    if (festatus & FE_HAS_LOCK)
        MP_VERBOSE(priv, " FE_HAS_LOCK");
    if (festatus & FE_HAS_CARRIER)
        MP_VERBOSE(priv, " FE_HAS_CARRIER");
    if (festatus & FE_HAS_VITERBI)
        MP_VERBOSE(priv, " FE_HAS_VITERBI");
    if (festatus & FE_HAS_SYNC)
        MP_VERBOSE(priv, " FE_HAS_SYNC");
    MP_VERBOSE(priv, "\n");
}

static int check_status(dvb_priv_t *priv, int fd_frontend, int tmout)
{
    int32_t strength;
    fe_status_t festatus;
    struct pollfd pfd[1];
    int ok = 0, locks = 0;
    time_t tm1, tm2;

    pfd[0].fd = fd_frontend;
    pfd[0].events = POLLPRI;

    MP_VERBOSE(priv, "Getting frontend status\n");
    tm1 = tm2 = time((time_t *) NULL);
    while (!ok) {
        festatus = 0;
        if (poll(pfd, 1, tmout * 1000) > 0) {
            if (pfd[0].revents & POLLPRI) {
                if (ioctl(fd_frontend, FE_READ_STATUS, &festatus) >= 0) {
                    if (festatus & FE_HAS_LOCK)
                        locks++;
                }
            }
        }
        usleep(10000);
        tm2 = time((time_t *) NULL);
        if ((festatus & FE_TIMEDOUT) || (locks >= 2) || (tm2 - tm1 >= tmout))
            ok = 1;
    }

    if (festatus & FE_HAS_LOCK) {
        strength = 0;
        if (ioctl(fd_frontend, FE_READ_BER, &strength) >= 0)
            MP_VERBOSE(priv, "Bit error rate: %d\n", strength);

        strength = 0;
        if (ioctl(fd_frontend, FE_READ_SIGNAL_STRENGTH, &strength) >= 0)
            MP_VERBOSE(priv, "Signal strength: %d\n", strength);

        strength = 0;
        if (ioctl(fd_frontend, FE_READ_SNR, &strength) >= 0)
            MP_VERBOSE(priv, "SNR: %d\n", strength);

        strength = 0;
        if (ioctl(fd_frontend, FE_READ_UNCORRECTED_BLOCKS, &strength) >= 0)
            MP_VERBOSE(priv, "UNC: %d\n", strength);

        print_status(priv, festatus);
    } else {
        MP_ERR(priv, "Not able to lock to the signal on the given frequency, "
               "timeout: %d\n", tmout);
        return -1;
    }
    return 0;
}

struct diseqc_cmd {
    struct dvb_diseqc_master_cmd cmd;
    uint32_t wait;
};

static int diseqc_send_msg(int fd, fe_sec_voltage_t v, struct diseqc_cmd *cmd,
                           fe_sec_tone_mode_t t, fe_sec_mini_cmd_t b)
{
    if (ioctl(fd, FE_SET_TONE, SEC_TONE_OFF) == -1)
        return -1;
    if (ioctl(fd, FE_SET_VOLTAGE, v) == -1)
        return -1;
    usleep(15 * 1000);
    if (ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &cmd->cmd) == -1)
        return -1;
    usleep(cmd->wait * 1000);
    usleep(15 * 1000);
    if (ioctl(fd, FE_DISEQC_SEND_BURST, b) == -1)
        return -1;
    usleep(15 * 1000);
    if (ioctl(fd, FE_SET_TONE, t) == -1)
        return -1;

    return 0;
}

/* digital satellite equipment control,
 * specification is available from http://www.eutelsat.com/
 */
static int do_diseqc(int secfd, int sat_no, int polv, int hi_lo)
{
    struct diseqc_cmd cmd =  { {{0xe0, 0x10, 0x38, 0xf0, 0x00, 0x00}, 4}, 0 };

    /* param: high nibble: reset bits, low nibble set bits,
     * bits are: option, position, polarizaion, band
     */
    cmd.cmd.msg[3] = 0xf0 | (((sat_no * 4) & 0x0f) | (hi_lo ? 1 : 0) | (polv ? 0 : 2));

    return diseqc_send_msg(secfd, polv ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18,
                           &cmd, hi_lo ? SEC_TONE_ON : SEC_TONE_OFF,
                           (sat_no / 4) % 2 ? SEC_MINI_B : SEC_MINI_A);
}

static int tune_it(dvb_priv_t *priv, int fd_frontend, int fd_sec,
                   unsigned int freq, unsigned int srate, char pol, int tone,
                   bool is_dvb_s2, int stream_id,
                   fe_spectral_inversion_t specInv, unsigned int diseqc,
                   fe_modulation_t modulation,
                   fe_code_rate_t HP_CodeRate,
                   fe_transmit_mode_t TransmissionMode,
                   fe_guard_interval_t guardInterval,
                   fe_bandwidth_t bandwidth,
                   fe_code_rate_t LP_CodeRate, fe_hierarchy_t hier,
                   int timeout)
{
    int hi_lo = 0, dfd;
    struct dvb_frontend_parameters feparams;
    struct dvb_frontend_info fe_info;

    MP_VERBOSE(priv, "TUNE_IT, fd_frontend %d, fd_sec %d\nfreq %lu, srate %lu, "
               "pol %c, tone %i, diseqc %u\n", fd_frontend, fd_sec,
               (long unsigned int)freq, (long unsigned int)srate, pol,
               tone, diseqc);

    memset(&feparams, 0, sizeof(feparams));
    if (ioctl(fd_frontend, FE_GET_INFO, &fe_info) < 0) {
        MP_FATAL(priv, "FE_GET_INFO FAILED\n");
        return -1;
    }

    MP_VERBOSE(priv, "Using DVB card \"%s\"\n", fe_info.name);

    {
        /* discard stale QPSK events */
        struct dvb_frontend_event ev;
        while (true) {
            if (ioctl(fd_frontend, FE_GET_EVENT, &ev) == -1)
                break;
        }
    }

    switch (fe_info.type) {
    case FE_OFDM:
        if (freq < 1000000)
            freq *= 1000UL;
        feparams.frequency = freq;
        feparams.inversion = specInv;
        feparams.u.ofdm.bandwidth = bandwidth;
        feparams.u.ofdm.code_rate_HP = HP_CodeRate;
        feparams.u.ofdm.code_rate_LP = LP_CodeRate;
        feparams.u.ofdm.constellation = modulation;
        feparams.u.ofdm.transmission_mode = TransmissionMode;
        feparams.u.ofdm.guard_interval = guardInterval;
        feparams.u.ofdm.hierarchy_information = hier;
        MP_VERBOSE(priv, "tuning DVB-T to %d Hz, bandwidth: %d\n",
                   freq, bandwidth);
        if (ioctl(fd_frontend, FE_SET_FRONTEND, &feparams) < 0) {
            MP_ERR(priv, "ERROR tuning channel\n");
            return -1;
        }
        break;
    case FE_QPSK:
        // DVB-S
        if (freq > 2200000) {
            // this must be an absolute frequency
            if (freq < SLOF) {
                freq = feparams.frequency = (freq - LOF1);
                hi_lo = 0;
            } else {
                freq = feparams.frequency = (freq - LOF2);
                hi_lo = 1;
            }
        } else {
            // this is an L-Band frequency
            feparams.frequency = freq;
        }

        feparams.inversion = specInv;
        feparams.u.qpsk.symbol_rate = srate;
        feparams.u.qpsk.fec_inner = HP_CodeRate;
        dfd = fd_frontend;

        MP_VERBOSE(priv, "tuning DVB-S%sto Freq: %u, Pol: %c Srate: %d, "
                   "22kHz: %s, LNB:  %d\n", is_dvb_s2 ? "2 " : " ", freq,
                   pol, srate, hi_lo ? "on" : "off", diseqc);

        if (do_diseqc(dfd, diseqc, (pol == 'V' ? 1 : 0), hi_lo) == 0) {
            MP_VERBOSE(priv, "DISEQC setting succeeded\n");
        } else {
            MP_ERR(priv, "DISEQC setting failed\n");
            return -1;
        }
        usleep(100000);

#ifdef DVB_USE_S2API
        /* S2API is the DVB API new since 2.6.28.
         * It is needed to tune to new delivery systems, e.g. DVB-S2.
         * It takes a struct with a list of pairs of command + parameter.
         */

        fe_delivery_system_t delsys = SYS_DVBS;
        if (is_dvb_s2)
            delsys = SYS_DVBS2;
        fe_rolloff_t rolloff = ROLLOFF_AUTO;

        struct dtv_property p[] = {
            { .cmd = DTV_DELIVERY_SYSTEM, .u.data = delsys },
            { .cmd = DTV_FREQUENCY, .u.data = freq },
            { .cmd = DTV_MODULATION, .u.data = modulation },
            { .cmd = DTV_SYMBOL_RATE, .u.data = srate },
            { .cmd = DTV_INNER_FEC, .u.data = HP_CodeRate },
            { .cmd = DTV_INVERSION, .u.data = specInv },
            { .cmd = DTV_ROLLOFF, .u.data = rolloff },
            { .cmd = DTV_PILOT, .u.data = PILOT_AUTO },
            { .cmd = DTV_STREAM_ID, .u.data = stream_id },
            { .cmd = DTV_TUNE },
        };
        struct dtv_properties cmdseq = {
            .num = sizeof(p) / sizeof(p[0]),
            .props = p
        };
        MP_VERBOSE(priv, "Tuning via S2API, channel is DVB-S%s.\n",
                   is_dvb_s2 ? "2" : "");
        if ((ioctl(fd_frontend, FE_SET_PROPERTY, &cmdseq)) == -1) {
            MP_ERR(priv, "ERROR tuning channel\n");
            return -1;
        }
#else
        MP_VERBOSE(priv, "Tuning via DVB-API version 3.\n");
        if (is_dvb_s2) {
            MP_ERR(priv, "ERROR: Can not tune to S2 channel, S2-API not "
                         "available, will tune to DVB-S!\n");
        }
        if (ioctl(fd_frontend, FE_SET_FRONTEND, &feparams) < 0) {
            MP_ERR(priv, "ERROR tuning channel\n");
            return -1;
        }
#endif
        break;
    case FE_QAM:
        feparams.frequency = freq;
        feparams.inversion = specInv;
        feparams.u.qam.symbol_rate = srate;
        feparams.u.qam.fec_inner = HP_CodeRate;
        feparams.u.qam.modulation = modulation;
        MP_VERBOSE(priv, "tuning DVB-C to %d, srate=%d\n", freq, srate);
        if (ioctl(fd_frontend, FE_SET_FRONTEND, &feparams) < 0) {
            MP_ERR(priv, "ERROR tuning channel\n");
            return -1;
        }
        break;
#ifdef DVB_ATSC
    case FE_ATSC:
        feparams.frequency = freq;
        feparams.u.vsb.modulation = modulation;
        MP_VERBOSE(priv, "tuning ATSC to %d, modulation=%d\n", freq, modulation);
        if (ioctl(fd_frontend, FE_SET_FRONTEND, &feparams) < 0) {
            MP_ERR(priv, "ERROR tuning channel\n");
            return -1;
        }
        break;
#endif
    default:
        MP_VERBOSE(priv, "Unknown FE type. Aborting\n");
        return 0;
    }

    return check_status(priv, fd_frontend, timeout);
}

int dvb_tune(dvb_priv_t *priv, int freq, char pol, int srate, int diseqc,
             int tone,
             bool is_dvb_s2, int stream_id, fe_spectral_inversion_t specInv,
             fe_modulation_t modulation, fe_guard_interval_t guardInterval,
             fe_transmit_mode_t TransmissionMode, fe_bandwidth_t bandWidth,
             fe_code_rate_t HP_CodeRate,
             fe_code_rate_t LP_CodeRate, fe_hierarchy_t hier,
             int timeout)
{
    MP_INFO(priv, "dvb_tune Freq: %lu\n", (long unsigned int) freq);

    int ris = tune_it(priv, priv->fe_fd, priv->sec_fd, freq, srate, pol, tone,
                      is_dvb_s2, stream_id, specInv, diseqc, modulation,
                      HP_CodeRate, TransmissionMode, guardInterval,
                      bandWidth, LP_CodeRate, hier, timeout);

    if (ris != 0)
        MP_INFO(priv, "dvb_tune, TUNING FAILED\n");

    return ris == 0;
}
