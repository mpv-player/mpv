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

#ifndef MPLAYER_PARSER_MPCMD_H
#define MPLAYER_PARSER_MPCMD_H

#include <stdbool.h>

struct playlist;
struct m_config;
struct mpv_global;

int m_config_parse_mp_command_line(m_config_t *config, struct playlist *files,
                                   struct mpv_global *global, char **argv);
void m_config_preparse_command_line(m_config_t *config, struct mpv_global *global,
                                    int *verbose, char **argv);

#endif /* MPLAYER_PARSER_MPCMD_H */
