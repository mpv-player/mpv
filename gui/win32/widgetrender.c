/*
  MPlayer Gui for win32
  Copyright (c) 2003 Sascha Sommer <saschasommer@freenet.de>
  Copyright (c) 2006 Erik Augustson <erik_27can@yahoo.com>
  Copyright (c) 2006 Gianluigi Tiesi <sherpya@netfarm.it>

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02111-1307 USA
*/

#include <stdio.h>
#include <ctype.h>
#include <windows.h>
#include <interface.h>
#include "gui.h"

extern char *codecname;
#define MAX_LABELSIZE 250

static void render(int bitsperpixel, image *dst, image *src, int x, int y, int sx, int sy, int sw, int sh, int transparent)
{
    int i;
    int bpp = bitsperpixel / 8;
    int offset = (dst->width * bpp * y) + (x * bpp);
    int soffset = (src->width * bpp * sy) + (sx * bpp);

    for(i=0; i<sh; i++)
    {
        int c;
        for(c=0; c < (sw * bpp); c += bpp)
        {
            if(bpp == 2)
            {
                if(!transparent || (((src->data + soffset + (i * src->width * bpp) + c)[0] != 0x1f)
                    && ((src->data + soffset + (i * src->width * bpp) + c)[1] != 0x7c)))
                    memcpy(dst->data + offset + c, src->data + soffset + (i * src->width * bpp) + c, bpp);
            }
            else if(bpp > 2)
            {
                if(!transparent || *((unsigned int *) (src->data + soffset + (i * src->width * bpp) + c)) != 0x00ff00ff)
                    memcpy(dst->data + offset + c, src->data + soffset + (i * src->width * bpp) + c, bpp);
            }
        }
        offset += (dst->width * bpp);
    }
}

static image *find_background(skin_t *skin, widget *item)
{
    unsigned int i;
    for (i=0; i < skin->windowcount; i++)
        if(skin->windows[i]->type == item->window)
            return skin->windows[i]->base->bitmap[0];
    return NULL;
}

/******************************************************************/
/*                      FONT related functions                    */
/******************************************************************/

/* returns the pos of s2 inside s1 or -1 if  s1 doesn't contain s2 */
static int strpos(char *s1, const char* s2)
{
    unsigned int i, x;
    for (i=0; i < strlen(s1); i++)
    {
        if(s1[i] == s2[0])
        {
            if(strlen(s1 + i) >= strlen(s2))
            {
                for (x=0; x <strlen(s2); x++)
                    if(s1[i + x] != s2[x]) break;
                if(x == strlen(s2)) return i;
            }
        }
    }
    return -1;
}

/* replaces all occurences of what in dest with format */
static void stringreplace(char *dest, const char *what, const char *format, ... )
{
    char tmp[MAX_LABELSIZE];
    int offset=0;
    va_list va;
    va_start(va, format);
    vsnprintf(tmp, MAX_LABELSIZE, format, va);
    va_end(va);
    /* no search string == replace the entire string */
    if(!what)
    {
        memcpy(dest, tmp, strlen(tmp));
        dest[strlen(tmp)] = 0;
        return;
    }
    while((offset = strpos(dest, what)) != -1)
    {
        memmove(dest + offset + strlen(tmp), dest + offset + strlen(what), strlen(dest + offset + strlen(what)) + 1);
        memcpy(dest + offset, tmp, strlen(tmp));
    }
}

