/*
 * MPlayer GUI for Win32
 * Copyright (C) 2003 Sascha Sommer <saschasommer@freenet.de>
 * Copyright (C) 2006 Erik Augustson <erik_27can@yahoo.com>
 * Copyright (C) 2006 Gianluigi Tiesi <sherpya@netfarm.it>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <windows.h>

#include "mp_msg.h"
#include "cpudetect.h"
#include "libswscale/rgb2rgb.h"
#include "libswscale/swscale.h"
#include "gui.h"
#include "gui/bitmap.h"

#define MAX_LINESIZE 256

typedef struct
{
    int msg;
    char *name;
} evName;

static const evName evNames[] =
{
    {   evNone,                 "evNone"                },
    {   evPlay,                 "evPlay"                },
    {   evDropFile,             "evDropFile"            },
    {   evStop,                 "evStop"                },
    {   evPause,                "evPause"               },
    {   evPrev,                 "evPrev"                },
    {   evNext,                 "evNext"                },
    {   evLoad,                 "evLoad"                },
    {   evEqualizer,            "evEqualizer"           },
    {   evEqualizer,            "evEqualeaser"          },
    {   evPlayList,             "evPlaylist"            },
    {   evExit,                 "evExit"                },
    {   evIconify,              "evIconify"             },
    {   evIncBalance,           "evIncBalance"          },
    {   evDecBalance,           "evDecBalance"          },
    {   evFullScreen,           "evFullScreen"          },
    {   evFName,                "evFName"               },
    {   evMovieTime,            "evMovieTime"           },
    {   evAbout,                "evAbout"               },
    {   evLoadPlay,             "evLoadPlay"            },
    {   evPreferences,          "evPreferences"         },
    {   evSkinBrowser,          "evSkinBrowser"         },
    {   evBackward10sec,        "evBackward10sec"       },
    {   evForward10sec,         "evForward10sec"        },
    {   evBackward1min,         "evBackward1min"        },
    {   evForward1min,          "evForward1min"         },
    {   evBackward10min,        "evBackward10min"       },
    {   evForward10min,         "evForward10min"        },
    {   evIncVolume,            "evIncVolume"           },
    {   evDecVolume,            "evDecVolume"           },
    {   evMute,                 "evMute"                },
    {   evIncAudioBufDelay,     "evIncAudioBufDelay"    },
    {   evDecAudioBufDelay,     "evDecAudioBufDelay"    },
    {   evPlaySwitchToPause,    "evPlaySwitchToPause"   },
    {   evPauseSwitchToPlay,    "evPauseSwitchToPlay"   },
    {   evNormalSize,           "evNormalSize"          },
    {   evDoubleSize,           "evDoubleSize"          },
    {   evSetMoviePosition,     "evSetMoviePosition"    },
    {   evSetVolume,            "evSetVolume"           },
    {   evSetBalance,           "evSetBalance"          },
    {   evHelp,                 "evHelp"                },
    {   evLoadSubtitle,         "evLoadSubtitle"        },
    {   evPlayDVD,              "evPlayDVD"             },
    {   evPlayVCD,              "evPlayVCD"             },
    {   evSetURL,               "evSetURL"              },
    {   evLoadAudioFile,        "evLoadAudioFile"       },
    {   evDropSubtitle,         "evDropSubtitle"        },
    {   evSetAspect,            "evSetAspect"           }
};

static const int evBoxs = sizeof(evNames) / sizeof(evName);

static char *geteventname(int event)
{
    int i;
    for(i=0; i<evBoxs; i++)
        if(evNames[i].msg == event)
            return evNames[i].name;
    return NULL;
}

static inline int get_sws_cpuflags(void)
{
    return (gCpuCaps.hasMMX ? SWS_CPU_CAPS_MMX : 0) |
           (gCpuCaps.hasMMX2 ? SWS_CPU_CAPS_MMX2 : 0) |
           (gCpuCaps.has3DNow ? SWS_CPU_CAPS_3DNOW : 0);
}

/* reads a complete image as is into image buffer */
static image *pngRead(skin_t *skin, unsigned char *fname)
{
    int i;
    txSample bmp;
    image *bf;
    char *filename = NULL;
    FILE *fp;

    if(!stricmp(fname, "NULL")) return 0;

    /* find filename in order file file.png */
    if(!(fp = fopen(fname, "rb")))
    {
        filename = calloc(1, strlen(skin->skindir) + strlen(fname) + 6);
        sprintf(filename, "%s\\%s.png", skin->skindir, fname);
        if(!(fp = fopen(filename, "rb")))
        {
            mp_msg(MSGT_GPLAYER, MSGL_ERR, "[png] cannot find image %s\n", filename);
            free(filename);
            return 0;
        }
    }
    fclose(fp);

    for (i=0; i < skin->imagecount; i++)
        if(!strcmp(fname, skin->images[i]->name))
        {
#ifdef DEBUG
            mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[png] skinfile %s already exists\n", fname);
#endif
            free(filename);
            return skin->images[i];
        }
    (skin->imagecount)++;
    skin->images = realloc(skin->images, sizeof(image *) * skin->imagecount);
    bf = skin->images[(skin->imagecount) - 1] = calloc(1, sizeof(image));
    bf->name = strdup(fname);
    bpRead(filename ? filename : fname, &bmp);
    free(filename);
    bf->width = bmp.Width; bf->height = bmp.Height;

#ifdef DEBUG
    mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[png] loaded image %s\n", fname);
    mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[png] size: %dx%d bits: %d\n", bf->width, bf->height, BPP);
    mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[png] imagesize: %u\n", imgsize);
#endif

    bf->size = bf->width * bf->height * skin->desktopbpp / 8;
    if (skin->desktopbpp == 32)
      bf->data = bmp.Image;
    else {
      bf->data = malloc(bf->size);
      rgb32tobgr32(bmp.Image, bmp.Image, bmp.ImageSize);
      if(skin->desktopbpp == 16) rgb32tobgr15(bmp.Image, bf->data, bmp.ImageSize);
      else if(skin->desktopbpp == 24) rgb32tobgr24(bmp.Image, bf->data, bmp.ImageSize);
      free(bmp.Image);
    }
    return bf;
}

