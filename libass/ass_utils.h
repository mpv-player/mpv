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

#ifndef __ASS_UTILS_H__
#define __ASS_UTILS_H__

int mystrtoi(char** p, int base, int* res);
int mystrtou32(char** p, int base, uint32_t* res);
int mystrtod(char** p, double* res);
int strtocolor(char** q, uint32_t* res);
unsigned ass_utf8_get_char(char **str);

#ifdef HAVE_ENCA
void* ass_guess_buffer_cp(unsigned char* buffer, int buflen, char *preferred_language, char *fallback);
#endif

#endif

