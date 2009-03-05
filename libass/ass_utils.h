// -*- c-basic-offset: 8; indent-tabs-mode: t -*-
// vim:ts=8:sw=8:noet:ai:
/*
 * Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>
 *
 * This file is part of libass.
 *
 * libass is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libass is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libass; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef LIBASS_UTILS_H
#define LIBASS_UTILS_H

#include <stdint.h>

int mystrtoi(char** p, int* res);
int mystrtoll(char** p, long long* res);
int mystrtou32(char** p, int base, uint32_t* res);
int mystrtod(char** p, double* res);
int strtocolor(char** q, uint32_t* res);
char parse_bool(char* str);

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
static inline double d16_to_double(int x) {
	return ((double)x) / 0x10000;
}
static inline int double_to_d16(double x) {
	return (int)(x * 0x10000);
}

#endif /* LIBASS_UTILS_H */
