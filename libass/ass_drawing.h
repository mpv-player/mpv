/*
 * Copyright (C) 2009 Grigori Goronzy <greg@geekmind.org>
 *
 * This file is part of libass.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef LIBASS_DRAWING_H
#define LIBASS_DRAWING_H

#include <ft2build.h>
#include FT_GLYPH_H

#include "ass.h"

#define DRAWING_INITIAL_SIZE 256

typedef enum {
    TOKEN_MOVE,
    TOKEN_MOVE_NC,
    TOKEN_LINE,
    TOKEN_CUBIC_BEZIER,
    TOKEN_CONIC_BEZIER,
    TOKEN_B_SPLINE,
    TOKEN_EXTEND_SPLINE,
    TOKEN_CLOSE
} ASS_TokenType;

typedef struct ass_drawing_token {
    ASS_TokenType type;
    FT_Vector point;
    struct ass_drawing_token *next;
    struct ass_drawing_token *prev;
} ASS_DrawingToken;

typedef struct {
    char *text; // drawing string
    int i;      // text index
    int scale;  // scale (1-64) for subpixel accuracy
    double pbo; // drawing will be shifted in y direction by this amount
    double scale_x;     // FontScaleX
    double scale_y;     // FontScaleY
    int asc;            // ascender
    int desc;           // descender
    FT_OutlineGlyph glyph;  // the "fake" glyph created for later rendering
    int hash;           // hash value (for caching)

    // private
    FT_Library ftlibrary;   // FT library instance, needed for font ops
    ASS_Library *library;
    int size;           // current buffer size
    ASS_DrawingToken *tokens;    // tokenized drawing
    int max_points;     // current maximum size
    int max_contours;
    double point_scale_x;
    double point_scale_y;
} ASS_Drawing;

ASS_Drawing *ass_drawing_new(void *fontconfig_priv, ASS_Font *font,
                             ASS_Hinting hint, FT_Library lib);
void ass_drawing_free(ASS_Drawing* drawing);
void ass_drawing_add_char(ASS_Drawing* drawing, char symbol);
void ass_drawing_hash(ASS_Drawing* drawing);
FT_OutlineGlyph *ass_drawing_parse(ASS_Drawing *drawing, int raw_mode);

#endif /* LIBASS_DRAWING_H */
