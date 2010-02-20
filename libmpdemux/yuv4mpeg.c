/*
 *  yuv4mpeg.c:  Functions for reading and writing "new" YUV4MPEG streams
 *
 *  Copyright (C) 2001 Matthew J. Marjanovic <maddog@mir.com>
 *
 *  This file is part of the MJPEG Tools package (mjpeg.sourceforge.net).
 *  Ported to mplayer by Rik Snel <rsnel@cube.dyndns.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "config.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "yuv4mpeg.h"
#include "yuv4mpeg_intern.h"
#include "mp_msg.h"

static int _y4mparam_allow_unknown_tags = 1;  /* default is forgiveness */

static void *(*_y4m_alloc)(size_t bytes) = malloc;
static void (*_y4m_free)(void *ptr) = free;

int y4m_allow_unknown_tags(int yn) {
	int old = _y4mparam_allow_unknown_tags;
  	if (yn >= 0) _y4mparam_allow_unknown_tags = (yn) ? 1 : 0;
  	return old;
}



/*************************************************************************
 *
 * Convenience functions for fd read/write
 *
 *   - guaranteed to transfer entire payload (or fail)
 *   - returns:
 *               0 on complete success
 *               +(# of remaining bytes) on eof (for y4m_read)
 *               -(# of rem. bytes) on error (and ERRNO should be set)
 *
 *************************************************************************/


ssize_t y4m_read(stream_t *s, char *buf, size_t len)
{
   ssize_t n;

   while (len > 0) {
     n = stream_read(s, buf, len);
     if (n <= 0) {
       /* return amount left to read */
       if (n == 0)
	 return len;  /* n == 0 --> eof */
       else
	 return -len; /* n < 0 --> error */
     }
     buf += n;
     len -= n;
   }
   return 0;
}


#if 0 /* not needed */
ssize_t y4m_write(int fd, char *buf, size_t len)
{
   ssize_t n;

   while (len > 0) {
     n = write(fd, buf, len);
     if (n < 0) return -len;  /* return amount left to write */
     buf += n;
     len -= n;
   }
   return 0;
}
#endif


/*************************************************************************
 *
 * "Extra tags" handling
 *
 *************************************************************************/


static char *y4m_new_xtag(void)
{
  return _y4m_alloc(Y4M_MAX_XTAG_SIZE);
}


void y4m_init_xtag_list(y4m_xtag_list_t *xtags)
{
  int i;
  xtags->count = 0;
  for (i = 0; i < Y4M_MAX_XTAGS; i++) {
    xtags->tags[i] = NULL;
  }
}


void y4m_fini_xtag_list(y4m_xtag_list_t *xtags)
{
  int i;
  for (i = 0; i < Y4M_MAX_XTAGS; i++) {
    if (xtags->tags[i] != NULL) {
      _y4m_free(xtags->tags[i]);
      xtags->tags[i] = NULL;
    }
  }
  xtags->count = 0;
}


void y4m_copy_xtag_list(y4m_xtag_list_t *dest, const y4m_xtag_list_t *src)
{
  int i;
  for (i = 0; i < src->count; i++) {
    if (dest->tags[i] == NULL)
      dest->tags[i] = y4m_new_xtag();
    strncpy(dest->tags[i], src->tags[i], Y4M_MAX_XTAG_SIZE);
  }
  dest->count = src->count;
}



#if 0
static int y4m_snprint_xtags(char *s, int maxn, y4m_xtag_list_t *xtags)
{
  int i, room;

  for (i = 0, room = maxn - 1; i < xtags->count; i++) {
    int n = snprintf(s, room + 1, " %s", xtags->tags[i]);
    if ((n < 0) || (n > room)) return Y4M_ERR_HEADER;
    s += n;
    room -= n;
  }
  s[0] = '\n';  /* finish off header with newline */
  s[1] = '\0';  /* ...and end-of-string           */
  return Y4M_OK;
}
#endif


int y4m_xtag_count(const y4m_xtag_list_t *xtags)
{
  return xtags->count;
}


const char *y4m_xtag_get(const y4m_xtag_list_t *xtags, int n)
{
  if (n >= xtags->count)
    return NULL;
  else
    return xtags->tags[n];
}


