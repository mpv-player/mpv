
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include "../cfgparser.h"

extern void demux_audio_register_options(m_config_t* cfg);
extern void demuxer_register_options(m_config_t* cfg);
extern void demux_rwaudio_register_options(m_config_t* cfg);
#ifdef HAVE_CDDA
extern void cdda_register_options(m_config_t* cfg);
#endif

void libmpdemux_register_options(m_config_t* cfg) {

  demux_audio_register_options(cfg);
  demuxer_register_options(cfg);
  demux_rwaudio_register_options(cfg);
#ifdef HAVE_CDDA
  cdda_register_options(cfg);
#endif
}
