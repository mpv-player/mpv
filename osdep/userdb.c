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

// systemd userdb integration for age verification via Varlink IPC.
//
// Queries the systemd JSON User Records database using the Varlink protocol
// (https://varlink.org/) over Unix domain sockets. The io.systemd.Multiplexer
// service on /run/systemd/userdb/io.systemd.Multiplexer aggregates all
// registered userdb providers (io.systemd.NameServiceSwitch, io.systemd.Home,
// io.systemd.DropIn, etc.) and exposes the io.systemd.UserDatabase interface.
//
// We call the io.systemd.UserDatabase.GetUserRecord method with the current
// user's name to retrieve the full JSON User Record as defined in the JSON
// User Records specification (https://systemd.io/USER_RECORD/).
//
// The birthDate field is expected in the regular section of the user record,
// using ISO 8601 date format (YYYY-MM-DD). It can be set via userdbctl or
// by placing a JSON drop-in in /etc/userdb/<username>.user.
//
// The Varlink protocol is a simple JSON-over-Unix-socket IPC: messages are
// JSON objects terminated by a NUL byte (\0). We connect to the multiplexer
// socket, send a GetUserRecord request, and parse the response for the
// birthDate field in the returned record object.
//
// If the socket is not available, the method fails, or the birthDate field
// is not present in the user record, the function returns -1 to signal that
// the caller should fall back to the --user-age option.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "config.h"
#include "userdb.h"
#include "common/msg.h"
#include "misc/json.h"
#include "misc/node.h"
#include "mpv_talloc.h"
#include <mpv/client.h>

#if HAVE_POSIX

#include <unistd.h>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>

// Path to the systemd userdb Varlink multiplexer socket.
// This service aggregates all userdb providers into a single endpoint.
// See systemd-userdbd.service(8) and https://systemd.io/USER_GROUP_API/.
#define USERDB_VARLINK_SOCKET "/run/systemd/userdb/io.systemd.Multiplexer"

// Varlink method for looking up a user record by name.
// Part of the io.systemd.UserDatabase interface.
#define USERDB_METHOD "io.systemd.UserDatabase.GetUserRecord"

// Maximum size of a Varlink response we'll accept (64 KiB).
// User records can be large due to signatures, PKCS#11 data, etc.
#define VARLINK_MAX_RESPONSE (64 * 1024)

// Timeout for Varlink socket operations in milliseconds.
#define VARLINK_TIMEOUT_MS 3000

// Parse ISO 8601 date (YYYY-MM-DD) and compute age in years
static int compute_age_from_birthdate(const char *date_str)
{
    int year, month, day;
    if (sscanf(date_str, "%d-%d-%d", &year, &month, &day) != 3)
        return -1;

    if (year < 1900 || year > 2100 || month < 1 || month > 12 ||
        day < 1 || day > 31)
        return -1;

    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    if (!tm_now)
        return -1;

    int age = (tm_now->tm_year + 1900) - year;
    if (tm_now->tm_mon + 1 < month ||
        (tm_now->tm_mon + 1 == month && tm_now->tm_mday < day))
    {
        age--;
    }

    return (age >= 0 && age <= 150) ? age : -1;
}

// Parse the Varlink JSON response and extract the birthDate from the
// user record. Uses mpv's json_parse() for proper structured parsing.
//
// Success response structure:
//   {"parameters":{"record":{..."birthDate":"YYYY-MM-DD"...}}}
// Error response structure:
//   {"error":"io.systemd.UserDatabase.NoRecordFound",...}
static int parse_varlink_response(struct mp_log *log, char *json)
{
    void *tmp = talloc_new(NULL);
    struct mpv_node root;

    int ret = json_parse(tmp, &root, &json, MAX_JSON_DEPTH);
    if (ret < 0) {
        mp_verbose(log, "Failed to parse Varlink JSON response.\n");
        talloc_free(tmp);
        return -1;
    }

    // Check for Varlink-level errors first
    struct mpv_node *err = node_map_get(&root, "error");
    if (err && err->format == MPV_FORMAT_STRING) {
        mp_verbose(log, "Varlink error: %s\n", err->u.string);
        talloc_free(tmp);
        return -1;
    }

    // Navigate: parameters -> record -> birthDate
    struct mpv_node *params = node_map_get(&root, "parameters");
    if (!params) {
        mp_verbose(log, "Varlink response missing 'parameters' key.\n");
        talloc_free(tmp);
        return -1;
    }

    struct mpv_node *record = node_map_get(params, "record");
    if (!record) {
        mp_verbose(log, "Varlink response missing 'record' in parameters.\n");
        talloc_free(tmp);
        return -1;
    }

    struct mpv_node *birth = node_map_get(record, "birthDate");
    if (!birth || birth->format != MPV_FORMAT_STRING) {
        mp_verbose(log, "birthDate field not found in user record.\n");
        talloc_free(tmp);
        return -1;
    }

    int age = compute_age_from_birthdate(birth->u.string);
    talloc_free(tmp);
    return age;
}