/* frees all skin images */
static void freeimages(skin_t *skin)
{
    unsigned int i;
    for (i=0; i<skin->imagecount; i++)
    {
        if(skin->images && skin->images[i])
        {
            if(skin->images[i]->data) free(skin->images[i]->data);
            if(skin->images[i]->name) free(skin->images[i]->name);
            free(skin->images[i]);
        }
    }
    free(skin->images);
}

#ifdef DEBUG
void dumpwidgets(skin_t *skin)
{
    unsigned int i;
    for (i=0; i<skin->widgetcount; i++)
        mp_msg(MSGT_GPLAYER, MSGL_V, "widget %p id %i\n", skin->widgets[i], skin->widgets[i]->id);
}
#endif

static int counttonextchar(const char *s1, char c)
{
    unsigned int i;
    for (i=0; i<strlen(s1); i++)
        if(s1[i] == c) return i;
    return 0;
}

static char *findnextstring(char *temp, const char *desc, int *base)
{
    int len = counttonextchar(*base + desc, ',');
    memset(temp, 0, strlen(desc) + 1);
    if(!len) len = strlen(desc);
    memcpy(temp, *base + desc, len);
    *base += (len+1);
    return temp;
}

static void freeskin(skin_t *skin)
{
    unsigned int i;
    if(skin->skindir)
    {
        free(skin->skindir);
        skin->skindir = NULL;
    }

    for (i=1; i<=skin->lastusedid; i++)
        skin->removewidget(skin, i);

    if(skin->widgets)
    {
        free(skin->widgets);
        skin->widgets = NULL;
    }

    freeimages(skin);
    for(i=0; i<skin->windowcount; i++)
    {
        if(skin->windows[i]->name)
        {
            free(skin->windows[i]->name);
            skin->windows[i]->name = NULL;
        }
        free(skin->windows[i]);
    }

    free(skin->windows);
    skin->windows = NULL;

    for (i=0; i<skin->fontcount; i++)
    {
        unsigned int x;
        if(skin->fonts[i]->name)
        {
            free(skin->fonts[i]->name);
            skin->fonts[i]->name = NULL;
        }

        if(skin->fonts[i]->id)
        {
            free(skin->fonts[i]->id);
            skin->fonts[i]->id = NULL;
        }

        for (x=0; x<skin->fonts[i]->charcount; x++)
        {
            free(skin->fonts[i]->chars[x]);
            skin->fonts[i]->chars[x] = NULL;
        }

        if(skin->fonts[i]->chars)
        {
            free(skin->fonts[i]->chars);
            skin->fonts[i]->chars = NULL;
        }

        free(skin->fonts[i]);
        skin->fonts[i] = NULL;
    }
    free(skin->fonts);
    skin->fonts = NULL;
#ifdef DEBUG
    mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[SKIN FREE] skin freed\n");
#endif
    free(skin);
    skin = NULL;
}

