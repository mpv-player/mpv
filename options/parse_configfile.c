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

#define PRINT_LINENUM   MP_ERR(config, "%s:%d: ", config_pathname, line_num)
#define CONFIGFILE_ERR(...)  do { PRINT_LINENUM; MP_ERR(config, __VA_ARGS__); } while (0)

/// Maximal include depth.
#define MAX_RECURSION_DEPTH 8

// Load options and profiles from from a config file.
//  config_pathname: path to the config file
//  initial_section: default section where to add normal options
//  flags: M_SETOPT_* bits
//  returns: 1 on sucess, -1 on error, 0 if file not accessible.
int m_config_parse_config_file(m_config_t *config, const char *config_pathname,
                               char *initial_section, int flags)
{
#define MAX_LINE_LEN    10000
#define MAX_OPT_LEN     1000
#define MAX_PARAM_LEN   1500
    FILE *config_fp = NULL;
    char *line = NULL;
    char opt[MAX_OPT_LEN + 1];
    char param[MAX_PARAM_LEN + 1];
    int line_num = 0;
    int line_pos, opt_pos, param_pos;
    int ret = 1;
    int errors = 0;
    m_profile_t *profile = m_config_add_profile(config, initial_section);

    flags |= M_SETOPT_FROM_CONFIG_FILE;

    MP_VERBOSE(config, "Reading config file %s\n.", config_pathname);

    if (config->recursion_depth > MAX_RECURSION_DEPTH) {
        MP_ERR(config, "maximum 'include' nesting depth exceeded\n");
        ret = -1;
        goto out;
    }

    if ((line = malloc(MAX_LINE_LEN + 1)) == NULL) {
        ret = -1;
        goto out;
    } else {
        MP_VERBOSE(config, "\n");
    }

    if ((config_fp = fopen(config_pathname, "r")) == NULL) {
        MP_VERBOSE(config, "Can't open config file: %s\n", mp_strerror(errno));
        ret = 0;
        goto out;
    }

    while (fgets(line, MAX_LINE_LEN, config_fp)) {
        if (errors >= 16) {
            MP_FATAL(config, "too many errors\n");
            goto out;
        }

        line_num++;
        line_pos = 0;

        /* skip BOM */
        if (strncmp(line, "\xEF\xBB\xBF", 3) == 0)
            line_pos += 3;

        /* skip whitespaces */
        while (mp_isspace(line[line_pos]))
            ++line_pos;

        /* EOL / comment */
        if (line[line_pos] == '\0' || line[line_pos] == '#')
            goto nextline;

        /* read option */
        for (opt_pos = 0; mp_isprint(line[line_pos]) &&
             line[line_pos] != ' ' &&
             line[line_pos] != '#' &&
             line[line_pos] != '='; /* NOTHING */) {
            opt[opt_pos++] = line[line_pos++];
            if (opt_pos >= MAX_OPT_LEN) {
                CONFIGFILE_ERR("option name too long\n");
                errors++;
                ret = -1;
                goto nextline;
            }
        }
        if (opt_pos == 0) {
            CONFIGFILE_ERR("parse error\n");
            ret = -1;
            errors++;
            goto nextline;
        }
        opt[opt_pos] = '\0';

        /* Profile declaration */
        if (opt_pos > 2 && opt[0] == '[' && opt[opt_pos - 1] == ']') {
            opt[opt_pos - 1] = '\0';
            profile = m_config_add_profile(config, opt + 1);
            goto nextline;
        }

        /* skip whitespaces */
        while (mp_isspace(line[line_pos]))
            ++line_pos;

        param_pos = 0;
        bool param_set = false;

        /* check '=' */
        if (line[line_pos] == '=') {
            line_pos++;
            param_set = true;

            /* skip whitespaces */
            while (mp_isspace(line[line_pos]))
                ++line_pos;

            /* read parameter */
            if (line[line_pos] == '"' || line[line_pos] == '\'') {
                char quote = line[line_pos];
                ++line_pos;
                for (param_pos = 0; line[line_pos] != quote; /* NOTHING */) {
                    if (line[line_pos] == '\0') {
                        CONFIGFILE_ERR("unterminated quotes\n");
                        ret = -1;
                        errors++;
                        goto nextline;
                    }
                    param[param_pos++] = line[line_pos++];
                    if (param_pos >= MAX_PARAM_LEN) {
                        CONFIGFILE_ERR("option '%s' parameter too long\n", opt);
                        ret = -1;
                        errors++;
                        goto nextline;
                    }
                }
                line_pos++; /* skip the closing " or ' */
                goto param_done;
            }

            if (line[line_pos] == '%') {
                char *start = &line[line_pos + 1];
                char *end = start;
                unsigned long len = strtoul(start, &end, 10);
                if (start != end && end[0] == '%') {
                    if (len >= MAX_PARAM_LEN - 1 ||
                        strlen(end + 1) < len)
                    {
                        CONFIGFILE_ERR("bogus %% length\n");
                        ret = -1;
                        errors++;
                        goto nextline;
                    }
                    param_pos = snprintf(param, sizeof(param), "%.*s",
                                         (int)len, end + 1);
                    line_pos += 1 + (end - start) + 1 + len;
                    goto param_done;
                }
            }

            for (param_pos = 0; mp_isprint(line[line_pos])
                    && !mp_isspace(line[line_pos])
                    && line[line_pos] != '#'; /* NOTHING */) {
                param[param_pos++] = line[line_pos++];
                if (param_pos >= MAX_PARAM_LEN) {
                    CONFIGFILE_ERR("parameter too long");
                    ret = -1;
                    errors++;
                    goto nextline;
                }
            }

        param_done:

            while (mp_isspace(line[line_pos]))
                ++line_pos;
        }
        param[param_pos] = '\0';

        /* EOL / comment */
        if (line[line_pos] != '\0' && line[line_pos] != '#') {
            CONFIGFILE_ERR("'%s': extra characters\n", line + line_pos);
            ret = -1;
        }

        bstr bopt = bstr0(opt);
        bstr bparam = bstr0(param);

        if (bopt.len >= 3)
            bstr_eatstart0(&bopt, "--");

        if (profile && bstr_equals0(bopt, "profile-desc")) {
            m_profile_set_desc(profile, bparam);
            goto nextline;
        }

        bool need_param = m_config_option_requires_param(config, bopt) > 0;
        if (need_param && !param_set) {
            CONFIGFILE_ERR("error parsing option '%.*s=%.*s': %s\n",
                   BSTR_P(bopt), BSTR_P(bparam),
                   m_option_strerror(M_OPT_MISSING_PARAM));
            goto nextline;
        }

        int r;
        if (profile)
            r = m_config_set_profile_option(config, profile, bopt, bparam);
        else
            r = m_config_set_option_ext(config, bopt, bparam, flags);
        if (r < 0) {
            CONFIGFILE_ERR("setting option '%.*s=%.*s' failed\n",
                   BSTR_P(bopt), BSTR_P(bparam));
            goto nextline;
        }

nextline:
        ;
    }

out:
    if (line)
        free(line);
    if (config_fp)
        fclose(config_fp);
    config->recursion_depth--;
    if (ret < 0)
        MP_FATAL(config, "error loading config file %s\n", config_pathname);
    return ret;
}
