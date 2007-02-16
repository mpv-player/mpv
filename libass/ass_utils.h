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

static inline int d6_to_int(int x) {
	return (x + 32) >> 6;
}
static inline int d16_to_int(int x) {
	return (x + 32768) >> 16;
}
static inline int int_to_d6(int x) {
	return x << 6;
}
static inline int int_to_d16(int x) {
	return x << 16;
}
static inline int d16_to_d6(int x) {
	return (x + 512) >> 10;
}
static inline int d6_to_d16(int x) {
	return x << 10;
}
static inline double d6_to_double(int x) {
	return x / 64.;
}
static inline int double_to_d6(double x) {
	return (int)(x * 64);
}

#endif

