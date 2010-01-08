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

#include "config.h"

#include <inttypes.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SYNTHESIS_H
#include FT_GLYPH_H
#include FT_TRUETYPE_TABLES_H
#include FT_OUTLINE_H

#include "ass.h"
#include "ass_library.h"
#include "ass_font.h"
#include "ass_bitmap.h"
#include "ass_cache.h"
#include "ass_fontconfig.h"
#include "ass_utils.h"

/**
 * Select Microfost Unicode CharMap, if the font has one.
 * Otherwise, let FreeType decide.
 */
static void charmap_magic(ASS_Library *library, FT_Face face)
{
    int i;
    for (i = 0; i < face->num_charmaps; ++i) {
        FT_CharMap cmap = face->charmaps[i];
        unsigned pid = cmap->platform_id;
        unsigned eid = cmap->encoding_id;
        if (pid == 3 /*microsoft */
            && (eid == 1 /*unicode bmp */
                || eid == 10 /*full unicode */ )) {
            FT_Set_Charmap(face, cmap);
            return;
        }
    }

    if (!face->charmap) {
        if (face->num_charmaps == 0) {
            ass_msg(library, MSGL_WARN, "Font face with no charmaps");
            return;
        }
        ass_msg(library, MSGL_WARN,
                "No charmap autodetected, trying the first one");
        FT_Set_Charmap(face, face->charmaps[0]);
        return;
    }
}

static void update_transform(ASS_Font *font)
{
    int i;
    FT_Matrix m;
    m.xx = double_to_d16(font->scale_x);
    m.yy = double_to_d16(font->scale_y);
    m.xy = m.yx = 0;
    for (i = 0; i < font->n_faces; ++i)
        FT_Set_Transform(font->faces[i], &m, &font->v);
}

/**
 * \brief find a memory font by name
 */
static int find_font(ASS_Library *library, char *name)
{
    int i;
    for (i = 0; i < library->num_fontdata; ++i)
        if (strcasecmp(name, library->fontdata[i].name) == 0)
            return i;
    return -1;
}

static void face_set_size(FT_Face face, double size);

static void buggy_font_workaround(FT_Face face)
{
    // Some fonts have zero Ascender/Descender fields in 'hhea' table.
    // In this case, get the information from 'os2' table or, as
    // a last resort, from face.bbox.
    if (face->ascender + face->descender == 0 || face->height == 0) {
        TT_OS2 *os2 = FT_Get_Sfnt_Table(face, ft_sfnt_os2);
        if (os2) {
            face->ascender = os2->sTypoAscender;
            face->descender = os2->sTypoDescender;
            face->height = face->ascender - face->descender;
        } else {
            face->ascender = face->bbox.yMax;
            face->descender = face->bbox.yMin;
            face->height = face->ascender - face->descender;
        }
    }
}

/**
 * \brief Select a face with the given charcode and add it to ASS_Font
 * \return index of the new face in font->faces, -1 if failed
 */
static int add_face(void *fc_priv, ASS_Font *font, uint32_t ch)
{
    char *path;
    int index;
    FT_Face face;
    int error;
    int mem_idx;

    if (font->n_faces == ASS_FONT_MAX_FACES)
        return -1;

    path =
        fontconfig_select(font->library, fc_priv, font->desc.family,
                          font->desc.treat_family_as_pattern,
                          font->desc.bold, font->desc.italic, &index, ch);
    if (!path)
        return -1;

    mem_idx = find_font(font->library, path);
    if (mem_idx >= 0) {
        error =
            FT_New_Memory_Face(font->ftlibrary,
                               (unsigned char *) font->library->
                               fontdata[mem_idx].data,
                               font->library->fontdata[mem_idx].size, 0,
                               &face);
        if (error) {
            ass_msg(font->library, MSGL_WARN,
                    "Error opening memory font: '%s'", path);
            free(path);
            return -1;
        }
    } else {
        error = FT_New_Face(font->ftlibrary, path, index, &face);
        if (error) {
            ass_msg(font->library, MSGL_WARN,
                    "Error opening font: '%s', %d", path, index);
            free(path);
            return -1;
        }
    }
    charmap_magic(font->library, face);
    buggy_font_workaround(face);

    font->faces[font->n_faces++] = face;
    update_transform(font);
    face_set_size(face, font->size);
    free(path);
    return font->n_faces - 1;
}