int y4m_xtag_add(y4m_xtag_list_t *xtags, const char *tag)
{
  if (xtags->count >= Y4M_MAX_XTAGS) return Y4M_ERR_XXTAGS;
  if (xtags->tags[xtags->count] == NULL) {
    xtags->tags[xtags->count] = y4m_new_xtag();
  }
  strncpy(xtags->tags[xtags->count], tag, Y4M_MAX_XTAG_SIZE);
  (xtags->count)++;
  return Y4M_OK;
}


int y4m_xtag_remove(y4m_xtag_list_t *xtags, int n)
{
  int i;
  char *q;

  if ((n < 0) || (n >= xtags->count)) return Y4M_ERR_RANGE;
  q = xtags->tags[n];
  for (i = n; i < (xtags->count - 1); i++)
    xtags->tags[i] = xtags->tags[i+1];
  xtags->tags[i] = q;
  (xtags->count)--;
  return Y4M_OK;
}


int y4m_xtag_clearlist(y4m_xtag_list_t *xtags)
{
  xtags->count = 0;
  return Y4M_OK;
}


int y4m_xtag_addlist(y4m_xtag_list_t *dest, const y4m_xtag_list_t *src)
{
  int i, j;

  if ((dest->count + src->count) > Y4M_MAX_XTAGS) return Y4M_ERR_XXTAGS;
  for (i = dest->count, j = 0;
       j < src->count;
       i++, j++) {
    if (dest->tags[i] == NULL)
      dest->tags[i] = y4m_new_xtag();
    strncpy(dest->tags[i], src->tags[i], Y4M_MAX_XTAG_SIZE);
  }
  dest->count += src->count;
  return Y4M_OK;
}


/*************************************************************************
 *
 * Creators/destructors for y4m_*_info_t structures
 *
 *************************************************************************/


void y4m_init_stream_info(y4m_stream_info_t *info)
{
  if (info == NULL) return;
  /* initialize info */
  info->width = Y4M_UNKNOWN;
  info->height = Y4M_UNKNOWN;
  info->interlace = Y4M_UNKNOWN;
  info->framerate = y4m_fps_UNKNOWN;
  info->sampleaspect = y4m_sar_UNKNOWN;
  y4m_init_xtag_list(&(info->x_tags));
}


void y4m_copy_stream_info(y4m_stream_info_t *dest, y4m_stream_info_t *src)
{
  if ((dest == NULL) || (src == NULL)) return;
  /* copy info */
  dest->width = src->width;
  dest->height = src->height;
  dest->interlace = src->interlace;
  dest->framerate = src->framerate;
  dest->sampleaspect = src->sampleaspect;
  y4m_copy_xtag_list(&(dest->x_tags), &(src->x_tags));
}


void y4m_fini_stream_info(y4m_stream_info_t *info)
{
  if (info == NULL) return;
  y4m_fini_xtag_list(&(info->x_tags));
}


void y4m_si_set_width(y4m_stream_info_t *si, int width)
{
  si->width = width;
  si->framelength = (si->height * si->width) * 3 / 2;
}

int y4m_si_get_width(y4m_stream_info_t *si)
{ return si->width; }

void y4m_si_set_height(y4m_stream_info_t *si, int height)
{
  si->height = height;
  si->framelength = (si->height * si->width) * 3 / 2;
}

int y4m_si_get_height(y4m_stream_info_t *si)
{ return si->height; }

void y4m_si_set_interlace(y4m_stream_info_t *si, int interlace)
{ si->interlace = interlace; }

int y4m_si_get_interlace(y4m_stream_info_t *si)
{ return si->interlace; }

void y4m_si_set_framerate(y4m_stream_info_t *si, y4m_ratio_t framerate)
{ si->framerate = framerate; }

y4m_ratio_t y4m_si_get_framerate(y4m_stream_info_t *si)
{ return si->framerate; }

void y4m_si_set_sampleaspect(y4m_stream_info_t *si, y4m_ratio_t sar)
{ si->sampleaspect = sar; }

y4m_ratio_t y4m_si_get_sampleaspect(y4m_stream_info_t *si)
{ return si->sampleaspect; }