/* replaces the chars with special meaning with the associated data from the player info struct */
static char *generatetextfromlabel(widget *item)
{
    char *text = malloc(MAX_LABELSIZE);
    char tmp[MAX_LABELSIZE];
    unsigned int i;
    if(!item)
    {
        free(text);
        return NULL;
    }
    strcpy(text, item->label);
    if(item->type == tySlabel) return text;
    stringreplace(text, "$1", "%.2i:%.2i:%.2i", guiIntfStruct.TimeSec / 3600,
                 (guiIntfStruct.TimeSec / 60) % 60, guiIntfStruct.TimeSec % 60);
    stringreplace(text, "$2", "%.4i:%.2i", guiIntfStruct.TimeSec / 60, guiIntfStruct.TimeSec % 60);
    stringreplace(text, "$3", "%.2i", guiIntfStruct.TimeSec / 3600);
    stringreplace(text, "$4", "%.2i", (guiIntfStruct.TimeSec / 60) % 60);
    stringreplace(text, "$5", "%.2i", guiIntfStruct.TimeSec % 60);
    stringreplace(text, "$6", "%.2i:%.2i:%.2i", guiIntfStruct.LengthInSec / 3600,
                 (guiIntfStruct.LengthInSec / 60) % 60, guiIntfStruct.LengthInSec % 60);
    stringreplace(text, "$7", "%.4i:%.2i", guiIntfStruct.LengthInSec / 60, guiIntfStruct.LengthInSec % 60);
    stringreplace(text, "$8", "%i:%.2i:%.2i", guiIntfStruct.TimeSec / 3600,
                 (guiIntfStruct.TimeSec / 60) % 60, guiIntfStruct.TimeSec % 60);
    stringreplace(text, "$v", "%3.2f", guiIntfStruct.Volume);
    stringreplace(text, "$V", "%3.1f", guiIntfStruct.Volume);
    stringreplace(text, "$b", "%3.2f", guiIntfStruct.Balance);
    stringreplace(text, "$B", "%3.1f", guiIntfStruct.Balance);
    stringreplace(text, "$t", "%.2i", guiIntfStruct.Track);
    stringreplace(text, "$o", "%s", guiIntfStruct.Filename);
    stringreplace(text, "$x", "%i", guiIntfStruct.MovieWidth);
    stringreplace(text, "$y", "%i", guiIntfStruct.MovieHeight);
    stringreplace(text, "$C", "%s", guiIntfStruct.sh_video ? codecname : "");
    stringreplace(text, "$$", "$");

    if(!strcmp(text, "$p") || !strcmp(text, "$s") || !strcmp(text, "$e"))
    {
        if(guiIntfStruct.Playing == 0) stringreplace(text, NULL, "s");
        else if(guiIntfStruct.Playing == 1) stringreplace(text, NULL, "p");
        else if(guiIntfStruct.Playing == 2) stringreplace(text, NULL, "e");
    }

    if(guiIntfStruct.AudioType == 0) stringreplace(text, "$a", "n");
    else if(guiIntfStruct.AudioType == 1) stringreplace(text, "$a", "m");
    else stringreplace(text, "$a", "t");

    if(guiIntfStruct.StreamType == 0)
        stringreplace(text, "$T", "f");
#ifdef USE_DVDREAD
    else if(guiIntfStruct.StreamType == STREAMTYPE_DVD || guiIntfStruct.StreamType == STREAMTYPE_DVDNAV)
        stringreplace(text, "$T", "d");
#endif
    else stringreplace(text, "$T", "u");

    if(guiIntfStruct.Filename)
    {
        for (i=0; i<strlen(guiIntfStruct.Filename); i++)
            tmp[i] = tolower(guiIntfStruct.Filename[i]);
        stringreplace(text, "$f", tmp);

        for (i=0; i<strlen(guiIntfStruct.Filename); i++)
            tmp[i] = toupper(guiIntfStruct.Filename[i]);
        stringreplace(text, "$F", tmp);
    }

    return text;
}

/* cuts text to buflen scrolling from right to left */
static void scrolltext(char *text, unsigned int buflen, float *value)
{
    char *buffer = (char *) malloc(buflen + 1);
    unsigned int x,i;
    if(*value < buflen) x = 0;
    else x = *value - buflen;
    memset(buffer, ' ', buflen);
    for (i = (*value>=buflen) ? 0 : buflen - *value; i<buflen; i++)
    {
        if(x < strlen(text))
            buffer[i] = text[x];
        x++;
    }
    buffer[buflen] = 0;
    *value += 1.0f;
    if(*value >= strlen(text) + buflen) *value = 0.0f;
    strcpy(text, buffer);
    free(buffer);
}

