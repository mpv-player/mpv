/*

   dvbstream
   (C) Dave Chapman <dave@dchapman.com> 2001, 2002.

   Original authors: Nico, probably Arpi

   Some code based on dvbstream, 0.4.3-pre3 (CVS checkout),
   http://sourceforge.net/projects/dvbtools/

   Modified for use with MPlayer, for details see the changelog at
   http://svn.mplayerhq.hu/mplayer/trunk/
   $Id$

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <libavutil/avstring.h>

#include "osdep/io.h"
#include "misc/ctype.h"

#include "stream.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "options/options.h"
#include "options/path.h"

#include "dvbin.h"
#include "dvb_tune.h"

#define MAX_CHANNELS 8
#define CHANNEL_LINE_LEN 256
#define min(a, b) ((a) <= (b) ? (a) : (b))

#define OPT_BASE_STRUCT struct dvb_params

/// URL definition
static const m_option_t stream_params[] = {
    OPT_STRING("prog", cfg_prog, 0),
    OPT_INTRANGE("card", cfg_card, 0, 1, 4),
    {0}
};

const struct m_sub_options stream_dvb_conf = {
    .opts = (const m_option_t[]) {
        OPT_STRING("prog", cfg_prog, 0),
        OPT_INTRANGE("card", cfg_card, 0, 1, 4),
        OPT_INTRANGE("timeout", cfg_timeout, 0, 1, 30),
        OPT_STRING("file", cfg_file, M_OPT_FILE),
        OPT_FLAG("full-transponder", cfg_full_transponder, 0),
        {0}
    },
    .size = sizeof(struct dvb_params),
    .defaults = &(const struct dvb_params){
        .cfg_prog = "",
        .cfg_card = 1,
        .cfg_timeout = 30,
    },
};

static void parse_vdr_par_string(const char *vdr_par_str, dvb_channel_t *ptr)
{
    //FIXME: There is more information in this parameter string, especially related
    // to non-DVB-S reception.
    if (vdr_par_str[0]) {
        const char *vdr_par = &vdr_par_str[0];
        while (vdr_par && *vdr_par) {
            switch (mp_toupper(*vdr_par)) {
            case 'H':
                ptr->pol = 'H';
                vdr_par++;
                break;
            case 'V':
                ptr->pol = 'V';
                vdr_par++;
                break;
            case 'S':
                vdr_par++;
                if (*vdr_par == '1') {
                    ptr->is_dvb_s2 = true;
                } else {
                    ptr->is_dvb_s2 = false;
                }
                vdr_par++;
                break;
            case 'P':
                vdr_par++;
                char *endptr = NULL;
                errno = 0;
                int n = strtol(vdr_par, &endptr, 10);
                if (!errno && endptr != vdr_par) {
                    ptr->stream_id = n;
                    vdr_par = endptr;
                }
                break;
            case 'I':
                vdr_par++;
                if (*vdr_par == '1') {
                    ptr->inv = INVERSION_ON;
                } else {
                    ptr->inv = INVERSION_OFF;
                }
                vdr_par++;
                break;
            default:
                vdr_par++;
            }
        }
    }
}

static char *dvb_strtok_r(char *s, const char *sep, char **p)
{
    if (!s && !(s = *p))
        return NULL;

    /* Skip leading separators. */
    s += strspn(s, sep);

    /* s points at first non-separator, or end of string. */
    if (!*s)
        return *p = 0;

    /* Move *p to next separator. */
    *p = s + strcspn(s, sep);
    if (**p) {
        *(*p)++ = 0;
    } else {
        *p = 0;
    }
    return s;
}

static bool parse_pid_string(struct mp_log *log, char *pid_string,
                             dvb_channel_t *ptr)
{
    if (pid_string[0]) {
        int pcnt = 0;
        /* These tokens also catch vdr-style PID lists.
         * They can contain 123=deu@3,124=eng+jap@4;125
         * 3 and 4 are codes for codec type, =langLeft+langRight is allowed,
         * and ; may separate a dolby channel.
         * With the numChars-test and the full token-list, all is handled
         * gracefully.
         */
        const char *tokens = "+,;";
        char *pidPart;
        char *savePtr = NULL;
        pidPart = dvb_strtok_r(pid_string, tokens, &savePtr);
        while (pidPart != NULL) {
            if (ptr->pids_cnt >= DMX_FILTER_SIZE - 1) {
                mp_verbose(log, "Maximum number of PIDs for one channel "
                                "reached, ignoring further ones!\n");
                return pcnt > 0;
            }
            int numChars = 0;
            int pid = 0;
            pcnt += sscanf(pidPart, "%d%n", &pid, &numChars);
            if (numChars > 0) {
                ptr->pids[ptr->pids_cnt] = pid;
                ptr->pids_cnt++;
            }
            pidPart = dvb_strtok_r(NULL, tokens, &savePtr);
        }
        if (pcnt > 0)
            return true;
    }
    return false;
}

