
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include "../cfgparser.h"

extern void demux_audio_register_options(m_config_t* cfg);
extern void demuxer_register_options(m_config_t* cfg);

void libmpdemux_register_options(m_config_t* cfg) {

  demux_audio_register_options(cfg);
  demuxer_register_options(cfg);

}
