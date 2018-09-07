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
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "osdep/io.h"

#include "mpv_talloc.h"
#include "common/msg.h"
#include "options/options.h"
#include "options/m_config.h"
#include "options/path.h"
#include "misc/ctype.h"

#include "stream/stream.h"
#include "demux.h"
#include "stheader.h"
#include "codec_tags.h"

#define MF_MAX_FILE_SIZE (1024 * 1024 * 256)

typedef struct mf {
    struct mp_log *log;
    struct sh_stream *sh;
    int curr_frame;
    int nr_of_files;
    char **names;
    // optional
    struct stream **streams;
} mf_t;


static void mf_add(mf_t *mf, const char *fname)
{
    char *entry = talloc_strdup(mf, fname);
    MP_TARRAY_APPEND(mf, mf->names, mf->nr_of_files, entry);
}

static mf_t *open_mf_pattern(void *talloc_ctx, struct mp_log *log, char *filename)
{
    int error_count = 0;
    int count = 0;

    mf_t *mf = talloc_zero(talloc_ctx, mf_t);
    mf->log = log;

    if (filename[0] == '@') {
        FILE *lst_f = fopen(filename + 1, "r");
        if (lst_f) {
            char *fname = talloc_size(mf, 512);
            while (fgets(fname, 512, lst_f)) {
                /* remove spaces from end of fname */
                char *t = fname + strlen(fname) - 1;
                while (t > fname && mp_isspace(*t))
                    *(t--) = 0;
                if (!mp_path_exists(fname)) {
                    mp_verbose(log, "file not found: '%s'\n", fname);
                } else {
                    mf_add(mf, fname);
                }
            }
            fclose(lst_f);

            mp_info(log, "number of files: %d\n", mf->nr_of_files);
            goto exit_mf;
        }
        mp_info(log, "%s is not indirect filelist\n", filename + 1);
    }

    if (strchr(filename, ',')) {
        mp_info(log, "filelist: %s\n", filename);
        bstr bfilename = bstr0(filename);

        while (bfilename.len) {
            bstr bfname;
            bstr_split_tok(bfilename, ",", &bfname, &bfilename);
            char *fname2 = bstrdup0(mf, bfname);

            if (!mp_path_exists(fname2))
                mp_verbose(log, "file not found: '%s'\n", fname2);
            else {
                mf_add(mf, fname2);
            }
            talloc_free(fname2);
        }
        mp_info(log, "number of files: %d\n", mf->nr_of_files);

        goto exit_mf;
    }

    char *fname = talloc_size(mf, strlen(filename) + 32);

#if HAVE_GLOB
    if (!strchr(filename, '%')) {
        strcpy(fname, filename);
        if (!strchr(filename, '*'))
            strcat(fname, "*");

        mp_info(log, "search expr: %s\n", fname);

        glob_t gg;
        if (glob(fname, 0, NULL, &gg)) {
            talloc_free(mf);
            return NULL;
        }

        for (int i = 0; i < gg.gl_pathc; i++) {
            if (mp_path_isdir(gg.gl_pathv[i]))
                continue;
            mf_add(mf, gg.gl_pathv[i]);
        }
        mp_info(log, "number of files: %d\n", mf->nr_of_files);
        globfree(&gg);
        goto exit_mf;
    }
#endif

    mp_info(log, "search expr: %s\n", filename);

    while (error_count < 5) {
        sprintf(fname, filename, count++);
        if (!mp_path_exists(fname)) {
            error_count++;
            mp_verbose(log, "file not found: '%s'\n", fname);
        } else {
            mf_add(mf, fname);
        }
    }

    mp_info(log, "number of files: %d\n", mf->nr_of_files);

exit_mf:
    return mf;
}

static mf_t *open_mf_single(void *talloc_ctx, struct mp_log *log, char *filename)
{
    mf_t *mf = talloc_zero(talloc_ctx, mf_t);
    mf->log = log;
    mf_add(mf, filename);
    return mf;
}

static void demux_seek_mf(demuxer_t *demuxer, double seek_pts, int flags)
{
    mf_t *mf = demuxer->priv;
    int newpos = seek_pts * mf->sh->codec->fps;
    if (flags & SEEK_FACTOR)
        newpos = seek_pts * (mf->nr_of_files - 1);
    if (newpos < 0)
        newpos = 0;
    if (newpos >= mf->nr_of_files)
        newpos = mf->nr_of_files;
    mf->curr_frame = newpos;
}

static bool demux_mf_read_packet(struct demuxer *demuxer,
                                 struct demux_packet **pkt)
{
    mf_t *mf = demuxer->priv;
    if (mf->curr_frame >= mf->nr_of_files)
        return false;

    struct stream *entry_stream = NULL;
    if (mf->streams)
        entry_stream = mf->streams[mf->curr_frame];
    struct stream *stream = entry_stream;
    if (!stream) {
        char *filename = mf->names[mf->curr_frame];
        if (filename)
            stream = stream_open(filename, demuxer->global);
    }