static void removewidget(skin_t *skin, int id)
{
    unsigned int i;
    unsigned int pos=0;
    widget **temp = calloc(skin->widgetcount - 1, sizeof(widget *));

    for (i=0; i<skin->widgetcount; i++)
    {
        if(skin->widgets[i]->id == id)
        {
            if(skin->widgets[i]->label)
                free(skin->widgets[i]->label);
            free(skin->widgets[i]);
            skin->widgets[i] = NULL;
        }
        else
        {
            temp[pos] = skin->widgets[i];
            pos++;
        }
    }
    if (pos != i)
    {
        (skin->widgetcount)--;
        free(skin->widgets);
        skin->widgets = temp;
#ifdef DEBUG
        mp_msg(MSGT_GPLAYER, MSGL_DBG4, "removed widget %i\n", id);
#endif
        return;
    }
    free(temp);
    mp_msg(MSGT_GPLAYER, MSGL_ERR, "widget %i not found\n", id);
}

static void addwidget(skin_t *skin, window *win, const char *desc)
{
    widget *mywidget;
    char *temp = calloc(1, strlen(desc) + 1);
    (skin->widgetcount)++;
    (skin->lastusedid)++;
    skin->widgets = realloc(skin->widgets, sizeof(widget *) * skin->widgetcount);
    mywidget = skin->widgets[(skin->widgetcount) - 1] = calloc(1, sizeof(widget));
    mywidget->id = skin->lastusedid;
    mywidget->window = win->type;
    /* parse and fill widget specific info */
    if(!strncmp(desc, "base", 4))
    {
        int base = counttonextchar(desc, '=') + 1;
        mywidget->type = tyBase;
        mywidget->bitmap[0] = pngRead(skin, findnextstring(temp, desc, &base));
        mywidget->wx = mywidget->x = atoi(findnextstring(temp, desc, &base));
        mywidget->wy = mywidget->y = atoi(findnextstring(temp, desc, &base));
        mywidget->wwidth = mywidget->width = atoi(findnextstring(temp, desc, &base));
        mywidget->wheight = mywidget->height = atoi(findnextstring(temp, desc, &base));
        win->base = mywidget;
#ifdef DEBUG
        mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[SKIN] [ITEM] [BASE] %s %i %i %i %i\n",
              (mywidget->bitmap[0]) ? mywidget->bitmap[0]->name : NULL,
               mywidget->x, mywidget->y, mywidget->width, mywidget->height);
#endif
    }
    else if(!strncmp(desc, "button", 6))
    {
        int base = counttonextchar(desc, '=') + 1;
        int i;
        mywidget->type = tyButton;
        mywidget->bitmap[0] = pngRead(skin, findnextstring(temp, desc, &base));
        mywidget->wx = mywidget->x = atoi(findnextstring(temp, desc, &base));
        mywidget->wy = mywidget->y = atoi(findnextstring(temp, desc, &base));
        mywidget->wwidth = mywidget->width = atoi(findnextstring(temp, desc, &base));
        mywidget->wheight = mywidget->height = atoi(findnextstring(temp, desc, &base));
        findnextstring(temp, desc, &base);

        /* Assign corresponding event to the widget */
        mywidget->msg = evNone;
        for (i=0; i<evBoxs; i++)
        {
            if(!strcmp(temp, evNames[i].name))
            {
                mywidget->msg = evNames[i].msg;
                break;
            }
        }

#ifdef DEBUG
        mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[SKIN] [ITEM] [BUTTON] %s %i %i %i %i msg %i\n",
              (mywidget->bitmap[0]) ? mywidget->bitmap[0]->name : NULL,
               mywidget->x, mywidget->y, mywidget->width, mywidget->height, mywidget->msg);