static dvb_channels_list *dvb_get_channels(struct mp_log *log,
                                           int cfg_full_transponder,
                                           char *filename,
                                           int type)
{
    dvb_channels_list *list;
    FILE *f;
    char line[CHANNEL_LINE_LEN], *colon;

    if (!filename)
        return NULL;

    int fields, cnt, k;
    int has8192, has0;
    dvb_channel_t *ptr, *tmp, chn;
    char tmp_lcr[256], tmp_hier[256], inv[256], bw[256], cr[256], mod[256],
         transm[256], gi[256], vpid_str[256], apid_str[256], tpid_str[256],
         vdr_par_str[256], vdr_loc_str[256];
    const char *cbl_conf =
        "%d:%255[^:]:%d:%255[^:]:%255[^:]:%255[^:]:%255[^:]\n";
    const char *sat_conf = "%d:%c:%d:%d:%255[^:]:%255[^:]\n";
    const char *ter_conf =
        "%d:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]\n";
    const char *atsc_conf = "%d:%255[^:]:%255[^:]:%255[^:]\n";

    const char *vdr_conf =
        "%d:%255[^:]:%255[^:]:%d:%255[^:]:%255[^:]:%255[^:]:%*255[^:]:%d:%*d:%*d:%*d\n%n";

    mp_verbose(log, "CONFIG_READ FILE: %s, type: %d\n", filename, type);
    if ((f = fopen(filename, "r")) == NULL) {
        mp_fatal(log, "CAN'T READ CONFIG FILE %s\n", filename);
        return NULL;
    }

    list = malloc(sizeof(dvb_channels_list));
    if (list == NULL) {
        fclose(f);
        mp_verbose(log, "DVB_GET_CHANNELS: couldn't malloc enough memory\n");
        return NULL;
    }

    ptr = &chn;
    list->NUM_CHANNELS = 0;
    list->channels = NULL;
    while (!feof(f)) {
        if (fgets(line, CHANNEL_LINE_LEN, f) == NULL)
            continue;

        if ((line[0] == '#') || (strlen(line) == 0))
            continue;

        colon = strchr(line, ':');
        if (colon) {
            k = colon - line;
            if (!k)
                continue;
            // In some modern VDR-style configs, channel name also has bouquet after ;.
            // Parse that off, we ignore it.
            char *bouquet_sep = strchr(line, ';');
            int channel_name_length = k;
            if (bouquet_sep && bouquet_sep < colon)
                channel_name_length = bouquet_sep - line;
            ptr->name = malloc(channel_name_length + 1);
            if (!ptr->name)
                continue;
            av_strlcpy(ptr->name, line, channel_name_length + 1);
        } else {
            continue;
        }
        k++;
        vpid_str[0] = apid_str[0] = tpid_str[0] = 0;
        vdr_loc_str[0] = vdr_par_str[0] = 0;
        ptr->pids_cnt = 0;
        ptr->freq = 0;
        ptr->is_dvb_s2 = false;
        ptr->service_id = -1;
        ptr->stream_id = NO_STREAM_ID_FILTER;
        ptr->inv = INVERSION_AUTO;

        // Check if VDR-type channels.conf-line - then full line is consumed by the scan.
        int num_chars = 0;
        fields = sscanf(&line[k], vdr_conf,
                        &ptr->freq, vdr_par_str, vdr_loc_str, &ptr->srate,
                        vpid_str, apid_str, tpid_str, &ptr->service_id,
                        &num_chars);

        if (num_chars == strlen(&line[k])) {
            // It's a VDR-style config line.
            parse_vdr_par_string(vdr_par_str, ptr);
            // We still need the special SAT-handling here.
            if (type != TUNER_TER && type != TUNER_CBL && type != TUNER_ATSC) {
                ptr->freq *=  1000UL;
                ptr->srate *=  1000UL;
                ptr->tone = -1;
                ptr->inv = INVERSION_AUTO;
                ptr->cr = FEC_AUTO;

                if (vdr_loc_str[0]) {
                    // In older vdr config format, this field contained the DISEQc information.
                    // If it is numeric, assume that's it.
                    int diseqc_info = 0;
                    int valid_digits = 0;
                    if (sscanf(vdr_loc_str, "%d%n", &diseqc_info,
                               &valid_digits) == 1)
                    {
                        if (valid_digits == strlen(vdr_loc_str)) {
                            ptr->diseqc = diseqc_info;
                            if ((ptr->diseqc > 4) || (ptr->diseqc < 0))
                                continue;
                            if (ptr->diseqc > 0)
                                ptr->diseqc--;
                        }
                    }
                }

                mp_verbose(log, "SAT, NUM: %d, NUM_FIELDS: %d, NAME: %s, "
                           "FREQ: %d, SRATE: %d, POL: %c, DISEQC: %d, S2: %s, "
                           "StreamID: %d, SID: %d", list->NUM_CHANNELS,
                           fields, ptr->name, ptr->freq, ptr->srate, ptr->pol,
                           ptr->diseqc, ptr->is_dvb_s2 ? "yes" : "no",
                           ptr->stream_id, ptr->service_id);
            } else {
                mp_verbose(log, "VDR, NUM: %d, NUM_FIELDS: %d, NAME: %s, "
                           "FREQ: %d, SRATE: %d", list->NUM_CHANNELS, fields,
                           ptr->name, ptr->freq, ptr->srate);
            }
        } else if (type == TUNER_TER) {
            fields = sscanf(&line[k], ter_conf,
                            &ptr->freq, inv, bw, cr, tmp_lcr, mod,
                            transm, gi, tmp_hier, vpid_str, apid_str);
            mp_verbose(log, "TER, NUM: %d, NUM_FIELDS: %d, NAME: %s, FREQ: %d",
                       list->NUM_CHANNELS, fields, ptr->name, ptr->freq);
        } else if (type == TUNER_CBL) {
            fields = sscanf(&line[k], cbl_conf,
                            &ptr->freq, inv, &ptr->srate,
                            cr, mod, vpid_str, apid_str);
            mp_verbose(log, "CBL, NUM: %d, NUM_FIELDS: %d, NAME: %s, FREQ: %d, "
                       "SRATE: %d", list->NUM_CHANNELS, fields, ptr->name,
                       ptr->freq, ptr->srate);
        }
#ifdef DVB_ATSC
        else if (type == TUNER_ATSC) {
            fields = sscanf(&line[k], atsc_conf,
                            &ptr->freq, mod, vpid_str, apid_str);
            mp_verbose(log, "ATSC, NUM: %d, NUM_FIELDS: %d, NAME: %s, FREQ: %d\n",
                       list->NUM_CHANNELS, fields, ptr->name, ptr->freq);
        }
#endif
        else {       //SATELLITE
            fields = sscanf(&line[k], sat_conf,
                            &ptr->freq, &ptr->pol, &ptr->diseqc, &ptr->srate,
                            vpid_str,
                            apid_str);
            ptr->pol = mp_toupper(ptr->pol);
            ptr->freq *=  1000UL;
            ptr->srate *=  1000UL;
            ptr->tone = -1;
            ptr->inv = INVERSION_AUTO;
            ptr->cr = FEC_AUTO;
            if ((ptr->diseqc > 4) || (ptr->diseqc < 0))
                continue;
            if (ptr->diseqc > 0)
                ptr->diseqc--;
            mp_verbose(log, "SAT, NUM: %d, NUM_FIELDS: %d, NAME: %s, FREQ: %d, "
                       "SRATE: %d, POL: %c, DISEQC: %d",
                       list->NUM_CHANNELS, fields, ptr->name, ptr->freq,
                       ptr->srate, ptr->pol, ptr->diseqc);
        }

        if (parse_pid_string(log, vpid_str, ptr))
            fields++;
        if (parse_pid_string(log, apid_str, ptr))
            fields++;
                 /* If we do not know the service_id, PMT can not be extracted.
                    Teletext decoding will fail without PMT. */
        if (ptr->service_id != -1) {
            if (parse_pid_string(log, tpid_str, ptr))
                fields++;
        }


        if ((fields < 2) || (ptr->pids_cnt <= 0) || (ptr->freq == 0) ||
            (strlen(ptr->name) == 0))
            continue;

        /* Add some PIDs which are mandatory in DVB,
         * and contain human-readable helpful data. */

        /* This is the STD, the service description table.
         * It contains service names and such, ffmpeg decodes it. */
        ptr->pids[ptr->pids_cnt] = 0x0011;
        ptr->pids_cnt++;

        /* This is the EIT, which contains EPG data.
         * ffmpeg can not decode it (yet), but e.g. VLC
         * shows what was recorded. */
        ptr->pids[ptr->pids_cnt] = 0x0012;
        ptr->pids_cnt++;

        if (ptr->service_id != -1) {
            /* We have the PMT-PID in addition.
               This will be found later, when we tune to the channel.
               Push back here to create the additional demux. */
            ptr->pids[ptr->pids_cnt] = -1;       // Placeholder.
            ptr->pids_cnt++;
        }

        has8192 = has0 = 0;
        for (cnt = 0; cnt < ptr->pids_cnt; cnt++) {
            if (ptr->pids[cnt] == 8192)
                has8192 = 1;
            if (ptr->pids[cnt] == 0)
                has0 = 1;
        }

        /* 8192 is the pseudo-PID for full TP dump,
           enforce that if requested. */
        if (!has8192 && cfg_full_transponder)
            has8192 = 1;
        if (has8192) {
            ptr->pids[0] = 8192;
            ptr->pids_cnt = 1;
        } else if (!has0) {
            ptr->pids[ptr->pids_cnt] = 0;               //PID 0 is the PAT
            ptr->pids_cnt++;
        }

        mp_verbose(log, " PIDS: ");
        for (cnt = 0; cnt < ptr->pids_cnt; cnt++)
            mp_verbose(log, " %d ", ptr->pids[cnt]);
        mp_verbose(log, "\n");

        if ((type == TUNER_TER) || (type == TUNER_CBL)) {
            if (!strcmp(inv, "INVERSION_ON")) {
                ptr->inv = INVERSION_ON;
            } else if (!strcmp(inv, "INVERSION_OFF")) {
                ptr->inv = INVERSION_OFF;
            } else {
                ptr->inv = INVERSION_AUTO;
            }


            if (!strcmp(cr, "FEC_1_2")) {
                ptr->cr = FEC_1_2;
            } else if (!strcmp(cr, "FEC_2_3")) {
                ptr->cr = FEC_2_3;
            } else if (!strcmp(cr, "FEC_3_4")) {
                ptr->cr = FEC_3_4;
            } else if (!strcmp(cr, "FEC_4_5")) {
                ptr->cr = FEC_4_5;
            } else if (!strcmp(cr, "FEC_6_7")) {
                ptr->cr = FEC_6_7;
            } else if (!strcmp(cr, "FEC_8_9")) {
                ptr->cr = FEC_8_9;
            } else if (!strcmp(cr, "FEC_5_6")) {
                ptr->cr = FEC_5_6;
            } else if (!strcmp(cr, "FEC_7_8")) {
                ptr->cr = FEC_7_8;
            } else if (!strcmp(cr, "FEC_NONE")) {
                ptr->cr = FEC_NONE;
            } else {
                ptr->cr = FEC_AUTO;
            }
        }


        if (type == TUNER_TER || type == TUNER_CBL || type == TUNER_ATSC) {
            if (!strcmp(mod, "QAM_128")) {
                ptr->mod = QAM_128;
            } else if (!strcmp(mod, "QAM_256")) {
                ptr->mod = QAM_256;
            } else if (!strcmp(mod, "QAM_64")) {
                ptr->mod = QAM_64;
            } else if (!strcmp(mod, "QAM_32")) {
                ptr->mod = QAM_32;
            } else if (!strcmp(mod, "QAM_16")) {
                ptr->mod = QAM_16;
#ifdef DVB_ATSC
            } else if (!strcmp(mod, "VSB_8") || !strcmp(mod, "8VSB")) {
                ptr->mod = VSB_8;
            } else if (!strcmp(mod, "VSB_16") || !strcmp(mod, "16VSB")) {
                ptr->mod = VSB_16;
            } else if (!strcmp(mod, "QAM_AUTO")) {
                ptr->mod = QAM_AUTO;
            }

#endif
        }

        if (type == TUNER_TER) {
            if (!strcmp(bw, "BANDWIDTH_6_MHZ")) {
                ptr->bw = BANDWIDTH_6_MHZ;
            } else if (!strcmp(bw, "BANDWIDTH_7_MHZ")) {
                ptr->bw = BANDWIDTH_7_MHZ;
            } else if (!strcmp(bw, "BANDWIDTH_8_MHZ")) {
                ptr->bw = BANDWIDTH_8_MHZ;
            }


            if (!strcmp(transm, "TRANSMISSION_MODE_2K")) {
                ptr->trans = TRANSMISSION_MODE_2K;
            } else if (!strcmp(transm, "TRANSMISSION_MODE_8K")) {
                ptr->trans = TRANSMISSION_MODE_8K;
            } else if (!strcmp(transm, "TRANSMISSION_MODE_AUTO")) {
                ptr->trans = TRANSMISSION_MODE_AUTO;
            }

            if (!strcmp(gi, "GUARD_INTERVAL_1_32")) {
                ptr->gi = GUARD_INTERVAL_1_32;
            } else if (!strcmp(gi, "GUARD_INTERVAL_1_16")) {
                ptr->gi = GUARD_INTERVAL_1_16;
            } else if (!strcmp(gi, "GUARD_INTERVAL_1_8")) {
                ptr->gi = GUARD_INTERVAL_1_8;
            } else if (!strcmp(gi, "GUARD_INTERVAL_1_4")) {
                ptr->gi = GUARD_INTERVAL_1_4;
            } else {
                ptr->gi = GUARD_INTERVAL_AUTO;
            }

            if (!strcmp(tmp_lcr, "FEC_1_2")) {
                ptr->cr_lp = FEC_1_2;
            } else if (!strcmp(tmp_lcr, "FEC_2_3")) {
                ptr->cr_lp = FEC_2_3;
            } else if (!strcmp(tmp_lcr, "FEC_3_4")) {
                ptr->cr_lp = FEC_3_4;
            } else if (!strcmp(tmp_lcr, "FEC_4_5")) {
                ptr->cr_lp = FEC_4_5;
            } else if (!strcmp(tmp_lcr, "FEC_6_7")) {
                ptr->cr_lp = FEC_6_7;
            } else if (!strcmp(tmp_lcr, "FEC_8_9")) {
                ptr->cr_lp = FEC_8_9;
            } else if (!strcmp(tmp_lcr, "FEC_5_6")) {
                ptr->cr_lp = FEC_5_6;
            } else if (!strcmp(tmp_lcr, "FEC_7_8")) {
                ptr->cr_lp = FEC_7_8;
            } else if (!strcmp(tmp_lcr, "FEC_NONE")) {
                ptr->cr_lp = FEC_NONE;
            } else {
                ptr->cr_lp = FEC_AUTO;
            }


            if (!strcmp(tmp_hier, "HIERARCHY_1")) {
                ptr->hier = HIERARCHY_1;
            } else if (!strcmp(tmp_hier, "HIERARCHY_2")) {
                ptr->hier = HIERARCHY_2;
            } else if (!strcmp(tmp_hier, "HIERARCHY_4")) {
                ptr->hier = HIERARCHY_4;
            } else if (!strcmp(tmp_hier, "HIERARCHY_AUTO")) {
                ptr->hier = HIERARCHY_AUTO;
            } else {
                ptr->hier = HIERARCHY_NONE;
            }
        }

        tmp = realloc(list->channels, sizeof(dvb_channel_t) *
                      (list->NUM_CHANNELS + 1));
        if (tmp == NULL)
            break;

        list->channels = tmp;
        memcpy(&(list->channels[list->NUM_CHANNELS]), ptr, sizeof(dvb_channel_t));
        list->NUM_CHANNELS++;
        if (sizeof(dvb_channel_t) * list->NUM_CHANNELS >= 1024 * 1024) {
            mp_verbose(log, "dvbin.c, > 1MB allocated for channels struct, "
                            "dropping the rest of the file\n");
            break;
        }
    }

    fclose(f);
    if (list->NUM_CHANNELS == 0) {
        free(list->channels);
        free(list);
        return NULL;
    }

    list->current = 0;
    return list;
}

