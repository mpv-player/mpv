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

#ifndef LIBASS_FONT_H
#define LIBASS_FONT_H

#include <stdint.h>
#include <ft2build.h>
#include FT_GLYPH_H
#include "ass.h"
#include "ass_types.h"

#define ASS_FONT_MAX_FACES 10
#define DECO_UNDERLINE 1
#define DECO_STRIKETHROUGH 2

typedef struct {
    char *family;
    unsigned bold;
    unsigned italic;
    int treat_family_as_pattern;
} ASS_FontDesc;

typedef struct {
    ASS_FontDesc desc;
    ASS_Library *library;
    FT_Library ftlibrary;
    FT_Face faces[ASS_FONT_MAX_FACES];
    int n_faces;
    double scale_x, scale_y;    // current transform
    FT_Vector v;                // current shift
    double size;
} ASS_Font;

// FIXME: passing the hashmap via a void pointer is very ugly.
ASS_Font *ass_font_new(void *font_cache, ASS_Library *library,
                       FT_Library ftlibrary, void *fc_priv,
                       ASS_FontDesc *desc);
void ass_font_set_transform(ASS_Font *font, double scale_x,
                            double scale_y, FT_Vector *v);
void ass_font_set_size(ASS_Font *font, double size);
void ass_font_get_asc_desc(ASS_Font *font, uint32_t ch, int *asc,
                           int *desc);
FT_Glyph ass_font_get_glyph(void *fontconfig_priv, ASS_Font *font,
                            uint32_t ch, ASS_Hinting hinting, int flags);
FT_Vector ass_font_get_kerning(ASS_Font *font, uint32_t c1, uint32_t c2);
void ass_font_free(ASS_Font *font);

#endif                          /* LIBASS_FONT_H */
