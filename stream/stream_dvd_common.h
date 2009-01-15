#ifndef MPLAYER_STREAM_DVD_COMMON_H
#define MPLAYER_STREAM_DVD_COMMON_H

#include "config.h"
#include <inttypes.h>
#include <dvdread/ifo_types.h>

int mp_dvdtimetomsec(dvd_time_t *dt);

#endif /* MPLAYER_STREAM_DVD_COMMON_H */
