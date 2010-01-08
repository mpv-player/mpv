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

#include <ft2build.h>
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_BBOX_H
#include <math.h>

#include "ass_utils.h"
#include "ass_font.h"
#include "ass_drawing.h"

#define CURVE_ACCURACY 64.0
#define GLYPH_INITIAL_POINTS 100
#define GLYPH_INITIAL_CONTOURS 5

/*
 * \brief Get and prepare a FreeType glyph
 */
static void drawing_make_glyph(ASS_Drawing *drawing, void *fontconfig_priv,
                               ASS_Font *font, ASS_Hinting hint)
{
    FT_OutlineGlyph glyph;

    // This is hacky...
    glyph = (FT_OutlineGlyph) ass_font_get_glyph(fontconfig_priv, font,
                                                 (uint32_t) ' ', hint, 0);
    if (glyph) {
        FT_Outline_Done(drawing->ftlibrary, &glyph->outline);
        FT_Outline_New(drawing->ftlibrary, GLYPH_INITIAL_POINTS,
                       GLYPH_INITIAL_CONTOURS, &glyph->outline);

        glyph->outline.n_contours = 0;
        glyph->outline.n_points = 0;
        glyph->root.advance.x = glyph->root.advance.y = 0;
    }
    drawing->glyph = glyph;
}

/*
 * \brief Add a single point to a contour.
 */
static inline void drawing_add_point(ASS_Drawing *drawing,
                                     FT_Vector *point)
{
    FT_Outline *ol = &drawing->glyph->outline;

    if (ol->n_points >= drawing->max_points) {
        drawing->max_points *= 2;
        ol->points = realloc(ol->points, sizeof(FT_Vector) *
                             drawing->max_points);
        ol->tags = realloc(ol->tags, drawing->max_points);
    }

    ol->points[ol->n_points].x = point->x;
    ol->points[ol->n_points].y = point->y;
    ol->tags[ol->n_points] = 1;
    ol->n_points++;
}

/*
 * \brief Close a contour and check glyph size overflow.
 */
static inline void drawing_close_shape(ASS_Drawing *drawing)
{
    FT_Outline *ol = &drawing->glyph->outline;

    if (ol->n_contours >= drawing->max_contours) {
        drawing->max_contours *= 2;
        ol->contours = realloc(ol->contours, sizeof(short) *
                               drawing->max_contours);
    }

    if (ol->n_points) {
        ol->contours[ol->n_contours] = ol->n_points - 1;
        ol->n_contours++;
    }
}

/*
 * \brief Prepare drawing for parsing.  This just sets a few parameters.
 */
static void drawing_prepare(ASS_Drawing *drawing)
{
    // Scaling parameters
    drawing->point_scale_x = drawing->scale_x *
                             64.0 / (1 << (drawing->scale - 1));
    drawing->point_scale_y = drawing->scale_y *
                             64.0 / (1 << (drawing->scale - 1));
}

/*
 * \brief Finish a drawing.  This only sets the horizontal advance according
 * to the glyph's bbox at the moment.
 */
static void drawing_finish(ASS_Drawing *drawing, int raw_mode)
{
    int i, offset;
    FT_BBox bbox;
    FT_Outline *ol = &drawing->glyph->outline;

    // Close the last contour
    drawing_close_shape(drawing);

#if 0
    // Dump points
    for (i = 0; i < ol->n_points; i++) {
        printf("point (%d, %d)\n", (int) ol->points[i].x,
               (int) ol->points[i].y);
    }

    // Dump contours
    for (i = 0; i < ol->n_contours; i++)
        printf("contour %d\n", ol->contours[i]);
#endif

    ass_msg(drawing->library, MSGL_V,
            "Parsed drawing with %d points and %d contours", ol->n_points,
            ol->n_contours);

    if (raw_mode)
        return;

    FT_Outline_Get_CBox(&drawing->glyph->outline, &bbox);
    drawing->glyph->root.advance.x = d6_to_d16(bbox.xMax - bbox.xMin);

    drawing->desc = double_to_d6(-drawing->pbo * drawing->scale_y);
    drawing->asc = bbox.yMax - bbox.yMin + drawing->desc;

    // Place it onto the baseline
    offset = (bbox.yMax - bbox.yMin) + double_to_d6(-drawing->pbo *
                                                    drawing->scale_y);
    for (i = 0; i < ol->n_points; i++)
        ol->points[i].y += offset;
}

/*
 * \brief Check whether a number of items on the list is available
 */
static int token_check_values(ASS_DrawingToken *token, int i, int type)
{
    int j;
    for (j = 0; j < i; j++) {
        if (!token || token->type != type) return 0;
        token = token->next;
    }

    return 1;
}

/*
 * \brief Tokenize a drawing string into a list of ASS_DrawingToken
 * This also expands points for closing b-splines
 */
