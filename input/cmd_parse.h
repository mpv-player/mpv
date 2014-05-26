/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef MP_PARSE_COMMAND_H
#define MP_PARSE_COMMAND_H

struct mp_log;
struct mp_cmd;

// Parse text and return corresponding struct mp_cmd.
// The location parameter is for error messages.
struct mp_cmd *mp_input_parse_cmd_(struct mp_log *log, bstr str, const char *loc);

// Similar to mp_input_parse_cmd(), but takes a list of strings instead.
// Also, def_flags contains initial command flags (see mp_cmd_flags; the default
// as used by mp_input_parse_cmd is MP_ON_OSD_AUTO | MP_EXPAND_PROPERTIES).
// Keep in mind that these functions (naturally) don't take multiple commands,
// i.e. a ";" argument does not start a new command.
// The _strv version is limitted to MP_CMD_MAX_ARGS argv array items.
struct mp_cmd *mp_input_parse_cmd_strv(struct mp_log *log, int def_flags,
                                       const char **argv, const char *location);
struct mp_cmd *mp_input_parse_cmd_bstrv(struct mp_log *log, int def_flags,
                                        int argc, bstr *argv,
                                        const char *location);

// After getting a command from mp_input_get_cmd you need to free it using this
// function
void mp_cmd_free(struct mp_cmd *cmd);

// This creates a copy of a command (used by the auto repeat stuff).
struct mp_cmd *mp_cmd_clone(struct mp_cmd *cmd);

extern const struct m_option_type m_option_type_cycle_dir;

#define OPT_CYCLEDIR(...) \
    OPT_GENERAL(double, __VA_ARGS__, .type = &m_option_type_cycle_dir)

#endif