void dvb_free_config(dvb_config_t *config)
{
    int i, j;

    for (i = 0; i < config->count; i++) {
        free(config->cards[i].name);
        if (!config->cards[i].list)
            continue;
        if (config->cards[i].list->channels) {
            for (j = 0; j < config->cards[i].list->NUM_CHANNELS; j++)
                free(config->cards[i].list->channels[j].name);
            free(config->cards[i].list->channels);
        }
        free(config->cards[i].list);
    }
    free(config);
}

static int dvb_streaming_read(stream_t *stream, char *buffer, int size)
{
    struct pollfd pfds[1];
    int pos = 0, tries, rk, fd;
    dvb_priv_t *priv  = (dvb_priv_t *) stream->priv;

    MP_TRACE(stream, "dvb_streaming_read(%d)\n", size);

    tries = priv->retry + 1;

    fd = priv->fd;
    while (pos < size) {
        pfds[0].fd = fd;
        pfds[0].events = POLLIN | POLLPRI;

        rk = size - pos;
        if (poll(pfds, 1, 500) <= 0) {
            MP_ERR(stream, "dvb_streaming_read, attempt N. %d failed with "
                   "errno %d when reading %d bytes\n", tries, errno, size - pos);
            errno = 0;
            if (--tries > 0)
                continue;
            break;
        }
        if ((rk = read(fd, &buffer[pos], rk)) > 0) {
            pos += rk;
            MP_TRACE(stream, "ret (%d) bytes\n", pos);
        } else {
          MP_ERR(stream, "dvb_streaming_read, poll ok but read failed with "
                 "errno %d when reading %d bytes, size: %d, pos: %d\n",
                 errno, size - pos, size, pos);
        }
    }

    if (!pos)
        MP_ERR(stream, "dvb_streaming_read, return %d bytes\n", pos);

    return pos;
}

