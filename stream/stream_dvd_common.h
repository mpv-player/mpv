#ifndef MPLAYER_STREAM_DVD_COMMON_H
#define MPLAYER_STREAM_DVD_COMMON_H

#include <inttypes.h>
#ifdef USE_DVDREAD_INTERNAL
#include <dvdread/ifo_types.h>
#else
#include <libdvdread/ifo_types.h>
#endif

int mp_dvdtimetomsec(dvd_time_t *dt);

#endif /* MPLAYER_STREAM_DVD_COMMON_H */
