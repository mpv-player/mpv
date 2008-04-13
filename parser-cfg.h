#ifndef MPLAYER_PARSER_CFG_H
#define MPLAYER_PARSER_CFG_H

#include "m_config.h"

int m_config_parse_config_file(m_config_t* config, char *conffile);

int m_config_preparse_command_line(m_config_t *config, int argc, char **argv);

#endif /* MPLAYER_PARSER_CFG_H */