static void dvbin_close(stream_t *stream);

int dvb_set_channel(stream_t *stream, int card, int n)
{
    dvb_channels_list *new_list;
    dvb_channel_t *channel;
    dvb_priv_t *priv = stream->priv;
    char buf[4096];
    dvb_config_t *conf = (dvb_config_t *) priv->config;
    int devno;
    int i;

    if ((card < 0) || (card > conf->count)) {
        MP_ERR(stream, "dvb_set_channel: INVALID CARD NUMBER: %d vs %d, abort\n",
               card, conf->count);
        return 0;
    }

    devno = conf->cards[card].devno;
    new_list = conf->cards[card].list;
    if ((n > new_list->NUM_CHANNELS) || (n < 0)) {
        MP_ERR(stream, "dvb_set_channel: INVALID CHANNEL NUMBER: %d, for "
               "card %d, abort\n", n, card);
        return 0;
    }
    channel = &(new_list->channels[n]);

    if (priv->is_on) {  //the fds are already open and we have to stop the demuxers
        for (i = 0; i < priv->demux_fds_cnt; i++)
            dvb_demux_stop(priv->demux_fds[i]);

        priv->retry = 0;
        //empty both the stream's and driver's buffer
        while (dvb_streaming_read(stream, buf, 4096) > 0) {}
        if (priv->card != card) {
            dvbin_close(stream);
            if (!dvb_open_devices(priv, devno, channel->pids_cnt)) {
                MP_ERR(stream, "DVB_SET_CHANNEL, COULDN'T OPEN DEVICES OF "
                       "CARD: %d, EXIT\n", card);
                return 0;
            }
        } else {
            // close all demux_fds with pos > pids required for the new channel
            // or open other demux_fds if we have too few
            if (!dvb_fix_demuxes(priv, channel->pids_cnt))
                return 0;
        }
    } else {
        if (!dvb_open_devices(priv, devno, channel->pids_cnt)) {
            MP_ERR(stream, "DVB_SET_CHANNEL2, COULDN'T OPEN DEVICES OF "
                   "CARD: %d, EXIT\n", card);
            return 0;
        }
    }

    priv->card = card;
    priv->list = new_list;
    priv->retry = 5;
    new_list->current = n;
    priv->fd = priv->dvr_fd;
    MP_VERBOSE(stream, "DVB_SET_CHANNEL: new channel name=%s, card: %d, "
               "channel %d\n", channel->name, card, n);

    stream_drop_buffers(stream);

    if (channel->freq != priv->last_freq) {
        if (!dvb_tune(priv, channel->freq, channel->pol, channel->srate,
                      channel->diseqc, channel->tone,
                      channel->is_dvb_s2, channel->stream_id, channel->inv,
                      channel->mod, channel->gi,
                      channel->trans, channel->bw, channel->cr, channel->cr_lp,
                      channel->hier, priv->cfg_timeout))
            return 0;
    }

    priv->last_freq = channel->freq;
    priv->is_on = 1;

    if (channel->service_id != -1) {
        /* We need the PMT-PID in addition.
           If it has not yet beem resolved, do it now. */
        for (i = 0; i < channel->pids_cnt; i++) {
            if (channel->pids[i] == -1) {
                MP_VERBOSE(stream, "DVB_SET_CHANNEL: PMT-PID for service %d "
                           "not resolved yet, parsing PAT...\n",
                           channel->service_id);
                int pmt_pid = dvb_get_pmt_pid(priv, card, channel->service_id);
                MP_VERBOSE(stream, "DVB_SET_CHANNEL: Found PMT-PID: %d\n",
                           pmt_pid);
                channel->pids[i] = pmt_pid;
            }
        }
    }

    // sets demux filters and restart the stream
    for (i = 0; i < channel->pids_cnt; i++) {
        if (channel->pids[i] == -1) {
            // In case PMT was not resolved, skip it here.
            MP_ERR(stream, "DVB_SET_CHANNEL: PMT-PID not found, "
                           "teletext-decoding may fail.\n");
        } else {
            if (!dvb_set_ts_filt(priv, priv->demux_fds[i], channel->pids[i],
                                 DMX_PES_OTHER))
                return 0;
        }
    }

    return 1;
}

