/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_PARSER_CFG_H
#define MPLAYER_PARSER_CFG_H

#include "m_config.h"

int m_config_parse_config_file(m_config_t* config, const char *conffile,
                               char *initial_section, int flags);

int m_config_parse(m_config_t *config, const char *location, bstr data,
                   char *initial_section, int flags);

#endif /* MPLAYER_PARSER_CFG_H */
