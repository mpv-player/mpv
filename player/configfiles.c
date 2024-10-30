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

#include <errno.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef _WIN32
#include <sys/utime.h>
#else
#include <utime.h>
#endif

#include <libavutil/md5.h>

#include "mpv_talloc.h"

#include "osdep/io.h"

#include "common/encode.h"
#include "common/msg.h"
#include "misc/ctype.h"
#include "options/path.h"
#include "options/m_config.h"
#include "options/m_config_frontend.h"
#include "options/parse_configfile.h"
#include "common/playlist.h"
#include "options/options.h"
#include "options/m_property.h"
#include "input/input.h"

#include "stream/stream.h"

#include "core.h"
#include "command.h"

static void load_all_cfgfiles(struct MPContext *mpctx, char *section,
                              char *filename)
{
    char **cf = mp_find_all_config_files(NULL, mpctx->global, filename);
    for (int i = 0; cf && cf[i]; i++)
        m_config_parse_config_file(mpctx->mconfig, mpctx->global, cf[i], section, 0);
    talloc_free(cf);
}

// This name is used in builtin.conf to force encoding defaults (like ao/vo).
#define SECT_ENCODE "encoding"

void mp_parse_cfgfiles(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;

    mp_mk_user_dir(mpctx->global, "home", "");

    char *p1 = mp_get_user_path(NULL, mpctx->global, "~~home/");
    char *p2 = mp_get_user_path(NULL, mpctx->global, "~~old_home/");
    if (strcmp(p1, p2) != 0 && mp_path_exists(p2)) {
        MP_WARN(mpctx, "Warning, two config dirs found:\n   %s (main)\n"
                "   %s (bogus)\nYou should merge or delete the second one.\n",
                p1, p2);
    }
    talloc_free(p1);
    talloc_free(p2);

    char *section = NULL;
    bool encoding = opts->encode_opts->file && opts->encode_opts->file[0];
    // In encoding mode, we don't want to apply normal config options.
    // So we "divert" normal options into a separate section, and the diverted
    // section is never used - unless maybe it's explicitly referenced from an
    // encoding profile.
    if (encoding)
        section = "playback-default";

    load_all_cfgfiles(mpctx, NULL, "encoding-profiles.conf");

    load_all_cfgfiles(mpctx, section, "mpv.conf|config");

    if (encoding) {
        m_config_set_profile(mpctx->mconfig, SECT_ENCODE, 0);
        mp_input_enable_section(mpctx->input, "encode", MP_INPUT_EXCLUSIVE);
    }
}

static int try_load_config(struct MPContext *mpctx, const char *file, int flags,
                           int msgl)
{
    if (!mp_path_exists(file))
        return 0;
    MP_MSG(mpctx, msgl, "Loading config '%s'\n", file);
    m_config_parse_config_file(mpctx->mconfig, mpctx->global, file, NULL, flags);
    return 1;
}

// Set options file-local, and don't set them if the user set them via the
// command line.
#define FILE_LOCAL_FLAGS (M_SETOPT_BACKUP | M_SETOPT_PRESERVE_CMDLINE)

static void mp_load_per_file_config(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    char *confpath;
    char cfg[512];
    const char *file = mpctx->filename;

    if (opts->use_filedir_conf) {
        if (snprintf(cfg, sizeof(cfg), "%s.conf", file) >= sizeof(cfg)) {
            MP_VERBOSE(mpctx, "Filename is too long, can not load file or "
                              "directory specific config files\n");
            return;
        }

        char *name = mp_basename(cfg);

        bstr dir = mp_dirname(cfg);
        char *dircfg = mp_path_join_bstr(NULL, dir, bstr0("mpv.conf"));
        try_load_config(mpctx, dircfg, FILE_LOCAL_FLAGS, MSGL_INFO);
        talloc_free(dircfg);

        if (try_load_config(mpctx, cfg, FILE_LOCAL_FLAGS, MSGL_INFO))
            return;

        if ((confpath = mp_find_config_file(NULL, mpctx->global, name))) {
            try_load_config(mpctx, confpath, FILE_LOCAL_FLAGS, MSGL_INFO);

            talloc_free(confpath);
        }
    }
}

static void mp_auto_load_profile(struct MPContext *mpctx, char *category,
                                 bstr item)
{
    if (!item.len)
        return;

    char t[512];
    snprintf(t, sizeof(t), "%s.%.*s", category, BSTR_P(item));
    m_profile_t *p = m_config_get_profile0(mpctx->mconfig, t);
    if (p) {
        MP_INFO(mpctx, "Auto-loading profile '%s'\n", t);
        m_config_set_profile(mpctx->mconfig, t, FILE_LOCAL_FLAGS);
    }
}

void mp_load_auto_profiles(struct MPContext *mpctx)
{
    mp_auto_load_profile(mpctx, "protocol",
                         mp_split_proto(bstr0(mpctx->filename), NULL));
    mp_auto_load_profile(mpctx, "extension",
                         bstr0(mp_splitext(mpctx->filename, NULL)));

    mp_load_per_file_config(mpctx);
}

#define MP_WATCH_LATER_CONF "watch_later"

