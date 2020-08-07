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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "osdep/io.h"

#include "parse_configfile.h"
#include "common/common.h"
#include "common/msg.h"
#include "misc/ctype.h"
#include "m_option.h"
#include "m_config.h"

// Skip whitespace and comments (assuming there are no line breaks)
static bool skip_ws(bstr *s)
{
    *s = bstr_lstrip(*s);
    if (bstr_startswith0(*s, "#"))
        s->len = 0;
    return s->len;
}

int m_config_parse(m_config_t *config, const char *location, bstr data,
                   char *initial_section, int flags)
{
    m_profile_t *profile = m_config_add_profile(config, initial_section);
    void *tmp = talloc_new(NULL);
    int line_no = 0;
    int errors = 0;

    bstr_eatstart0(&data, "\xEF\xBB\xBF"); // skip BOM

    while (data.len) {
        talloc_free_children(tmp);
        bool ok = false;

        line_no++;
        char loc[512];
        snprintf(loc, sizeof(loc), "%s:%d:", location, line_no);

        bstr line = bstr_strip_linebreaks(bstr_getline(data, &data));
        if (!skip_ws(&line))
            continue;

        // Profile declaration
        if (bstr_eatstart0(&line, "[")) {
            bstr profilename;
            if (!bstr_split_tok(line, "]", &profilename, &line)) {
                MP_ERR(config, "%s missing closing ]\n", loc);
                goto error;
            }
            if (skip_ws(&line)) {
                MP_ERR(config, "%s unparseable extra characters: '%.*s'\n",
                       loc, BSTR_P(line));
                goto error;
            }
            profile = m_config_add_profile(config, bstrto0(tmp, profilename));
            continue;
        }

        bstr_eatstart0(&line, "--");

        bstr option = line;
        while (line.len && (mp_isalnum(line.start[0]) || line.start[0] == '_' ||
                            line.start[0] == '-'))
            line = bstr_cut(line, 1);
        option.len = option.len - line.len;
        skip_ws(&line);

        bstr value = {0};
        if (bstr_eatstart0(&line, "=")) {
            skip_ws(&line);
            if (line.len && (line.start[0] == '"' || line.start[0] == '\'')) {
                // Simple quoting, like "value"
                char term[2] = {line.start[0], 0};
                line = bstr_cut(line, 1);
                if (!bstr_split_tok(line, term, &value, &line)) {
                    MP_ERR(config, "%s unterminated quote\n", loc);
                    goto error;
                }
            } else if (bstr_eatstart0(&line, "%")) {
                // Quoting with length, like %5%value
                bstr rest;
                long long len = bstrtoll(line, &rest, 10);
                if (rest.len == line.len || !bstr_eatstart0(&rest, "%") ||
                    len > rest.len)
                {
                    MP_ERR(config, "%s fixed-length quoting expected - put "
                           "\"quotes\" around the option value if you did not "
                           "intend to use this, but your option value starts "
                           "with '%%'\n", loc);
                    goto error;
                }
                value = bstr_splice(rest, 0, len);
                line = bstr_cut(rest, len);
            } else {
                // No quoting; take everything until the comment or end of line
                int end = bstrchr(line, '#');
                value = bstr_strip(end < 0 ? line : bstr_splice(line, 0, end));
                line.len = 0;
            }
        }
        if (skip_ws(&line)) {
            MP_ERR(config, "%s unparseable extra characters: '%.*s'\n",
                   loc, BSTR_P(line));
            goto error;
        }

        int res = m_config_set_profile_option(config, profile, option, value);
        if (res < 0) {
            MP_ERR(config, "%s setting option %.*s='%.*s' failed.\n",
                   loc, BSTR_P(option), BSTR_P(value));
            goto error;
        }

        ok = true;
    error:
        if (!ok)
            errors++;
        if (errors > 16) {
            MP_ERR(config, "%s: too many errors, stopping.\n", location);
            break;
        }
    }

    if (config->recursion_depth == 0)
        m_config_finish_default_profile(config, flags);

    talloc_free(tmp);
    return 1;
}

static bstr read_file(struct mp_log *log, const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f) {
        mp_verbose(log, "Can't open config file: %s\n", mp_strerror(errno));
        return (bstr){0};
    }
    char *data = talloc_array(NULL, char, 0);
    size_t size = 0;
    while (1) {
        size_t left = talloc_get_size(data) - size;
        if (!left) {
            MP_TARRAY_GROW(NULL, data, size + 1);
            continue;
        }
        size_t s = fread(data + size, 1, left, f);
        if (!s) {
            if (ferror(f))
                mp_err(log, "Error reading config file.\n");
            fclose(f);
            MP_TARRAY_APPEND(NULL, data, size, 0);
            return (bstr){data, size - 1};
        }
        size += s;
    }
    assert(0);
}

// Load options and profiles from from a config file.
//  conffile: path to the config file
//  initial_section: default section where to add normal options
//  flags: M_SETOPT_* bits
//  returns: 1 on success, -1 on error, 0 if file not accessible.
int m_config_parse_config_file(m_config_t *config, const char *conffile,
                               char *initial_section, int flags)
{
    flags = flags | M_SETOPT_FROM_CONFIG_FILE;

    MP_VERBOSE(config, "Reading config file %s\n", conffile);

    bstr data = read_file(config->log, conffile);
    if (!data.start)
        return 0;

    int r = m_config_parse(config, conffile, data, initial_section, flags);
    talloc_free(data.start);
    return r;
}