/**
 * \brief Create a new ASS_Font according to "desc" argument
 */
ASS_Font *ass_font_new(void *font_cache, ASS_Library *library,
                       FT_Library ftlibrary, void *fc_priv,
                       ASS_FontDesc *desc)
{
    int error;
    ASS_Font *fontp;
    ASS_Font font;

    fontp = ass_font_cache_find((Hashmap *) font_cache, desc);
    if (fontp)
        return fontp;

    font.library = library;
    font.ftlibrary = ftlibrary;
    font.n_faces = 0;
    font.desc.family = strdup(desc->family);
    font.desc.treat_family_as_pattern = desc->treat_family_as_pattern;
    font.desc.bold = desc->bold;
    font.desc.italic = desc->italic;

    font.scale_x = font.scale_y = 1.;
    font.v.x = font.v.y = 0;
    font.size = 0.;

    error = add_face(fc_priv, &font, 0);
    if (error == -1) {
        free(font.desc.family);
        return 0;
    } else
        return ass_font_cache_add((Hashmap *) font_cache, &font);
}

/**
 * \brief Set font transformation matrix and shift vector
 **/
void ass_font_set_transform(ASS_Font *font, double scale_x,
                            double scale_y, FT_Vector *v)
{
    font->scale_x = scale_x;
    font->scale_y = scale_y;
    if (v) {
        font->v.x = v->x;
        font->v.y = v->y;
    }
    update_transform(font);
}

static void face_set_size(FT_Face face, double size)
{
    TT_HoriHeader *hori = FT_Get_Sfnt_Table(face, ft_sfnt_hhea);
    TT_OS2 *os2 = FT_Get_Sfnt_Table(face, ft_sfnt_os2);
    double mscale = 1.;
    FT_Size_RequestRec rq;
    FT_Size_Metrics *m = &face->size->metrics;
    // VSFilter uses metrics from TrueType OS/2 table
    // The idea was borrowed from asa (http://asa.diac24.net)
    if (hori && os2) {
        int hori_height = hori->Ascender - hori->Descender;
        int os2_height = os2->usWinAscent + os2->usWinDescent;
        if (hori_height && os2_height)
            mscale = (double) hori_height / os2_height;
    }
    memset(&rq, 0, sizeof(rq));
    rq.type = FT_SIZE_REQUEST_TYPE_REAL_DIM;
    rq.width = 0;
    rq.height = double_to_d6(size * mscale);
    rq.horiResolution = rq.vertResolution = 0;
    FT_Request_Size(face, &rq);
    m->ascender /= mscale;
    m->descender /= mscale;
    m->height /= mscale;
}

/**
 * \brief Set font size
 **/
void ass_font_set_size(ASS_Font *font, double size)
{
    int i;
    if (font->size != size) {
        font->size = size;
        for (i = 0; i < font->n_faces; ++i)
            face_set_size(font->faces[i], size);
    }
}

/**
 * \brief Get maximal font ascender and descender.
 * \param ch character code
 * The values are extracted from the font face that provides glyphs for the given character
 **/
void ass_font_get_asc_desc(ASS_Font *font, uint32_t ch, int *asc,
                           int *desc)
{
    int i;
    for (i = 0; i < font->n_faces; ++i) {
        FT_Face face = font->faces[i];
        TT_OS2 *os2 = FT_Get_Sfnt_Table(face, ft_sfnt_os2);
        if (FT_Get_Char_Index(face, ch)) {
            int y_scale = face->size->metrics.y_scale;
            if (os2) {
                *asc = FT_MulFix(os2->usWinAscent, y_scale);
                *desc = FT_MulFix(os2->usWinDescent, y_scale);
            } else {
                *asc = FT_MulFix(face->ascender, y_scale);
                *desc = FT_MulFix(-face->descender, y_scale);
            }
            return;
        }
    }

    *asc = *desc = 0;
}