int dvb_step_channel(stream_t *stream, int dir)
{
    int new_current;
    dvb_channels_list *list;
    dvb_priv_t *priv = stream->priv;

    MP_VERBOSE(stream, "DVB_STEP_CHANNEL dir %d\n", dir);

    if (priv == NULL) {
        MP_ERR(stream, "dvb_step_channel: NULL priv_ptr, quit\n");
        return 0;
    }

    list = priv->list;
    if (list == NULL) {
        MP_ERR(stream, "dvb_step_channel: NULL list_ptr, quit\n");
        return 0;
    }

    new_current = (list->NUM_CHANNELS + list->current +
                  (dir >= 0 ? 1 : -1)) % list->NUM_CHANNELS;

    return dvb_set_channel(stream, priv->card, new_current);
}

static int dvbin_stream_control(struct stream *s, int cmd, void *arg)
{
    int r;
    switch (cmd) {
    case STREAM_CTRL_DVB_SET_CHANNEL: {
        int *iarg = arg;
        r = dvb_set_channel(s, iarg[1], iarg[0]);
        return r ? STREAM_OK : STREAM_ERROR;
    }
    case STREAM_CTRL_DVB_STEP_CHANNEL:
        r = dvb_step_channel(s, *(int *)arg);
        return r ? STREAM_OK : STREAM_ERROR;
    }
    return STREAM_UNSUPPORTED;
}

