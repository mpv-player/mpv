#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include "cfgparser.h"

extern void mp_input_register_options(m_config_t* cfg);
extern void libmpdemux_register_options(m_config_t* cfg);
extern void libvo_register_options(m_config_t* cfg);

void
mp_register_options(m_config_t* cfg) {
  
  mp_input_register_options(cfg);
  libmpdemux_register_options(cfg);
  libvo_register_options(cfg);
}
