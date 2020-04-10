/*

   dvbstream
   (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
   (C) Rozhuk Ivan <rozhuk.im@gmail.com> 2016 - 2017

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
#include <pthread.h>

#include <libavutil/avstring.h>

#include "osdep/io.h"
#include "misc/ctype.h"
#include "osdep/timer.h"

#include "stream.h"
#include "common/tags.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "options/options.h"
#include "options/path.h"

#include "dvbin.h"
#include "dvb_tune.h"

#if !HAVE_GPL
#error GPL only
#endif

#define CHANNEL_LINE_LEN 256
#define min(a, b) ((a) <= (b) ? (a) : (b))

#define OPT_BASE_STRUCT dvb_opts_t

static dvb_state_t *global_dvb_state = NULL;
static pthread_mutex_t global_dvb_state_lock = PTHREAD_MUTEX_INITIALIZER;

const struct m_sub_options stream_dvb_conf = {
    .opts = (const m_option_t[]) {
        {"prog", OPT_STRING(cfg_prog), .flags = UPDATE_DVB_PROG},
        {"card", OPT_INT(cfg_devno), M_RANGE(0, MAX_ADAPTERS-1)},
        {"timeout", OPT_INT(cfg_timeout), M_RANGE(1, 30)},
        {"file", OPT_STRING(cfg_file), .flags = M_OPT_FILE},
        {"full-transponder", OPT_FLAG(cfg_full_transponder)},
        {"channel-switch-offset", OPT_INT(cfg_channel_switch_offset),
            .flags = UPDATE_DVB_PROG},
        {0}
    },
    .size = sizeof(dvb_opts_t),
    .defaults = &(const dvb_opts_t){
        .cfg_prog = NULL,
        .cfg_devno = 0,
        .cfg_timeout = 30,
    },
};

void dvbin_close(stream_t *stream);

static fe_modulation_t parse_vdr_modulation(const char** modstring) {
    if (!strncmp(*modstring, "16", 2)) {
        (*modstring)+=2;
        return QAM_16;
    } else if (!strncmp(*modstring, "32", 2)) {
        (*modstring)+=2;
        return QAM_32;
    } else if (!strncmp(*modstring, "64", 2)) {
        (*modstring)+=2;
        return QAM_64;
    } else if (!strncmp(*modstring, "128", 3)) {
        (*modstring)+=3;
        return QAM_128;
    } else if (!strncmp(*modstring, "256", 3)) {
        (*modstring)+=3;
        return QAM_256;
    } else if (!strncmp(*modstring, "998", 3)) {
        (*modstring)+=3;
        return QAM_AUTO;
    } else if (!strncmp(*modstring, "2", 1)) {
        (*modstring)++;
        return QPSK;
    } else if (!strncmp(*modstring, "5", 1)) {
        (*modstring)++;
        return PSK_8;
    } else if (!strncmp(*modstring, "6", 1)) {
        (*modstring)++;
        return APSK_16;
    } else if (!strncmp(*modstring, "7", 1)) {
        (*modstring)++;
        return APSK_32;
    } else if (!strncmp(*modstring, "10", 2)) {
        (*modstring)+=2;
        return VSB_8;
    } else if (!strncmp(*modstring, "11", 2)) {
        (*modstring)+=2;
        return VSB_16;
    } else if (!strncmp(*modstring, "12", 2)) {
        (*modstring)+=2;
        return DQPSK;
    } else {
        return QAM_AUTO;
    }
}

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
                    ptr->is_dvb_x2 = true;
                } else {
                    ptr->is_dvb_x2 = false;
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
            case 'M':
                vdr_par++;
                ptr->mod = parse_vdr_modulation(&vdr_par);
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

static dvb_channels_list_t *dvb_get_channels(struct mp_log *log,
                                           dvb_channels_list_t *list_add,
                                           int cfg_full_transponder,
                                           char *filename,
                                           unsigned int frontend,
                                           int delsys, unsigned int delsys_mask)
{
    dvb_channels_list_t *list = list_add;
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
#ifdef DVB_ATSC
    const char *atsc_conf = "%d:%255[^:]:%255[^:]:%255[^:]\n";
#endif
    const char *vdr_conf =
        "%d:%255[^:]:%255[^:]:%d:%255[^:]:%255[^:]:%255[^:]:%*255[^:]:%d:%*d:%*d:%*d\n%n";

    mp_verbose(log, "CONFIG_READ FILE: %s, type: %s\n",
               filename, get_dvb_delsys(delsys));
    if ((f = fopen(filename, "r")) == NULL) {
        mp_fatal(log, "CAN'T READ CONFIG FILE %s\n", filename);
        return NULL;
    }

    if (list == NULL) {
        list = malloc(sizeof(dvb_channels_list_t));
        if (list == NULL) {
            fclose(f);
            mp_verbose(log, "DVB_GET_CHANNELS: couldn't malloc enough memory\n");
            return NULL;
        }
        memset(list, 0x00, sizeof(dvb_channels_list_t));
    }

    ptr = &chn;
    while (!feof(f)) {
        if (fgets(line, CHANNEL_LINE_LEN, f) == NULL)
            continue;

        if ((line[0] == '#') || (strlen(line) == 0))
            continue;

        memset(ptr, 0x00, sizeof(dvb_channel_t));
        vpid_str[0] = apid_str[0] = tpid_str[0] = 0;
        vdr_loc_str[0] = vdr_par_str[0] = 0;

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
                channel_name_length = (bouquet_sep - line);
            ptr->name = malloc((channel_name_length + 1));
            if (!ptr->name)
                continue;
            av_strlcpy(ptr->name, line, (channel_name_length + 1));
        } else {
            continue;
        }
        k++;
        ptr->pids_cnt = 0;
        ptr->freq = 0;
        ptr->service_id = -1;
        ptr->is_dvb_x2 = false;
        ptr->frontend = frontend;
        ptr->delsys = delsys;
        ptr->diseqc = 0;
        ptr->stream_id = NO_STREAM_ID_FILTER;
        ptr->inv = INVERSION_AUTO;
        ptr->bw = BANDWIDTH_AUTO;
        ptr->cr = FEC_AUTO;
        ptr->cr_lp = FEC_AUTO;
        ptr->mod = QAM_AUTO;
        ptr->hier = HIERARCHY_AUTO;
        ptr->gi = GUARD_INTERVAL_AUTO;
        ptr->trans = TRANSMISSION_MODE_AUTO;

        // Check if VDR-type channels.conf-line - then full line is consumed by the scan.
        int num_chars = 0;
        fields = sscanf(&line[k], vdr_conf,
                        &ptr->freq, vdr_par_str, vdr_loc_str, &ptr->srate,
                        vpid_str, apid_str, tpid_str, &ptr->service_id,
                        &num_chars);

        if (num_chars == strlen(&line[k])) {
            // Modulation parsed here, not via old xine-parsing path.
            mod[0] = '\0';
            // It's a VDR-style config line.
            parse_vdr_par_string(vdr_par_str, ptr);
            // Frequency in VDR-style config files is in MHz for DVB-S,
            // and may be in MHz, kHz or Hz for DVB-C and DVB-T.
            // General rule to get useful units is to multiply by 1000 until value is larger than 1000000.
            while (ptr->freq < 1000000UL) {
                ptr->freq *= 1000UL;
            }
            // Symbol rate in VDR-style config files is divided by 1000.
            ptr->srate *= 1000UL;
            switch (delsys) {
            case SYS_DVBT:
            case SYS_DVBT2:
                /* Fix delsys value. */
                if (ptr->is_dvb_x2) {
                    ptr->delsys = delsys = SYS_DVBT2;
                } else {
                    ptr->delsys = delsys = SYS_DVBT;
                }
                if (!DELSYS_IS_SET(delsys_mask, delsys))
                    continue; /* Skip channel. */
                mp_verbose(log, "VDR, %s, NUM: %d, NUM_FIELDS: %d, NAME: %s, "
                           "FREQ: %d, SRATE: %d, T2: %s",
                           get_dvb_delsys(delsys),
                           list->NUM_CHANNELS, fields,
                           ptr->name, ptr->freq, ptr->srate,
                           (delsys == SYS_DVBT2) ? "yes" : "no");
                break;
            case SYS_DVBC_ANNEX_A:
            case SYS_DVBC_ANNEX_C:
            case SYS_ATSC:
            case SYS_DVBC_ANNEX_B:
                mp_verbose(log, "VDR, %s, NUM: %d, NUM_FIELDS: %d, NAME: %s, "
                           "FREQ: %d, SRATE: %d",
                           get_dvb_delsys(delsys),
                           list->NUM_CHANNELS, fields,
                           ptr->name, ptr->freq, ptr->srate);
                break;
            case SYS_DVBS:
            case SYS_DVBS2:
                /* Fix delsys value. */
                if (ptr->is_dvb_x2) {
                    ptr->delsys = delsys = SYS_DVBS2;
                } else {
                    ptr->delsys = delsys = SYS_DVBS;
                }
                if (!DELSYS_IS_SET(delsys_mask, delsys))
                    continue; /* Skip channel. */

                if (vdr_loc_str[0]) {
                    // In older vdr config format, this field contained the DISEQc information.
                    // If it is numeric, assume that's it.
                    int diseqc_info = 0;
                    int valid_digits = 0;
                    if (sscanf(vdr_loc_str, "%d%n", &diseqc_info,
                               &valid_digits) == 1)
                    {
                        if (valid_digits == strlen(vdr_loc_str)) {
                            ptr->diseqc = (unsigned int)diseqc_info;
                            if (ptr->diseqc > 4)
                                continue;
                            if (ptr->diseqc > 0)
                                ptr->diseqc--;
                        }
                    }
                }

                mp_verbose(log, "VDR, %s, NUM: %d, NUM_FIELDS: %d, NAME: %s, "
                           "FREQ: %d, SRATE: %d, POL: %c, DISEQC: %d, S2: %s, "
                           "StreamID: %d, SID: %d",
                           get_dvb_delsys(delsys),
                           list->NUM_CHANNELS,
                           fields, ptr->name, ptr->freq, ptr->srate, ptr->pol,
                           ptr->diseqc, (delsys == SYS_DVBS2) ? "yes" : "no",
                           ptr->stream_id, ptr->service_id);
                break;
            default:
                break;
            }
        } else {
            switch (delsys) {
            case SYS_DVBT:
            case SYS_DVBT2:
                fields = sscanf(&line[k], ter_conf,
                                &ptr->freq, inv, bw, cr, tmp_lcr, mod,
                                transm, gi, tmp_hier, vpid_str, apid_str);
                mp_verbose(log, "%s, NUM: %d, NUM_FIELDS: %d, NAME: %s, FREQ: %d",
                           get_dvb_delsys(delsys), list->NUM_CHANNELS,
                           fields, ptr->name, ptr->freq);
                break;
            case SYS_DVBC_ANNEX_A:
            case SYS_DVBC_ANNEX_C:
                fields = sscanf(&line[k], cbl_conf,
                                &ptr->freq, inv, &ptr->srate,
                                cr, mod, vpid_str, apid_str);
                mp_verbose(log, "%s, NUM: %d, NUM_FIELDS: %d, NAME: %s, FREQ: %d, "
                           "SRATE: %d",
                           get_dvb_delsys(delsys),
                           list->NUM_CHANNELS, fields, ptr->name,
                           ptr->freq, ptr->srate);
                break;
#ifdef DVB_ATSC
            case SYS_ATSC:
            case SYS_DVBC_ANNEX_B:
                fields = sscanf(&line[k], atsc_conf,
                                &ptr->freq, mod, vpid_str, apid_str);
                mp_verbose(log, "%s, NUM: %d, NUM_FIELDS: %d, NAME: %s, FREQ: %d\n",
                           get_dvb_delsys(delsys), list->NUM_CHANNELS,
                           fields, ptr->name, ptr->freq);
                break;
#endif
            case SYS_DVBS:
            case SYS_DVBS2:
                fields = sscanf(&line[k], sat_conf,
                                &ptr->freq, &ptr->pol, &ptr->diseqc, &ptr->srate,
                                vpid_str,
                                apid_str);
                ptr->pol = mp_toupper(ptr->pol);
                ptr->freq *=  1000UL;
                ptr->srate *=  1000UL;
                if (ptr->diseqc > 4)
                    continue;
                if (ptr->diseqc > 0)
                    ptr->diseqc--;
                mp_verbose(log, "%s, NUM: %d, NUM_FIELDS: %d, NAME: %s, FREQ: %d, "
                           "SRATE: %d, POL: %c, DISEQC: %d",
                           get_dvb_delsys(delsys),
                           list->NUM_CHANNELS, fields, ptr->name, ptr->freq,
                           ptr->srate, ptr->pol, ptr->diseqc);
                break;
            default:
                break;
            }
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


        if (fields < 2 || ptr->pids_cnt == 0 || ptr->freq == 0 ||
            strlen(ptr->name) == 0)
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

        switch (delsys) {
        case SYS_DVBT:
        case SYS_DVBT2:
        case SYS_DVBC_ANNEX_A:
        case SYS_DVBC_ANNEX_C:
            if (!strcmp(inv, "INVERSION_ON")) {
                ptr->inv = INVERSION_ON;
            } else if (!strcmp(inv, "INVERSION_OFF")) {
                ptr->inv = INVERSION_OFF;
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
            }
        }

        switch (delsys) {
        case SYS_DVBT:
        case SYS_DVBT2:
        case SYS_DVBC_ANNEX_A:
        case SYS_DVBC_ANNEX_C:
        case SYS_ATSC:
        case SYS_DVBC_ANNEX_B:
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
#endif
            }
        }
