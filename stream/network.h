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

#pragma once

#include <stdbool.h>

struct m_sub_options;

struct mp_network_opts {
    bool cookies_enabled;
    char *cookies_file;
    char *useragent;
    char *referrer;
    char **http_header_fields;
    bool tls_verify;
    char *tls_ca_file;
    char *tls_cert_file;
    char *tls_key_file;
    double timeout;
    char *http_proxy;
};

extern const struct m_sub_options mp_network_conf;
