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
#include "playtree.h"
#include "parser-mpcmd.h"
#include "osdep/macosx_finder_args.h"

#define GLOBAL 0
#define LOCAL 1

#define dvd_range(a)  (a > 0 && a < 256)


static inline void add_entry(play_tree_t **last_parentp,
                             play_tree_t **last_entryp, play_tree_t *entry)
{
    if (*last_entryp == NULL)
        play_tree_set_child(*last_parentp, entry);
    else
        play_tree_append_entry(*last_entryp, entry);
    *last_entryp = entry;
}

static bool split_opt(struct bstr *opt, struct bstr *param, bool *old_syntax)
{
    if (!bstr_startswith0(*opt, "-") || opt->len == 1)
        return false;
    if (bstr_startswith0(*opt, "--")) {
        *old_syntax = false;
        *opt = bstr_cut(*opt, 2);
        *param = bstr(NULL);
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


// Parse command line to set up config and playtree
play_tree_t *m_config_parse_mp_command_line(m_config_t *config, int argc,
                                            char **argv)
{
    int mode = 0;
    bool no_more_opts = false;
    bool opt_exit = false;   // exit immediately after parsing (help options)
    play_tree_t *last_parent, *last_entry = NULL, *root;
    struct bstr orig_opt;

    assert(config != NULL);
    assert(argv != NULL);
    assert(argc >= 1);

    config->mode = M_COMMAND_LINE;
    mode = GLOBAL;
#ifdef CONFIG_MACOSX_FINDER
    root = macosx_finder_args(config, argc, argv);
    if (root)
        return root;
#endif

    last_parent = root = play_tree_new();

    for (int i = 1; i < argc; i++) {
        //next:
        struct bstr opt = bstr(argv[i]);
        orig_opt = opt;
        /* check for -- (no more options id.) except --help! */
        if (!bstrcmp0(opt, "--")) {
            no_more_opts = true;
            continue;
        }
        if (!bstrcmp0(opt, "{")) {
            play_tree_t *entry = play_tree_new();
            mode = LOCAL;
            if (last_parent->flags & PLAY_TREE_RND)
                entry->flags |= PLAY_TREE_RND;
            if (last_entry == NULL)
                play_tree_set_child(last_parent, entry);
            else {
                play_tree_append_entry(last_entry, entry);
                last_entry = NULL;
            }
            last_parent = entry;
            continue;
        }

        if (!bstrcmp0(opt, "}")) {
            if (!last_parent || !last_parent->parent) {
                mp_msg(MSGT_CFGPARSER, MSGL_ERR, "too much }-\n");
                goto err_out;
            }
            last_entry = last_parent;
            last_parent = last_entry->parent;
            continue;
        }

        struct bstr param = bstr(i+1 < argc ? argv[i+1] : NULL);
        bool old_syntax;
        if (!no_more_opts && split_opt(&opt, &param, &old_syntax)) {
            // Handle some special arguments outside option parser.
            // --loop when it applies to a group of files (per-file is option)
            if (bstrcasecmp0(opt, "loop") == 0 &&
                (!last_entry || last_entry->child)) {
                struct bstr rest;
                int l = bstrtoll(param, &rest, 0);
                if (!param.len || rest.len) {
                    mp_tmsg(MSGT_CFGPARSER, MSGL_ERR,
                            "The loop option must be an integer: \"%.*s\"\n",
                            BSTR_P(param));
                    goto print_err;
                } else {
                    play_tree_t *pt = last_entry ? last_entry : last_parent;
                    l = l <= 0 ? -1 : l;
                    pt->loop = l;
                    i += old_syntax;
                }
            } else if (bstrcasecmp0(opt, "shuffle") == 0) {
                if (last_entry && last_entry->child)
                    last_entry->flags |= PLAY_TREE_RND;
                else
                    last_parent->flags |= PLAY_TREE_RND;
            } else if (bstrcasecmp0(opt, "noshuffle") == 0 ||
                       bstrcasecmp0(opt, "no-shuffle") == 0) {
                if (last_entry && last_entry->child)
                    last_entry->flags &= ~PLAY_TREE_RND;
                else
                    last_parent->flags &= ~PLAY_TREE_RND;
            } else if (bstrcasecmp0(opt, "playlist") == 0) {
                if (param.len <= 0)
                    goto print_err;
                struct play_tree *entry = parse_playlist_file(config, param);
                if (!entry)
                    goto print_err;
                add_entry(&last_parent, &last_entry, entry);
                if ((last_parent->flags & PLAY_TREE_RND) && entry->child)
                    entry->flags |= PLAY_TREE_RND;
                mode = LOCAL;
                i += old_syntax;
            } else {
                // "normal" options
                const struct m_option *mp_opt;
                mp_opt = m_config_get_option(config, opt);
                if (!mp_opt) {
                    mp_tmsg(MSGT_CFGPARSER, MSGL_ERR,
                            "Unknown option on the command line: --%.*s\n",
                            BSTR_P(opt));
                    goto print_err;
                }
                int r;
                if (mode == GLOBAL || (mp_opt->flags & M_OPT_GLOBAL)) {
                    r = m_config_set_option(config, opt, param, old_syntax);
                } else {
                    r = m_config_check_option(config, opt, param, old_syntax);
                    if (r >= 0) {
                        play_tree_t *pt = last_entry ? last_entry : last_parent;
                        if (r == 0)
                            param = bstr(NULL);  // for old_syntax case
                        play_tree_set_param(pt, opt, param);
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
            play_tree_t *entry = play_tree_new();
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
                            if (j != start_title)
                                entry = play_tree_new();
                            char entbuf[15];
                            snprintf(entbuf, sizeof(entbuf),
                                    is_dvdnav ? "dvdnav://%d" : "dvd://%d", j);
                            play_tree_add_file(entry, entbuf);
                            add_entry(&last_parent, &last_entry, entry);
                            last_entry = entry;
                        }
                    } else
                        mp_tmsg(MSGT_CFGPARSER, MSGL_ERR,
                                "Invalid play entry %s\n", argv[i]);

                } else // dvd:// or dvd://x entry
                    play_tree_add_file(entry, argv[i]);
            } else
                play_tree_add_file(entry, argv[i]);

            // Lock stdin if it will be used as input
            if (strcasecmp(argv[i], "-") == 0)
                m_config_set_option0(config, "consolecontrols", "no", false);
            add_entry(&last_parent, &last_entry, entry);
            mode = LOCAL; // We start entry specific options
        }
    }

    if (opt_exit)
        goto err_out;
    if (last_parent != root)
        mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Missing }- ?\n");
    return root;

print_err:
    mp_tmsg(MSGT_CFGPARSER, MSGL_FATAL,
            "Error parsing option on the command line: %.*s\n",
            BSTR_P(orig_opt));
err_out:
    play_tree_free(root, 1);
    return NULL;
}

extern int mp_msg_levels[];

/* Parse some command line options early before main parsing.
 * --noconfig prevents reading configuration files (otherwise done before
 * command line parsing), and --really-quiet suppresses messages printed
 * during normal options parsing.
 */
int m_config_preparse_command_line(m_config_t *config, int argc, char **argv)
{
    int ret = 0;

    // Hack to shut up parser error messages
    int msg_lvl_backup = mp_msg_levels[MSGT_CFGPARSER];
    mp_msg_levels[MSGT_CFGPARSER] = -11;

    config->mode = M_COMMAND_LINE_PRE_PARSE;

    for (int i = 1 ; i < argc ; i++) {
        struct bstr opt = bstr(argv[i]);
        // No more options after --
        if (!bstrcmp0(opt, "--"))
            break;
        struct bstr param = bstr(i+1 < argc ? argv[i+1] : NULL);
        bool old_syntax;
        if (!split_opt(&opt, &param, &old_syntax))
            continue;   // Ignore non-option arguments
        // Ignore invalid options
        if (!m_config_get_option(config, opt))
            continue;
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
