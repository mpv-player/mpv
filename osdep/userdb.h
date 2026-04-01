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

#ifndef MP_OSDEP_USERDB_H
#define MP_OSDEP_USERDB_H

#include <time.h>

struct mp_log;

// Attempt to determine the current user's age by querying the system
// user database. On Linux, this uses the Varlink IPC protocol to connect
// to the systemd userdb multiplexer at
// /run/systemd/userdb/io.systemd.Multiplexer and calls the
// io.systemd.UserDatabase.GetUserRecord method to retrieve the current
// user's JSON User Record (https://systemd.io/USER_RECORD/).
//
// The birthDate field is expected in the regular section of the user
// record, using ISO 8601 date format (YYYY-MM-DD). It can be set via
// userdbctl or by placing a JSON drop-in in /etc/userdb/<user>.user.
//
// Returns the user's age in years, or -1 if the age could not be determined
// (socket unavailable, method error, or birthDate field missing).
// Callers should fall back to --user-age when this returns -1.
int mp_userdb_get_user_age(struct mp_log *log);

#endif