#endif
    }
    else if(!strncmp(desc, "hpotmeter", 9) || !strncmp(desc, "vpotmeter", 9))
    {
        int base = counttonextchar(desc, '=') + 1;
        int i;
        /* hpotmeter = button, bwidth, bheight, phases, numphases, default, X, Y, width, height, message */
        if(!strncmp(desc, "hpotmeter", 9)) mywidget->type = tyHpotmeter;
        else mywidget->type = tyVpotmeter;
        mywidget->bitmap[0] = pngRead(skin, findnextstring(temp, desc, &base));
        mywidget->width = atoi(findnextstring(temp, desc, &base));
        mywidget->height = atoi(findnextstring(temp, desc, &base));
        mywidget->bitmap[1] = pngRead(skin, findnextstring(temp, desc, &base));
        mywidget->phases = atoi(findnextstring(temp, desc, &base));
        mywidget->value = atof(findnextstring(temp, desc, &base));
        mywidget->x = mywidget->wx = atoi(findnextstring(temp, desc, &base));
        mywidget->y = mywidget->wy = atoi(findnextstring(temp, desc, &base));
        mywidget->wwidth = atoi(findnextstring(temp, desc, &base));
        mywidget->wheight = atoi(findnextstring(temp, desc, &base));
        findnextstring(temp, desc, &base);
        mywidget->msg = evNone;
        for (i=0; i<evBoxs; i++)
        {
            if(!strcmp(temp, evNames[i].name))
            {
                mywidget->msg = evNames[i].msg;
                break;
            }
        }
#ifdef DEBUG
        mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[SKIN] [ITEM] %s %s %i %i %s %i %f %i %i %i %i msg %i\n",
                (mywidget->type == tyHpotmeter) ? "[HPOTMETER]" : "[VPOTMETER]",
                (mywidget->bitmap[0]) ? mywidget->bitmap[0]->name : NULL,
                mywidget->width, mywidget->height,
                (mywidget->bitmap[1]) ? mywidget->bitmap[1]->name : NULL,
                mywidget->phases, mywidget->value,
                mywidget->wx, mywidget->wy, mywidget->wwidth, mywidget->wwidth,
                mywidget->msg);
#endif
    }
    else if(!strncmp(desc, "potmeter", 8))
    {
        int base = counttonextchar(desc, '=') + 1;
        int i;
        /* potmeter = phases, numphases, default, X, Y, width, height, message */
        mywidget->type = tyPotmeter;
        mywidget->bitmap[0] = pngRead(skin, findnextstring(temp, desc, &base));
        mywidget->phases = atoi(findnextstring(temp, desc, &base));
        mywidget->value = atof(findnextstring(temp, desc, &base));
        mywidget->wx = mywidget->x = atoi(findnextstring(temp, desc, &base));
        mywidget->wy = mywidget->y = atoi(findnextstring(temp, desc, &base));
        mywidget->wwidth = mywidget->width = atoi(findnextstring(temp, desc, &base));
        mywidget->wheight = mywidget->height = atoi(findnextstring(temp, desc, &base));
        findnextstring(temp, desc, &base);
        mywidget->msg = evNone;
        for (i=0; i<evBoxs; i++)
        {
            if(!strcmp(temp, evNames[i].name))
            {
                mywidget->msg=evNames[i].msg;
                break;
            }
        }
#ifdef DEBUG
        mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[SKIN] [ITEM] [POTMETER] %s %i %i %i %f %i %i msg %i\n",
                (mywidget->bitmap[0]) ? mywidget->bitmap[0]->name : NULL,
                mywidget->width, mywidget->height,
                mywidget->phases, mywidget->value,
                mywidget->x, mywidget->y,
                mywidget->msg);
#endif
    }
    else if(!strncmp(desc, "menu", 4))
    {
        int base = counttonextchar(desc, '=') + 1;
        int i;
        mywidget->type = tyMenu;
        mywidget->wx=atoi(findnextstring(temp, desc, &base));
        mywidget->x=0;
        mywidget->wy=mywidget->y=atoi(findnextstring(temp, desc, &base));
        mywidget->wwidth=mywidget->width=atoi(findnextstring(temp, desc, &base));
        mywidget->wheight=mywidget->height=atoi(findnextstring(temp, desc, &base));
        findnextstring(temp, desc, &base);
        mywidget->msg = evNone;
        for (i=0; i<evBoxs; i++)
        {
            if(!strcmp(temp, evNames[i].name))
            {
                mywidget->msg = evNames[i].msg;
                break;
            }
        }
#ifdef DEBUG
        mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[SKIN] [ITEM] [MENU] %i %i %i %i msg %i\n",
               mywidget->x, mywidget->y, mywidget->width, mywidget->height, mywidget->msg);