static void dvbin_close(stream_t *stream)
{
    int i;
    dvb_priv_t *priv  = (dvb_priv_t *) stream->priv;

    for (i = priv->demux_fds_cnt - 1; i >= 0; i--) {
        priv->demux_fds_cnt--;
        MP_VERBOSE(stream, "DVBIN_CLOSE, close(%d), fd=%d, COUNT=%d\n", i,
                   priv->demux_fds[i], priv->demux_fds_cnt);
        close(priv->demux_fds[i]);
    }
    close(priv->dvr_fd);

    close(priv->fe_fd);
    priv->fe_fd = priv->sec_fd = priv->dvr_fd = -1;

    priv->is_on = 0;
    dvb_free_config(priv->config);
}

static int dvb_streaming_start(stream_t *stream, int tuner_type, char *progname)
{
    int i;
    dvb_channel_t *channel = NULL;
    dvb_priv_t *priv = stream->priv;
    dvb_priv_t *opts = priv;

    MP_VERBOSE(stream, "\r\ndvb_streaming_start(PROG: %s, CARD: %d)\n",
               opts->cfg_prog, opts->cfg_card);

    priv->is_on = 0;

    i = 0;
    while ((channel == NULL) && i < priv->list->NUM_CHANNELS) {
        if (!strcmp(priv->list->channels[i].name, progname))
            channel = &(priv->list->channels[i]);

        i++;
    }

    if (channel != NULL) {
        priv->list->current = i - 1;
        MP_VERBOSE(stream, "PROGRAM NUMBER %d: name=%s, freq=%u\n", i - 1,
                   channel->name, channel->freq);
    } else {
        MP_ERR(stream, "\n\nDVBIN: no such channel \"%s\"\n\n", progname);
        return 0;
    }


    if (!dvb_set_channel(stream, priv->card, priv->list->current)) {
        MP_ERR(stream, "ERROR, COULDN'T SET CHANNEL  %i: ", priv->list->current);
        dvbin_close(stream);
        return 0;
    }

    MP_VERBOSE(stream, "SUCCESSFUL EXIT from dvb_streaming_start\n");

    return 1;
}