static bool check_mtime(const char *f1, const char *f2)
{
    struct stat st1, st2;
    if (stat(f1, &st1) != 0 || stat(f2, &st2) != 0)
        return false;
    return st1.st_mtime == st2.st_mtime;
}

static bool copy_mtime(const char *f1, const char *f2)
{
    struct stat st1, st2;

    if (stat(f1, &st1) != 0 || stat(f2, &st2) != 0)
        return false;

    struct utimbuf ut = {
        .actime = st2.st_atime,  // we want to pass this through intact
        .modtime = st1.st_mtime,
    };

    if (utime(f2, &ut) != 0)
        return false;

    return true;
}

static char *mp_get_playback_resume_dir(struct MPContext *mpctx)
{
    char *wl_dir = mpctx->opts->watch_later_dir;
    if (wl_dir && wl_dir[0]) {
        wl_dir = mp_get_user_path(mpctx, mpctx->global, wl_dir);
    } else {
        wl_dir = mp_find_user_file(mpctx, mpctx->global, "state", MP_WATCH_LATER_CONF);
    }
    return wl_dir;
}

static char *mp_get_playback_resume_config_filename(struct MPContext *mpctx,
                                                    const char *fname)
{
    struct MPOpts *opts = mpctx->opts;
    char *res = NULL;
    void *tmp = talloc_new(NULL);
    const char *path = NULL;
    if (opts->ignore_path_in_watch_later_config && !mp_is_url(bstr0(path))) {
        path = mp_basename(fname);
    } else {
        path = mp_normalize_path(tmp, fname);
        if (!path)
            goto exit;
    }
    uint8_t md5[16];
    av_md5_sum(md5, path, strlen(path));
    char *conf = talloc_strdup(tmp, "");
    for (int i = 0; i < 16; i++)
        conf = talloc_asprintf_append(conf, "%02X", md5[i]);

    char *wl_dir = mp_get_playback_resume_dir(mpctx);
    if (wl_dir && wl_dir[0])
        res = mp_path_join(NULL, wl_dir, conf);

exit:
    talloc_free(tmp);
    return res;
}

// Should follow what parser-cfg.c does/needs
static bool needs_config_quoting(const char *s)
{
    if (s[0] == '%')
        return true;
    for (int i = 0; s[i]; i++) {
        unsigned char c = s[i];
        if (!mp_isprint(c) || mp_isspace(c) || c == '#' || c == '\'' || c == '"')
            return true;
    }
    return false;
}

static void write_filename(struct MPContext *mpctx, FILE *file, char *filename)
{
    if (mpctx->opts->ignore_path_in_watch_later_config && !mp_is_url(bstr0(filename)))
        filename = mp_basename(filename);

    if (mpctx->opts->write_filename_in_watch_later_config) {
        char write_name[1024] = {0};
        for (int n = 0; filename[n] && n < sizeof(write_name) - 1; n++)
            write_name[n] = (unsigned char)filename[n] < 32 ? '_' : filename[n];
        fprintf(file, "# %s\n", write_name);
    }
}

static void write_redirect(struct MPContext *mpctx, char *path)
{
    char *conffile = mp_get_playback_resume_config_filename(mpctx, path);
    if (conffile) {
        FILE *file = fopen(conffile, "wb");
        if (file) {
            fprintf(file, "# redirect entry\n");
            write_filename(mpctx, file, path);
            fclose(file);
        }

        if (mpctx->opts->position_check_mtime &&
            !mp_is_url(bstr0(path)) && !copy_mtime(path, conffile))
            MP_WARN(mpctx, "Can't copy mtime from %s to %s\n", path, conffile);

        talloc_free(conffile);
    }
}

static void write_redirects_for_parent_dirs(struct MPContext *mpctx, char *path)
{
    if (mp_is_url(bstr0(path)) || mpctx->opts->ignore_path_in_watch_later_config)
        return;

    // Write redirect entries for the file's parent directories to allow
    // resuming playback when playing parent directories whose entries are
    // expanded only the first time they are "played". For example, if
    // "/a/b/c.mkv" is the current entry, also create resume files for /a/b and
    // /a, so that "mpv --directory-mode=lazy /a" resumes playback from
    // /a/b/c.mkv even when b isn't the first directory in /a.
    bstr dir = mp_dirname(path);
    // There is no need to write a redirect entry for "/".
    while (dir.len > 1 && dir.len < strlen(path)) {
        path[dir.len] = '\0';
        mp_path_strip_trailing_separator(path);
        write_redirect(mpctx, path);
        dir = mp_dirname(path);
    }
}