#endif
    }
    else if(!strncmp(desc, "selected", 8))
    {
        win->base->bitmap[1] = pngRead(skin, (char *) desc + 9);
#ifdef DEBUG
        mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[SKIN] [ITEM] [BASE] added image %s\n", win->base->bitmap[1]->name);
#endif
    }
    else if(!strncmp(desc, "slabel",6))
    {
        int base = counttonextchar(desc, '=') + 1;
        unsigned int i;
        mywidget->type = tySlabel;
        mywidget->wx = mywidget->x = atoi(findnextstring(temp, desc, &base));
        mywidget->wy = mywidget->y = atoi(findnextstring(temp, desc, &base));
        findnextstring(temp, desc, &base);
        mywidget->font = NULL;
        for (i=0; i<skin->fontcount; i++)
        {
            if(!strcmp(temp, skin->fonts[i]->name))
            {
                mywidget->font = skin->fonts[i];
                break;
            }
        }
        mywidget->label = strdup(findnextstring(temp, desc, &base));
#ifdef DEBUG
        mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[SKIN] [ITEM] [SLABEL] %i %i %s %s\n",
               mywidget->x, mywidget->y, mywidget->font->name, mywidget->label);
#endif
    }
    else if(!strncmp(desc, "dlabel", 6))
    {
        int base = counttonextchar(desc, '=') + 1;
        unsigned int i;
        mywidget->type = tyDlabel;
        mywidget->wx = mywidget->x = atoi(findnextstring(temp, desc, &base));
        mywidget->wy = mywidget->y = atoi(findnextstring(temp, desc, &base));
        mywidget->length = atoi(findnextstring(temp, desc, &base));
        mywidget->align = atoi(findnextstring(temp, desc, &base));
        findnextstring(temp, desc, &base);
        mywidget->font = NULL;
        for (i=0; i<skin->fontcount; i++)
        {
            if(!strcmp(temp, skin->fonts[i]->name))
            {
                mywidget->font=skin->fonts[i];
                break;
            }
        }
        mywidget->label=strdup(findnextstring(temp, desc, &base));
#ifdef DEBUG
        mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[SKIN] [ITEM] [DLABEL] %i %i %i %i %s \"%s\"\n",
               mywidget->x, mywidget->y, mywidget->length, mywidget->align, mywidget->font->name, mywidget->label);
#endif
    }
    free(temp);
}