int y4m_si_get_framelength(y4m_stream_info_t *si)
{ return si->framelength; }

y4m_xtag_list_t *y4m_si_xtags(y4m_stream_info_t *si)
{ return &(si->x_tags); }



void y4m_init_frame_info(y4m_frame_info_t *info)
{
  if (info == NULL) return;
  /* initialize info */
  y4m_init_xtag_list(&(info->x_tags));
}


void y4m_copy_frame_info(y4m_frame_info_t *dest, y4m_frame_info_t *src)
{
  if ((dest == NULL) || (src == NULL)) return;
  /* copy info */
  y4m_copy_xtag_list(&(dest->x_tags), &(src->x_tags));
}


void y4m_fini_frame_info(y4m_frame_info_t *info)
{
  if (info == NULL) return;
  y4m_fini_xtag_list(&(info->x_tags));
}



/*************************************************************************
 *
 * Tag parsing
 *
 *************************************************************************/

int y4m_parse_stream_tags(char *s, y4m_stream_info_t *i)
{
  char *token, *value;
  char tag;
  int err;

  /* parse fields */
  for (token = strtok(s, Y4M_DELIM);
       token != NULL;
       token = strtok(NULL, Y4M_DELIM)) {
    if (token[0] == '\0') continue;   /* skip empty strings */
    tag = token[0];
    value = token + 1;
    switch (tag) {
    case 'W':  /* width */
      i->width = atoi(value);
      if (i->width <= 0) return Y4M_ERR_RANGE;
      break;
    case 'H':  /* height */
      i->height = atoi(value);
      if (i->height <= 0) return Y4M_ERR_RANGE;
      break;
    case 'F':  /* frame rate (fps) */
      if ((err = y4m_parse_ratio(&(i->framerate), value)) != Y4M_OK)
	return err;
      if (i->framerate.n < 0) return Y4M_ERR_RANGE;
      break;
    case 'I':  /* interlacing */
      switch (value[0]) {
      case 'p':  i->interlace = Y4M_ILACE_NONE; break;
      case 't':  i->interlace = Y4M_ILACE_TOP_FIRST; break;
      case 'b':  i->interlace = Y4M_ILACE_BOTTOM_FIRST; break;
      case '?':
      default:
	i->interlace = Y4M_UNKNOWN; break;
      }
      break;
    case 'A':  /* sample (pixel) aspect ratio */
      if ((err = y4m_parse_ratio(&(i->sampleaspect), value)) != Y4M_OK)
	return err;
      if (i->sampleaspect.n < 0) return Y4M_ERR_RANGE;
      break;
    case 'X':  /* 'X' meta-tag */
      if ((err = y4m_xtag_add(&(i->x_tags), token)) != Y4M_OK) return err;
      break;
    default:
      /* possible error on unknown options */
      if (_y4mparam_allow_unknown_tags) {
	/* unknown tags ok:  store in xtag list and warn... */
	if ((err = y4m_xtag_add(&(i->x_tags), token)) != Y4M_OK) return err;
	mp_msg(MSGT_DEMUX, MSGL_WARN, "Unknown stream tag encountered:  '%s'\n", token);
      } else {
	/* unknown tags are *not* ok */
	return Y4M_ERR_BADTAG;
      }
      break;
    }
  }
  /* Error checking... width and height must be known since we can't
   * parse without them
   */
  if( i->width == Y4M_UNKNOWN || i->height == Y4M_UNKNOWN )
	  return Y4M_ERR_HEADER;
  /* ta da!  done. */
  return Y4M_OK;
}



