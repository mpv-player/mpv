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
#include <stdbool.h>

#include "core/mp_msg.h"
#include "core/m_option.h"
#include "m_config.h"
#include "playlist.h"
#include "playlist_parser.h"
#include "parser-mpcmd.h"
#include "osdep/macosx_finder_args.h"

#define GLOBAL 0
#define LOCAL 1

#define dvd_range(a)  (a > 0 && a < 256)


struct parse_state {
    struct m_config *config;
    int argc;
    char **argv;

    bool no_more_opts;
    bool error;

    bool is_opt;
    struct bstr arg;
    struct bstr param;
};

// Returns 0 if a valid option/file is available, <0 on error, 1 on end of args.
static int split_opt_silent(struct parse_state *p)
{
    assert(!p->error);

    if (p->argc < 1)
        return 1;

    p->is_opt = false;
    p->arg = bstr0(p->argv[0]);
    p->param = bstr0(NULL);

    p->argc--;
    p->argv++;

    if (p->no_more_opts || !bstr_startswith0(p->arg, "-") || p->arg.len == 1)
        return 0;

    if (bstrcmp0(p->arg, "--") == 0) {
        p->no_more_opts = true;
        return split_opt_silent(p);
    }

    p->is_opt = true;

    if (!bstr_eatstart0(&p->arg, "--"))
        bstr_eatstart0(&p->arg, "-");

    bool ambiguous = !bstr_split_tok(p->arg, "=", &p->arg, &p->param);

    int r = m_config_option_requires_param(p->config, p->arg);
    if (r < 0)
        return r;

    if (ambiguous && r > 0) {
        if (p->argc < 1)
            return M_OPT_MISSING_PARAM;
        p->param = bstr0(p->argv[0]);
        p->argc--;
        p->argv++;
    }

    return 0;
}

// Returns true if more args, false if all parsed or an error occurred.
static bool split_opt(struct parse_state *p)
{
    int r = split_opt_silent(p);
    if (r >= 0)
        return r == 0;
    p->error = true;

    mp_tmsg(MSGT_CFGPARSER, MSGL_FATAL,
            "Error parsing commandline option %.*s: %s\n",
            BSTR_P(p->arg), m_option_strerror(r));
    return false;
}

static bool parse_flag(bstr name, bstr f)
{
    struct m_option opt = {NULL, NULL, CONF_TYPE_FLAG, 0, 0, 1, NULL};
    int val = 0;
    m_option_parse(&opt, name, f, &val);
    return !!val;
}

