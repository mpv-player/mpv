/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Or, point your browser to http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 * Based on dvbv5-zap.c from v4l-utils
 */

#include "misc/ctype.h"
#include "osdep/timer.h"
#include <pthread.h>

#include "stream.h"
#include "common/tags.h"
#include "options/m_option.h"
#include "options/m_config.h"
#include "options/options.h"
#include "options/path.h"

#include <linux/dvb/dmx.h>
#include "libdvbv5/dvb-file.h"
#include "libdvbv5/dvb-demux.h"
#include "libdvbv5/dvb-dev.h"
#include "libdvbv5/dvb-v5-std.h"
#include "libdvbv5/dvb-scan.h"
#include "libdvbv5/countries.h"

#include <unistd.h>
#include <strings.h>


#define DVB_BUF_SIZE    (4096 * 8 * 188)


typedef struct dvbv5_opts {
    int cfg_adapter;
    int cfg_frontend;
    int cfg_demux;
    char* cfg_cc;
    int n_vpid, n_apid;
    int cfg_verbose;
    bool cfg_rec_psi;
    char* cfg_lnb_name;
    int cfg_lna;
    int cfg_sat_number;
    int cfg_diseqc_wait;
    int cfg_freq_bpf;
    int cfg_port;
    char *cfg_server;
    int cfg_timeout;
    bool cfg_keep_tune;
} dvbv5_opts_t;

typedef struct {
    struct dvb_open_descriptor *dvr_fd;
    dvbv5_opts_t *opts;
    struct m_config_cache *opts_cache;
    struct dvb_device *dvb;
    char *demux_dev, *dvr_dev;
    struct dvb_open_descriptor *audio_fd, *video_fd;
} dvb_priv_t;

#define OPT_BASE_STRUCT struct dvbv5_opts
const struct m_sub_options stream_dvbv5_conf = {
    .opts = (const m_option_t[]) {
        {"adapter", OPT_INT(cfg_adapter), M_RANGE(0, 100)},
        {"frontend", OPT_INT(cfg_frontend), M_RANGE(0, 100)},
        {"demux", OPT_INT(cfg_demux), M_RANGE(0, 100)},
        {"cc", OPT_STRING(cfg_cc)},
        {"video-pid", OPT_INT(n_vpid), M_RANGE(0, 9999)},
        {"audio-pid", OPT_INT(n_apid), M_RANGE(0, 9999)},
        {"verbose", OPT_INT(cfg_verbose), M_RANGE(0, 9999)},
        {"pat", OPT_BOOL(cfg_rec_psi)},
        {"lnb-name", OPT_STRING(cfg_lnb_name)},
        {"lna", OPT_CHOICE(cfg_lna,
            {"auto", LNA_AUTO}, {"disable", 0}, {"enable", 1})},
        {"sat-number", OPT_INT(cfg_sat_number), M_RANGE(0, 9999)},
        {"diseqc-wait", OPT_INT(cfg_diseqc_wait), M_RANGE(0, 9999)},
        {"freq-bpf", OPT_INT(cfg_freq_bpf), M_RANGE(0, 9999)},
        {"port", OPT_INT(cfg_port), M_RANGE(0, 65536)},
        {"server", OPT_STRING(cfg_server)},
        {"timeout", OPT_INT(cfg_timeout), M_RANGE(0, 3600)},
        {"keep-tune", OPT_BOOL(cfg_keep_tune)},
        {0}
    },
    .size = sizeof(struct dvbv5_opts),
    .defaults = &(const dvbv5_opts_t){
        .cfg_adapter = 0,
        .cfg_frontend = 0,
        .cfg_demux = 0,
        .cfg_cc = NULL,
        .n_vpid = 0,
        .n_apid = 0,
        .cfg_verbose = 0,
        .cfg_rec_psi = 0,
        .cfg_lnb_name = NULL,
        .cfg_lna = LNA_AUTO,
        .cfg_sat_number = -1,
        .cfg_diseqc_wait = 0,
        .cfg_freq_bpf = 0,
        .cfg_port = 0,
        .cfg_server = NULL,
        .cfg_timeout = 120,
        .cfg_keep_tune = 0,
    },
};
static int timeout_flag = 0;