static int y4m_parse_frame_tags(char *s, y4m_frame_info_t *i)
{
  char *token, *value;
  char tag;
  int err;

  /* parse fields */
  for (token = strtok(s, Y4M_DELIM);
       token != NULL;
       token = strtok(NULL, Y4M_DELIM)) {
    if (token[0] == '\0') continue;   /* skip empty strings */
    tag = token[0];
    value = token + 1;
    switch (tag) {
    case 'X':  /* 'X' meta-tag */
      if ((err = y4m_xtag_add(&(i->x_tags), token)) != Y4M_OK) return err;
      break;
    default:
      /* possible error on unknown options */
      if (_y4mparam_allow_unknown_tags) {
	/* unknown tags ok:  store in xtag list and warn... */
	if ((err = y4m_xtag_add(&(i->x_tags), token)) != Y4M_OK) return err;
	mp_msg(MSGT_DEMUX, MSGL_WARN, "Unknown frame tag encountered:  '%s'\n", token);
      } else {
	/* unknown tags are *not* ok */
	return Y4M_ERR_BADTAG;
      }
      break;
    }
  }
  /* ta da!  done. */
  return Y4M_OK;
}





/*************************************************************************
 *
 * Read/Write stream header
 *
 *************************************************************************/


int y4m_read_stream_header(stream_t *s, y4m_stream_info_t *i)
{
   char line[Y4M_LINE_MAX];
   char *p;
   int n;
   int err;

   /* read the header line */
   for (n = 0, p = line; n < Y4M_LINE_MAX; n++, p++) {
     if (y4m_read(s, p, 1))
       return Y4M_ERR_SYSTEM;
     if (*p == '\n') {
       *p = '\0';           /* Replace linefeed by end of string */
       break;
     }
   }
   if (n >= Y4M_LINE_MAX)
      return Y4M_ERR_HEADER;
   /* look for keyword in header */
   if (strncmp(line, Y4M_MAGIC, strlen(Y4M_MAGIC)))
    return Y4M_ERR_MAGIC;
   if ((err = y4m_parse_stream_tags(line + strlen(Y4M_MAGIC), i)) != Y4M_OK)
     return err;

   i->framelength = (i->height * i->width) * 3 / 2;
   return Y4M_OK;
}


#if 0
int y4m_write_stream_header(int fd, y4m_stream_info_t *i)
{
  char s[Y4M_LINE_MAX+1];
  int n;
  int err;

  y4m_ratio_reduce(&(i->framerate));
  y4m_ratio_reduce(&(i->sampleaspect));
  n = snprintf(s, sizeof(s), "%s W%d H%d F%d:%d I%s A%d:%d",
	       Y4M_MAGIC,
	       i->width,
	       i->height,
	       i->framerate.n, i->framerate.d,
	       (i->interlace == Y4M_ILACE_NONE) ? "p" :
	       (i->interlace == Y4M_ILACE_TOP_FIRST) ? "t" :
	       (i->interlace == Y4M_ILACE_BOTTOM_FIRST) ? "b" : "?",
	       i->sampleaspect.n, i->sampleaspect.d);
  if ((n < 0) || (n > Y4M_LINE_MAX)) return Y4M_ERR_HEADER;
  if ((err = y4m_snprint_xtags(s + n, sizeof(s) - n - 1, &(i->x_tags)))
      != Y4M_OK)
    return err;
  /* non-zero on error */
  return (y4m_write(fd, s, strlen(s)) ? Y4M_ERR_SYSTEM : Y4M_OK);
}
#endif




/*************************************************************************
 *
 * Read/Write frame header
 *
 *************************************************************************/

int y4m_read_frame_header(stream_t *s, y4m_frame_info_t *i)
{
  char line[Y4M_LINE_MAX];
  char *p;
  int n;
  ssize_t remain;

  /* This is more clever than read_stream_header...
     Try to read "FRAME\n" all at once, and don't try to parse
     if nothing else is there...
  */
  remain = y4m_read(s, line, sizeof(Y4M_FRAME_MAGIC));
  if (remain != 0)
  {
	  /* A clean EOF should end exactly at a frame-boundary */
	  if( remain == sizeof(Y4M_FRAME_MAGIC) )
		  return Y4M_ERR_EOF;
	  else
		  return Y4M_ERR_SYSTEM;
  }
  if (strncmp(line, Y4M_FRAME_MAGIC, sizeof(Y4M_FRAME_MAGIC)-1))
    return Y4M_ERR_MAGIC;
  if (line[sizeof(Y4M_FRAME_MAGIC)-1] == '\n')
    return Y4M_OK; /* done -- no tags:  that was the end-of-line. */

  if (line[sizeof(Y4M_FRAME_MAGIC)-1] != Y4M_DELIM[0]) {
    return Y4M_ERR_MAGIC; /* wasn't a space -- what was it? */
  }

  /* proceed to get the tags... (overwrite the magic) */
  for (n = 0, p = line; n < Y4M_LINE_MAX; n++, p++) {
    if (y4m_read(s, p, 1))
      return Y4M_ERR_SYSTEM;
    if (*p == '\n') {
      *p = '\0';           /* Replace linefeed by end of string */
      break;
    }
  }
  if (n >= Y4M_LINE_MAX) return Y4M_ERR_HEADER;
  /* non-zero on error */
  return y4m_parse_frame_tags(line, i);
}


