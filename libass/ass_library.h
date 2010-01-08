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

#ifndef LIBASS_LIBRARY_H
#define LIBASS_LIBRARY_H

#include <stdarg.h>

typedef struct {
    char *name;
    char *data;
    int size;
} ASS_Fontdata;

struct ass_library {
    char *fonts_dir;
    int extract_fonts;
    char **style_overrides;

    ASS_Fontdata *fontdata;
    int num_fontdata;
    void (*msg_callback)(int, const char *, va_list, void *);
    void *msg_callback_data;
};

#endif                          /* LIBASS_LIBRARY_H */
