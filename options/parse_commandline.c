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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdbool.h>

#include "config.h"

#include "osdep/io.h"
#include "common/global.h"
#include "common/msg.h"
#include "common/msg_control.h"
#include "m_option.h"
#include "m_config.h"
#include "options.h"
#include "common/playlist.h"
#include "parse_commandline.h"

#define GLOBAL 0
#define LOCAL 1

struct parse_state {
    struct m_config *config;
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

    if (!p->argv || !p->argv[0])
        return 1;

    p->is_opt = false;
    p->arg = bstr0(p->argv[0]);
    p->param = bstr0(NULL);

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

    bool need_param = m_config_option_requires_param(p->config, p->arg) > 0;

    if (ambiguous && need_param) {
        if (!p->argv[0])
            return M_OPT_MISSING_PARAM;
        p->param = bstr0(p->argv[0]);
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

    MP_FATAL(p->config, "Error parsing commandline option %.*s: %s\n",
             BSTR_P(p->arg), m_option_strerror(r));
    return false;
}

#ifdef __MINGW32__
static void process_non_option(struct playlist *files, const char *arg)
{
    glob_t gg;

    // Glob filenames on Windows (cmd.exe doesn't do this automatically)
    if (glob(arg, 0, NULL, &gg)) {
        playlist_add_file(files, arg);
    } else {
        for (int i = 0; i < gg.gl_pathc; i++)
            playlist_add_file(files, gg.gl_pathv[i]);

        globfree(&gg);
    }
}
#else
static void process_non_option(struct playlist *files, const char *arg)
{
    playlist_add_file(files, arg);
}
#endif

// returns M_OPT_... error code
int m_config_parse_mp_command_line(m_config_t *config, struct playlist *files,
                                   struct mpv_global *global, char **argv)
{
    int ret = M_OPT_UNKNOWN;
    int mode = 0;
    struct playlist_entry *local_start = NULL;

    int local_params_count = 0;
    struct playlist_param *local_params = 0;

    assert(config != NULL);

    mode = GLOBAL;

    struct parse_state p = {config, argv};
    while (split_opt(&p)) {
        if (p.is_opt) {
            int flags = M_SETOPT_FROM_CMDLINE;
            if (mode == LOCAL)
                flags |= M_SETOPT_BACKUP | M_SETOPT_CHECK_ONLY;
            int r = m_config_set_option_cli(config, p.arg, p.param, flags);
            if (r == M_OPT_EXIT) {
                ret = r;
                goto err_out;
            } else if (r < 0) {
                MP_FATAL(config, "Setting commandline option --%.*s=%.*s failed.\n",
                         BSTR_P(p.arg), BSTR_P(p.param));
                goto err_out;
            }

            // Handle some special arguments outside option parser.

            if (!bstrcmp0(p.arg, "{")) {
                if (mode != GLOBAL) {
                    MP_ERR(config, "'--{' can not be nested.\n");
                    goto err_out;
                }
                mode = LOCAL;
                assert(!local_start);
                local_start = files->last;
                continue;
            }

            if (!bstrcmp0(p.arg, "}")) {
                if (mode != LOCAL) {
                    MP_ERR(config, "Too many closing '--}'.\n");
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
                        MP_WARN(config, "Ignored options!\n");
                    while (cur) {
                        playlist_entry_add_params(cur, local_params,
                                                local_params_count);
                        cur = cur->next;
                    }
                }
                local_params_count = 0;
                mode = GLOBAL;
                m_config_restore_backups(config);
                local_start = NULL;
                continue;
            }

            if (bstrcmp0(p.arg, "playlist") == 0) {
                // append the playlist to the local args
                char *param0 = bstrdup0(NULL, p.param);
                struct playlist *pl = playlist_parse_file(param0, NULL, global);
                talloc_free(param0);
                if (!pl) {
                    MP_FATAL(config, "Error reading playlist '%.*s'\n",
                             BSTR_P(p.param));
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
            char *file0 = bstrdup0(NULL, p.arg);
            process_non_option(files, file0);
            talloc_free(file0);
        }
    }

    if (p.error)
        goto err_out;

    if (mode != GLOBAL) {
        MP_ERR(config, "Missing closing --} on command line.\n");
        goto err_out;
    }

    ret = 0; // success

err_out:
    talloc_free(local_params);
    m_config_restore_backups(config);
    return ret;
}

/* Parse some command line options early before main parsing.
 * --no-config prevents reading configuration files (otherwise done before
 * command line parsing), and --really-quiet suppresses messages printed
 * during normal options parsing.
 */
void m_config_preparse_command_line(m_config_t *config, struct mpv_global *global,
                                    int *verbose, char **argv)
{
    struct parse_state p = {config, argv};
    while (split_opt_silent(&p) == 0) {
        if (p.is_opt) {
            // Ignore non-pre-parse options. They will be set later.
            // Option parsing errors will be handled later as well.
            int flags = M_SETOPT_FROM_CMDLINE | M_SETOPT_PRE_PARSE_ONLY;
            m_config_set_option_cli(config, p.arg, p.param, flags);
            if (bstrcmp0(p.arg, "v") == 0)
                (*verbose)++;
        }
    }

    for (int n = 0; n < config->num_opts; n++)
        config->opts[n].warning_was_printed = false;
}
