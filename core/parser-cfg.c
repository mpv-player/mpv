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
#include <ctype.h>
#include <assert.h>

#include "osdep/io.h"

#include "parser-cfg.h"
#include "core/mp_msg.h"
#include "core/m_option.h"
#include "m_config.h"

/// Maximal include depth.
#define MAX_RECURSION_DEPTH 8

/// Current include depth.
static int recursion_depth = 0;

/// Setup the \ref Config from a config file.
/** \param config The config object.
 *  \param conffile Path to the config file.
 *  \return 1 on sucess, -1 on error, 0 if file not accessible.
 */
int m_config_parse_config_file(m_config_t *config, const char *conffile)
{
#define PRINT_LINENUM   mp_msg(MSGT_CFGPARSER, MSGL_ERR, "%s:%d: ", conffile, line_num)
#define MAX_LINE_LEN    10000
#define MAX_OPT_LEN     1000
#define MAX_PARAM_LEN   1500
    FILE *fp = NULL;
    char *line = NULL;
    char opt[MAX_OPT_LEN + 1];
    char param[MAX_PARAM_LEN + 1];
    char c;             /* for the "" and '' check */
    int tmp;
    int line_num = 0;
    int line_pos;       /* line pos */
    int opt_pos;        /* opt pos */
    int param_pos;      /* param pos */
    int ret = 1;
    int errors = 0;
    m_profile_t *profile = NULL;

    mp_msg(MSGT_CFGPARSER, MSGL_V, "Reading config file %s", conffile);

    if (recursion_depth > MAX_RECURSION_DEPTH) {
        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
               ": too deep 'include'. check your configfiles\n");
        ret = -1;
        goto out;
    }

    if ((line = malloc(MAX_LINE_LEN + 1)) == NULL) {
        mp_msg(MSGT_CFGPARSER, MSGL_FATAL,
               "\ncan't get memory for 'line': %s", strerror(errno));
        ret = -1;
        goto out;
    } else

        mp_msg(MSGT_CFGPARSER, MSGL_V, "\n");

    if ((fp = fopen(conffile, "r")) == NULL) {
        mp_msg(MSGT_CFGPARSER, MSGL_V, ": %s\n", strerror(errno));
        ret = 0;
        goto out;
    }

    while (fgets(line, MAX_LINE_LEN, fp)) {
        if (errors >= 16) {
            mp_msg(MSGT_CFGPARSER, MSGL_FATAL, "too many errors\n");
            goto out;
        }

        line_num++;
        line_pos = 0;

        /* skip whitespaces */
        while (isspace(line[line_pos]))
            ++line_pos;

        /* EOL / comment */
        if (line[line_pos] == '\0' || line[line_pos] == '#')
            continue;

        /* read option. */
        for (opt_pos = 0; isprint(line[line_pos]) &&
             line[line_pos] != ' ' &&
             line[line_pos] != '#' &&
             line[line_pos] != '='; /* NOTHING */) {
            opt[opt_pos++] = line[line_pos++];
            if (opt_pos >= MAX_OPT_LEN) {
                PRINT_LINENUM;
                mp_msg(MSGT_CFGPARSER, MSGL_ERR, "too long option\n");
                errors++;
                ret = -1;
                goto nextline;
            }
        }
        if (opt_pos == 0) {
            PRINT_LINENUM;
            mp_msg(MSGT_CFGPARSER, MSGL_ERR, "parse error\n");
            ret = -1;
            errors++;
            continue;
        }
        opt[opt_pos] = '\0';

        /* Profile declaration */
        if (opt_pos > 2 && opt[0] == '[' && opt[opt_pos - 1] == ']') {
            opt[opt_pos - 1] = '\0';
            if (strcmp(opt + 1, "default"))
                profile = m_config_add_profile(config, opt + 1);
            else
                profile = NULL;
            continue;
        }

        /* skip whitespaces */
        while (isspace(line[line_pos]))
            ++line_pos;

        param_pos = 0;
        bool param_set = false;

        /* check '=' */
        if (line[line_pos] == '=') {
            line_pos++;
            param_set = true;

            /* whitespaces... */
            while (isspace(line[line_pos]))
                ++line_pos;

            /* read the parameter */
            if (line[line_pos] == '"' || line[line_pos] == '\'') {
                c = line[line_pos];
                ++line_pos;
                for (param_pos = 0; line[line_pos] != c; /* NOTHING */) {
                    param[param_pos++] = line[line_pos++];
                    if (param_pos >= MAX_PARAM_LEN) {
                        PRINT_LINENUM;
                        mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                               "option %s has a too long parameter\n", opt);
                        ret = -1;
                        errors++;
                        goto nextline;
                    }
                }
                line_pos++;                 /* skip the closing " or ' */
            } else {
                for (param_pos = 0; isprint(line[line_pos])
                        && !isspace(line[line_pos])
                        && line[line_pos] != '#'; /* NOTHING */) {
                    param[param_pos++] = line[line_pos++];
                    if (param_pos >= MAX_PARAM_LEN) {
                        PRINT_LINENUM;
                        mp_msg(MSGT_CFGPARSER, MSGL_ERR, "too long parameter\n");
                        ret = -1;
                        errors++;
                        goto nextline;
                    }
                }
            }

            while (isspace(line[line_pos]))
                ++line_pos;
        }
        param[param_pos] = '\0';

        /* EOL / comment */
        if (line[line_pos] != '\0' && line[line_pos] != '#') {
            PRINT_LINENUM;
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "extra characters: %s\n", line + line_pos);
            ret = -1;
        }

        bstr bopt = bstr0(opt);
        bstr bparam = bstr0(param);

        if (profile && bstr_equals0(bopt, "profile-desc")) {
            m_profile_set_desc(profile, param);
            goto nextline;
        }

        tmp = m_config_option_requires_param(config, bopt);
        if (tmp > 0 && !param_set)
            tmp = M_OPT_MISSING_PARAM;
        if (tmp < 0) {
            PRINT_LINENUM;
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "error parsing option %s=%s: %s\n",
                   opt, param, m_option_strerror(tmp));
            continue;
        }

        if (profile) {
            tmp = m_config_set_profile_option(config, profile, bopt, bparam);
        } else {
            tmp = m_config_set_option_ext(config, bopt, bparam,
                                          M_SETOPT_FROM_CONFIG_FILE);
        }
        if (tmp < 0) {
            PRINT_LINENUM;
            mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                   "setting option %s='%s' failed.\n", opt, param);
            continue;
            /* break */
        }
nextline:
        ;
    }

out:
    free(line);
    if (fp)
        fclose(fp);
    --recursion_depth;
    if (ret < 0) {
        mp_msg(MSGT_CFGPARSER, MSGL_FATAL, "Error loading config file %s.\n",
               conffile);
    }
    return ret;
}