static int dvb_open(stream_t *stream)
{
    // I don't force  the file format bacause, although it's almost always TS,
    // there are some providers that stream an IP multicast with M$ Mpeg4 inside
    dvb_priv_t *priv = stream->priv;
    priv->log = stream->log;
    dvb_priv_t *p = priv;
    char *progname;
    int tuner_type = 0, i;

    priv->fe_fd = priv->sec_fd = priv->dvr_fd = -1;
    priv->config = dvb_get_config(stream);
    if (priv->config == NULL) {
        MP_ERR(stream, "DVB CONFIGURATION IS EMPTY, exit\n");
        return STREAM_ERROR;
    }

    priv->card = -1;
    for (i = 0; i < priv->config->count; i++) {
        if (priv->config->cards[i].devno + 1 == p->cfg_card) {
            priv->card = i;
            break;
        }
    }

    if (priv->card == -1) {
        MP_ERR(stream, "NO CONFIGURATION FOUND FOR CARD N. %d, exit\n",
               p->cfg_card);
        return STREAM_ERROR;
    }
    priv->timeout = p->cfg_timeout;

    tuner_type = priv->config->cards[priv->card].type;

    if (tuner_type == 0) {
        MP_VERBOSE(stream,
                   "OPEN_DVB: UNKNOWN OR UNDETECTABLE TUNER TYPE, EXIT\n");
        return STREAM_ERROR;
    }

    priv->tuner_type = tuner_type;

    MP_VERBOSE(stream, "OPEN_DVB: prog=%s, card=%d, type=%d\n",
               p->cfg_prog, priv->card + 1, priv->tuner_type);

    priv->list = priv->config->cards[priv->card].list;

    if ((!strcmp(p->cfg_prog, "")) && (priv->list != NULL)) {
        progname = priv->list->channels[0].name;
    } else {
        progname = p->cfg_prog;
    }


    if (!dvb_streaming_start(stream, tuner_type, progname))
        return STREAM_ERROR;

    stream->type = STREAMTYPE_DVB;
    stream->fill_buffer = dvb_streaming_read;
    stream->close = dvbin_close;
    stream->control = dvbin_stream_control;
    stream->streaming = true;

    stream->demuxer = "lavf";
    stream->lavf_type = "mpegts";

    return STREAM_OK;
}

