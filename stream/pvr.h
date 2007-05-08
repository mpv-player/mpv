#ifndef PVR_H
#define PVR_H

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

#endif /* PVR_H */