// Connect to the Varlink multiplexer Unix domain socket.
// Returns the socket fd on success, -1 on failure.
static int varlink_connect(struct mp_log *log)
{
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        mp_verbose(log, "Failed to create Unix socket: %s\n", strerror(errno));
        return -1;
    }

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
    };
    static_assert(sizeof(USERDB_VARLINK_SOCKET) <= sizeof(addr.sun_path),
                  "socket path too long");
    memcpy(addr.sun_path, USERDB_VARLINK_SOCKET,
           sizeof(USERDB_VARLINK_SOCKET));

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        mp_verbose(log, "Cannot connect to %s: %s\n",
                   USERDB_VARLINK_SOCKET, strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

// Send a NUL-terminated Varlink message on the socket.
// The Varlink protocol terminates each JSON message with a \0 byte.
static bool varlink_send(struct mp_log *log, int fd,
                         const char *msg, size_t len)
{
    // The message must be sent including the trailing NUL terminator
    size_t total = len + 1; // +1 for the NUL separator
    size_t sent = 0;
    while (sent < total) {
        ssize_t n = send(fd, msg + sent, total - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            mp_verbose(log, "Varlink send failed: %s\n", strerror(errno));
            return false;
        }
        sent += n;
    }
    return true;
}

// Receive a Varlink response (reads until NUL terminator or timeout).
// Returns the number of bytes read (excluding NUL), or -1 on error.
static ssize_t varlink_recv(struct mp_log *log, int fd,
                            char *buf, size_t bufsize)
{
    size_t total = 0;
    while (total < bufsize - 1) {
        struct pollfd pfd = { .fd = fd, .events = POLLIN };
        int ret = poll(&pfd, 1, VARLINK_TIMEOUT_MS);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            mp_verbose(log, "Varlink poll failed: %s\n", strerror(errno));
            return -1;
        }
        if (ret == 0) {
            mp_verbose(log, "Varlink response timed out after %d ms.\n",
                       VARLINK_TIMEOUT_MS);
            return -1;
        }

        ssize_t n = recv(fd, buf + total, bufsize - 1 - total, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            mp_verbose(log, "Varlink recv failed: %s\n", strerror(errno));
            return -1;
        }
        if (n == 0)
            break; // connection closed

        // Check for the NUL message terminator
        for (ssize_t i = 0; i < n; i++) {
            if (buf[total + i] == '\0') {
                total += i;
                buf[total] = '\0';
                return total;
            }
        }
        total += n;
    }

    buf[total] = '\0';
    return total;
}

int mp_userdb_get_user_age(struct mp_log *log)
{
    mp_verbose(log, "Querying systemd userdb via Varlink for user age...\n");
    mp_verbose(log, "Connecting to %s\n", USERDB_VARLINK_SOCKET);

    // Get current username
    const char *user = getenv("USER");
    if (!user) {
        struct passwd *pw = getpwuid(getuid());
        if (pw)
            user = pw->pw_name;
    }
    if (!user) {
        mp_verbose(log, "Could not determine current username.\n");
        return -1;
    }

    // Validate username - only allow safe characters per systemd user name rules
    size_t ulen = strlen(user);
    if (ulen == 0 || ulen > 64) {
        mp_verbose(log, "Username empty or too long.\n");
        return -1;
    }
    for (size_t i = 0; i < ulen; i++) {
        char c = user[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'))
        {
            mp_verbose(log, "Username contains characters outside "
                       "the allowed set [a-zA-Z0-9._-].\n");
            return -1;
        }
    }

    // Connect to the Varlink multiplexer socket
    int fd = varlink_connect(log);
    if (fd < 0) {
        mp_verbose(log, "systemd-userdbd not running or socket not available. "
                   "Use --user-age=<age> to set your age manually.\n");
        return -1;
    }

    // Build the Varlink request for io.systemd.UserDatabase.GetUserRecord.
    // The Varlink protocol sends JSON objects terminated by a NUL byte.
    // We request the record by userName and specify the service to query.
    char request[512];
    int reqlen = snprintf(request, sizeof(request),
        "{\"method\":\"" USERDB_METHOD "\","
         "\"parameters\":{\"userName\":\"%s\","
         "\"service\":\"io.systemd.Multiplexer\"}}", user);
    if (reqlen < 0 || reqlen >= (int)sizeof(request)) {
        mp_verbose(log, "Varlink request too large.\n");
        close(fd);
        return -1;
    }

    mp_verbose(log, "Sending Varlink call: %s.GetUserRecord(userName=%s)\n",
               "io.systemd.UserDatabase", user);

    // Send the request (varlink_send appends the NUL terminator)
    if (!varlink_send(log, fd, request, reqlen)) {
        close(fd);
        return -1;
    }

    // Receive the response
    char *response = malloc(VARLINK_MAX_RESPONSE);
    if (!response) {
        close(fd);
        return -1;
    }

    ssize_t rlen = varlink_recv(log, fd, response, VARLINK_MAX_RESPONSE);
    close(fd);

    if (rlen <= 0) {
        mp_verbose(log, "Empty or failed Varlink response.\n");
        free(response);
        return -1;
    }

    mp_verbose(log, "Received Varlink response (%zd bytes).\n", rlen);

    // Parse the JSON response using mpv's json_parse() and traverse
    // the node tree: parameters -> record -> birthDate
    mp_verbose(log, "Parsing JSON User Record for birthDate field...\n");
    int age = parse_varlink_response(log, response);
    free(response);

    if (age >= 0) {
        mp_info(log, "systemd userdb: birthDate found, computed user age: %d\n",
                age);
    } else {
        mp_verbose(log, "birthDate field not found in user record.\n");
        mp_verbose(log, "To set your birth date in your user record, create a "
                   "drop-in file:\n");
        mp_verbose(log, "  /etc/userdb/%s.user containing: "
                   "{\"birthDate\":\"YYYY-MM-DD\"}\n", user);
        mp_verbose(log, "Or use: userdbctl update --identity=- %s <<< "
                   "'{\"birthDate\":\"YYYY-MM-DD\"}'\n", user);
        mp_verbose(log, "Alternatively, set --user-age=<age> manually.\n");
    }

    return age;
}

#else

int mp_userdb_get_user_age(struct mp_log *log)
{
    mp_verbose(log, "systemd userdb Varlink API is only available on Linux. "
               "Use --user-age=<age> to set your age manually.\n");
    return -1;
}

#endif
