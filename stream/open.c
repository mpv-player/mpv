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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"
#include "talloc.h"

#ifdef __FreeBSD__
#include <sys/cdrio.h>
#endif

#include "m_option.h"
#include "options.h"
#include "stream.h"
#include "libmpdemux/demuxer.h"


/// We keep these 2 for the gui atm, but they will be removed.
int vcd_track=0;
char* cdrom_device=NULL;
char* dvd_device=NULL;
int dvd_title=0;

#ifdef CONFIG_LIBQUVI

#include <quvi/quvi.h>

static const char *resolve_quvi(const char *url, struct MPOpts *opts)
{
    char *media_title, *media_url;
    quvi_media_t m;
    QUVIcode rc;
    quvi_t q;

    rc = quvi_init(&q);
    if (rc != QUVI_OK)
        return NULL;

    // Don't try to use quvi on an URL that's not directly supported, since
    // quvi will do a network access anyway in order to check for HTTP
    // redirections etc.
    // The documentation says this will fail on "shortened" URLs.
    if (quvi_supported(q, (char *)url) != QUVI_OK) {
        quvi_close(&q);
        return NULL;
    }

    mp_msg(MSGT_OPEN, MSGL_INFO, "[quvi] Checking URL...\n");

    // Can use quvi_query_formats() to get a list of formats like this:
    // "fmt05_240p|fmt18_360p|fmt34_360p|fmt35_480p|fmt43_360p|fmt44_480p"
    // (This example is youtube specific.)
    // That call requires an extra net access. quvi_next_media_url() doesn't
    // seem to do anything useful. So we can't really do anything useful
    // except pass through the user's format setting.
    quvi_setopt(q, QUVIOPT_FORMAT, opts->quvi_format
                                   ? opts->quvi_format : "best");

    rc = quvi_parse(q, (char *)url, &m);
    if (rc != QUVI_OK) {
        mp_msg(MSGT_OPEN, MSGL_ERR, "[quvi] %s\n", quvi_strerror(q, rc));
        quvi_close(&q);
        return NULL;
    }

    quvi_getprop(m, QUVIPROP_PAGETITLE, &media_title);
    quvi_getprop(m, QUVIPROP_MEDIAURL, &media_url);

    mp_msg(MSGT_OPEN, MSGL_INFO, "[quvi] Site media title: '%s'\n",
           media_title);
    media_url = talloc_strdup(NULL, media_url);

    quvi_parse_close(&m);
    quvi_close(&q);

    return media_url;
}
#endif

// Open a new stream  (stdin/file/vcd/url)

stream_t* open_stream(const char *filename, struct MPOpts *options,
                      int *file_format)
{
  if (!file_format)
    file_format = &(int){DEMUXER_TYPE_UNKNOWN};
  // Check if playlist or unknown
  if (*file_format != DEMUXER_TYPE_PLAYLIST){
    *file_format=DEMUXER_TYPE_UNKNOWN;
  }

if(!filename) {
   mp_msg(MSGT_OPEN,MSGL_ERR,"NULL filename, report this bug\n");
   return NULL;
}

  const char *resolved = NULL;

#ifdef CONFIG_LIBQUVI
  resolved = resolve_quvi(filename, options);
#endif

  if (resolved)
      filename = resolved;

  stream_t *res = open_stream_full(filename,STREAM_READ,options,file_format);
  talloc_free((void *)resolved);
  return res;
}
