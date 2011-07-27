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

static int mode = 0;

#define GLOBAL 0
#define LOCAL 1
#define DROP_LOCAL 2

#define dvd_range(a)  (a > 0 && a < 256)


static int is_entry_option(struct m_config *mconfig, char *opt, char *param,
                           play_tree_t **ret)
{
    *ret = NULL;

    if (strcasecmp(opt, "playlist") == 0) { // We handle playlist here
        if (!param)
            return M_OPT_MISSING_PARAM;

        *ret = parse_playlist_file(mconfig, param);
        if (!*ret)
            return -1;
        else
            return 1;
    }
    return 0;
}

static inline void add_entry(play_tree_t **last_parentp,
                             play_tree_t **last_entryp, play_tree_t *entry)
{
    if (*last_entryp == NULL)
        play_tree_set_child(*last_parentp, entry);
    else
        play_tree_append_entry(*last_entryp, entry);
    *last_entryp = entry;
}

// Parse command line to set up config and playtree
play_tree_t *m_config_parse_mp_command_line(m_config_t *config, int argc,
                                            char **argv)
{
    int i, j, start_title = -1, end_title = -1;
    char *opt, *splitpos = NULL;
    char entbuf[15];
    int no_more_opts = 0;
    int opt_exit = 0;   // whether mplayer should exit without playing anything
    play_tree_t *last_parent, *last_entry = NULL, *root;

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

    for (i = 1; i < argc; i++) {
        //next:
        opt = argv[i];
        /* check for -- (no more options id.) except --help! */
        if (!strcmp(opt, "--")) {
            no_more_opts = 1;
            if (i + 1 >= argc) {
                mp_tmsg(MSGT_CFGPARSER, MSGL_ERR,
                        "'--' indicates no more options, "
                        "but no filename was given on the command line.\n");
                goto err_out;
            }
            continue;
        }
        if ((opt[0] == '{') && (opt[1] == '\0')) {
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

        if ((opt[0] == '}') && (opt[1] == '\0')) {
            if (!last_parent || !last_parent->parent) {
                mp_msg(MSGT_CFGPARSER, MSGL_ERR, "too much }-\n");
                goto err_out;
            }
            last_entry = last_parent;
            last_parent = last_entry->parent;
            continue;
        }

        if ((no_more_opts == 0) && (*opt == '-') && (*(opt + 1) != 0)) {
            int tmp = 0;
            /* remove trailing '-' */
            opt++;

            mp_msg(MSGT_CFGPARSER, MSGL_DBG3, "this_opt = option: %s\n", opt);
            // We handle here some specific option
            // Loop option when it apply to a group
            if (strcasecmp(opt, "loop") == 0 &&
                (!last_entry || last_entry->child)) {
                int l;
                char *end = NULL;
                l = (i + 1 < argc) ? strtol(argv[i + 1], &end, 0) : 0;
                if (!end || *end != '\0') {
                    mp_tmsg(MSGT_CFGPARSER, MSGL_ERR,
                            "The loop option must be an integer: %s\n",
                            argv[i + 1]);
                    tmp = ERR_OUT_OF_RANGE;
                } else {
                    play_tree_t *pt = last_entry ? last_entry : last_parent;
                    l = l <= 0 ? -1 : l;
                    pt->loop = l;
                    tmp = 1;
                }
            } else if (strcasecmp(opt, "shuffle") == 0) {
                if (last_entry && last_entry->child)
                    last_entry->flags |= PLAY_TREE_RND;
                else
                    last_parent->flags |= PLAY_TREE_RND;
            } else if (strcasecmp(opt, "noshuffle") == 0) {
                if (last_entry && last_entry->child)
                    last_entry->flags &= ~PLAY_TREE_RND;
                else
                    last_parent->flags &= ~PLAY_TREE_RND;
            } else {
                const m_option_t *mp_opt = NULL;
                play_tree_t *entry = NULL;

                tmp = is_entry_option(config, opt,
                        (i + 1 < argc) ? argv[i + 1] : NULL, &entry);
                if (tmp > 0) { // It's an entry
                    if (entry) {
                        add_entry(&last_parent, &last_entry, entry);
                        if ((last_parent->flags & PLAY_TREE_RND)
                                && entry->child)
                            entry->flags |= PLAY_TREE_RND;
                        mode = LOCAL;
                    } else if (mode == LOCAL) // Drop params for empty entry
                        mode = DROP_LOCAL;
                } else if (tmp == 0) { // 'normal' options
                    mp_opt = m_config_get_option(config, opt);
                    if (mp_opt != NULL) { // Option exist
                        if (mode == GLOBAL || (mp_opt->flags & M_OPT_GLOBAL))
                            tmp = (i + 1 < argc)
                                ? m_config_set_option(config, opt, argv[i + 1],
                                                      true)
                                : m_config_set_option(config, opt, NULL, false);
                        else {
                            tmp = m_config_check_option(config, opt,
                                    (i + 1 < argc) ? argv[i + 1] : NULL, true);
                            if (tmp >= 0 && mode != DROP_LOCAL) {
                                play_tree_t *pt =
                                        last_entry ? last_entry : last_parent;
                                play_tree_set_param(pt, opt, argv[i + 1]);
                            }
                        }
                    } else {
                        tmp = M_OPT_UNKNOWN;
                        mp_tmsg(MSGT_CFGPARSER, MSGL_ERR,
                                "Unknown option on the command line: -%s\n",
                                opt);
                    }
                }
            }

            if (tmp <= M_OPT_EXIT) {
                opt_exit = 1;
                tmp = M_OPT_EXIT - tmp;
            } else if (tmp < 0) {
                mp_tmsg(MSGT_CFGPARSER, MSGL_FATAL,
                        "Error parsing option on the command line: -%s\n",
                        opt);
                goto err_out;
            }
            i += tmp;
        } else { /* filename */
            int is_dvdnav = strstr(argv[i], "dvdnav://") != NULL;
            play_tree_t *entry = play_tree_new();
            mp_msg(MSGT_CFGPARSER, MSGL_DBG2, "Adding file %s\n", argv[i]);
            // expand DVD filename entries like dvd://1-3 into component titles
            if (strstr(argv[i], "dvd://") != NULL || is_dvdnav) {
                int offset = is_dvdnav ? 9 : 6;
                splitpos = strstr(argv[i] + offset, "-");
                if (splitpos != NULL) {
                    start_title = strtol(argv[i] + offset, NULL, 10);
                    //entries like dvd://-2 imply start at title 1
                    if (start_title < 0) {
                        end_title = abs(start_title);
                        start_title = 1;
                    } else
                        end_title = strtol(splitpos + 1, NULL, 10);

                    if (dvd_range(start_title) && dvd_range(end_title)
                            && (start_title < end_title)) {
                        for (j = start_title; j <= end_title; j++) {
                            if (j != start_title)
                                entry = play_tree_new();
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
                m_config_set_option(config, "consolecontrols", "no", false);
            add_entry(&last_parent, &last_entry, entry);
            mode = LOCAL; // We start entry specific options
        }
    }

    if (opt_exit)
        goto err_out;
    if (last_parent != root)
        mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Missing }- ?\n");
    return root;

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
        const struct m_option *opt;
        char *arg = argv[i];
        // Ignore non option
        if (arg[0] != '-' || arg[1] == 0) continue;
        // No more options after --
        if (!strcmp(arg, "--"))
            break;
        arg++;

        opt = m_config_get_option(config, arg);
        // Ignore invalid option
        if (!opt)
            continue;
        // Set, non-pre-parse options will be ignored
        int r = m_config_set_option(config, arg, i+1 < argc ? argv[i+1] : NULL,
                                    true);
        if (r < 0)
            ret = r;
        else
            i += r;
    }

    mp_msg_levels[MSGT_CFGPARSER] = msg_lvl_backup;

    return ret;
}
