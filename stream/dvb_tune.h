/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_DVB_TUNE_H
#define MPLAYER_DVB_TUNE_H

#include "dvbin.h"

int dvb_get_tuner_type(int fe_fd);
int dvb_open_devices(dvb_priv_t *priv, int n, int demux_cnt);
int dvb_fix_demuxes(dvb_priv_t *priv, int cnt);
int dvb_set_ts_filt(int fd, uint16_t pid, dmx_pes_type_t pestype);
int dvb_demux_stop(int fd);
int dvb_demux_start(int fd);
int dvb_tune(dvb_priv_t *priv, int freq, char pol, int srate, int diseqc,
             int tone, fe_spectral_inversion_t specInv,
             fe_modulation_t modulation, fe_guard_interval_t guardInterval,
             fe_transmit_mode_t TransmissionMode, fe_bandwidth_t bandWidth,
             fe_code_rate_t HP_CodeRate, fe_code_rate_t LP_CodeRate,
             fe_hierarchy_t hier, int timeout);

#endif /* MPLAYER_DVB_TUNE_H */
