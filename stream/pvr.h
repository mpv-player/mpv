/*
 * stream layer for hardware MPEG 1/2/4 encoders a.k.a PVR
 *  (such as WinTV PVR-150/250/350/500 (a.k.a IVTV), pvrusb2 and cx88)
 * See http://ivtvdriver.org/index.php/Main_Page for more details on the
 *  cards supported by the ivtv driver.
 *
 * Copyright (C) 2006 Benjamin Zores
 * Copyright (C) 2007 Sven Gothel (Channel Navigation)
 *
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

#ifndef MPLAYER_PVR_H
#define MPLAYER_PVR_H

#include "stream.h"
#include "tv.h"

/**
 * @brief Get the current station name.
 *        The pointer is valid, till the stream is closed.
 * @return The stream's station name
 */
const char *pvr_get_current_stationname (stream_t *stream);

/**
 * @brief Get the current channel name.
 *        The pointer is valid, till the stream is closed.
 * @return The stream's channel name 
 */
const char *pvr_get_current_channelname (stream_t *stream);

/**
 * @brief Get the current frequency.
 * @return frequency
 */
int pvr_get_current_frequency (stream_t *stream);

/**
 * @brief Set the current station using the channel name.
 *        This function will fail,
 *        if the channel does not exist, or the station is not enabled
 * @return 0 if the station is available, otherwise -1
 */
int pvr_set_channel (stream_t *stream, const char *channel);

/**
 * @brief Set the current station using to the last set channel
 * @return 0 if the station is available, otherwise -1
 */
int pvr_set_lastchannel (stream_t *stream);

/**
 * @brief Set the current channel using the frequency.
 *        This function will fail,
 *        if the frequency does not exist, or the station is not enabled
 * @return 0 if the station is available, otherwise -1
 */
int pvr_set_freq (stream_t *stream, int freq);

/**
 * @brief Set the current station while stepping.
 *        This function will fail,
 *        if the station does not exist, or the station is not enabled
 * @return 0 if the station is available, otherwise -1
 */
int pvr_set_channel_step (stream_t *stream, int step);

/**
 * @brief Set the current frequency while stepping
 *        This function will fail,
 *        if the frequency is invalid, i.e. <0
 * @return 0 if success, otherwise -1
 */
int pvr_force_freq_step (stream_t *stream, int step);

#endif /* MPLAYER_PVR_H */
