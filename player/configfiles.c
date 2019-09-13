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

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <libavutil/md5.h>

#include "config.h"
#include "mpv_talloc.h"

#include "osdep/io.h"

#include "common/global.h"
#include "common/encode.h"
#include "common/msg.h"
#include "misc/ctype.h"
#include "options/path.h"
#include "options/m_config.h"
#include "options/parse_configfile.h"
#include "common/playlist.h"
#include "options/options.h"
#include "options/m_property.h"

#include "stream/stream.h"

#include "core.h"
#include "command.h"

static void load_all_cfgfiles(struct MPContext *mpctx, char *section,
                              char *filename)
{
    char **cf = mp_find_all_config_files(NULL, mpctx->global, filename);
    for (int i = 0; cf && cf[i]; i++)
        m_config_parse_config_file(mpctx->mconfig, cf[i], section, 0);
    talloc_free(cf);
}

// This name is used in builtin.conf to force encoding defaults (like ao/vo).
#define SECT_ENCODE "encoding"

void mp_parse_cfgfiles(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;

    mp_mk_config_dir(mpctx->global, "");

    m_config_t *conf = mpctx->mconfig;
    char *section = NULL;
    bool encoding = opts->encode_opts &&
        opts->encode_opts->file && opts->encode_opts->file[0];
    // In encoding mode, we don't want to apply normal config options.
    // So we "divert" normal options into a separate section, and the diverted
    // section is never used - unless maybe it's explicitly referenced from an
    // encoding profile.
    if (encoding)
        section = "playback-default";

    load_all_cfgfiles(mpctx, NULL, "encoding-profiles.conf");

    load_all_cfgfiles(mpctx, section, "mpv.conf|config");

    if (encoding)
        m_config_set_profile(conf, SECT_ENCODE, 0);
}

static int try_load_config(struct MPContext *mpctx, const char *file, int flags,
                           int msgl)
{
    if (!mp_path_exists(file))
        return 0;
    MP_MSG(mpctx, msgl, "Loading config '%s'\n", file);
    m_config_parse_config_file(mpctx->mconfig, file, NULL, flags);
    return 1;
}

// Set options file-local, and don't set them if the user set them via the
// command line.
#define FILE_LOCAL_FLAGS \
    (M_SETOPT_BACKUP | M_SETOPT_RUNTIME | M_SETOPT_PRESERVE_CMDLINE)

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

static char *mp_get_playback_resume_config_filename(struct MPContext *mpctx,
                                                    const char *fname)
{
    struct MPOpts *opts = mpctx->opts;
    char *res = NULL;
    void *tmp = talloc_new(NULL);
    const char *realpath = fname;
    bstr bfname = bstr0(fname);
    if (!mp_is_url(bfname)) {
        if (opts->ignore_path_in_watch_later_config) {
            realpath = mp_basename(fname);
        } else {
            char *cwd = mp_getcwd(tmp);
            if (!cwd)
                goto exit;
            realpath = mp_path_join(tmp, cwd, fname);
        }
    }
    uint8_t md5[16];
    av_md5_sum(md5, realpath, strlen(realpath));
    char *conf = talloc_strdup(tmp, "");
    for (int i = 0; i < 16; i++)
        conf = talloc_asprintf_append(conf, "%02X", md5[i]);

    if (!mpctx->cached_watch_later_configdir) {
        char *wl_dir = mpctx->opts->watch_later_directory;
        if (wl_dir && wl_dir[0]) {
            mpctx->cached_watch_later_configdir =
                mp_get_user_path(mpctx, mpctx->global, wl_dir);
        }
    }

    if (!mpctx->cached_watch_later_configdir) {
        mpctx->cached_watch_later_configdir =
            mp_find_user_config_file(mpctx, mpctx->global, MP_WATCH_LATER_CONF);
    }

    if (mpctx->cached_watch_later_configdir)
        res = mp_path_join(NULL, mpctx->cached_watch_later_configdir, conf);

exit:
    talloc_free(tmp);
    return res;
}

static const char *const backup_properties[] = {
    "osd-level",
    //"loop",
    "speed",
    "options/edition",
    "pause",
    "volume",
    "mute",
    "audio-delay",
    //"balance",
    "fullscreen",
    "ontop",
    "border",
    "gamma",
    "brightness",
    "contrast",
    "saturation",
    "hue",
    "options/deinterlace",
    "vf",
    "af",
    "panscan",
    "options/aid",
    "options/vid",
    "options/sid",
    "sub-delay",
    "sub-speed",
    "sub-pos",
    "sub-visibility",
    "sub-scale",
    "sub-use-margins",
    "sub-ass-force-margins",
    "sub-ass-vsfilter-aspect-compat",
    "sub-style-override",
    "ab-loop-a",
    "ab-loop-b",
    "options/video-aspect",
    0
};

