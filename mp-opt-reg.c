
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include "cfgparser.h"

#ifdef HAVE_NEW_INPUT
extern void mp_input_register_options(m_config_t* cfg);
#endif
extern void libmpdemux_register_options(m_config_t* cfg);
extern void libvo_register_options(m_config_t* cfg);

void
mp_register_options(m_config_t* cfg) {
  
#ifdef HAVE_NEW_INPUT
  mp_input_register_options(cfg);
#endif
  libmpdemux_register_options(cfg);
  libvo_register_options(cfg);
}