/*
 * Strike a glyph with a horizontal line; it's possible to underline it
 * and/or strike through it.  For the line's position and size, truetype
 * tables are consulted.  Obviously this relies on the data in the tables
 * being accurate.
 *
 */
static int ass_strike_outline_glyph(FT_Face face, ASS_Font *font,
                                    FT_Glyph glyph, int under, int through)
{
    TT_OS2 *os2 = FT_Get_Sfnt_Table(face, ft_sfnt_os2);
    TT_Postscript *ps = FT_Get_Sfnt_Table(face, ft_sfnt_post);
    FT_Outline *ol = &((FT_OutlineGlyph) glyph)->outline;
    int bear, advance, y_scale, i, dir;

    if (!under && !through)
        return 0;

    // Grow outline
    i = (under ? 4 : 0) + (through ? 4 : 0);
    ol->points = realloc(ol->points, sizeof(FT_Vector) *
                         (ol->n_points + i));
    ol->tags = realloc(ol->tags, ol->n_points + i);
    i = !!under + !!through;
    ol->contours = realloc(ol->contours, sizeof(short) *
                           (ol->n_contours + i));

    // If the bearing is negative, the glyph starts left of the current
    // pen position
    bear = FFMIN(face->glyph->metrics.horiBearingX, 0);
    // We're adding half a pixel to avoid small gaps
    advance = d16_to_d6(glyph->advance.x) + 32;
    y_scale = face->size->metrics.y_scale;

    // Reverse drawing direction for non-truetype fonts
    dir = FT_Outline_Get_Orientation(ol);

    // Add points to the outline
    if (under && ps) {
        int pos, size;
        pos = FT_MulFix(ps->underlinePosition, y_scale * font->scale_y);
        size = FT_MulFix(ps->underlineThickness,
                         y_scale * font->scale_y / 2);

        if (pos > 0 || size <= 0)
            return 1;

        FT_Vector points[4] = {
            {.x = bear,      .y = pos + size},
            {.x = advance,   .y = pos + size},
            {.x = advance,   .y = pos - size},
            {.x = bear,      .y = pos - size},
        };

        if (dir == FT_ORIENTATION_TRUETYPE) {
            for (i = 0; i < 4; i++) {
                ol->points[ol->n_points] = points[i];
                ol->tags[ol->n_points++] = 1;
            }
        } else {
            for (i = 3; i >= 0; i--) {
                ol->points[ol->n_points] = points[i];
                ol->tags[ol->n_points++] = 1;
            }
        }

        ol->contours[ol->n_contours++] = ol->n_points - 1;
    }

    if (through && os2) {
        int pos, size;
        pos = FT_MulFix(os2->yStrikeoutPosition, y_scale * font->scale_y);
        size = FT_MulFix(os2->yStrikeoutSize, y_scale * font->scale_y / 2);

        if (pos < 0 || size <= 0)
            return 1;

        FT_Vector points[4] = {
            {.x = bear,      .y = pos + size},
            {.x = advance,   .y = pos + size},
            {.x = advance,   .y = pos - size},
            {.x = bear,      .y = pos - size},
        };

        if (dir == FT_ORIENTATION_TRUETYPE) {
            for (i = 0; i < 4; i++) {
                ol->points[ol->n_points] = points[i];
                ol->tags[ol->n_points++] = 1;
            }
        } else {
            for (i = 3; i >= 0; i--) {
                ol->points[ol->n_points] = points[i];
                ol->tags[ol->n_points++] = 1;
            }
        }

        ol->contours[ol->n_contours++] = ol->n_points - 1;
    }

    return 0;
}

/**
 * Slightly embold a glyph without touching its metrics
 */
static void ass_glyph_embolden(FT_GlyphSlot slot)
{
    int str;

    if (slot->format != FT_GLYPH_FORMAT_OUTLINE)
        return;

    str = FT_MulFix(slot->face->units_per_EM,
                    slot->face->size->metrics.y_scale) / 64;

    FT_Outline_Embolden(&slot->outline, str);
}