static ASS_DrawingToken *drawing_tokenize(char *str)
{
    char *p = str;
    int i, val, type = -1, is_set = 0;
    FT_Vector point = {0, 0};

    ASS_DrawingToken *root = NULL, *tail = NULL, *spline_start = NULL;

    while (*p) {
        if (*p == 'c' && spline_start) {
            // Close b-splines: add the first three points of the b-spline
            // back to the end
            if (token_check_values(spline_start->next, 2, TOKEN_B_SPLINE)) {
                for (i = 0; i < 3; i++) {
                    tail->next = calloc(1, sizeof(ASS_DrawingToken));
                    tail->next->prev = tail;
                    tail = tail->next;
                    tail->type = TOKEN_B_SPLINE;
                    tail->point = spline_start->point;
                    spline_start = spline_start->next;
                }
                spline_start = NULL;
            }
        } else if (!is_set && mystrtoi(&p, &val)) {
            point.x = val;
            is_set = 1;
            p--;
        } else if (is_set == 1 && mystrtoi(&p, &val)) {
            point.y = val;
            is_set = 2;
            p--;
        } else if (*p == 'm')
            type = TOKEN_MOVE;
        else if (*p == 'n')
            type = TOKEN_MOVE_NC;
        else if (*p == 'l')
            type = TOKEN_LINE;
        else if (*p == 'b')
            type = TOKEN_CUBIC_BEZIER;
        else if (*p == 'q')
            type = TOKEN_CONIC_BEZIER;
        else if (*p == 's')
            type = TOKEN_B_SPLINE;
        // We're simply ignoring TOKEN_EXTEND_B_SPLINE here.
        // This is not harmful at all, since it can be ommitted with
        // similar result (the spline is extended anyway).

        if (type != -1 && is_set == 2) {
            if (root) {
                tail->next = calloc(1, sizeof(ASS_DrawingToken));
                tail->next->prev = tail;
                tail = tail->next;
            } else
                root = tail = calloc(1, sizeof(ASS_DrawingToken));
            tail->type = type;
            tail->point = point;
            is_set = 0;
            if (type == TOKEN_B_SPLINE && !spline_start)
                spline_start = tail->prev;
        }
        p++;
    }

#if 0
    // Check tokens
    ASS_DrawingToken *t = root;
    while(t) {
        printf("token %d point (%d, %d)\n", t->type, t->point.x, t->point.y);
        t = t->next;
    }
#endif

    return root;
}

/*
 * \brief Free a list of tokens
 */
static void drawing_free_tokens(ASS_DrawingToken *token)
{
    while (token) {
        ASS_DrawingToken *at = token;
        token = token->next;
        free(at);
    }
}

/*
 * \brief Translate and scale a point coordinate according to baseline
 * offset and scale.
 */
static inline void translate_point(ASS_Drawing *drawing, FT_Vector *point)
{
    point->x = drawing->point_scale_x * point->x;
    point->y = drawing->point_scale_y * -point->y;
}

/*
 * \brief Evaluate a curve into lines
 * This curve evaluator is also used in VSFilter (RTS.cpp); it's a simple
 * implementation of the De Casteljau algorithm.
 */
static void drawing_evaluate_curve(ASS_Drawing *drawing,
                                   ASS_DrawingToken *token, char spline,
                                   int started)
{
    double cx3, cx2, cx1, cx0, cy3, cy2, cy1, cy0;
    double t, h, max_accel, max_accel1, max_accel2;
    FT_Vector cur = {0, 0};

    cur = token->point;
    translate_point(drawing, &cur);
    int x0 = cur.x;
    int y0 = cur.y;
    token = token->next;
    cur = token->point;
    translate_point(drawing, &cur);
    int x1 = cur.x;
    int y1 = cur.y;
    token = token->next;
    cur = token->point;
    translate_point(drawing, &cur);
    int x2 = cur.x;
    int y2 = cur.y;
    token = token->next;
    cur = token->point;
    translate_point(drawing, &cur);
    int x3 = cur.x;
    int y3 = cur.y;

    if (spline) {
        // 1   [-1 +3 -3 +1]
        // - * [+3 -6 +3  0]
        // 6   [-3  0 +3  0]
        //	   [+1 +4 +1  0]

        double div6 = 1.0/6.0;

        cx3 = div6*(-  x0+3*x1-3*x2+x3);
        cx2 = div6*( 3*x0-6*x1+3*x2);
        cx1 = div6*(-3*x0	   +3*x2);
        cx0 = div6*(   x0+4*x1+1*x2);

        cy3 = div6*(-  y0+3*y1-3*y2+y3);
        cy2 = div6*( 3*y0-6*y1+3*y2);
        cy1 = div6*(-3*y0     +3*y2);
        cy0 = div6*(   y0+4*y1+1*y2);
    } else {
        // [-1 +3 -3 +1]
        // [+3 -6 +3  0]
        // [-3 +3  0  0]
        // [+1  0  0  0]

        cx3 = -  x0+3*x1-3*x2+x3;
        cx2 =  3*x0-6*x1+3*x2;
        cx1 = -3*x0+3*x1;
        cx0 =    x0;

        cy3 = -  y0+3*y1-3*y2+y3;
        cy2 =  3*y0-6*y1+3*y2;
        cy1 = -3*y0+3*y1;
        cy0 =    y0;
    }

    max_accel1 = fabs(2 * cy2) + fabs(6 * cy3);
    max_accel2 = fabs(2 * cx2) + fabs(6 * cx3);

    max_accel = FFMAX(max_accel1, max_accel2);
    h = 1.0;

    if (max_accel > CURVE_ACCURACY)
        h = sqrt(CURVE_ACCURACY / max_accel);

    if (!started) {
        cur.x = cx0;
        cur.y = cy0;
        drawing_add_point(drawing, &cur);
    }

    for (t = 0; t < 1.0; t += h) {
        cur.x = cx0 + t * (cx1 + t * (cx2 + t * cx3));
        cur.y = cy0 + t * (cy1 + t * (cy2 + t * cy3));
        drawing_add_point(drawing, &cur);
    }

    cur.x = cx0 + cx1 + cx2 + cx3;
    cur.y = cy0 + cy1 + cy2 + cy3;
    drawing_add_point(drawing, &cur);
}