void mp_write_watch_later_conf(struct MPContext *mpctx)
{
    struct playlist_entry *cur = mpctx->playing;
    char *conffile = NULL;
    void *ctx = talloc_new(NULL);

    if (!cur)
        goto exit;

    char *path = mp_normalize_path(ctx, cur->filename);
    if (!path)
        goto exit;

    struct demuxer *demux = mpctx->demuxer;

    conffile = mp_get_playback_resume_config_filename(mpctx, path);
    if (!conffile)
        goto exit;

    char *wl_dir = mp_get_playback_resume_dir(mpctx);
    mp_mkdirp(wl_dir);

    MP_INFO(mpctx, "Saving state.\n");

    FILE *file = fopen(conffile, "wb");
    if (!file) {
        MP_WARN(mpctx, "Can't open %s for writing\n", conffile);
        goto exit;
    }

    write_filename(mpctx, file, path);

    bool write_start = true;
    double pos = get_playback_time(mpctx);

    if ((demux && (!demux->seekable || demux->partially_seekable)) ||
        pos == MP_NOPTS_VALUE)
    {
        write_start = false;
        MP_INFO(mpctx, "Not seekable, or time unknown - not saving position.\n");
    }
    char **watch_later_options = mpctx->opts->watch_later_options;
    for (int i = 0; watch_later_options && watch_later_options[i]; i++) {
        char *pname = watch_later_options[i];
        // Always save start if we have it in the array.
        if (write_start && strcmp(pname, "start") == 0) {
            fprintf(file, "%s=%f\n", pname, pos);
            continue;
        }
        // Only store it if it's different from the initial value.
        if (m_config_watch_later_backup_opt_changed(mpctx->mconfig, pname)) {
            char *val = NULL;
            mp_property_do(pname, M_PROPERTY_GET_STRING, &val, mpctx);
            if (needs_config_quoting(val)) {
                // e.g. '%6%STRING'
                fprintf(file, "%s=%%%d%%%s\n", pname, (int)strlen(val), val);
            } else {
                fprintf(file, "%s=%s\n", pname, val);
            }
            talloc_free(val);
        }
    }
    fclose(file);

    if (mpctx->opts->position_check_mtime && !mp_is_url(bstr0(path)) &&
        !copy_mtime(path, conffile))
    {
        MP_WARN(mpctx, "Can't copy mtime from %s to %s\n", cur->filename,
                conffile);
    }

    write_redirects_for_parent_dirs(mpctx, path);

    // Also write redirect entries for a playlist that mpv expanded if the
    // current entry is a URL, this is mostly useful for playing multiple
    // archives of images, e.g. with mpv 1.zip 2.zip and quit-watch-later
    // on 2.zip, write redirect entries for 2.zip, not just for the archive://
    // URL.
    if (cur->playlist_path && mp_is_url(bstr0(path))) {
        char *playlist_path = mp_normalize_path(ctx, cur->playlist_path);
        write_redirect(mpctx, playlist_path);
        write_redirects_for_parent_dirs(mpctx, playlist_path);
    }

exit:
    talloc_free(conffile);
    talloc_free(ctx);
}

void mp_delete_watch_later_conf(struct MPContext *mpctx, const char *file)
{
    void *ctx = talloc_new(NULL);
    char *path = mp_normalize_path(ctx, file ? file : mpctx->filename);
    if (!path)
        goto exit;

    char *fname = mp_get_playback_resume_config_filename(mpctx, path);
    if (fname) {
        unlink(fname);
        talloc_free(fname);
    }

    if (mp_is_url(bstr0(path)) || mpctx->opts->ignore_path_in_watch_later_config)
        goto exit;

    bstr dir = mp_dirname(path);
    while (dir.len > 1 && dir.len < strlen(path)) {
        path[dir.len] = '\0';
        mp_path_strip_trailing_separator(path);
        fname = mp_get_playback_resume_config_filename(mpctx, path);
        if (fname) {
            unlink(fname);
            talloc_free(fname);
        }
        dir = mp_dirname(path);
    }

exit:
    talloc_free(ctx);
}

bool mp_load_playback_resume(struct MPContext *mpctx, const char *file)
{
    bool resume = false;
    if (!mpctx->opts->position_resume)
        return resume;
    char *fname = mp_get_playback_resume_config_filename(mpctx, file);
    if (fname && mp_path_exists(fname)) {
        if (mpctx->opts->position_check_mtime &&
            !mp_is_url(bstr0(file)) && !check_mtime(file, fname))
        {
            talloc_free(fname);
            return resume;
        }

        // Never apply the saved start position to following files
        m_config_backup_opt(mpctx->mconfig, "start");
        MP_INFO(mpctx, "Resuming playback. This behavior can "
               "be disabled with --no-resume-playback.\n");
        try_load_config(mpctx, fname, M_SETOPT_PRESERVE_CMDLINE, MSGL_V);
        resume = true;
    }
    talloc_free(fname);
    return resume;
}

// Returns the first file that has a resume config.
// Compared to hashing the playlist file or contents and managing separate
// resume file for them, this is simpler, and also has the nice property
// that appending to a playlist doesn't interfere with resuming (especially
// if the playlist comes from the command line).
struct playlist_entry *mp_check_playlist_resume(struct MPContext *mpctx,
                                                struct playlist *playlist)
{
    if (!mpctx->opts->position_resume)
        return NULL;
    for (int n = 0; n < playlist->num_entries; n++) {
        struct playlist_entry *e = playlist->entries[n];
        char *conf = mp_get_playback_resume_config_filename(mpctx, e->filename);
        bool exists = conf && mp_path_exists(conf);
        talloc_free(conf);
        if (exists)
            return e;
    }
    return NULL;
}