#define MAX_CARDS 4
dvb_config_t *dvb_get_config(stream_t *stream)
{
    struct mp_log *log = stream->log;
    struct mpv_global *global = stream->global;
    dvb_priv_t *priv = stream->priv;
    int i, fd, type, size;
    char filename[30], *name;
    dvb_channels_list *list;
    dvb_card_config_t *cards = NULL, *tmp;
    dvb_config_t *conf = NULL;

    conf = malloc(sizeof(dvb_config_t));
    if (conf == NULL)
        return NULL;

    conf->priv = NULL;
    conf->count = 0;
    conf->cards = NULL;
    for (i = 0; i < MAX_CARDS; i++) {
        snprintf(filename, sizeof(filename), "/dev/dvb/adapter%d/frontend0", i);
        fd = open(filename, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            mp_verbose(log, "DVB_CONFIG, can't open device %s, skipping\n",
                       filename);
            continue;
        }

        type = dvb_get_tuner_type(fd, log);
        close(fd);
        if (type != TUNER_SAT && type != TUNER_TER && type != TUNER_CBL &&
            type != TUNER_ATSC) {
            mp_verbose(log, "DVB_CONFIG, can't detect tuner type of "
                       "card %d, skipping\n", i);
            continue;
        }

        void *talloc_ctx = talloc_new(NULL);
        char *conf_file = NULL;
        if (priv->cfg_file && priv->cfg_file[0])
            conf_file = priv->cfg_file;
        else {
            switch (type) {
            case TUNER_TER:
                conf_file = mp_find_config_file(talloc_ctx, global,
                                                "channels.conf.ter");
                break;
            case TUNER_CBL:
                conf_file = mp_find_config_file(talloc_ctx, global,
                                                "channels.conf.cbl");
                break;
            case TUNER_SAT:
                conf_file = mp_find_config_file(talloc_ctx, global,
                                                "channels.conf.sat");
                break;
            case TUNER_ATSC:
                conf_file = mp_find_config_file(talloc_ctx, global,
                                                "channels.conf.atsc");
                break;
            }
            if (conf_file) {
                mp_verbose(log, "Ignoring other channels.conf files.\n");
            } else {
                conf_file = mp_find_config_file(talloc_ctx, global,
                                                "channels.conf");
            }
        }

        list = dvb_get_channels(log, priv->cfg_full_transponder, conf_file,
                                type);
        talloc_free(talloc_ctx);

        if (list == NULL)
            continue;

        size = sizeof(dvb_card_config_t) * (conf->count + 1);
        tmp = realloc(conf->cards, size);

        if (tmp == NULL) {
            fprintf(stderr, "DVB_CONFIG, can't realloc %d bytes, skipping\n",
                    size);
            continue;
        }
        cards = tmp;

        name = malloc(20);
        if (name == NULL) {
            fprintf(stderr, "DVB_CONFIG, can't realloc 20 bytes, skipping\n");
            continue;
        }

        conf->cards = cards;
        conf->cards[conf->count].devno = i;
        conf->cards[conf->count].list = list;
        conf->cards[conf->count].type = type;
        snprintf(name, 20, "DVB-%c card n. %d",
                 type == TUNER_TER ? 'T' : (type == TUNER_CBL ? 'C' : 'S'),
                 conf->count + 1);
        conf->cards[conf->count].name = name;
        conf->count++;
    }

    if (conf->count == 0) {
        free(conf);
        conf = NULL;
    }

    return conf;
}

static void *get_defaults(stream_t *st)
{
    return m_sub_options_copy(st, &stream_dvb_conf, st->opts->stream_dvb_opts);
}

const stream_info_t stream_info_dvb = {
    .name = "dvbin",
    .open = dvb_open,
    .protocols = (const char *const[]){ "dvb", NULL },
    .priv_size = sizeof(dvb_priv_t),
    .get_defaults = get_defaults,
    .options = stream_params,
    .url_options = (const char *const[]){
        "hostname=prog",
        "username=card",
        NULL
    },
};