/*
 * \brief Create and initialize a new drawing and return it
 */
ASS_Drawing *ass_drawing_new(void *fontconfig_priv, ASS_Font *font,
                             ASS_Hinting hint, FT_Library lib)
{
    ASS_Drawing *drawing;

    drawing = calloc(1, sizeof(*drawing));
    drawing->text = calloc(1, DRAWING_INITIAL_SIZE);
    drawing->size = DRAWING_INITIAL_SIZE;

    drawing->ftlibrary = lib;
    if (font) {
        drawing->library = font->library;
        drawing_make_glyph(drawing, fontconfig_priv, font, hint);
    }

    drawing->scale_x = 1.;
    drawing->scale_y = 1.;
    drawing->max_contours = GLYPH_INITIAL_CONTOURS;
    drawing->max_points = GLYPH_INITIAL_POINTS;

    return drawing;
}

/*
 * \brief Free a drawing
 */
void ass_drawing_free(ASS_Drawing* drawing)
{
    if (drawing) {
        if (drawing->glyph)
            FT_Done_Glyph((FT_Glyph) drawing->glyph);
        free(drawing->text);
    }
    free(drawing);
}

/*
 * \brief Add one ASCII character to the drawing text buffer
 */
void ass_drawing_add_char(ASS_Drawing* drawing, char symbol)
{
    drawing->text[drawing->i++] = symbol;
    drawing->text[drawing->i] = 0;

    if (drawing->i + 1 >= drawing->size) {
        drawing->size *= 2;
        drawing->text = realloc(drawing->text, drawing->size);
    }
}

/*
 * \brief Create a hashcode for the drawing
 * XXX: To avoid collisions a better hash algorithm might be useful.
 */
void ass_drawing_hash(ASS_Drawing* drawing)
{
    drawing->hash = fnv_32a_str(drawing->text, FNV1_32A_INIT);
}

/*
 * \brief Convert token list to outline.  Calls the line and curve evaluators.
 */
FT_OutlineGlyph *ass_drawing_parse(ASS_Drawing *drawing, int raw_mode)
{
    int started = 0;
    ASS_DrawingToken *token;
    FT_Vector pen = {0, 0};

    if (!drawing->glyph)
        return NULL;

    drawing->tokens = drawing_tokenize(drawing->text);
    drawing_prepare(drawing);

    token = drawing->tokens;
    while (token) {
        // Draw something according to current command
        switch (token->type) {
        case TOKEN_MOVE_NC:
            pen = token->point;
            translate_point(drawing, &pen);
            token = token->next;
            break;
        case TOKEN_MOVE:
            pen = token->point;
            translate_point(drawing, &pen);
            if (started) {
                drawing_close_shape(drawing);
                started = 0;
            }
            token = token->next;
            break;
        case TOKEN_LINE: {
            FT_Vector to;
            to = token->point;
            translate_point(drawing, &to);
            if (!started) drawing_add_point(drawing, &pen);
            drawing_add_point(drawing, &to);
            started = 1;
            token = token->next;
            break;
        }
        case TOKEN_CUBIC_BEZIER:
            if (token_check_values(token, 3, TOKEN_CUBIC_BEZIER) &&
                token->prev) {
                drawing_evaluate_curve(drawing, token->prev, 0, started);
                token = token->next;
                token = token->next;
                token = token->next;
                started = 1;
            } else
                token = token->next;
            break;
        case TOKEN_B_SPLINE:
            if (token_check_values(token, 3, TOKEN_B_SPLINE) &&
                token->prev) {
                drawing_evaluate_curve(drawing, token->prev, 1, started);
                token = token->next;
                started = 1;
            } else
                token = token->next;
            break;
        default:
            token = token->next;
            break;
        }
    }

    drawing_finish(drawing, raw_mode);
    drawing_free_tokens(drawing->tokens);
    return &drawing->glyph;
}