static void loadfonts(skin_t* skin)
{
    unsigned int x;
    for (x=0; x<skin->fontcount; x++)
    {
        FILE *fp;
        int linenumber=0;
        char *filename;
        char *tmp = calloc(1, MAX_LINESIZE);
        char *desc = calloc(1, MAX_LINESIZE);
        filename = calloc(1, strlen(skin->skindir) + strlen(skin->fonts[x]->name) + 6);
        sprintf(filename, "%s\\%s.fnt", skin->skindir, skin->fonts[x]->name);
        if(!(fp = fopen(filename,"rb")))
        {
            mp_msg(MSGT_GPLAYER, MSGL_ERR, "[FONT LOAD] Font not found \"%s\"\n", skin->fonts[x]->name);
            return;
        }
        while(!feof(fp))
        {
            int pos = 0;
            unsigned int i;
            fgets(tmp, MAX_LINESIZE, fp);
            linenumber++;
            memset(desc, 0, MAX_LINESIZE);
            for (i=0; i<strlen(tmp); i++)
            {
                /* remove spaces and linebreaks */
                if((tmp[i] == ' ') || (tmp[i] == '\n') || (tmp[i] == '\r')) continue;
                /* remove comments */
                if((tmp[i] == ';') &&  ((i < 1) || (tmp[i-1] != '\"')))
                {
#ifdef DEBUG
                    mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[FONT LOAD] Comment: %s", tmp + i + 1);
#endif
                    break;
                }
                desc[pos] = tmp[i];
                pos++;
            }
            if(!strlen(desc)) continue;
            /* now we have "readable" output -> parse it */
            if(!strncmp(desc, "image", 5))
            {
                skin->fonts[x]->image = pngRead(skin, desc + 6);
#ifdef DEBUG
                mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[FONT] [IMAGE] \"%s\"\n", desc + 6);
#endif
            }
            else
            {
                int base = 4;
                if(*desc != '"') break;
                if(*(desc + 1) == 0) break;
                (skin->fonts[x]->charcount)++;
                skin->fonts[x]->chars = realloc(skin->fonts[x]->chars, sizeof(char_t *) *skin->fonts[x]->charcount);
                skin->fonts[x]->chars[skin->fonts[x]->charcount - 1]=calloc(1, sizeof(char_t));
                skin->fonts[x]->chars[skin->fonts[x]->charcount - 1]->c = ((*(desc + 1) == '"') && (*(desc + 2) != '"')) ? ' ': *(desc + 1);
                if((*(desc + 1) == '"') && (*(desc + 2) != '"')) base = 3;
                skin->fonts[x]->chars[skin->fonts[x]->charcount - 1]->x = atoi(findnextstring(tmp, desc, &base));
                skin->fonts[x]->chars[skin->fonts[x]->charcount - 1]->y = atoi(findnextstring(tmp, desc, &base));
                skin->fonts[x]->chars[skin->fonts[x]->charcount - 1]->width = atoi(findnextstring(tmp, desc, &base));
                skin->fonts[x]->chars[skin->fonts[x]->charcount - 1]->height = atoi(findnextstring(tmp, desc, &base));
#ifdef DEBUG
                mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[FONT] [CHAR] %c %i %i %i %i\n",
                        skin->fonts[x]->chars[skin->fonts[x]->charcount - 1]->c,
                        skin->fonts[x]->chars[skin->fonts[x]->charcount - 1]->x,
                        skin->fonts[x]->chars[skin->fonts[x]->charcount - 1]->y,
                        skin->fonts[x]->chars[skin->fonts[x]->charcount - 1]->width,
                        skin->fonts[x]->chars[skin->fonts[x]->charcount - 1]->height);
#endif
            }
        }
        free(desc);
        free(filename);
        free(tmp);
        fclose(fp);
    }
}

skin_t* loadskin(char* skindir, int desktopbpp)
{
    FILE *fp;
    int reachedendofwindow = 0;
    int linenumber = 0;
    skin_t *skin = calloc(1, sizeof(skin_t));
    char *filename;
    char *tmp = calloc(1, MAX_LINESIZE);
    char *desc = calloc(1, MAX_LINESIZE);
    window* mywindow = NULL;

    /* init swscaler */
    sws_rgb2rgb_init(get_sws_cpuflags());
    /* setup funcs */
    skin->freeskin = freeskin;
    skin->pngRead = pngRead;
    skin->addwidget = addwidget;
    skin->removewidget = removewidget;
    skin->geteventname = geteventname;
    skin->desktopbpp = desktopbpp;
    skin->skindir = strdup(skindir);

    filename = calloc(1, strlen(skin->skindir) + strlen("skin") + 2);
    sprintf(filename, "%s\\skin", skin->skindir);
    if(!(fp = fopen(filename, "rb")))
    {
        mp_msg(MSGT_GPLAYER, MSGL_FATAL, "[SKIN LOAD] Skin \"%s\" not found\n", skindir);
        skin->freeskin(skin);
        return NULL;
    }

    while(!feof(fp))
    {
        int pos = 0;
        unsigned int i;
        int insidequote = 0;
        fgets(tmp, MAX_LINESIZE, fp);
        linenumber++;
        memset(desc, 0, MAX_LINESIZE);
        for (i=0; i<strlen(tmp); i++)
        {
            if((tmp[i] == '"') && !insidequote) { insidequote=1; continue; }
            else if((tmp[i] == '"') && insidequote) { insidequote=0 ; continue; }
            /* remove spaces and linebreaks */
            if((!insidequote && (tmp[i] == ' ')) || (tmp[i] == '\n') || (tmp[i] == '\r')) continue;
            /* remove comments */
            else if(tmp[i] == ';')
            {
#ifdef DEBUG
                mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[SKIN LOAD] Comment: %s", tmp + i + 1);
#endif
                break;
            }
            desc[pos] = tmp[i];
            pos++;
        }

        if(!strlen(desc)) continue;
        /* now we have "readable" output -> parse it */
        /* parse window specific info */
        if(!strncmp(desc, "section", 7))
        {
#ifdef DEBUG
            mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[SKIN] [SECTION] \"%s\"\n", desc + 8);
#endif
        }
        else if(!strncmp(desc, "window", 6))
        {
#ifdef DEBUG
            mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[SKIN] [WINDOW] \"%s\"\n", desc + 7);
#endif
            reachedendofwindow = 0;
            (skin->windowcount)++;
            skin->windows = realloc(skin->windows, sizeof(window *) * skin->windowcount);
            mywindow = skin->windows[(skin->windowcount) - 1] = calloc(1, sizeof(window));
            mywindow->name = strdup(desc + 7);
            if(!strncmp(desc + 7, "main", 4)) mywindow->type = wiMain;
            else if(!strncmp(desc+7, "sub", 3))
            {
                mywindow->type = wiSub;
                mywindow->decoration = 1;
            }
            else if(!strncmp(desc + 7, "menu", 4)) mywindow->type = wiMenu;
            else if(!strncmp(desc + 7, "playbar", 7)) mywindow->type = wiPlaybar;
            else mp_msg(MSGT_GPLAYER, MSGL_V, "[SKIN] warning found unknown windowtype");
        }
        else if(!strncmp(desc, "decoration", 10) && !strncmp(desc + 11, "enable", 6))
        {
            mywindow->decoration = 1;
#ifdef DEBUG
            mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[SKIN] [DECORATION] enabled decoration for window \"%s\"\n", mywindow->name);