    if (stream) {
        stream_seek(stream, 0);
        bstr data = stream_read_complete(stream, NULL, MF_MAX_FILE_SIZE);
        if (data.len) {
            demux_packet_t *dp = new_demux_packet(data.len);
            if (dp) {
                memcpy(dp->buffer, data.start, data.len);
                dp->pts = mf->curr_frame / mf->sh->codec->fps;
                dp->keyframe = true;
                dp->stream = mf->sh->index;
                *pkt = dp;
            }
        }
        talloc_free(data.start);
    }

    if (stream && stream != entry_stream)
        free_stream(stream);

    mf->curr_frame++;
    return true;
}

// map file extension/type to a codec name

static const struct {
    const char *type;
    const char *codec;
} type2format[] = {
    { "bmp",            "bmp" },
    { "dpx",            "dpx" },
    { "j2c",            "jpeg2000" },
    { "j2k",            "jpeg2000" },
    { "jp2",            "jpeg2000" },
    { "jpc",            "jpeg2000" },
    { "jpeg",           "mjpeg" },
    { "jpg",            "mjpeg" },
    { "jps",            "mjpeg" },
    { "jls",            "ljpeg" },
    { "thm",            "mjpeg" },
    { "db",             "mjpeg" },
    { "pcx",            "pcx" },
    { "png",            "png" },
    { "pns",            "png" },
    { "ptx",            "ptx" },
    { "tga",            "targa" },
    { "tif",            "tiff" },
    { "tiff",           "tiff" },
    { "sgi",            "sgi" },
    { "sun",            "sunrast" },
    { "ras",            "sunrast" },
    { "rs",             "sunrast" },
    { "ra",             "sunrast" },
    { "im1",            "sunrast" },
    { "im8",            "sunrast" },
    { "im24",           "sunrast" },
    { "im32",           "sunrast" },
    { "sunras",         "sunrast" },
    { "xbm",            "xbm" },
    { "pam",            "pam" },
    { "pbm",            "pbm" },
    { "pgm",            "pgm" },
    { "pgmyuv",         "pgmyuv" },
    { "ppm",            "ppm" },
    { "pnm",            "ppm" },
    { "gif",            "gif" }, // usually handled by demux_lavf
    { "pix",            "brender_pix" },
    { "exr",            "exr" },
    { "pic",            "pictor" },
    { "xface",          "xface" },
    { "xwd",            "xwd" },
    {0}
};

static const char *probe_format(mf_t *mf, char *type, enum demux_check check)
{
    if (check > DEMUX_CHECK_REQUEST)
        return NULL;
    char *org_type = type;
    if (!type || !type[0]) {
        char *p = strrchr(mf->names[0], '.');
        if (p)
            type = p + 1;
    }
    for (int i = 0; type2format[i].type; i++) {
        if (type && strcasecmp(type, type2format[i].type) == 0)
            return type2format[i].codec;
    }
    if (check == DEMUX_CHECK_REQUEST) {
        if (!org_type) {
            MP_ERR(mf, "file type was not set! (try --mf-type=ext)\n");
        } else {
            MP_ERR(mf, "--mf-type set to an unknown codec!\n");
        }
    }
    return NULL;
}

static int demux_open_mf(demuxer_t *demuxer, enum demux_check check)
{
    mf_t *mf;

    if (strncmp(demuxer->stream->url, "mf://", 5) == 0 &&
        demuxer->stream->info && strcmp(demuxer->stream->info->name, "mf") == 0)
    {
        mf = open_mf_pattern(demuxer, demuxer->log, demuxer->stream->url + 5);
    } else {
        mf = open_mf_single(demuxer, demuxer->log, demuxer->stream->url);
        int bog = 0;
        MP_TARRAY_APPEND(mf, mf->streams, bog, demuxer->stream);
    }

    if (!mf || mf->nr_of_files < 1)
        goto error;

    double mf_fps;
    char *mf_type;
    mp_read_option_raw(demuxer->global, "mf-fps", &m_option_type_double, &mf_fps);
    mp_read_option_raw(demuxer->global, "mf-type", &m_option_type_string, &mf_type);

    const char *codec = mp_map_mimetype_to_video_codec(demuxer->stream->mime_type);
    if (!codec || (mf_type && mf_type[0]))
        codec = probe_format(mf, mf_type, check);
    talloc_free(mf_type);
    if (!codec)
        goto error;

    mf->curr_frame = 0;

    // create a new video stream header
    struct sh_stream *sh = demux_alloc_sh_stream(STREAM_VIDEO);
    struct mp_codec_params *c = sh->codec;

    c->codec = codec;
    c->disp_w = 0;
    c->disp_h = 0;
    c->fps = mf_fps;
    c->reliable_fps = true;

    demux_add_sh_stream(demuxer, sh);

    mf->sh = sh;
    demuxer->priv = (void *)mf;
    demuxer->seekable = true;
    demuxer->duration = mf->nr_of_files / mf->sh->codec->fps;

    return 0;

error:
    return -1;
}

static void demux_close_mf(demuxer_t *demuxer)
{
}

const demuxer_desc_t demuxer_desc_mf = {
    .name = "mf",
    .desc = "image files (mf)",
    .read_packet = demux_mf_read_packet,
    .open = demux_open_mf,
    .close = demux_close_mf,
    .seek = demux_seek_mf,
};
