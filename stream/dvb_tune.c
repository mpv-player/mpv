/* dvbtune - tune.c

   Copyright (C) Dave Chapman 2001,2002
   Copyright (C) Rozhuk Ivan <rozhuk.im@gmail.com> 2016 - 2017

   Modified for use with MPlayer, for details see the changelog at
   http://svn.mplayerhq.hu/mplayer/trunk/
   $Id$

   This file is part of mpv.

   mpv is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   mpv is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with mpv.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>

#include "common/common.h"
#include "osdep/io.h"
#include "osdep/poll_wrapper.h"
#include "osdep/timer.h"
#include "dvbin.h"
#include "dvb_tune.h"
#include "common/msg.h"

/* Keep in sync with enum fe_delivery_system. */
static const char *dvb_delsys_str[] = {
    "UNDEFINED",
    "DVB-C ANNEX A",
    "DVB-C ANNEX B",
    "DVB-T",
    "DSS",
    "DVB-S",
    "DVB-S2",
    "DVB-H",
    "ISDBT",
    "ISDBS",
    "ISDBC",
    "ATSC",
    "ATSCMH",
    "DTMB",
    "CMMB",
    "DAB",
    "DVB-T2",
    "TURBO",
    "DVB-C ANNEX C",
    NULL
};

const char *get_dvb_delsys(unsigned int delsys)
{
    if (SYS_DVB__COUNT__ <= delsys)
        return dvb_delsys_str[0];
    return dvb_delsys_str[delsys];
}

unsigned int dvb_get_tuner_delsys_mask(int fe_fd, struct mp_log *log)
{
    unsigned int ret_mask = 0, delsys;
    struct dtv_property prop[1];
    struct dtv_properties cmdseq = {.num = 1, .props = prop};

    prop[0].cmd = DTV_ENUM_DELSYS;
    if (ioctl(fe_fd, FE_GET_PROPERTY, &cmdseq) < 0) {
        mp_err(log, "DVBv5: FE_GET_PROPERTY(DTV_ENUM_DELSYS) error: %d\n", errno);
        return ret_mask;
    }
    unsigned int delsys_count = prop[0].u.buffer.len;
    if (delsys_count == 0) {
        mp_err(log, "DVBv5: Frontend returned no delivery systems!\n");
        return ret_mask;
    }
    mp_verbose(log, "DVBv5: Number of supported delivery systems: %d\n", delsys_count);
    for (unsigned int i = 0; i < delsys_count; i++) {
        delsys = (unsigned int)prop[0].u.buffer.data[i];
        DELSYS_SET(ret_mask, delsys);
        mp_verbose(log, " %s\n", get_dvb_delsys(delsys));
    }

    return ret_mask;
}