#endif
        }
        else if(!strncmp(desc, "background", 10))
        {
            int base = counttonextchar(desc, '=') + 1;
            char temp[MAX_LINESIZE];
            mywindow->backgroundcolor[0] = atoi(findnextstring(temp, desc, &base));
            mywindow->backgroundcolor[1] = atoi(findnextstring(temp, desc, &base));
            mywindow->backgroundcolor[2] = atoi(findnextstring(temp, desc, &base));
#ifdef DEBUG
            mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[SKIN] [BACKGROUND] window \"%s\" has backgroundcolor (%i,%i,%i)\n", mywindow->name,
                    mywindow->backgroundcolor[0],
                    mywindow->backgroundcolor[1],
                    mywindow->backgroundcolor[2]);
#endif
        }
        else if(!strncmp(desc, "end", 3))
        {
            if(reachedendofwindow)
            {
#ifdef DEBUG
                mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[SKIN] [END] of section\n");
#endif
            }
            else
            {
                reachedendofwindow = 1;
#ifdef DEBUG
                mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[SKIN] [END] of window \"%s\"\n", mywindow->name);
#endif
            }
        }
        else if(!strncmp(desc, "font", 4))
        {
            unsigned int i;
            int id = 0;
            char temp[MAX_LINESIZE];
            int base = counttonextchar(desc, '=')+1;
            findnextstring(temp, desc, &base);
            findnextstring(temp, desc, &base);
            for (i=0; i<skin->fontcount; i++)
                if(!strcmp(skin->fonts[i]->id, temp))
                {
                    id = i;
                    break;
                }
            if(!id)
            {
                int base = counttonextchar(desc, '=') + 1;
                findnextstring(temp, desc, &base);
                id = skin->fontcount;
                (skin->fontcount)++;
                skin->fonts = realloc(skin->fonts, sizeof(font_t *) * skin->fontcount);
                skin->fonts[id]=calloc(1, sizeof(font_t));
                skin->fonts[id]->name = strdup(temp);
                skin->fonts[id]->id = strdup(findnextstring(temp, desc, &base));
            }
#ifdef DEBUG
            mp_msg(MSGT_GPLAYER, MSGL_DBG4, "[SKIN] [FONT] id  \"%s\" name \"%s\"\n", skin->fonts[id]->name, skin->fonts[id]->id);
#endif
        }
        else
            skin->addwidget(skin, mywindow, desc);
    }

    free(desc);
    free(filename);
    free(tmp);
    fclose(fp);
    loadfonts(skin);
    mp_msg(MSGT_GPLAYER, MSGL_V, "[SKIN LOAD] loaded skin \"%s\"\n", skin->skindir);
    /* dumpwidgets(skin); */
    return skin;
}
