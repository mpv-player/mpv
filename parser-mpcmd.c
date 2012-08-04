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

#include "mp_msg.h"
#include "m_option.h"
#include "m_config.h"
#include "playlist.h"
#include "playlist_parser.h"
#include "parser-mpcmd.h"
#include "osdep/macosx_finder_args.h"

#define GLOBAL 0
#define LOCAL 1

#define dvd_range(a)  (a > 0 && a < 256)


static bool split_opt(struct bstr *opt, struct bstr *param, bool *old_syntax)
{
    if (!bstr_startswith0(*opt, "-") || opt->len == 1)
        return false;
    if (bstr_startswith0(*opt, "--")) {
        *old_syntax = false;
        *opt = bstr_cut(*opt, 2);
        *param = bstr0(NULL);
        int idx = bstrchr(*opt, '=');
        if (idx > 0) {
            *param = bstr_cut(*opt, idx + 1);
            *opt = bstr_splice(*opt, 0, idx);
        }
    } else {
        *old_syntax = true;
        *opt = bstr_cut(*opt, 1);
    }
    return true;
}

static int map_to_option(struct m_config *config, bool old_syntax,
                         const struct m_option **mp_opt,
                         struct bstr *optname, struct bstr *param)
{
    if (!mp_opt)
        mp_opt = &(const struct m_option *){0};
    *mp_opt = m_config_get_option(config, *optname);
    if (*mp_opt)
        return 0;
    if (!bstr_startswith0(*optname, "no-"))
        return -1;
    struct bstr s = bstr_cut(*optname, 3);
    *mp_opt = m_config_get_option(config, s);
    if (!*mp_opt || (*mp_opt)->type != &m_option_type_flag)
        return -1;
    if (param->len)
        return -2;
    if (old_syntax)
        return -3;
    *optname = s;
    *param = bstr0("no");
    return 0;
}

