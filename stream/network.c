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

#include <float.h>

#include "network.h"
#include "options/m_option.h"

#define OPT_BASE_STRUCT struct mp_network_opts

const struct m_sub_options mp_network_conf = {
    .opts = (const m_option_t[]) {
        {"http-header-fields", OPT_STRINGLIST(http_header_fields)},
        {"user-agent", OPT_STRING(useragent)},
        {"referrer", OPT_STRING(referrer)},
        {"cookies", OPT_BOOL(cookies_enabled)},
        {"cookies-file", OPT_STRING(cookies_file), .flags = M_OPT_FILE},
        {"tls-verify", OPT_BOOL(tls_verify)},
        {"tls-ca-file", OPT_STRING(tls_ca_file), .flags = M_OPT_FILE},
        {"tls-cert-file", OPT_STRING(tls_cert_file), .flags = M_OPT_FILE},
        {"tls-key-file", OPT_STRING(tls_key_file), .flags = M_OPT_FILE},
        {"network-timeout", OPT_DOUBLE(timeout), M_RANGE(0, DBL_MAX)},
        {"http-proxy", OPT_STRING(http_proxy)},
        {0}
    },
    .size = sizeof(struct mp_network_opts),
    .defaults = &(const struct mp_network_opts){
        .useragent = "libmpv",
        .timeout = 60,
    },
};
