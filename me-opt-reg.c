
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include "cfgparser.h"

extern void libmpdemux_register_options(m_config_t* cfg);

void
me_register_options(m_config_t* cfg) {

  libmpdemux_register_options(cfg);

}