static void *do_timeout(void *priv_ptr) {
    dvb_priv_t *priv = priv_ptr;
    usleep(priv->opts->cfg_timeout * 1000000);
    timeout_flag = 1;
    return NULL;
}

static void dvbv5_close(stream_t *stream)
{
    dvb_priv_t *priv = stream->priv;
    dvb_dev_close(priv->dvr_fd);
    if (!priv->opts->cfg_keep_tune) {
        dvb_dev_free(priv->dvb);
    }
}

static int dvbv5_stream_control(struct stream *s, int cmd, void *arg)
{
    switch (cmd) {
    case STREAM_CTRL_GET_METADATA: {
        struct mp_tags *metadata = talloc_zero(NULL, struct mp_tags);
        char *progname = s->path;
        mp_tags_set_str(metadata, "title", progname);
        *(struct mp_tags **)arg = metadata;
        return STREAM_OK;
    }
    }
    return STREAM_UNSUPPORTED;
}

static int dvbv5_streaming_read(stream_t *stream, void *buffer, int size)
{
    dvb_priv_t *priv = stream->priv;
    ssize_t r;
    r = dvb_dev_read(priv->dvr_fd, buffer, size);
    return r;
}

/*
 * This function was adapted for MPV from v4l-utils/utils/dvb/dvbv5-zap.c
 * version v4l-utils-1.20.0-118-g9a628fbc
 */
