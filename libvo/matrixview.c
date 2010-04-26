/*
 * Copyright (C) 2003 Alex Zolotov <nightradio@knoppix.ru>
 * Mucked with by Tugrul Galatali <tugrul@galatali.com>
 *
 * MatrixView is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MatrixView is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MatrixView; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * Ported to an MPlayer video out plugin by Pigeon <pigeon at pigeond.net>
 * August 2006
 */

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "gl_common.h"
#include "matrixview.h"
#include "matrixview_font.h"

static float matrix_contrast   = 1.5;
static float matrix_brightness = 1.0;

// Settings for our light.  Try playing with these (or add more lights).
static float Light_Ambient[]  = { 0.1f, 0.1f, 0.1f, 1.0f };
static float Light_Diffuse[]  = { 1.2f, 1.2f, 1.2f, 1.0f };
static float Light_Position[] = { 2.0f, 2.0f, 0.0f, 1.0f };

static const uint8_t flare[4][4] = {
    {  0,   0,   0,   0},
    {  0, 180,   0,   0},
    {  0,   0,   0,   0},
    {  0,   0,   0,   0}
};

#define MAX_TEXT_X 0x4000
#define MAX_TEXT_Y 0x4000
static int text_x = 0;
static int text_y = 0;
#define _text_x text_x/2
#define _text_y text_y/2

// Scene position
#define Z_Off -128.0f
#define Z_Depth 8

static uint8_t *speed;
static uint8_t *text;
static uint8_t *text_light;
static float *text_depth;

static float *bump_pic;

static void draw_char(int num, float light, float x, float y, float z)
{
    float tx, ty;
    int num2, num3;

    num &= 63;
    //light = light / 255;        //light=7-light;num+=(light*60);
    light = light / 255 * matrix_brightness;
    num2 = num / 10;
    num3 = num - (num2 * 10);
    ty = (float)num2 / 7;
    tx = (float)num3 / 10;
    mpglNormal3f(0.0f, 0.0f, 1.0f);        // Needed for lighting
    mpglColor4f(0.0, 1.0, 0.0, light);        // Basic polygon color

    mpglTexCoord2f(tx, ty);
    mpglVertex3f(x, y, z);
    mpglTexCoord2f(tx + 0.1, ty);
    mpglVertex3f(x + 1, y, z);
    mpglTexCoord2f(tx + 0.1, ty + 0.166);
    mpglVertex3f(x + 1, y - 1, z);
    mpglTexCoord2f(tx, ty + 0.166);
    mpglVertex3f(x, y - 1, z);
}

static void draw_illuminatedchar(int num, float x, float y, float z)
{
    float tx, ty;
    int num2, num3;

    num2 = num / 10;
    num3 = num - (num2 * 10);
    ty = (float)num2 / 7;
    tx = (float)num3 / 10;
    mpglNormal3f(0.0f, 0.0f, 1.0f);        // Needed for lighting
    mpglColor4f(1.0, 1.0, 1.0, .5);        // Basic polygon color

    mpglTexCoord2f(tx, ty);
    mpglVertex3f(x, y, z);
    mpglTexCoord2f(tx + 0.1, ty);
    mpglVertex3f(x + 1, y, z);
    mpglTexCoord2f(tx + 0.1, ty + 0.166);
    mpglVertex3f(x + 1, y - 1, z);
    mpglTexCoord2f(tx, ty + 0.166);
    mpglVertex3f(x, y - 1, z);
}

static void draw_flare(float x, float y, float z)        //flare
{
    mpglNormal3f(0.0f, 0.0f, 1.0f);        // Needed for lighting
    mpglColor4f(1.0, 1.0, 1.0, .8);        // Basic polygon color

    mpglTexCoord2f(0, 0);
    mpglVertex3f(x - 1, y + 1, z);
    mpglTexCoord2f(0.75, 0);
    mpglVertex3f(x + 2, y + 1, z);
    mpglTexCoord2f(0.75, 0.75);
    mpglVertex3f(x + 2, y - 2, z);
    mpglTexCoord2f(0, 0.75);
    mpglVertex3f(x - 1, y - 2, z);
}

static void draw_text(uint8_t *pic)
{
    int x, y;
    int p = 0;
    int c, c_pic;
    int pic_fade = 255;

    for (y = _text_y; y > -_text_y; y--) {
        for (x = -_text_x; x < _text_x; x++) {
            c  = text_light[p] - (text[p] >> 1);
            c += pic_fade;
            if (c > 255)
                c = 255;

            if (pic) {
                // Original code
                //c_pic = pic[p] * matrix_contrast - (255 - pic_fade);

                c_pic = (255 - pic[p]) * matrix_contrast - (255 - pic_fade);

                if (c_pic < 0)
                    c_pic = 0;

                c -= c_pic;

                if (c < 0)
                    c = 0;

                bump_pic[p] = (255.0f - c_pic) / (256 / Z_Depth);
            } else {
                bump_pic[p] = Z_Depth;
            }

            if (text[p] && c > 10)
                draw_char(text[p] + 1, c, x, y, text_depth[p] + bump_pic[p]);

            if (text_depth[p] < 0.1)
                text_depth[p] = 0;
            else
                text_depth[p] /= 1.1;

            if (text_light[p] > 128 && text_light[p + text_x] < 10)
                draw_illuminatedchar(text[p] + 1, x, y,
                                     text_depth[p] + bump_pic[p]);

            p++;
        }
    }
}