// Used to retrieve default settings, which should not be stored in the
// resume config. Uses backup_properties[] meaning/order of values.
// This explicitly includes values set by config files and command line.
void mp_get_resume_defaults(struct MPContext *mpctx)
{
    char **list =
        talloc_zero_array(mpctx, char*, MP_ARRAY_SIZE(backup_properties));
    for (int i = 0; backup_properties[i]; i++) {
        const char *pname = backup_properties[i];
        char *val = NULL;
        int r = mp_property_do(pname, M_PROPERTY_GET_STRING, &val, mpctx);
        if (r == M_PROPERTY_OK)
            list[i] = talloc_steal(list, val);
    }
    mpctx->resume_defaults = list;
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
        talloc_free(conffile);
    }
}

void mp_write_watch_later_conf(struct MPContext *mpctx)
{
    struct playlist_entry *cur = mpctx->playing;
    char *conffile = NULL;
    if (!cur)
        goto exit;

    struct demuxer *demux = mpctx->demuxer;
    if (demux && (!demux->seekable || demux->partially_seekable)) {
        MP_INFO(mpctx, "Not seekable - not saving state.\n");
        goto exit;
    }

    conffile = mp_get_playback_resume_config_filename(mpctx, cur->filename);
    if (!conffile)
        goto exit;

    mp_mk_config_dir(mpctx->global, mpctx->cached_watch_later_configdir);

    MP_INFO(mpctx, "Saving state.\n");

    FILE *file = fopen(conffile, "wb");
    if (!file)
        goto exit;

    write_filename(mpctx, file, cur->filename);

    double pos = get_current_time(mpctx);
    if (pos != MP_NOPTS_VALUE)
        fprintf(file, "start=%f\n", pos);
    for (int i = 0; backup_properties[i]; i++) {
        const char *pname = backup_properties[i];
        char *val = NULL;
        int r = mp_property_do(pname, M_PROPERTY_GET_STRING, &val, mpctx);
        if (r == M_PROPERTY_OK) {
            if (strncmp(pname, "options/", 8) == 0)
                pname += 8;
            // Only store it if it's different from the initial value.
            char *prev = mpctx->resume_defaults[i];
            if (!prev || strcmp(prev, val) != 0) {
                if (needs_config_quoting(val)) {
                    // e.g. '%6%STRING'
                    fprintf(file, "%s=%%%d%%%s\n", pname, (int)strlen(val), val);
                } else {
                    fprintf(file, "%s=%s\n", pname, val);
                }
            }
        }
        talloc_free(val);
    }
    fclose(file);

    // This allows us to recursively resume directories etc., whose entries are
    // expanded the first time it's "played". For example, if "/a/b/c.mkv" is
    // the current entry, then we want to resume this file if the user does
    // "mpv /a". This would expand to the directory entries in "/a", and if
    // "/a/a.mkv" is not the first entry, this would be played.
    // Here, we write resume entries for "/a" and "/a/b".
    // (Unfortunately, this will leave stray resume files on resume, because
    // obviously it resumes only from one of those paths.)
    for (int n = 0; n < cur->num_redirects; n++)
        write_redirect(mpctx, cur->redirects[n]);
    // And at last, for local directories, we write an entry for each path
    // prefix, so the user can resume from an arbitrary directory. This starts
    // with the first redirect (all other redirects are further prefixes).
    if (cur->num_redirects) {
        char *path = cur->redirects[0];
        char tmp[4096];
        if (!mp_is_url(bstr0(path)) && strlen(path) < sizeof(tmp)) {
            snprintf(tmp, sizeof(tmp), "%s", path);
            for (;;) {
                bstr dir = mp_dirname(tmp);
                if (dir.len == strlen(tmp) || !dir.len || bstr_equals0(dir, "."))
                    break;

                tmp[dir.len] = '\0';
                if (strlen(tmp) >= 2) // keep "/"
                    mp_path_strip_trailing_separator(tmp);
                write_redirect(mpctx, tmp);
            }
        }
    }

exit:
    talloc_free(conffile);
}

void mp_load_playback_resume(struct MPContext *mpctx, const char *file)
{
    if (!mpctx->opts->position_resume)
        return;
    char *fname = mp_get_playback_resume_config_filename(mpctx, file);
    if (fname && mp_path_exists(fname)) {
        // Never apply the saved start position to following files
        m_config_backup_opt(mpctx->mconfig, "start");
        MP_INFO(mpctx, "Resuming playback. This behavior can "
               "be disabled with --no-resume-playback.\n");
        try_load_config(mpctx, fname, M_SETOPT_PRESERVE_CMDLINE, MSGL_V);
        unlink(fname);
    }
    talloc_free(fname);
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
    for (struct playlist_entry *e = playlist->first; e; e = e->next) {
        char *conf = mp_get_playback_resume_config_filename(mpctx, e->filename);
        bool exists = conf && mp_path_exists(conf);
        talloc_free(conf);
        if (exists)
            return e;
    }
    return NULL;
}