static int parse(stream_t *stream, struct dvb_v5_fe_parms *parms,
         int *vpid, int *apid, int *sid)
{
    struct mpv_global *global = stream->global;
    void *talloc_ctx;
    char *conf_file;
    char *channel;
    enum dvb_file_formats input_format;
    channel = stream->path;
    dvb_priv_t *priv;
    struct dvbv5_opts *args;

    priv = stream->priv;
    args = priv->opts;

    input_format = dvb_parse_format("DVBV5");

    struct dvb_file *dvb_file;
    struct dvb_entry *entry;
    int i;
    uint32_t sys;

    /* This is used only when reading old formats */
    switch (parms->current_sys) {
    case SYS_DVBT:
    case SYS_DVBS:
    case SYS_DVBC_ANNEX_A:
    case SYS_ATSC:
        sys = parms->current_sys;
        break;
    case SYS_DVBC_ANNEX_C:
        sys = SYS_DVBC_ANNEX_A;
        break;
    case SYS_DVBC_ANNEX_B:
        sys = SYS_ATSC;
        break;
    case SYS_ISDBT:
    case SYS_DTMB:
        sys = SYS_DVBT;
        break;
    default:
        sys = SYS_UNDEFINED;
        break;
    }
    talloc_ctx = talloc_new(NULL);
    conf_file = mp_find_config_file(talloc_ctx, global, "channels.conf.dvbv5");
    dvb_file = dvb_read_file_format(conf_file, sys, input_format);
    if (!dvb_file)
        return -2;

    for (entry = dvb_file->first_entry; entry != NULL; entry = entry->next) {
        if (entry->channel && !strcmp(entry->channel, channel))
            break;
        if (entry->vchannel && !strcmp(entry->vchannel, channel))
            break;
    }
    /*
     * Give a second shot, using a case insensitive seek
     */
    if (!entry) {
        for (entry = dvb_file->first_entry; entry != NULL;
             entry = entry->next) {
            if (entry->channel && !strcasecmp(entry->channel, channel))
                break;
        }
    }

    if (!entry) {
        MP_ERR(stream, "Can't find channel\n");
        dvb_file_free(dvb_file);
        return -3;
    }

    /*
     * Both the DVBv5 format and the command line parameters may
     * specify the LNBf. If both have the definition, use the one
     * provided by the command line parameter, overriding the one
     * stored in the channel file.
     */
    if (entry->lnb && !parms->lnb) {
        int lnb = dvb_sat_search_lnb(entry->lnb);
        if (lnb == -1) {
            MP_ERR(stream, "unknown LNB %s\n", entry->lnb);
            dvb_file_free(dvb_file);
            return -1;
        }
        parms->lnb = dvb_sat_get_lnb(lnb);
    }

    if (parms->sat_number < 0 && entry->sat_number >= 0)
        parms->sat_number = entry->sat_number;

    if (entry->video_pid) {
        if (args->n_vpid < entry->video_pid_len)
            *vpid = entry->video_pid[args->n_vpid];
        else
            *vpid = entry->video_pid[0];
    }
    if (entry->audio_pid) {
        if (args->n_apid < entry->audio_pid_len)
            *apid = entry->audio_pid[args->n_apid];
        else
        *apid = entry->audio_pid[0];
    }
    if (entry->other_el_pid) {
        int type = -1;
        for (i = 0; i < entry->other_el_pid_len; i++) {
            if (type != entry->other_el_pid[i].type) {
                type = entry->other_el_pid[i].type;
            }
        }
    }
    *sid = entry->service_id;

    /* First of all, set the delivery system */
    dvb_retrieve_entry_prop(entry, DTV_DELIVERY_SYSTEM, &sys);
    if (dvb_set_compat_delivery_system(parms, sys)) {
        MP_ERR(stream, "dvb_set_compat_delivery_system failed\n");
        return -4;
    }

    /* Copy data into parms */
    for (i = 0; i < entry->n_props; i++) {
        uint32_t data = entry->props[i].u.data;
        /* Don't change the delivery system */
        if (entry->props[i].cmd == DTV_DELIVERY_SYSTEM)
            continue;
        dvb_fe_store_parm(parms, entry->props[i].cmd, data);
        if (parms->current_sys == SYS_ISDBT) {
            dvb_fe_store_parm(parms, DTV_ISDBT_PARTIAL_RECEPTION, 0);
            dvb_fe_store_parm(parms, DTV_ISDBT_SOUND_BROADCASTING, 0);
            dvb_fe_store_parm(parms, DTV_ISDBT_LAYER_ENABLED, 0x07);
            if (entry->props[i].cmd == DTV_CODE_RATE_HP) {
                dvb_fe_store_parm(parms, DTV_ISDBT_LAYERA_FEC,
                          data);
                dvb_fe_store_parm(parms, DTV_ISDBT_LAYERB_FEC,
                          data);
                dvb_fe_store_parm(parms, DTV_ISDBT_LAYERC_FEC,
                          data);
            } else if (entry->props[i].cmd == DTV_MODULATION) {
                dvb_fe_store_parm(parms,
                          DTV_ISDBT_LAYERA_MODULATION,
                          data);
                dvb_fe_store_parm(parms,
                          DTV_ISDBT_LAYERB_MODULATION,
                          data);
                dvb_fe_store_parm(parms,
                          DTV_ISDBT_LAYERC_MODULATION,
                          data);
            }
        }
        if (parms->current_sys == SYS_ATSC &&
            entry->props[i].cmd == DTV_MODULATION) {
            if (data != VSB_8 && data != VSB_16)
                dvb_fe_store_parm(parms,
                          DTV_DELIVERY_SYSTEM,
                          SYS_DVBC_ANNEX_B);
        }
    }

    dvb_file_free(dvb_file);
    return 0;
}

/*
 * This function was adapted for MPV from v4l-utils/utils/dvb/dvbv5-zap.c
 * version v4l-utils-1.20.0-118-g9a628fbc
 */
static int setup_frontend(stream_t *stream, struct dvb_v5_fe_parms *parms)
{
    int rc;

    rc = dvb_fe_set_parms(parms);
    if (rc < 0) {
        MP_ERR(stream, "dvb_fe_set_parms failed\n");
        return -1;
    }

    return 0;
}

/*
 * This function was adapted for MPV from v4l-utils/utils/dvb/dvbv5-zap.c
 * version v4l-utils-1.20.0-118-g9a628fbc
 */
static int check_frontend(stream_t *stream, struct dvb_v5_fe_parms *parms)
{
    int rc;
    fe_status_t status = 0;
    do {
        rc = dvb_fe_get_stats(parms);
        if (rc) {
            MP_ERR(stream, "dvb_fe_get_stats failed\n");
            usleep(1000000);
            continue;
        }

        status = 0;
        rc = dvb_fe_retrieve_stats(parms, DTV_STATUS, &status);
        if (status & FE_HAS_LOCK)
            break;
        usleep(1000000);
    } while (!timeout_flag);

    return status & FE_HAS_LOCK;
}