/**
 * \brief Get a glyph
 * \param ch character code
 **/
FT_Glyph ass_font_get_glyph(void *fontconfig_priv, ASS_Font *font,
                            uint32_t ch, ASS_Hinting hinting, int deco)
{
    int error;
    int index = 0;
    int i;
    FT_Glyph glyph;
    FT_Face face = 0;
    int flags = 0;

    if (ch < 0x20)
        return 0;
    // Handle NBSP like a regular space when rendering the glyph
    if (ch == 0xa0)
        ch = ' ';
    if (font->n_faces == 0)
        return 0;

    for (i = 0; i < font->n_faces; ++i) {
        face = font->faces[i];
        index = FT_Get_Char_Index(face, ch);
        if (index)
            break;
    }

#ifdef CONFIG_FONTCONFIG
    if (index == 0) {
        int face_idx;
        ass_msg(font->library, MSGL_INFO,
                "Glyph 0x%X not found, selecting one more "
                "font for (%s, %d, %d)", ch, font->desc.family,
                font->desc.bold, font->desc.italic);
        face_idx = add_face(fontconfig_priv, font, ch);
        if (face_idx >= 0) {
            face = font->faces[face_idx];
            index = FT_Get_Char_Index(face, ch);
            if (index == 0) {
                ass_msg(font->library, MSGL_ERR,
                        "Glyph 0x%X not found in font for (%s, %d, %d)",
                        ch, font->desc.family, font->desc.bold,
                        font->desc.italic);
            }
        }
    }
#endif

    switch (hinting) {
    case ASS_HINTING_NONE:
        flags = FT_LOAD_NO_HINTING;
        break;
    case ASS_HINTING_LIGHT:
        flags = FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_LIGHT;
        break;
    case ASS_HINTING_NORMAL:
        flags = FT_LOAD_FORCE_AUTOHINT;
        break;
    case ASS_HINTING_NATIVE:
        flags = 0;
        break;
    }

    error = FT_Load_Glyph(face, index, FT_LOAD_NO_BITMAP | flags);
    if (error) {
        ass_msg(font->library, MSGL_WARN, "Error loading glyph, index %d",
                index);
        return 0;
    }
    if (!(face->style_flags & FT_STYLE_FLAG_ITALIC) &&
        (font->desc.italic > 55)) {
        FT_GlyphSlot_Oblique(face->glyph);
    }

    if (!(face->style_flags & FT_STYLE_FLAG_BOLD) &&
        (font->desc.bold > 80)) {
        ass_glyph_embolden(face->glyph);
    }
    error = FT_Get_Glyph(face->glyph, &glyph);
    if (error) {
        ass_msg(font->library, MSGL_WARN, "Error loading glyph, index %d",
                index);
        return 0;
    }

    ass_strike_outline_glyph(face, font, glyph, deco & DECO_UNDERLINE,
                             deco & DECO_STRIKETHROUGH);

    return glyph;
}

/**
 * \brief Get kerning for the pair of glyphs.
 **/
FT_Vector ass_font_get_kerning(ASS_Font *font, uint32_t c1, uint32_t c2)
{
    FT_Vector v = { 0, 0 };
    int i;

    for (i = 0; i < font->n_faces; ++i) {
        FT_Face face = font->faces[i];
        int i1 = FT_Get_Char_Index(face, c1);
        int i2 = FT_Get_Char_Index(face, c2);
        if (i1 && i2) {
            if (FT_HAS_KERNING(face))
                FT_Get_Kerning(face, i1, i2, FT_KERNING_DEFAULT, &v);
            return v;
        }
        if (i1 || i2)           // these glyphs are from different font faces, no kerning information
            return v;
    }
    return v;
}

/**
 * \brief Deallocate ASS_Font
 **/
void ass_font_free(ASS_Font *font)
{
    int i;
    for (i = 0; i < font->n_faces; ++i)
        if (font->faces[i])
            FT_Done_Face(font->faces[i]);
    if (font->desc.family)
        free(font->desc.family);
    free(font);
}
