
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include "cfgparser.h"

#ifdef HAVE_NEW_INPUT
extern void mp_input_register_options(m_config_t* cfg);
#endif

void
mp_register_options(m_config_t* cfg) {
  
#ifdef HAVE_NEW_INPUT
  mp_input_register_options(cfg);
#endif

}