/*
 * This function was adapted for MPV from v4l-utils/utils/dvb/dvbv5-zap.c
 * version v4l-utils-1.20.0-118-g9a628fbc
 */
static int dvbv5_open(stream_t *stream)
{
    pthread_t timeout;
    char *demux_dev, *dvr_dev;
    int lnb = -1;
    int vpid = -1, apid = -1, sid = -1;
    int pmtpid = 0;
    struct dvb_open_descriptor *pat_fd = NULL, *pmt_fd = NULL;
    struct dvb_open_descriptor *sdt_fd = NULL;
    struct dvb_open_descriptor *sid_fd = NULL, *dvr_fd = NULL;
    struct dvb_open_descriptor *audio_fd = NULL, *video_fd = NULL;
    int r, ret;
    struct dvb_v5_fe_parms *parms = NULL;
    struct dvb_device *dvb;
    struct dvb_dev_list *dvb_dev;
    static dvb_priv_t cached_priv = {0};
    dvb_priv_t *priv = NULL;
    struct m_config_cache *opts_cache;
    dvbv5_opts_t *opts;

    opts_cache = m_config_cache_alloc(stream, stream->global, &stream_dvbv5_conf);
    opts = opts_cache->opts;

    if (opts->cfg_keep_tune && cached_priv.dvr_fd) {
        stream->fill_buffer = dvbv5_streaming_read;
        stream->close = dvbv5_close;
        stream->control = dvbv5_stream_control;
        stream->streaming = true;
        stream->demuxer = "lavf";
        stream->lavf_type = "mpegts";
        stream->priv = talloc_memdup(stream, &cached_priv, sizeof(dvb_priv_t));
        priv = stream->priv;
        priv->opts_cache = opts_cache;
        priv->opts = opts;
        dvb = priv->dvb;
        dvr_dev = priv->dvr_dev;
        demux_dev = priv->demux_dev;
        audio_fd = priv->audio_fd;
        video_fd = priv->video_fd;
        MP_INFO(stream, "Going cached!\n");


        parms = dvb->fe_parms;
        if (parse(stream, parms, &vpid, &apid, &sid)) {
            dvb_dev_free(dvb);
            return STREAM_ERROR;
        }

        uint32_t current_frequency, new_frequency;
        dvb_fe_retrieve_parm(dvb->fe_parms, DTV_FREQUENCY, &current_frequency);
        dvb_fe_retrieve_parm(parms, DTV_FREQUENCY, &new_frequency);

        if (current_frequency == new_frequency) {
            if (vpid >= 0) {
                if (!video_fd) {
                    MP_ERR(stream, "failed opening '%s'\n", demux_dev);
                    dvb_dev_free(dvb);
                    return STREAM_ERROR;
                }

                dvb_dev_set_bufsize(video_fd, DVB_BUF_SIZE);

                if (dvb_dev_dmx_set_pesfilter(video_fd, vpid, DMX_PES_VIDEO,
                    DMX_OUT_TS_TAP,
                    64 * 1024) < 0) {
                    dvb_dev_free(dvb);
                    return STREAM_ERROR;
                }
            }

            if (apid > 0) {
                if (!audio_fd) {
                    MP_ERR(stream, "failed opening '%s'\n", demux_dev);
                    dvb_dev_free(dvb);
                    return STREAM_ERROR;
                }
                if (dvb_dev_dmx_set_pesfilter(audio_fd, apid, DMX_PES_AUDIO,
                        DMX_OUT_TS_TAP,
                        64 * 1024) < 0) {
                    dvb_dev_free(dvb);
                    return STREAM_ERROR;
                }
            }
            dvr_fd = dvb_dev_open(dvb, dvr_dev, O_RDONLY);
            if (!dvr_fd) {
                MP_ERR(stream, "failed opening '%s'\n", dvr_dev);
                dvb_dev_free(dvb);
                return STREAM_ERROR;
            }

            priv->dvr_fd = dvr_fd;
            return STREAM_OK;
        } else {
            dvb_dev_free(priv->dvb);

            MP_INFO(stream, "Frequency changed!\n");
        }
    }

    stream->priv = talloc_zero(stream, dvb_priv_t);
    priv = stream->priv;

    priv->opts_cache = opts_cache;
    priv->opts = opts;

    stream->fill_buffer = dvbv5_streaming_read;
    stream->close = dvbv5_close;
    stream->control = dvbv5_stream_control;
    stream->streaming = true;
    stream->demuxer = "lavf";
    stream->lavf_type = "mpegts";

    if (priv->opts->cfg_lnb_name) {
        lnb = dvb_sat_search_lnb(priv->opts->cfg_lnb_name);
    }

    dvb = dvb_dev_alloc();
    if (!dvb)
        return STREAM_ERROR;

    if (priv->opts->cfg_server && priv->opts->cfg_port) {
        MP_INFO(stream, "Connecting to %s:%d\n", priv->opts->cfg_server, priv->opts->cfg_port);
        ret = dvb_dev_remote_init(dvb, priv->opts->cfg_server, priv->opts->cfg_port);
        if (ret < 0) {
            MP_ERR(stream, "dvb remote init failed: %i\n", ret);
            return STREAM_ERROR;
        }
    }

    dvb_dev_set_log(dvb, priv->opts->cfg_verbose, NULL);
    dvb_dev_find(dvb, NULL, NULL);
    parms = dvb->fe_parms;

    dvb_dev = dvb_dev_seek_by_adapter(dvb, priv->opts->cfg_adapter, priv->opts->cfg_demux, DVB_DEVICE_DEMUX);
    if (!dvb_dev) {
        dvb_dev_free(dvb);
        return STREAM_ERROR;
    }

    demux_dev = dvb_dev->sysname;

    dvb_dev = dvb_dev_seek_by_adapter(dvb, priv->opts->cfg_adapter, priv->opts->cfg_demux, DVB_DEVICE_DVR);
    if (!dvb_dev) {
        MP_ERR(stream, "Couldn't find dvr device node\n");
        dvb_dev_free(dvb);
        return STREAM_ERROR;
    }
    dvr_dev = dvb_dev->sysname;

    dvb_dev = dvb_dev_seek_by_adapter(dvb, priv->opts->cfg_adapter, priv->opts->cfg_frontend, DVB_DEVICE_FRONTEND);
    if (!dvb_dev)
        return STREAM_ERROR;

    if (!dvb_dev_open(dvb, dvb_dev->sysname, O_RDWR)) {
        dvb_dev_free(dvb);
        return STREAM_ERROR;
    }
    if (lnb >= 0)
        parms->lnb = dvb_sat_get_lnb(lnb);
    if (priv->opts->cfg_sat_number >= 0)
        parms->sat_number = priv->opts->cfg_sat_number;
    parms->diseqc_wait = priv->opts->cfg_diseqc_wait;
    parms->freq_bpf = priv->opts->cfg_freq_bpf;
    parms->lna = priv->opts->cfg_lna;

    r = dvb_fe_set_default_country(parms, priv->opts->cfg_cc);
    if (r < 0)
        MP_ERR(stream, "Failed to set the country code:%s\n", priv->opts->cfg_cc);

    if (parse(stream, parms, &vpid, &apid, &sid)) {
        dvb_dev_free(dvb);
        return STREAM_ERROR;
    }

    if (setup_frontend(stream, parms) < 0) {
        dvb_dev_free(dvb);
        return STREAM_ERROR;
    }

    if (priv->opts->cfg_rec_psi) {
        if (sid < 0) {
            MP_ERR(stream, "Service id 0x%04x was not specified at the file\n",
                sid);
            dvb_dev_free(dvb);
            return STREAM_ERROR;
        }

        sid_fd = dvb_dev_open(dvb, demux_dev, O_RDWR);
        if (!sid_fd) {
            MP_ERR(stream, "opening sid demux failed\n");
            dvb_dev_free(dvb);
            return STREAM_ERROR;
        }
        pmtpid = dvb_dev_dmx_get_pmt_pid(sid_fd, sid);
        dvb_dev_close(sid_fd);
        if (pmtpid <= 0) {
            MP_ERR(stream, "couldn't find pmt-pid for sid %04x\n",
                sid);

            dvb_dev_free(dvb);
            return STREAM_ERROR;
        }

        pat_fd = dvb_dev_open(dvb, demux_dev, O_RDWR);
        if (!pat_fd) {
            MP_ERR(stream, "opening pat demux failed\n");
            dvb_dev_free(dvb);
            return STREAM_ERROR;
        }
        if (dvb_dev_dmx_set_pesfilter(pat_fd, 0, DMX_PES_OTHER,
                DMX_OUT_TS_TAP,
                64 * 1024) < 0) {
            dvb_dev_free(dvb);
            return STREAM_ERROR;
        }

        pmt_fd = dvb_dev_open(dvb, demux_dev, O_RDWR);
        if (!pmt_fd) {
            MP_ERR(stream, "opening pmt demux failed\n");
            dvb_dev_free(dvb);
            return STREAM_ERROR;
        }
        if (dvb_dev_dmx_set_pesfilter(pmt_fd, pmtpid, DMX_PES_OTHER,
                DMX_OUT_TS_TAP,
                64 * 1024) < 0) {
            dvb_dev_free(dvb);
            return STREAM_ERROR;
        }

        /*
         * SDT may also be needed in order to play some streams
         */
        sdt_fd = dvb_dev_open(dvb, demux_dev, O_RDWR);
        if (!sdt_fd) {
            MP_ERR(stream, "opening sdt demux failed\n");
            dvb_dev_free(dvb);
            return STREAM_ERROR;
        }
        if (dvb_dev_dmx_set_pesfilter(sdt_fd, 0x0011, DMX_PES_OTHER,
                DMX_OUT_TS_TAP,
                64 * 1024) < 0) {
            dvb_dev_free(dvb);
            return STREAM_ERROR;
        }
    }

    if (vpid >= 0) {
        video_fd = dvb_dev_open(dvb, demux_dev, O_RDWR);
        if (!video_fd) {
            MP_ERR(stream, "failed opening '%s'\n", demux_dev);
            dvb_dev_free(dvb);
            return STREAM_ERROR;
        }

        dvb_dev_set_bufsize(video_fd, DVB_BUF_SIZE);

        if (dvb_dev_dmx_set_pesfilter(video_fd, vpid, DMX_PES_VIDEO,
            DMX_OUT_TS_TAP,
            64 * 1024) < 0) {
            dvb_dev_free(dvb);
            return STREAM_ERROR;
        }
    }

    if (apid > 0) {
        audio_fd = dvb_dev_open(dvb, demux_dev, O_RDWR);
        if (!audio_fd) {
            MP_ERR(stream, "failed opening '%s'\n", demux_dev);
            dvb_dev_free(dvb);
            return STREAM_ERROR;
        }
        if (dvb_dev_dmx_set_pesfilter(audio_fd, apid, DMX_PES_AUDIO,
                DMX_OUT_TS_TAP,
                64 * 1024) < 0) {
            dvb_dev_free(dvb);
            return STREAM_ERROR;
        }
    }

    pthread_create(&timeout, NULL, do_timeout, priv);

    if (!check_frontend(stream, parms)) {

        MP_ERR(stream, "frontend doesn't lock\n");
        dvb_dev_free(dvb);
        return STREAM_ERROR;
    }

    pthread_cancel(timeout);

    dvr_fd = dvb_dev_open(dvb, dvr_dev, O_RDONLY);
    if (!dvr_fd) {
        MP_ERR(stream, "failed opening '%s'\n", dvr_dev);
        dvb_dev_free(dvb);
        return STREAM_ERROR;
    }

    priv->demux_dev = demux_dev;
    priv->dvr_dev = dvr_dev;
    priv->dvr_fd = dvr_fd;
    priv->dvb = dvb;
    priv->audio_fd = audio_fd;
    priv->video_fd = video_fd;

    memcpy(&cached_priv, stream->priv, sizeof(dvb_priv_t));

    return STREAM_OK;
}

const stream_info_t stream_info_dvbv5 = {
    .name = "dvbv5",
    .open = dvbv5_open,
    .protocols = (const char *const[]){ "dvbv5", NULL },
    .stream_origin = STREAM_ORIGIN_UNSAFE,
};