int dvb_open_devices(dvb_priv_t *priv, unsigned int adapter,
    unsigned int frontend, unsigned int demux_cnt)
{
    dvb_state_t *state = priv->state;

    char frontend_dev[100], dvr_dev[100], demux_dev[100];
    snprintf(frontend_dev, sizeof(frontend_dev), "/dev/dvb/adapter%u/frontend%u", adapter, frontend);
    snprintf(dvr_dev, sizeof(dvr_dev), "/dev/dvb/adapter%u/dvr0", adapter);
    snprintf(demux_dev, sizeof(demux_dev), "/dev/dvb/adapter%u/demux0", adapter);

    MP_VERBOSE(priv, "Opening frontend device %s\n", frontend_dev);
    state->fe_fd = open(frontend_dev, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (state->fe_fd < 0) {
        MP_ERR(priv, "Error opening frontend device: %d\n", errno);
        return 0;
    }

    state->demux_fds_cnt = 0;
    MP_VERBOSE(priv, "Opening %d demuxers\n", demux_cnt);
    for (unsigned int i = 0; i < demux_cnt; i++) {
        state->demux_fds[i] = open(demux_dev, O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (state->demux_fds[i] < 0) {
            MP_ERR(priv, "Error opening demux0: %d\n", errno);
            return 0;
        }
        state->demux_fds_cnt++;
    }

    state->dvr_fd = open(dvr_dev, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (state->dvr_fd < 0) {
        MP_ERR(priv, "Error opening dvr device %s: %d\n", dvr_dev, errno);
        return 0;
    }

    return 1;
}

int dvb_fix_demuxes(dvb_priv_t *priv, unsigned int cnt)
{
    dvb_state_t *state = priv->state;

    char demux_dev[100];
    snprintf(demux_dev, sizeof(demux_dev), "/dev/dvb/adapter%d/demux0",
            state->adapters[state->cur_adapter].devno);

    MP_VERBOSE(priv, "Changing demuxer count %d -> %d\n", state->demux_fds_cnt, cnt);
    if (state->demux_fds_cnt >= cnt) {
        for (int i = state->demux_fds_cnt - 1; i >= (int)cnt; i--) {
            close(state->demux_fds[i]);
        }
        state->demux_fds_cnt = cnt;
    } else {
        for (int i = state->demux_fds_cnt; i < cnt; i++) {
            state->demux_fds[i] = open(demux_dev,
                                      O_RDWR | O_NONBLOCK | O_CLOEXEC);
            if (state->demux_fds[i] < 0) {
                MP_ERR(priv, "Error opening demux0: %d\n", errno);
                return 0;
            }
            state->demux_fds_cnt++;
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
        int buffersize = 256 * 1024;
        if (ioctl(fd, DMX_SET_BUFFER_SIZE, buffersize) < 0)
            MP_ERR(priv, "Error in DMX_SET_BUFFER_SIZE %i: errno=%d\n",
                   pid, errno);
    }

    errno = 0;
    if ((i = ioctl(fd, DMX_SET_PES_FILTER, &pesFilterParams)) < 0) {
        MP_ERR(priv, "Error in DMX_SET_PES_FILTER %i: errno=%d\n",
               pid, errno);
        return 0;
    }

    return 1;
}

int dvb_get_pmt_pid(dvb_priv_t *priv, int devno, int service_id)
{
    /* We need special filters on the demux,
       so open one locally, and close also here. */
    char demux_dev[100];
    snprintf(demux_dev, sizeof(demux_dev), "/dev/dvb/adapter%d/demux0", devno);

    struct dmx_sct_filter_params fparams = {0};
    fparams.pid = 0;
    fparams.filter.filter[0] = 0x00;
    fparams.filter.mask[0] = 0xff;
    fparams.timeout = 0;
    fparams.flags = DMX_IMMEDIATE_START | DMX_CHECK_CRC;

    int pat_fd;
    if ((pat_fd = open(demux_dev, O_RDWR)) < 0) {
        MP_ERR(priv, "Opening PAT demux failed: %d", errno);
        return -1;
    }

    if (ioctl(pat_fd, DMX_SET_FILTER, &fparams) < 0) {
        MP_ERR(priv, "ioctl DMX_SET_FILTER failed: %d", errno);
        close(pat_fd);
        return -1;
    }

    int bytes_read;
    unsigned char buft[4096];
    unsigned char *bufptr = buft;

    int pmt_pid = -1;

    bool pat_read = false;
    while (!pat_read) {
        bytes_read = read(pat_fd, bufptr, sizeof(buft));
        if (bytes_read < 0 && errno == EOVERFLOW)
            bytes_read = read(pat_fd, bufptr, sizeof(buft));
        if (bytes_read < 0) {
            MP_ERR(priv, "PAT: read error: %d", errno);
            close(pat_fd);
            return -1;
        }

        int section_length = ((bufptr[1] & 0x0f) << 8) | bufptr[2];
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

static int check_status(dvb_priv_t *priv, int fd_frontend, float tmout)
{
    fe_status_t festatus;
    bool ok = false;
    int locks = 0;

    struct pollfd pfd[1];
    pfd[0].fd = fd_frontend;
    pfd[0].events = POLLPRI;

    MP_VERBOSE(priv, "Getting frontend status\n");
    int tm1 = (int)mp_time_sec();
    while (!ok) {
        festatus = 0;
        if (mp_poll(pfd, 1, MP_TIME_S_TO_NS(tmout)) > 0) {
            if (pfd[0].revents & POLLPRI) {
                if (ioctl(fd_frontend, FE_READ_STATUS, &festatus) >= 0) {
                    if (festatus & FE_HAS_LOCK)
                        locks++;
                }
            }
        }
        usleep(10000);
        int tm2 = (int)mp_time_sec();
        if ((festatus & FE_TIMEDOUT) || (locks >= 2) || (tm2 - tm1 >= tmout))
            ok = true;
    }

    if (!(festatus & FE_HAS_LOCK)) {
        MP_ERR(priv, "Not able to lock to the signal on the given frequency, "
               "timeout: %g\n", tmout);
        return -1;
    }

    int32_t strength = 0;
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

    return 0;
}

struct diseqc_cmd {
    struct dvb_diseqc_master_cmd cmd;
    uint32_t wait;
};

static int diseqc_send_msg(int fd, fe_sec_voltage_t v, struct diseqc_cmd *cmd,
                           fe_sec_tone_mode_t t, fe_sec_mini_cmd_t b)
{
    if (ioctl(fd, FE_SET_TONE, SEC_TONE_OFF) < 0)
        return -1;
    if (ioctl(fd, FE_SET_VOLTAGE, v) < 0)
        return -1;
    usleep(15 * 1000);
    if (ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &cmd->cmd) < 0)
        return -1;
    usleep(cmd->wait * 1000);
    usleep(15 * 1000);
    if (ioctl(fd, FE_DISEQC_SEND_BURST, b) < 0)
        return -1;
    usleep(15 * 1000);
    if (ioctl(fd, FE_SET_TONE, t) < 0)
        return -1;
    usleep(100000);

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
                           ((sat_no / 4) % 2) ? SEC_MINI_B : SEC_MINI_A);
}

static int dvbv5_tune(dvb_priv_t *priv, int fd_frontend,
                       unsigned int delsys, struct dtv_properties* cmdseq)
{
    MP_VERBOSE(priv, "Dumping raw tuning commands and values:\n");
    for (int i = 0; i < cmdseq->num; ++i) {
        MP_VERBOSE(priv, " %02d: 0x%x(%d) => 0x%x(%d)\n",
                   i, cmdseq->props[i].cmd, cmdseq->props[i].cmd,
                   cmdseq->props[i].u.data, cmdseq->props[i].u.data);
    }
    if (ioctl(fd_frontend, FE_SET_PROPERTY, cmdseq) < 0) {
        MP_ERR(priv, "Error tuning channel\n");
        return -1;
    }
    return 0;
}

static int tune_it(dvb_priv_t *priv, int fd_frontend, unsigned int delsys,
                   unsigned int freq, unsigned int srate, char pol,
                   int stream_id,
                   fe_spectral_inversion_t specInv, unsigned int diseqc,
                   fe_modulation_t modulation,
                   fe_code_rate_t HP_CodeRate,
                   fe_transmit_mode_t TransmissionMode,
                   fe_guard_interval_t guardInterval,
                   fe_bandwidth_t bandwidth,
                   fe_code_rate_t LP_CodeRate, fe_hierarchy_t hier,
                   float timeout)
{
    dvb_state_t *state = priv->state;

    MP_VERBOSE(priv, "tune_it: fd_frontend %d, %s freq %lu, srate %lu, "
               "pol %c, diseqc %u\n", fd_frontend,
               get_dvb_delsys(delsys),
               (long unsigned int)freq, (long unsigned int)srate,
               (pol > ' ' ? pol : '-'), diseqc);

    MP_VERBOSE(priv, "Using %s adapter %d\n",
        get_dvb_delsys(delsys),
        state->adapters[state->cur_adapter].devno);

    {
        /* discard stale QPSK events */
        struct dvb_frontend_event ev;
        while (true) {
            if (ioctl(fd_frontend, FE_GET_EVENT, &ev) < 0)
                break;
        }
    }

    /* Prepare params, be verbose. */
    int hi_lo = 0, bandwidth_hz = 0;
    switch (delsys) {
    case SYS_DVBT2:
    case SYS_DVBT:
    case SYS_ISDBT:
        if (freq < 1000000)
            freq *= 1000UL;
        switch (bandwidth) {
        case BANDWIDTH_5_MHZ:
            bandwidth_hz = 5000000;
            break;
        case BANDWIDTH_6_MHZ:
            bandwidth_hz = 6000000;
            break;
        case BANDWIDTH_7_MHZ:
            bandwidth_hz = 7000000;
            break;
        case BANDWIDTH_8_MHZ:
            bandwidth_hz = 8000000;
            break;
        case BANDWIDTH_10_MHZ:
            bandwidth_hz = 10000000;
            break;
        case BANDWIDTH_AUTO:
            if (freq < 474000000) {
                bandwidth_hz = 7000000;
            } else {
                bandwidth_hz = 8000000;
            }
            break;
        default:
            bandwidth_hz = 0;
            break;
        }

        MP_VERBOSE(priv, "tuning %s to %d Hz, bandwidth: %d\n",
                   get_dvb_delsys(delsys), freq, bandwidth_hz);
        break;
    case SYS_DVBS2:
    case SYS_DVBS:
        if (freq > 2200000) {
            // this must be an absolute frequency
            if (freq < SLOF) {
                freq -= LOF1;
                hi_lo = 0;
            } else {
                freq -= LOF2;
                hi_lo = 1;
            }
        }
        MP_VERBOSE(priv, "tuning %s to Freq: %u, Pol: %c Srate: %d, "
                   "22kHz: %s, LNB:  %d\n", get_dvb_delsys(delsys), freq,
                   pol, srate, hi_lo ? "on" : "off", diseqc);

        if (do_diseqc(fd_frontend, diseqc, (pol == 'V' ? 1 : 0), hi_lo) == 0) {
            MP_VERBOSE(priv, "DISEQC setting succeeded\n");
        } else {
            MP_ERR(priv, "DISEQC setting failed\n");
            return -1;
        }

        break;
    case SYS_DVBC_ANNEX_A:
    case SYS_DVBC_ANNEX_C:
        MP_VERBOSE(priv, "tuning %s to %d, srate=%d\n",
                   get_dvb_delsys(delsys), freq, srate);
        break;
    case SYS_ATSC:
    case SYS_DVBC_ANNEX_B:
        MP_VERBOSE(priv, "tuning %s to %d, modulation=%d\n",
                   get_dvb_delsys(delsys), freq, modulation);
        break;
    default:
        MP_VERBOSE(priv, "Unknown FE type, aborting.\n");
        return 0;
    }

    /* S2API is the DVB API new since 2.6.28.
     * It is needed to tune to new delivery systems, e.g. DVB-S2.
     * It takes a struct with a list of pairs of command + parameter.
     */

    /* Reset before tune. */
    struct dtv_property p_clear[] = {
        { .cmd = DTV_CLEAR },
    };
    struct dtv_properties cmdseq_clear = {
        .num = 1,
        .props = p_clear
    };
    if (ioctl(fd_frontend, FE_SET_PROPERTY, &cmdseq_clear) < 0) {
        MP_ERR(priv, "DTV_CLEAR failed\n");
    }

    /* Tune. */
    switch (delsys) {
    case SYS_DVBS:
    case SYS_DVBS2:
        {
            struct dtv_property p[] = {
                { .cmd = DTV_DELIVERY_SYSTEM, .u.data = delsys },
                { .cmd = DTV_FREQUENCY, .u.data = freq },
                { .cmd = DTV_MODULATION, .u.data = modulation },
                { .cmd = DTV_SYMBOL_RATE, .u.data = srate },
                { .cmd = DTV_INNER_FEC, .u.data = HP_CodeRate },
                { .cmd = DTV_INVERSION, .u.data = specInv },
                { .cmd = DTV_ROLLOFF, .u.data = ROLLOFF_AUTO },
                { .cmd = DTV_PILOT, .u.data = PILOT_AUTO },
                { .cmd = DTV_TUNE },
            };
            struct dtv_properties cmdseq = {
                .num = sizeof(p) / sizeof(p[0]),
                .props = p
            };
            if (dvbv5_tune(priv, fd_frontend, delsys, &cmdseq) != 0) {
                goto error_tune;
            }
        }
        break;
    case SYS_DVBT:
    case SYS_DVBT2:
    case SYS_ISDBT:
        {
            struct dtv_property p[] = {
                { .cmd = DTV_DELIVERY_SYSTEM, .u.data = delsys },
                { .cmd = DTV_FREQUENCY, .u.data = freq },
                { .cmd = DTV_MODULATION, .u.data = modulation },
                { .cmd = DTV_SYMBOL_RATE, .u.data = srate },
                { .cmd = DTV_CODE_RATE_HP, .u.data = HP_CodeRate },
                { .cmd = DTV_CODE_RATE_LP, .u.data = LP_CodeRate },
                { .cmd = DTV_INVERSION, .u.data = specInv },
                { .cmd = DTV_BANDWIDTH_HZ, .u.data = bandwidth_hz },
                { .cmd = DTV_TRANSMISSION_MODE, .u.data = TransmissionMode },
                { .cmd = DTV_GUARD_INTERVAL, .u.data = guardInterval },
                { .cmd = DTV_HIERARCHY, .u.data = hier },
                { .cmd = DTV_STREAM_ID, .u.data = stream_id },
                { .cmd = DTV_TUNE },
            };
            struct dtv_properties cmdseq = {
                .num = sizeof(p) / sizeof(p[0]),
                .props = p
            };
            if (dvbv5_tune(priv, fd_frontend, delsys, &cmdseq) != 0) {
                goto error_tune;
            }
        }
        break;
    case SYS_DVBC_ANNEX_A:
    case SYS_DVBC_ANNEX_C:
        {
            struct dtv_property p[] = {
                { .cmd = DTV_DELIVERY_SYSTEM, .u.data = delsys },
                { .cmd = DTV_FREQUENCY, .u.data = freq },
                { .cmd = DTV_MODULATION, .u.data = modulation },
                { .cmd = DTV_SYMBOL_RATE, .u.data = srate },
                { .cmd = DTV_INNER_FEC, .u.data = HP_CodeRate },
                { .cmd = DTV_INVERSION, .u.data = specInv },
                { .cmd = DTV_TUNE },
            };
            struct dtv_properties cmdseq = {
                .num = sizeof(p) / sizeof(p[0]),
                .props = p
            };
            if (dvbv5_tune(priv, fd_frontend, delsys, &cmdseq) != 0) {
                goto error_tune;
            }
        }
        break;
    case SYS_ATSC:
    case SYS_DVBC_ANNEX_B:
        {
            struct dtv_property p[] = {
                { .cmd = DTV_DELIVERY_SYSTEM, .u.data = delsys },
                { .cmd = DTV_FREQUENCY, .u.data = freq },
                { .cmd = DTV_INVERSION, .u.data = specInv },
                { .cmd = DTV_MODULATION, .u.data = modulation },
                { .cmd = DTV_TUNE },
            };
            struct dtv_properties cmdseq = {
                .num = sizeof(p) / sizeof(p[0]),
                .props = p
            };
            if (dvbv5_tune(priv, fd_frontend, delsys, &cmdseq) != 0) {
                goto error_tune;
            }
        }
        break;
    }

    int tune_status = check_status(priv, fd_frontend, timeout);
    if (tune_status != 0) {
        MP_ERR(priv, "Error locking to channel\n");
    }
    return tune_status;

error_tune:
    MP_ERR(priv, "Error tuning channel\n");
    return -1;
}

int dvb_tune(dvb_priv_t *priv, unsigned int delsys,
             int freq, char pol, int srate, int diseqc,
             int stream_id, fe_spectral_inversion_t specInv,
             fe_modulation_t modulation, fe_guard_interval_t guardInterval,
             fe_transmit_mode_t TransmissionMode, fe_bandwidth_t bandWidth,
             fe_code_rate_t HP_CodeRate,
             fe_code_rate_t LP_CodeRate, fe_hierarchy_t hier,
             float timeout)
{
    MP_INFO(priv, "Tuning to %s frequency %lu Hz\n",
            get_dvb_delsys(delsys), (long unsigned int) freq);

    dvb_state_t *state = priv->state;

    int ris = tune_it(priv, state->fe_fd, delsys, freq, srate, pol,
                      stream_id, specInv, diseqc, modulation,
                      HP_CodeRate, TransmissionMode, guardInterval,
                      bandWidth, LP_CodeRate, hier, timeout);

    if (ris != 0)
        MP_INFO(priv, "Tuning failed\n");

    return ris == 0;
}