#if 0
int y4m_write_frame_header(int fd, y4m_frame_info_t *i)
{
  char s[Y4M_LINE_MAX+1];
  int n;
  int err;

  n = snprintf(s, sizeof(s), "%s", Y4M_FRAME_MAGIC);
  if ((n < 0) || (n > Y4M_LINE_MAX)) return Y4M_ERR_HEADER;
  if ((err = y4m_snprint_xtags(s + n, sizeof(s) - n - 1, &(i->x_tags)))
      != Y4M_OK)
    return err;
  /* non-zero on error */
  return (y4m_write(fd, s, strlen(s)) ? Y4M_ERR_SYSTEM : Y4M_OK);
}
#endif



/*************************************************************************
 *
 * Read/Write entire frame
 *
 *************************************************************************/

int y4m_read_frame(stream_t *s, y4m_stream_info_t *si,
		   y4m_frame_info_t *fi, unsigned char *yuv[3])
{
  int err;
  int w = si->width;
  int h = si->height;

  /* Read frame header */
  if ((err = y4m_read_frame_header(s, fi)) != Y4M_OK) return err;
  /* Read luminance scanlines */
  if (y4m_read(s, yuv[0], w*h)) return Y4M_ERR_SYSTEM;
  /* Read chrominance scanlines */
  if (y4m_read(s, yuv[1], w*h/4)) return Y4M_ERR_SYSTEM;
  if (y4m_read(s, yuv[2], w*h/4)) return Y4M_ERR_SYSTEM;

  return Y4M_OK;
}



#if 0
int y4m_write_frame(int fd, y4m_stream_info_t *si,
		    y4m_frame_info_t *fi, unsigned char *yuv[3])
{
  int err;
  int w = si->width;
  int h = si->height;

  /* Write frame header */
  if ((err = y4m_write_frame_header(fd, fi)) != Y4M_OK) return err;
  /* Write luminance,chrominance scanlines */
  if (y4m_write(fd, yuv[0], w*h) ||
      y4m_write(fd, yuv[1], w*h/4) ||
      y4m_write(fd, yuv[2], w*h/4))
    return Y4M_ERR_SYSTEM;
  return Y4M_OK;
}
#endif


/*************************************************************************
 *
 * Read/Write entire frame, (de)interleaved (to)from two separate fields
 *
 *************************************************************************/

#if 0
int y4m_read_fields(int fd, y4m_stream_info_t *si, y4m_frame_info_t *fi,
                    unsigned char *upper_field[3],
                    unsigned char *lower_field[3])
{
  int i, y, err;
  int width = si->width;
  int height = si->height;

  /* Read frame header */
  if ((err = y4m_read_frame_header(fd, fi)) != Y4M_OK) return err;
  /* Read Y', Cb, and Cr planes */
  for (i = 0; i < 3; i++) {
    unsigned char *srctop = upper_field[i];
    unsigned char *srcbot = lower_field[i];
    /* alternately write one line from each */
    for (y = 0; y < height; y += 2) {
      if (y4m_read(fd, srctop, width)) return Y4M_ERR_SYSTEM;
      srctop += width;
      if (y4m_read(fd, srcbot, width)) return Y4M_ERR_SYSTEM;
      srcbot += width;
    }
    /* for chroma, width/height are half as big */
    if (i == 0) {
      width /= 2;
      height /= 2;
    }
  }
  return Y4M_OK;
}