bool m_config_parse_mp_command_line(m_config_t *config, struct playlist *files,
                                    int argc, char **argv)
{
    int mode = 0;
    struct playlist_entry *local_start = NULL;
    bool shuffle = false;

    int local_params_count = 0;
    struct playlist_param *local_params = 0;

    assert(config != NULL);
    assert(!config->file_local_mode);

    mode = GLOBAL;
#ifdef CONFIG_MACOSX_FINDER
    if (macosx_finder_args(config, files, argc, argv))
        return true;
#endif

    struct parse_state p = {config, argc, argv};
    while (split_opt(&p)) {
        if (p.is_opt) {
            int r;
            r = m_config_set_option_ext(config, p.arg, p.param,
                                        mode == LOCAL ? M_SETOPT_CHECK_ONLY : 0);
            if (r <= M_OPT_EXIT)
                goto err_out;
            if (r < 0) {
                mp_tmsg(MSGT_CFGPARSER, MSGL_FATAL,
                        "Setting commandline option --%.*s=%.*s failed.\n",
                        BSTR_P(p.arg), BSTR_P(p.param));
                goto err_out;
            }

            // Handle some special arguments outside option parser.

            if (!bstrcmp0(p.arg, "{")) {
                if (mode != GLOBAL) {
                    mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                           "'--{' can not be nested.\n");
                    goto err_out;
                }
                mode = LOCAL;
                // Needed for option checking.
                m_config_enter_file_local(config);
                assert(!local_start);
                local_start = files->last;
                continue;
            }

            if (!bstrcmp0(p.arg, "}")) {
                if (mode != LOCAL) {
                    mp_msg(MSGT_CFGPARSER, MSGL_ERR,
                           "Too many closing '--}'.\n");
                    goto err_out;
                }
                if (local_params_count) {
                    // The files added between '{' and '}' are the entries from
                    // the entry _after_ local_start, until the end of the list.
                    // If local_start is NULL, the list was empty on '{', and we
                    // want all files in the list.
                    struct playlist_entry *cur
                        = local_start ? local_start->next : files->first;
                    if (!cur)
                        mp_msg(MSGT_CFGPARSER, MSGL_WARN, "Ignored options!\n");
                    while (cur) {
                        playlist_entry_add_params(cur, local_params,
                                                local_params_count);
                        cur = cur->next;
                    }
                }
                local_params_count = 0;
                mode = GLOBAL;
                m_config_leave_file_local(config);
                local_start = NULL;
                shuffle = false;
                continue;
            }

            if (bstrcmp0(p.arg, "shuffle") == 0) {
                shuffle = parse_flag(p.arg, p.param);
                continue;
            }
            if (bstrcmp0(p.arg, "no-shuffle") == 0) {
                shuffle = false;
                continue;
            }

            if (bstrcmp0(p.arg, "playlist") == 0) {
                // append the playlist to the local args
                char *param0 = bstrdup0(NULL, p.param);
                struct playlist *pl = playlist_parse_file(param0);
                talloc_free(param0);
                if (!pl) {
                    mp_tmsg(MSGT_CFGPARSER, MSGL_FATAL,
                            "Error reading playlist '%.*s'", BSTR_P(p.param));
                    goto err_out;
                }
                playlist_transfer_entries(files, pl);
                talloc_free(pl);
                continue;
            }

            if (mode == LOCAL) {
                MP_TARRAY_APPEND(NULL, local_params, local_params_count,
                                 (struct playlist_param) {p.arg, p.param});
            }
        } else {
            // filename
            void *tmp = talloc_new(NULL);
            bstr file = p.arg;
            char *file0 = bstrdup0(tmp, p.arg);
            // expand DVD filename entries like dvd://1-3 into component titles
            if (bstr_startswith0(file, "dvd://")) {
                int offset = 6;
                char *splitpos = strstr(file0 + offset, "-");
                if (splitpos != NULL) {
                    char *endpos;
                    int start_title = strtol(file0 + offset, &endpos, 10);
                    int end_title;
                    //entries like dvd://-2 imply start at title 1
                    if (start_title < 0) {
                        end_title = abs(start_title);
                        start_title = 1;
                    } else
                        end_title = strtol(splitpos + 1, &endpos, 10);

                    if (dvd_range(start_title) && dvd_range(end_title)
                            && (start_title < end_title)) {
                        for (int j = start_title; j <= end_title; j++) {
                            char *f = talloc_asprintf(tmp, "dvd://%d%s", j,
                                                      endpos);
                            playlist_add_file(files, f);
                        }
                    } else
                        mp_tmsg(MSGT_CFGPARSER, MSGL_ERR,
                                "Invalid play entry %s\n", file0);

                } else // dvd:// or dvd://x entry
                    playlist_add_file(files, file0);
            } else
                playlist_add_file(files, file0);
            talloc_free(tmp);

            // Lock stdin if it will be used as input
            if (bstrcmp0(file, "-") == 0)
                m_config_set_option0(config, "consolecontrols", "no");
        }
    }

    if (p.error)
        goto err_out;

    if (mode != GLOBAL) {
        mp_tmsg(MSGT_CFGPARSER, MSGL_ERR,
                "Missing closing --} on command line.\n");
        goto err_out;
    }

    if (shuffle)
        playlist_shuffle(files);

    talloc_free(local_params);
    assert(!config->file_local_mode);
    return true;

err_out:
    talloc_free(local_params);
    if (config->file_local_mode)
        m_config_leave_file_local(config);
    return false;
}

extern int mp_msg_levels[];

/* Parse some command line options early before main parsing.
 * --no-config prevents reading configuration files (otherwise done before
 * command line parsing), and --really-quiet suppresses messages printed
 * during normal options parsing.
 */
void m_config_preparse_command_line(m_config_t *config, int argc, char **argv)
{
    // Hack to shut up parser error messages
    int msg_lvl_backup = mp_msg_levels[MSGT_CFGPARSER];
    mp_msg_levels[MSGT_CFGPARSER] = -11;

    struct parse_state p = {config, argc, argv};
    while (split_opt_silent(&p) == 0) {
        if (p.is_opt) {
            // Ignore non-pre-parse options. They will be set later.
            // Option parsing errors will be handled later as well.
            m_config_set_option_ext(config, p.arg, p.param,
                                    M_SETOPT_PRE_PARSE_ONLY);
            if (bstrcmp0(p.arg, "v") == 0)
                verbose++;
        }
    }

    mp_msg_levels[MSGT_CFGPARSER] = msg_lvl_backup;
}