#ifdef DVB_ATSC
        /* Modulation defines real delsys for ATSC:
           Terrestrial (VSB) is SYS_ATSC, Cable (QAM) is SYS_DVBC_ANNEX_B. */
        if (delsys == SYS_ATSC || delsys == SYS_DVBC_ANNEX_B) {
            if (ptr->mod == VSB_8 || ptr->mod == VSB_16) {
                delsys = SYS_ATSC;
            } else {
                delsys = SYS_DVBC_ANNEX_B;
            }
            if (!DELSYS_IS_SET(delsys_mask, delsys))
                continue; /* Skip channel. */
            mp_verbose(log, "Switched to delivery system for ATSC: %s (guessed from modulation).\n",
                       get_dvb_delsys(delsys));
        }
#endif

        switch (delsys) {
        case SYS_DVBT:
        case SYS_DVBT2:
            if (!strcmp(bw, "BANDWIDTH_5_MHZ")) {
                ptr->bw = BANDWIDTH_5_MHZ;
            } else if (!strcmp(bw, "BANDWIDTH_6_MHZ")) {
                ptr->bw = BANDWIDTH_6_MHZ;
            } else if (!strcmp(bw, "BANDWIDTH_7_MHZ")) {
                ptr->bw = BANDWIDTH_7_MHZ;
            } else if (!strcmp(bw, "BANDWIDTH_8_MHZ")) {
                ptr->bw = BANDWIDTH_8_MHZ;
            } else if (!strcmp(bw, "BANDWIDTH_10_MHZ")) {
                ptr->bw = BANDWIDTH_10_MHZ;
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
            }


            if (!strcmp(tmp_hier, "HIERARCHY_1")) {
                ptr->hier = HIERARCHY_1;
            } else if (!strcmp(tmp_hier, "HIERARCHY_2")) {
                ptr->hier = HIERARCHY_2;
            } else if (!strcmp(tmp_hier, "HIERARCHY_4")) {
                ptr->hier = HIERARCHY_4;
            } else if (!strcmp(tmp_hier, "HIERARCHY_NONE")) {
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

    return list;
}

void dvb_free_state(dvb_state_t *state)
{
    int i, j;

    for (i = 0; i < state->adapters_count; i++) {
        if (!state->adapters[i].list)
            continue;
        if (state->adapters[i].list->channels) {
            for (j = 0; j < state->adapters[i].list->NUM_CHANNELS; j++)
                free(state->adapters[i].list->channels[j].name);
            free(state->adapters[i].list->channels);
        }
        free(state->adapters[i].list);
    }
    free(state);
}

static int dvb_streaming_read(stream_t *stream, void *buffer, int size)
{
    struct pollfd pfds[1];
    int pos = 0, tries, rk, fd;
    dvb_priv_t *priv  = (dvb_priv_t *) stream->priv;
    dvb_state_t *state = priv->state;

    MP_TRACE(stream, "dvb_streaming_read(%d)\n", size);

    tries = state->retry;
    fd = state->dvr_fd;
    while (pos < size) {
        rk = read(fd, (char *)buffer + pos, (size - pos));
        if (rk <= 0) {
            if (pos || tries == 0)
                break;
            tries --;
            pfds[0].fd = fd;
            pfds[0].events = POLLIN | POLLPRI;
            if (poll(pfds, 1, 2000) <= 0) {
                MP_ERR(stream, "dvb_streaming_read, failed with "
                        "errno %d when reading %d bytes\n", errno, size - pos);
                errno = 0;
                break;
            }
            continue;
        }
        pos += rk;
        MP_TRACE(stream, "ret (%d) bytes\n", pos);
    }

    if (!pos)
        MP_ERR(stream, "dvb_streaming_read, return 0 bytes\n");

    // Check if config parameters have been updated.
    dvb_update_config(stream);

    return pos;
}

int dvb_set_channel(stream_t *stream, unsigned int adapter, unsigned int n)
{
    dvb_channels_list_t *new_list;
    dvb_channel_t *channel;
    dvb_priv_t *priv = stream->priv;
    char buf[4096];
    dvb_state_t *state = (dvb_state_t *) priv->state;
    int devno;
    int i;

    if (adapter >= state->adapters_count) {
        MP_ERR(stream, "dvb_set_channel: INVALID internal ADAPTER NUMBER: %d vs %d, abort\n",
               adapter, state->adapters_count);
        return 0;
    }

    devno = state->adapters[adapter].devno;
    new_list = state->adapters[adapter].list;
    if (n > new_list->NUM_CHANNELS) {
        MP_ERR(stream, "dvb_set_channel: INVALID CHANNEL NUMBER: %d, for "
               "adapter %d, abort\n", n, devno);
        return 0;
    }
    channel = &(new_list->channels[n]);

    if (state->is_on) {  //the fds are already open and we have to stop the demuxers
        /* Remove all demuxes. */
        dvb_fix_demuxes(priv, 0);

        state->retry = 0;
        //empty both the stream's and driver's buffer
        while (dvb_streaming_read(stream, buf, sizeof(buf)) > 0) {}
        if (state->cur_adapter != adapter ||
            state->cur_frontend != channel->frontend) {
            dvbin_close(stream);
            if (!dvb_open_devices(priv, devno, channel->frontend, channel->pids_cnt)) {
                MP_ERR(stream, "DVB_SET_CHANNEL, COULDN'T OPEN DEVICES OF "
                       "ADAPTER: %d, EXIT\n", devno);
                return 0;
            }
        } else {
            // close all demux_fds with pos > pids required for the new channel
            // or open other demux_fds if we have too few
            if (!dvb_fix_demuxes(priv, channel->pids_cnt))
                return 0;
        }
    } else {
        if (!dvb_open_devices(priv, devno, channel->frontend, channel->pids_cnt)) {
            MP_ERR(stream, "DVB_SET_CHANNEL2, COULDN'T OPEN DEVICES OF "
                   "ADAPTER: %d, EXIT\n", devno);
            return 0;
        }
    }

    state->retry = 5;
    new_list->current = n;
    MP_VERBOSE(stream, "DVB_SET_CHANNEL: new channel name=%s, adapter: %d, "
               "channel %d\n", channel->name, devno, n);

    if (channel->freq != state->last_freq) {
        if (!dvb_tune(priv, channel->delsys, channel->freq,
                      channel->pol, channel->srate, channel->diseqc,
                      channel->stream_id, channel->inv,
                      channel->mod, channel->gi,
                      channel->trans, channel->bw, channel->cr, channel->cr_lp,
                      channel->hier, priv->opts->cfg_timeout))
            return 0;
    }

    state->is_on = 1;
    state->last_freq = channel->freq;
    state->cur_adapter = adapter;
    state->cur_frontend = channel->frontend;

    if (channel->service_id != -1) {
        /* We need the PMT-PID in addition.
           If it has not yet beem resolved, do it now. */
        for (i = 0; i < channel->pids_cnt; i++) {
            if (channel->pids[i] == -1) {
                MP_VERBOSE(stream, "DVB_SET_CHANNEL: PMT-PID for service %d "
                           "not resolved yet, parsing PAT...\n",
                           channel->service_id);
                int pmt_pid = dvb_get_pmt_pid(priv, adapter, channel->service_id);
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
            if (!dvb_set_ts_filt(priv, state->demux_fds[i], channel->pids[i],
                                 DMX_PES_OTHER))
                return 0;
        }
    }

    return 1;
}

static int dvbin_stream_control(struct stream *s, int cmd, void *arg)
{
    dvb_priv_t *priv  = (dvb_priv_t *) s->priv;
    dvb_state_t *state = priv->state;
    dvb_channels_list_t *list = NULL;

    if (state->cur_adapter >= state->adapters_count)
        return STREAM_ERROR;
    list = state->adapters[state->cur_adapter].list;

    switch (cmd) {
    case STREAM_CTRL_GET_METADATA: {
        struct mp_tags *metadata = talloc_zero(NULL, struct mp_tags);
        char *progname = list->channels[list->current].name;
        mp_tags_set_str(metadata, "title", progname);
        *(struct mp_tags **)arg = metadata;
        return STREAM_OK;
    }
    }
    return STREAM_UNSUPPORTED;
}

void dvbin_close(stream_t *stream)
{
    dvb_priv_t *priv  = (dvb_priv_t *) stream->priv;
    dvb_state_t *state = priv->state;

    if (state->switching_channel && state->is_on) {
      // Prevent state destruction, reset channel-switch.
      state->switching_channel = false;
      pthread_mutex_lock(&global_dvb_state_lock);
      global_dvb_state->stream_used = false;
      pthread_mutex_unlock(&global_dvb_state_lock);
      return;
    }

    for (int i = state->demux_fds_cnt - 1; i >= 0; i--) {
        state->demux_fds_cnt--;
        MP_VERBOSE(stream, "DVBIN_CLOSE, close(%d), fd=%d, COUNT=%d\n", i,
                   state->demux_fds[i], state->demux_fds_cnt);
        close(state->demux_fds[i]);
    }
    close(state->dvr_fd);
    close(state->fe_fd);
    state->fe_fd = state->dvr_fd = -1;

    state->is_on = 0;
    state->cur_adapter = -1;
    state->cur_frontend = -1;

    pthread_mutex_lock(&global_dvb_state_lock);
    dvb_free_state(state);
    global_dvb_state = NULL;
    pthread_mutex_unlock(&global_dvb_state_lock);
}

static int dvb_streaming_start(stream_t *stream, char *progname)
{
    int i;
    dvb_channel_t *channel = NULL;
    dvb_priv_t *priv = stream->priv;
    dvb_state_t *state = priv->state;
    dvb_channels_list_t *list;

    if (progname == NULL)
        return 0;
    MP_VERBOSE(stream, "\r\ndvb_streaming_start(PROG: %s, ADAPTER: %d)\n",
               progname, priv->devno);

    list = state->adapters[state->cur_adapter].list;
    for (i = 0; i < list->NUM_CHANNELS; i ++) {
        if (!strcmp(list->channels[i].name, progname)) {
            channel = &(list->channels[i]);
            break;
        }
    }

    if (channel == NULL) {
        MP_ERR(stream, "\n\nDVBIN: no such channel \"%s\"\n\n", progname);
        return 0;
    }

    list->current = i;

    // When switching channels, cfg_channel_switch_offset
    // keeps the offset to the initially chosen channel.
    list->current = (list->NUM_CHANNELS + list->current + priv->opts->cfg_channel_switch_offset) % list->NUM_CHANNELS;
    channel = &(list->channels[list->current]);
    MP_INFO(stream, "Tuning to channel \"%s\"...\n", channel->name);
    MP_VERBOSE(stream, "PROGRAM NUMBER %d: name=%s, freq=%u\n", i,
               channel->name, channel->freq);

    if (!dvb_set_channel(stream, state->cur_adapter, list->current)) {
        MP_ERR(stream, "ERROR, COULDN'T SET CHANNEL  %i: \"%s\"\n", list->current, progname);
        dvbin_close(stream);
        return 0;
    }

    MP_VERBOSE(stream, "SUCCESSFUL EXIT from dvb_streaming_start\n");

    return 1;
}

void dvb_update_config(stream_t *stream)
{
    static int last_check = 0;
    int now = (int)(mp_time_sec()*10);

    // Throttle the check to at maximum once every 0.1 s.
    if (now != last_check) {
        last_check = now;
        dvb_priv_t *priv  = (dvb_priv_t *) stream->priv;
        if (m_config_cache_update(priv->opts_cache)) {
            dvb_state_t *state = priv->state;

            // Re-parse stream path, if we have cfg parameters now,
            // these should be preferred.
            if (!dvb_parse_path(stream)) {
                MP_ERR(stream, "error parsing DVB config, not tuning.");
                return;
            }

            int r = dvb_streaming_start(stream, priv->prog);
            if (r) {
                // Stream will be pulled down after channel switch,
                // persist state.
                state->switching_channel = true;
            }
        }
    }
}

static int dvb_open(stream_t *stream)
{
    // I don't force  the file format because, although it's almost always TS,
    // there are some providers that stream an IP multicast with M$ Mpeg4 inside
    dvb_priv_t *priv = NULL;

    pthread_mutex_lock(&global_dvb_state_lock);
    if (global_dvb_state && global_dvb_state->stream_used) {
      MP_ERR(stream, "DVB stream already in use, only one DVB stream can exist at a time!\n");
      pthread_mutex_unlock(&global_dvb_state_lock);
      goto err_out;
    }

    // Need to re-get config in any case, not part of global state.
    stream->priv = talloc_zero(stream, dvb_priv_t);
    priv = stream->priv;
    priv->opts_cache = m_config_cache_alloc(stream, stream->global, &stream_dvb_conf);
    priv->opts = priv->opts_cache->opts;

    dvb_state_t *state = dvb_get_state(stream);

    priv->state = state;
    priv->log = stream->log;
    if (state == NULL) {
        MP_ERR(stream, "DVB CONFIGURATION IS EMPTY, exit\n");
        pthread_mutex_unlock(&global_dvb_state_lock);
        goto err_out;
    }

    if (!dvb_parse_path(stream)) {
        goto err_out;
    }

    state->stream_used = true;
    pthread_mutex_unlock(&global_dvb_state_lock);

    if (state->is_on != 1) {
      // State could be already initialized, for example, we just did a channel switch.
      // The following setup only has to be done once.

      state->cur_frontend = -1;

      MP_VERBOSE(stream, "OPEN_DVB: prog=%s, devno=%d\n",
                 priv->prog, state->adapters[state->cur_adapter].devno);

      if (!dvb_streaming_start(stream, priv->prog))
          goto err_out;
    }

    stream->fill_buffer = dvb_streaming_read;
    stream->close = dvbin_close;
    stream->control = dvbin_stream_control;
    stream->streaming = true;
    stream->demuxer = "lavf";
    stream->lavf_type = "mpegts";

    return STREAM_OK;

err_out:
    talloc_free(priv);
    stream->priv = NULL;
    return STREAM_ERROR;
}

int dvb_parse_path(stream_t *stream)
{
    dvb_priv_t *priv = stream->priv;
    dvb_state_t *state = priv->state;

    // Parse stream path. Common rule: cfg wins over stream path,
    // since cfg may be changed at runtime.
    bstr prog, devno;
    if (!bstr_split_tok(bstr0(stream->path), "@", &devno, &prog)) {
        prog = devno;
        devno.len = 0;
    }

    if (priv->opts->cfg_devno != 0) {
        priv->devno = priv->opts->cfg_devno;
    } else if (devno.len) {
        bstr r;
        priv->devno = bstrtoll(devno, &r, 0);
        if (r.len || priv->devno < 0 || priv->devno >= MAX_ADAPTERS) {
            MP_ERR(stream, "invalid devno: '%.*s'\n", BSTR_P(devno));
            return 0;
        }
    } else {
        // Default to the default of cfg_devno.
        priv->devno = priv->opts->cfg_devno;
    }

    // Current adapter is derived from devno.
    state->cur_adapter = -1;
    for (int i = 0; i < state->adapters_count; i++) {
        if (state->adapters[i].devno == priv->devno) {
            state->cur_adapter = i;
            break;
        }
    }

    if (state->cur_adapter == -1) {
        MP_ERR(stream, "NO CONFIGURATION FOUND FOR ADAPTER N. %d!\n",
               priv->devno);
        return 0;
    }

    if (priv->opts->cfg_prog != NULL && strlen(priv->opts->cfg_prog) > 0) {
        talloc_free(priv->prog);
        priv->prog = talloc_strdup(priv, priv->opts->cfg_prog);
    } else if (prog.len) {
        talloc_free(priv->prog);
        priv->prog = bstrto0(priv, prog);
    } else {
        // We use the first program from the channel list.
        if (state->adapters[state->cur_adapter].list == NULL) {
            MP_ERR(stream, "No channel list available for adapter %d!\n", priv->devno);
            return 0;
        }
        talloc_free(priv->prog);
        priv->prog = talloc_strdup(priv, state->adapters[state->cur_adapter].list->channels[0].name);
    }

    MP_VERBOSE(stream, "DVB_CONFIG: prog=%s, devno=%d\n",
               priv->prog, priv->devno);
    return 1;
}

dvb_state_t *dvb_get_state(stream_t *stream)
{
    if (global_dvb_state != NULL) {
      return global_dvb_state;
    }
    struct mp_log *log = stream->log;
    struct mpv_global *global = stream->global;
    dvb_priv_t *priv = stream->priv;
    unsigned int delsys, delsys_mask[MAX_FRONTENDS], size;
    char filename[PATH_MAX], *conf_file;
    const char *conf_file_name;
    void *talloc_ctx;
    dvb_channels_list_t *list;
    dvb_adapter_config_t *adapters = NULL, *tmp;
    dvb_state_t *state = NULL;

    state = malloc(sizeof(dvb_state_t));
    if (state == NULL)
        return NULL;
    memset(state, 0x00, sizeof(dvb_state_t));
    state->switching_channel = false;
    state->is_on = 0;
    state->stream_used = true;
    state->fe_fd = state->dvr_fd = -1;
    for (unsigned int i = 0; i < MAX_ADAPTERS; i++) {
        list = NULL;
        for (unsigned int f = 0; f < MAX_FRONTENDS; f++) {
            snprintf(filename, sizeof(filename), "/dev/dvb/adapter%u/frontend%u", i, f);
            int fd = open(filename, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (fd < 0)
                continue;

            mp_verbose(log, "Opened device %s, FD: %d\n", filename, fd);
            delsys_mask[f] = dvb_get_tuner_delsys_mask(fd, log);
            delsys_mask[f] &= DELSYS_SUPP_MASK; /* Filter unsupported delivery systems. */
            close(fd);
            if (delsys_mask[f] == 0) {
                mp_verbose(log, "Frontend device %s has no supported delivery systems.\n",
                       filename);
                continue; /* Skip tuner. */
            }
            mp_verbose(log, "Frontend device %s offers some supported delivery systems.\n",
                   filename);
            /* Create channel list for adapter. */
            for (delsys = 0; delsys < SYS_DVB__COUNT__; delsys++) {
                if (!DELSYS_IS_SET(delsys_mask[f], delsys))
                    continue; /* Skip unsupported. */

                switch (delsys) {
                case SYS_DVBC_ANNEX_A:
                case SYS_DVBC_ANNEX_C:
                    conf_file_name = "channels.conf.cbl";
                    break;
                case SYS_ATSC:
                    conf_file_name = "channels.conf.atsc";
                    break;
                case SYS_DVBT:
                    if (DELSYS_IS_SET(delsys_mask[f], SYS_DVBT2))
                        continue; /* Add all channels later with T2. */
                    conf_file_name = "channels.conf.ter";
                    break;
                case SYS_DVBT2:
                    conf_file_name = "channels.conf.ter";
                    break;
                case SYS_DVBS:
                    if (DELSYS_IS_SET(delsys_mask[f], SYS_DVBS2))
                        continue; /* Add all channels later with S2. */
                    conf_file_name = "channels.conf.sat";
                    break;
                case SYS_DVBS2:
                    conf_file_name = "channels.conf.sat";
                    break;
                default:
                    continue;
                }

                if (priv->opts->cfg_file && priv->opts->cfg_file[0]) {
                    talloc_ctx = NULL;
                    conf_file = priv->opts->cfg_file;
                } else {
                    talloc_ctx = talloc_new(NULL);
                    conf_file = mp_find_config_file(talloc_ctx, global, conf_file_name);
                    if (conf_file) {
                        mp_verbose(log, "Ignoring other channels.conf files.\n");
                    } else {
                        conf_file = mp_find_config_file(talloc_ctx, global,
                                        "channels.conf");
                    }
                }

                list = dvb_get_channels(log, list, priv->opts->cfg_full_transponder,
                                        conf_file, f, delsys, delsys_mask[f]);
                talloc_free(talloc_ctx);
            }
        }
        /* Add adapter with non zero channel list. */
        if (list == NULL)
            continue;

        size = sizeof(dvb_adapter_config_t) * (state->adapters_count + 1);
        tmp = realloc(state->adapters, size);

        if (tmp == NULL) {
            mp_err(log, "DVB_CONFIG, can't realloc %d bytes, skipping\n",
                   size);
            free(list);
            continue;
        }
        adapters = tmp;

        state->adapters = adapters;
        state->adapters[state->adapters_count].devno = i;
        memcpy(&state->adapters[state->adapters_count].delsys_mask,
            &delsys_mask, (sizeof(unsigned int) * MAX_FRONTENDS));
        state->adapters[state->adapters_count].list = list;
        state->adapters_count++;
    }

    if (state->adapters_count == 0) {
        free(state);
        state = NULL;
    }

    global_dvb_state = state;
    return state;
}

const stream_info_t stream_info_dvb = {
    .name = "dvbin",
    .open = dvb_open,
    .protocols = (const char *const[]){ "dvb", NULL },
    .stream_origin = STREAM_ORIGIN_UNSAFE,
};