int y4m_write_fields(int fd, y4m_stream_info_t *si, y4m_frame_info_t *fi,
                     unsigned char *upper_field[3],
                     unsigned char *lower_field[3])
{
  int i, y, err;
  int width = si->width;
  int height = si->height;

  /* Write frame header */
  if ((err = y4m_write_frame_header(fd, fi)) != Y4M_OK) return err;
  /* Write Y', Cb, and Cr planes */
  for (i = 0; i < 3; i++) {
    unsigned char *srctop = upper_field[i];
    unsigned char *srcbot = lower_field[i];
    /* alternately write one line from each */
    for (y = 0; y < height; y += 2) {
      if (y4m_write(fd, srctop, width)) return Y4M_ERR_SYSTEM;
      srctop += width;
      if (y4m_write(fd, srcbot, width)) return Y4M_ERR_SYSTEM;
      srcbot += width;
    }
    /* for chroma, width/height are half as big */
    if (i == 0) {
      width /= 2;
      height /= 2;
    }
  }
  return Y4M_OK;
}
#endif


/*************************************************************************
 *
 * Handy logging of stream info
 *
 *************************************************************************/

void y4m_log_stream_info(const char *prefix, y4m_stream_info_t *i)
{
  char s[256];

  snprintf(s, sizeof(s), "  frame size:  ");
  if (i->width == Y4M_UNKNOWN)
    snprintf(s+strlen(s), sizeof(s)-strlen(s), "(?)x");
  else
    snprintf(s+strlen(s), sizeof(s)-strlen(s), "%dx", i->width);
  if (i->height == Y4M_UNKNOWN)
    snprintf(s+strlen(s), sizeof(s)-strlen(s), "(?) pixels ");
  else
    snprintf(s+strlen(s), sizeof(s)-strlen(s), "%d pixels ", i->height);
  if (i->framelength == Y4M_UNKNOWN)
    snprintf(s+strlen(s), sizeof(s)-strlen(s), "(? bytes)");
  else
    snprintf(s+strlen(s), sizeof(s)-strlen(s), "(%d bytes)", i->framelength);
  mp_msg(MSGT_DEMUX, MSGL_V, "%s%s\n", prefix, s);
  if ((i->framerate.n == 0) && (i->framerate.d == 0))
    mp_msg(MSGT_DEMUX, MSGL_V, "%s  frame rate:  ??? fps\n", prefix);
  else
    mp_msg(MSGT_DEMUX, MSGL_V, "%s  frame rate:  %d/%d fps (~%f)\n", prefix,
	      i->framerate.n, i->framerate.d,
	      (double) i->framerate.n / (double) i->framerate.d);
  mp_msg(MSGT_DEMUX, MSGL_V, "%s   interlace:  %s\n", prefix,
	  (i->interlace == Y4M_ILACE_NONE) ? "none/progressive" :
	  (i->interlace == Y4M_ILACE_TOP_FIRST) ? "top-field-first" :
	  (i->interlace == Y4M_ILACE_BOTTOM_FIRST) ? "bottom-field-first" :
	  "anyone's guess");
  if ((i->sampleaspect.n == 0) && (i->sampleaspect.d == 0))
    mp_msg(MSGT_DEMUX, MSGL_V, "%ssample aspect ratio:  ?:?\n", prefix);
  else
    mp_msg(MSGT_DEMUX, MSGL_V, "%ssample aspect ratio:  %d:%d\n", prefix,
	      i->sampleaspect.n, i->sampleaspect.d);
}


/*************************************************************************
 *
 * Convert error code to string
 *
 *************************************************************************/

const char *y4m_strerr(int err)
{
  switch (err) {
  case Y4M_OK:          return "no error";
  case Y4M_ERR_RANGE:   return "parameter out of range";
  case Y4M_ERR_SYSTEM:  return "stream ended unexpectedly (failed read/write)";
  case Y4M_ERR_HEADER:  return "bad stream or frame header";
  case Y4M_ERR_BADTAG:  return "unknown header tag";
  case Y4M_ERR_MAGIC:   return "bad header magic";
  case Y4M_ERR_XXTAGS:  return "too many xtags";
  case Y4M_ERR_EOF:     return "end-of-file";
  default:
    return "unknown error code";
  }
}