/* updates all dlabels and slabels */
void renderinfobox(skin_t *skin, window_priv_t *priv)
{
    unsigned int i;
    if (!priv) return;

    /* repaint the area behind the text*/
    /* we have to do this for all labels here, because they may overlap in buggy skins ;( */

    for (i=0; i<skin->widgetcount; i++)
        if((skin->widgets[i]->type == tyDlabel) || (skin->widgets[i]->type == tySlabel))
        {
            if(skin->widgets[i]->window == priv->type)
                render(skin->desktopbpp,
                       &priv->img,
                       find_background(skin, skin->widgets[i]),
                       skin->widgets[i]->x,
                       skin->widgets[i]->y,
                       skin->widgets[i]->x,
                       skin->widgets[i]->y,
                       skin->widgets[i]->length,
                       skin->widgets[i]->font->chars[0]->height,
                       1);
        }

    /* load all slabels and dlabels */
    for (i=0; i<skin->widgetcount; i++)
    {
        widget *item = skin->widgets[i];
        if(item->window != priv->type) continue;
        if((i == skin->widgetcount) || (item->type == tyDlabel) || (item->type == tySlabel))
        {
            char *text = generatetextfromlabel(item);
            unsigned int current, c;
            int offset = 0;
            unsigned int textlen;
            if(!text) continue;
            textlen = strlen(text);

            /* render(win, win->background, gui->skin->widgets[i]->x, gui->skin->widgets[i]->y,
                      gui->skin->widgets[i]->x, gui->skin->widgets[i]->y,
                      gui->skin->widgets[i]->length, gui->skin->widgets[i]->font->chars[0]->height,1); */

            /* calculate text size */
            for (current=0; current<textlen; current++)
            {
                for (c=0; c<item->font->charcount; c++)
                    if(item->font->chars[c]->c == text[current])
                    {
                        offset += item->font->chars[c]->width;
                        break;
                    }
            }

            /* labels can be scrolled if they are to big */
            if((item->type == tyDlabel) && (item->length < offset))
            {
                int tomuch = (offset - item->length) / (offset /textlen);
                scrolltext(text, textlen - tomuch - 1, &skin->widgets[i]->value);
                textlen = strlen(text);
            }

            /* align the text */
            if(item->align == 1)
                offset = (item->length-offset) / 2;
            else if(item->align == 2)
                offset = item->length-offset;
            else
                offset = 0;

            if(offset < 0) offset = 0;

            /* render the text */
            for (current=0; current<textlen; current++)
            {
                for (c=0; c<item->font->charcount; c++)
                {
                    char_t *cchar = item->font->chars[c];
                    if(cchar->c == *(text + current))
                    {
                        render(skin->desktopbpp,
                               &priv->img,
                               item->font->image,
                               item->x + offset,
                               item->y,
                               cchar->x,
                               cchar->y,
                               (cchar->width + offset > item->length) ? item->length - offset : cchar->width,
                               cchar->height,
                               1);
                        offset += cchar->width;
                    break;
                    }
                }
            }
            free(text);
        }
    }
}

/******************************************************************/
/*                   WIDGET related functions                     */
/******************************************************************/

void renderwidget(skin_t *skin, image *dest, widget *item, int state)
{
    image *img = NULL;
    int height;
    int y;

    if(!dest) return;
    if((item->type == tyButton) || (item->type == tyHpotmeter) || (item->type == tyPotmeter))
        img = item->bitmap[0];

    if(!img) return;

    y = item->y;
    if(item->type == tyPotmeter)
    {
        height = img->height / item->phases;
        y =  height * (int)(item->value * item->phases / 100);
        if(y > img->height-height)
            y = img->height - height;
    }
    else
    {
        height = img->height / 3;
        y = state * height;
    }

    /* redraw background */
    if(item->type == tyButton)
        render(skin->desktopbpp, dest, find_background(skin,item), item->x, item->y, item->x, item->y, img->width, height, 1);

    if((item->type == tyHpotmeter) || (item->type == tyPotmeter))
    {
        /* repaint the area behind the slider */
        render(skin->desktopbpp, dest, find_background(skin, item), item->wx, item->wy, item->wx, item->wy, item->wwidth, item->height, 1);
        item->x = item->value * (item->wwidth-item->width) / 100 + item->wx;
        if((item->x + item->width) > (item->wx + item->wwidth))
            item->x = item->wx + item->wwidth - item->width;
        if(item->x < item->wx)
            item->x = item->wx;
        /* workaround for blue */
        if(item->type == tyHpotmeter)
            height = (item->height < img->height / 3) ? item->height : img->height / 3;
    }
    render(skin->desktopbpp, dest, img, item->x, item->y, 0, y, img->width, height, 1);
}
