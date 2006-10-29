// -*- c-basic-offset: 8; indent-tabs-mode: t -*-
// vim:ts=8:sw=8:noet:ai:
/*
  Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "config.h"

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#ifdef HAVE_ENCA
#include <enca.h>
#endif

#include "mp_msg.h"
#include "ass_utils.h"

int mystrtoi(char** p, int base, int* res)
{
	char* start = *p;
	*res = strtol(*p, p, base);
	if (*p != start) return 1;
	else return 0;
}

int mystrtou32(char** p, int base, uint32_t* res)
{
	char* start = *p;
	*res = strtoll(*p, p, base);
	if (*p != start) return 1;
	else return 0;
}

int mystrtod(char** p, double* res)
{
	char* start = *p;
	*res = strtod(*p, p);
	if (*p != start) return 1;
	else return 0;
}

int strtocolor(char** q, uint32_t* res)
{
	uint32_t color = 0;
	int result;
	char* p = *q;
	
	if (*p == '&') ++p; 
	else mp_msg(MSGT_GLOBAL, MSGL_DBG2, "suspicious color format: \"%s\"\n", p);
	
	if (*p == 'H' || *p == 'h') { 
		++p;
		result = mystrtou32(&p, 16, &color);
	} else {
		result = mystrtou32(&p, 0, &color);
	}
	
	{
		unsigned char* tmp = (unsigned char*)(&color);
		unsigned char b;
		b = tmp[0]; tmp[0] = tmp[3]; tmp[3] = b;
		b = tmp[1]; tmp[1] = tmp[2]; tmp[2] = b;
	}
	if (*p == '&') ++p;
	*q = p;

	*res = color;
	return result;
}

unsigned ass_utf8_get_char(char **str)
{
  uint8_t *strp = (uint8_t *)*str;
  unsigned c = *strp++;
  unsigned mask = 0x80;
  int len = -1;
  while (c & mask) {
    mask >>= 1;
    len++;
  }
  if (len <= 0 || len > 4)
    goto no_utf8;
  c &= mask - 1;
  while ((*strp & 0xc0) == 0x80) {
    if (len-- <= 0)
      goto no_utf8;
    c = (c << 6) | (*strp++ & 0x3f);
  }
  if (len)
    goto no_utf8;
  *str = (char *)strp;
  return c;

no_utf8:
  strp = (uint8_t *)*str;
  c = *strp++;
  *str = (char *)strp;
  return c;
}

#ifdef HAVE_ENCA
void* ass_guess_buffer_cp(unsigned char* buffer, int buflen, char *preferred_language, char *fallback)
{
    const char **languages;
    size_t langcnt;
    EncaAnalyser analyser;
    EncaEncoding encoding;
    char *detected_sub_cp = NULL;
    int i;

    languages = enca_get_languages(&langcnt);
    mp_msg(MSGT_SUBREADER, MSGL_V, "ENCA supported languages: ");
    for (i = 0; i < langcnt; i++) {
	mp_msg(MSGT_SUBREADER, MSGL_V, "%s ", languages[i]);
    }
    mp_msg(MSGT_SUBREADER, MSGL_V, "\n");
    
    for (i = 0; i < langcnt; i++) {
	const char *tmp;
	
	if (strcasecmp(languages[i], preferred_language) != 0) continue;
	analyser = enca_analyser_alloc(languages[i]);
	encoding = enca_analyse_const(analyser, buffer, buflen);
	tmp = enca_charset_name(encoding.charset, ENCA_NAME_STYLE_ICONV);
	if (tmp && encoding.charset != ENCA_CS_UNKNOWN) {
	    detected_sub_cp = strdup(tmp);
	    mp_msg(MSGT_SUBREADER, MSGL_INFO, "ENCA detected charset: %s\n", tmp);
	}
	enca_analyser_free(analyser);
    }
    
    free(languages);

    if (!detected_sub_cp) {
	detected_sub_cp = strdup(fallback);
	mp_msg(MSGT_SUBREADER, MSGL_INFO, "ENCA detection failed: fallback to %s\n", fallback);
    }

    return detected_sub_cp;
}
#endif
