#ifndef MPLAYER_STREAM_DVD_COMMON_H
#define MPLAYER_STREAM_DVD_COMMON_H

#include "config.h"
#include <inttypes.h>
#include <dvdread/ifo_types.h>

extern char *dvd_device;
extern const char * const dvd_audio_stream_channels[6];
extern const char * const dvd_audio_stream_types[8];

int mp_dvdtimetomsec(dvd_time_t *dt);

#endif /* MPLAYER_STREAM_DVD_COMMON_H */