static void draw_flares(void)
{
    float x, y;
    int p = 0;

    for (y = _text_y; y > -_text_y; y--) {
        for (x = -_text_x; x < _text_x; x++) {
            if (text_light[p] > 128 && text_light[p + text_x] < 10)
                draw_flare(x, y, text_depth[p] + bump_pic[p]);
            p++;
        }
    }
}

static void scroll(double dCurrentTime)
{
    int a, s, polovina;
    //static double dLastCycle = -1;
    static double dLastMove = -1;

    if (dCurrentTime - dLastMove > 1.0 / (text_y / 1.5)) {
        dLastMove = dCurrentTime;

        polovina = text_x * text_y / 2;
        s = 0;
        for (a = text_x * text_y + text_x - 1; a >= text_x; a--) {
            if (speed[s])
                text_light[a] = text_light[a - text_x];        //scroll light table down
            s++;
            if (s >= text_x)
                s = 0;
        }
        memmove(text_light + text_x, text_light, text_x * text_y);
        memset(text_light, 253, text_x);

        s = 0;
        for (a = polovina; a < text_x * text_y; a++) {
            if (text_light[a] == 255)
                text_light[s] = text_light[s + text_x] >> 1;        //make black bugs in top line

            s++;

            if (s >= text_x)
                s = 0;
        }
    }
}

static void make_change(double dCurrentTime)
{
    int r = rand() % text_x * text_y;

    text[r] += 133;        //random bugs

    r = rand() % (4 * text_x);
    if (r < text_x && text_light[r])
        text_light[r] = 255;        //white bugs

    scroll (dCurrentTime);
}


static void make_text(void)
{
    int a;

    for (a = 0; a < text_x * text_y; a++)
        text[a] = rand() >> 8; // avoid the lowest bits of rand()

    for (a = 0; a < text_x; a++)
        speed[a] = rand() >= RAND_MAX / 2;
}

static void ourBuildTextures(void)
{
    mpglTexImage2D(GL_TEXTURE_2D, 0, 1, 128, 64, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                   font_texture);
    mpglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    mpglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    mpglBindTexture(GL_TEXTURE_2D, 1);
    mpglTexImage2D(GL_TEXTURE_2D, 0, 1, 4, 4, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                   flare);
    mpglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    mpglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Some pretty standard settings for wrapping and filtering.
    mpglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    mpglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    mpglBindTexture(GL_TEXTURE_2D, 0);
}

void matrixview_init(int w, int h)
{
    make_text();

    ourBuildTextures();

    // Color to clear color buffer to.
    mpglClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    // Depth to clear depth buffer to; type of test.
    mpglClearDepth(1.0);
    mpglDepthFunc(GL_LESS);

    // Enables Smooth Color Shading; try GL_FLAT for (lack of) fun.
    mpglShadeModel(GL_SMOOTH);

    // Set up a light, turn it on.
    mpglLightfv(GL_LIGHT1, GL_POSITION, Light_Position);
    mpglLightfv(GL_LIGHT1, GL_AMBIENT, Light_Ambient);
    mpglLightfv(GL_LIGHT1, GL_DIFFUSE, Light_Diffuse);
    mpglEnable(GL_LIGHT1);

    // A handy trick -- have surface material mirror the color.
    mpglColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    mpglEnable(GL_COLOR_MATERIAL);

    // Allow adjusting of texture color via glColor
    mpglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    matrixview_reshape(w, h);
}


void matrixview_reshape(int w, int h)
{
    mpglViewport(0, 0, w, h);

    mpglMatrixMode(GL_PROJECTION);
    mpglLoadIdentity();
    mpglFrustum(-_text_x, _text_x, -_text_y, _text_y, -Z_Off - Z_Depth, -Z_Off);

    mpglMatrixMode(GL_MODELVIEW);
}


void matrixview_draw(int w, int h, double currentTime, float frameTime,
                     uint8_t *data)
{
    mpglEnable(GL_BLEND);
    mpglEnable(GL_TEXTURE_2D);

    mpglDisable(GL_LIGHTING);
    mpglBlendFunc(GL_SRC_ALPHA, GL_ONE);
    mpglDisable(GL_DEPTH_TEST);

    mpglMatrixMode(GL_MODELVIEW);
    mpglLoadIdentity();
    mpglTranslated(0.0f, 0.0f, Z_Off);

    // Clear the color and depth buffers.
    mpglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // OK, let's start drawing our planer quads.
    mpglBegin(GL_QUADS);
    draw_text(data);
    mpglEnd();

    mpglBindTexture(GL_TEXTURE_2D, 1);
    mpglBegin(GL_QUADS);
    draw_flares();
    mpglEnd();
    mpglBindTexture(GL_TEXTURE_2D, 0);

    make_change(currentTime);

    mpglLoadIdentity();
    mpglMatrixMode(GL_PROJECTION);
}

void matrixview_contrast_set(float contrast)
{
    matrix_contrast = contrast;
}

void matrixview_brightness_set(float brightness)
{
    matrix_brightness = brightness;
}


void matrixview_matrix_resize(int w, int h)
{
    int elems;
    free(speed);
    speed = NULL;
    free(text);
    text = NULL;
    free(text_light);
    text_light = NULL;
    free(text_depth);
    text_depth = NULL;
    if (w > MAX_TEXT_X || h > MAX_TEXT_Y)
        return;
    elems = w * (h + 1);
    speed      = calloc(w,     sizeof(*speed));
    text       = calloc(elems, sizeof(*text));
    text_light = calloc(elems, sizeof(*text_light));
    text_depth = calloc(elems, sizeof(*text_depth));
    bump_pic   = calloc(elems, sizeof(*bump_pic));
    text_x = w;
    text_y = h;
    make_text();
}