bool m_config_parse_mp_command_line(m_config_t *config, struct playlist *files,
                                    int argc, char **argv)
{
    int mode = 0;
    bool no_more_opts = false;
    bool opt_exit = false;   // exit immediately after parsing (help options)
    struct playlist_entry *local_start = NULL;
    struct bstr orig_opt;
    bool shuffle = false;

    int local_params_count = 0;
    struct playlist_param *local_params = 0;

    assert(config != NULL);
    assert(!config->file_local_mode);
    assert(argv != NULL);
    assert(argc >= 1);

    config->mode = M_COMMAND_LINE;
    mode = GLOBAL;
#ifdef CONFIG_MACOSX_FINDER
    if (macosx_finder_args(config, files, argc, argv))
        return true;
#endif

    for (int i = 1; i < argc; i++) {
        //next:
        struct bstr opt = bstr0(argv[i]);
        orig_opt = opt;
        /* check for -- (no more options id.) except --help! */
        if (!bstrcmp0(opt, "--")) {
            no_more_opts = true;
            continue;
        }
        if (!bstrcmp0(opt, "--{")) {
            if (mode != GLOBAL) {
                mp_msg(MSGT_CFGPARSER, MSGL_ERR, "'--{' can not be nested\n");
                goto err_out;
            }
            mode = LOCAL;
            // Needed for option checking.
            m_config_enter_file_local(config);
            assert(!local_start);
            local_start = files->last;
            continue;
        }

        if (!bstrcmp0(opt, "--}")) {
            if (mode != LOCAL) {
                mp_msg(MSGT_CFGPARSER, MSGL_ERR, "too many closing '--}'\n");
                goto err_out;
            }
            if (local_params_count) {
                // The files added between '{' and '}' are the entries from the
                // entry _after_ local_start, until the end of the list. If
                // local_start is NULL, the list was empty on '{', and we want
                // all files in the list.
                struct playlist_entry *cur
                    = local_start ? local_start->next : files->first;
                if (!cur)
                    mp_msg(MSGT_CFGPARSER, MSGL_WARN, "ignored options\n");
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
            continue;
        }

        struct bstr param = bstr0(i+1 < argc ? argv[i+1] : NULL);
        bool old_syntax;
        if (!no_more_opts && split_opt(&opt, &param, &old_syntax)) {
            const struct m_option *mp_opt;
            int ok = map_to_option(config, old_syntax, &mp_opt, &opt, &param);
            if (ok < 0) {
                if (ok == -3)
                    mp_tmsg(MSGT_CFGPARSER, MSGL_ERR,
                            "Option --%.*s can't be used with single-dash "
                            "syntax\n", BSTR_P(opt));
                else if (ok == -2)
                    mp_tmsg(MSGT_CFGPARSER, MSGL_ERR,
                            "A --no-* option can't take parameters: "
                            "--%.*s=%.*s\n", BSTR_P(opt), BSTR_P(param));
                else
                    mp_tmsg(MSGT_CFGPARSER, MSGL_ERR,
                            "Unknown option on the command line: --%.*s\n",
                            BSTR_P(opt));
                goto print_err;
            }
            // Handle some special arguments outside option parser.
            // --loop when it applies to a group of files (per-file is option)
            if (bstrcasecmp0(opt, "shuffle") == 0) {
                shuffle = true;
            } else if (bstrcasecmp0(opt, "no-shuffle") == 0) {
                shuffle = false;
            } else if (bstrcasecmp0(opt, "playlist") == 0) {
                if (param.len <= 0)
                    goto print_err;
                // append the playlist to the local args
                char *param0 = bstrdup0(NULL, param);
                struct playlist *pl = playlist_parse_file(param0);
                talloc_free(param0);
                if (!pl)
                    goto print_err;
                playlist_transfer_entries(files, pl);
                talloc_free(pl);
                i += old_syntax;
            } else {
                // "normal" options
                int r;
                if (mode == GLOBAL) {
                    r = m_config_set_option(config, opt, param, old_syntax);
                } else {
                    r = m_config_check_option(config, opt, param, old_syntax);
                    if (r >= 0) {
                        if (r == 0)
                            param = bstr0(NULL);  // for old_syntax case
                        struct playlist_param p = {opt, param};
                        MP_TARRAY_APPEND(NULL, local_params,
                                         local_params_count, p);
                    }
                }
                if (r <= M_OPT_EXIT) {
                    opt_exit = true;
                    r = M_OPT_EXIT - r;
                } else if (r < 0) {
                    char *msg = m_option_strerror(r);
                    if (!msg)
                        goto print_err;
                    mp_tmsg(MSGT_CFGPARSER, MSGL_FATAL,
                            "Error parsing commandline option \"%.*s\": %s\n",
                            BSTR_P(orig_opt), msg);
                    goto err_out;
                }
                if (old_syntax)
                    i += r;
            }
        } else {  /* filename */
            int is_dvdnav = strstr(argv[i], "dvdnav://") != NULL;
            mp_msg(MSGT_CFGPARSER, MSGL_DBG2, "Adding file %s\n", argv[i]);
            // expand DVD filename entries like dvd://1-3 into component titles
            if (strstr(argv[i], "dvd://") != NULL || is_dvdnav) {
                int offset = is_dvdnav ? 9 : 6;
                char *splitpos = strstr(argv[i] + offset, "-");
                if (splitpos != NULL) {
                    int start_title = strtol(argv[i] + offset, NULL, 10);
                    int end_title;
                    //entries like dvd://-2 imply start at title 1
                    if (start_title < 0) {
                        end_title = abs(start_title);
                        start_title = 1;
                    } else
                        end_title = strtol(splitpos + 1, NULL, 10);

                    if (dvd_range(start_title) && dvd_range(end_title)
                            && (start_title < end_title)) {
                        for (int j = start_title; j <= end_title; j++) {
                            char entbuf[15];
                            snprintf(entbuf, sizeof(entbuf),
                                    is_dvdnav ? "dvdnav://%d" : "dvd://%d", j);
                            playlist_add_file(files, entbuf);
                        }
                    } else
                        mp_tmsg(MSGT_CFGPARSER, MSGL_ERR,
                                "Invalid play entry %s\n", argv[i]);

                } else // dvd:// or dvd://x entry
                    playlist_add_file(files, argv[i]);
            } else
                playlist_add_file(files, argv[i]);

            // Lock stdin if it will be used as input
            if (strcasecmp(argv[i], "-") == 0)
                m_config_set_option0(config, "consolecontrols", "no", false);
        }
    }

    if (opt_exit)
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

print_err:
    mp_tmsg(MSGT_CFGPARSER, MSGL_FATAL,
            "Error parsing option on the command line: %.*s\n",
            BSTR_P(orig_opt));
err_out:
    talloc_free(local_params);
    if (config->file_local_mode)
        m_config_leave_file_local(config);
    return false;
}

extern int mp_msg_levels[];

/* Parse some command line options early before main parsing.
 * --noconfig prevents reading configuration files (otherwise done before
 * command line parsing), and --really-quiet suppresses messages printed
 * during normal options parsing.
 */
int m_config_preparse_command_line(m_config_t *config, int argc, char **argv,
                                   int *verbose)
{
    int ret = 0;

    // Hack to shut up parser error messages
    int msg_lvl_backup = mp_msg_levels[MSGT_CFGPARSER];
    mp_msg_levels[MSGT_CFGPARSER] = -11;

    config->mode = M_COMMAND_LINE_PRE_PARSE;

    for (int i = 1 ; i < argc ; i++) {
        struct bstr opt = bstr0(argv[i]);
        // No more options after --
        if (!bstrcmp0(opt, "--"))
            break;
        struct bstr param = bstr0(i+1 < argc ? argv[i+1] : NULL);
        bool old_syntax;
        if (!split_opt(&opt, &param, &old_syntax))
            continue;   // Ignore non-option arguments
        // Ignore invalid options
        if (map_to_option(config, old_syntax, NULL, &opt, &param) < 0)
            continue;
        // "-v" is handled here
        if (!bstrcmp0(opt, "v")) {
            (*verbose)++;
            continue;
        }
        // Set, non-pre-parse options will be ignored
        int r = m_config_set_option(config, opt, param, old_syntax);
        if (r < 0)
            ret = r;
        else if (old_syntax)
            i += r;
    }

    mp_msg_levels[MSGT_CFGPARSER] = msg_lvl_backup;

    return ret;
}
