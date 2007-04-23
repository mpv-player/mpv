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

#include <windows.h>
#include <interface.h>
#include <m_option.h>
#include <mixer.h>
#include <mp_msg.h>
#include <help_mp.h>
#include <codec-cfg.h>
#include <stream/stream.h>
#include <libmpdemux/demuxer.h>
#include <libmpdemux/stheader.h>
#ifdef USE_DVDREAD
#include <stream/stream_dvd.h>
#endif
#include <input/input.h>
#include <libvo/video_out.h>
#include <libao2/audio_out.h>
#include <access_mpcontext.h>
#include "gui.h"
#include "dialogs.h"
#include "wincfg.h"
#ifdef HAVE_LIBCDIO
#include <cdio/cdio.h>
#endif

extern m_obj_settings_t *vf_settings;
extern void exit_player(const char *how);
extern char *filename;
extern int abs_seek_pos;
extern float rel_seek_secs;
extern int audio_id;
extern int video_id;
extern int dvdsub_id;
extern int vobsub_id;
extern int stream_cache_size;
extern int autosync;
extern int vcd_track;
extern int dvd_title;
extern float force_fps;
extern af_cfg_t af_cfg;
int guiWinID = 0;

char *skinName = NULL;
char *codecname = NULL;
int mplGotoTheNext = 1;
static gui_t *mygui = NULL;
static int update_subwindow(void);
static RECT old_rect;
static DWORD style;
ao_functions_t *audio_out = NULL;
vo_functions_t *video_out = NULL;
mixer_t *mixer = NULL;

/* test for playlist files, no need to specify -playlist on the commandline.
 * add any conceivable playlist extensions here.
 * - Erik
 */
int parse_filename(char *file, play_tree_t *playtree, m_config_t *mconfig, int clear)
{
    if(clear)
        mygui->playlist->clear_playlist(mygui->playlist);

    if(strstr(file, ".m3u") || strstr(file, ".pls"))
    {
        playtree = parse_playlist_file(file);
        import_playtree_playlist_into_gui(playtree, mconfig);
        return 1;
    }
    return 0;
}

/**
 * \brief this actually creates a new list containing only one element...
 */
void gaddlist( char ***list, const char *entry)
{
    int i;

    if (*list)
    {
        for (i=0; (*list)[i]; i++) free((*list)[i]);
        free(*list);
    }

    *list = malloc(2 * sizeof(char **));
    (*list)[0] = gstrdup(entry);
    (*list)[1] = NULL;
}

char *gstrdup(const char *str)
{
    if (!str) return NULL;
    return strdup(str);
}

/**
 * \brief this replaces a string starting with search by replace.
 * If not found, replace is appended.
 */
void greplace(char ***list, char *search, char *replace)
{
    int i = 0;
    int len = (search) ? strlen(search) : 0;

    if (*list)
    {
        for (i = 0; (*list)[i]; i++)
        {
            if (search && (!strncmp((*list)[i], search, len)))
            {
                free((*list)[i]);
                (*list)[i] = gstrdup(replace);
                return;
            }
        }
    *list = realloc(*list, (i + 2) * sizeof(char *));
    }
    else
        *list = malloc(2 * sizeof(char *));

    (*list)[i] = gstrdup(replace);
    (*list)[i + 1] = NULL;
}

/* this function gets called by the gui to update mplayer */
static void guiSetEvent(int event)
{
    if(guiIntfStruct.mpcontext)
        mixer = mpctx_get_mixer(guiIntfStruct.mpcontext);

    switch(event)
    {
        case evPlay:
        case evPlaySwitchToPause:
            mplPlay();
            break;
        case evPause:
            mplPause();
            break;
#ifdef USE_DVDREAD
        case evPlayDVD:
        {
            static char dvdname[MAX_PATH];
            guiIntfStruct.DVD.current_title = dvd_title;
            guiIntfStruct.DVD.current_chapter = dvd_chapter;
            guiIntfStruct.DVD.current_angle = dvd_angle;
            guiIntfStruct.DiskChanged = 1;

            mplSetFileName(NULL, dvd_device, STREAMTYPE_DVD);
            dvdname[0] = 0;
            strcat(dvdname, "DVD Movie");
            GetVolumeInformation(dvd_device, dvdname, MAX_PATH, NULL, NULL, NULL, NULL, 0);
            capitalize(dvdname);
            mp_msg(MSGT_GPLAYER, MSGL_V, "Opening DVD %s -> %s\n", dvd_device, dvdname);
            guiGetEvent(guiSetParameters, (char *) STREAMTYPE_DVD);
            mygui->playlist->clear_playlist(mygui->playlist);
            mygui->playlist->add_track(mygui->playlist, filename, NULL, dvdname, 0);
            mygui->startplay(mygui);
            break;
        }
#endif
#ifdef HAVE_LIBCDIO
        case evPlayCD:
        {
            int i;
            char track[10];
            char trackname[10];
            CdIo_t *p_cdio = cdio_open(NULL, DRIVER_UNKNOWN);
            track_t i_tracks;

            if(p_cdio == NULL) printf("Couldn't find a driver.\n");
            i_tracks = cdio_get_num_tracks(p_cdio);

            mygui->playlist->clear_playlist(mygui->playlist);
            for(i=0;i<i_tracks;i++)
            {
                sprintf(track, "cdda://%d", i+1);
                sprintf(trackname, "Track %d", i+1);
                mygui->playlist->add_track(mygui->playlist, track, NULL, trackname, 0);
            }
            cdio_destroy(p_cdio);
            mygui->startplay(mygui);
            break;
        }
#endif
        case evFullScreen:
            mp_input_queue_cmd(mp_input_parse_cmd("vo_fullscreen"));
            break;
        case evExit:
        {
            /* We are asking mplayer to exit, later it will ask us after uninit is made
               this should be the only safe way to quit */
            mygui->activewidget = NULL;
            mp_input_queue_cmd(mp_input_parse_cmd("quit"));
            break;
        }
        case evStop:
            if(guiIntfStruct.Playing)
                guiGetEvent(guiCEvent, (void *) guiSetStop);
            break;
        case evSetMoviePosition:
        {
            rel_seek_secs = guiIntfStruct.Position / 100.0f;
            abs_seek_pos = 3;
            break;
        }
        case evForward10sec:
        {
            rel_seek_secs = 10.0f;
            abs_seek_pos = 0;
            break;
        }
        case evBackward10sec:
        {
            rel_seek_secs = -10.0f;
            abs_seek_pos = 0;
            break;
        }
        case evSetBalance:
        case evSetVolume:
        {
            float l,r;

            if (guiIntfStruct.Playing == 0)
                break;

            if (guiIntfStruct.Balance == 50.0f)
                mixer_setvolume(mixer, guiIntfStruct.Volume, guiIntfStruct.Volume);

            l = guiIntfStruct.Volume * ((100.0f - guiIntfStruct.Balance) / 50.0f);
            r = guiIntfStruct.Volume * ((guiIntfStruct.Balance) / 50.0f);

            if (l > guiIntfStruct.Volume) l=guiIntfStruct.Volume;
            if (r > guiIntfStruct.Volume) r=guiIntfStruct.Volume;
            mixer_setvolume(mixer, l, r);
            /* Check for balance support on mixer - there is a better way ?? */
            if (r != l)
            {
                mixer_getvolume(mixer, &l, &r);
                if (r == l)
                {
                    mp_msg(MSGT_GPLAYER, MSGL_V, "[GUI] Mixer doesn't support balanced audio\n");
                    mixer_setvolume(mixer, guiIntfStruct.Volume, guiIntfStruct.Volume);
                    guiIntfStruct.Balance = 50.0f;
                }
            }
            break;
        }
        case evMute:
        {
            mp_cmd_t * cmd = calloc(1, sizeof(*cmd));
            cmd->id=MP_CMD_MUTE;
            cmd->name=strdup("mute");
            mp_input_queue_cmd(cmd);
            break;
        }
        case evDropFile:
        case evLoadPlay:
        {
            switch(guiIntfStruct.StreamType)
            {
                case STREAMTYPE_DVD:
                {
                    guiIntfStruct.Title = guiIntfStruct.DVD.current_title;
                    guiIntfStruct.Chapter = guiIntfStruct.DVD.current_chapter;
                    guiIntfStruct.Angle = guiIntfStruct.DVD.current_angle;
                    guiIntfStruct.DiskChanged = 1;
                    guiGetEvent(guiCEvent, (void *) guiSetPlay);
                    break;
                }
                default:
                {
                    guiIntfStruct.FilenameChanged = guiIntfStruct.NewPlay = 1;
                    update_playlistwindow();
                    mplGotoTheNext = guiIntfStruct.Playing? 0 : 1;
                    guiGetEvent(guiCEvent, (void *) guiSetStop);
                    guiGetEvent(guiCEvent, (void *) guiSetPlay);
                    break;
               }
           }
           break;
        }
        case evNext:
            mplNext();
            break;
        case evPrev:
            mplPrev();
            break;
    }
}

void mplPlay( void )
{
   if((!guiIntfStruct.Filename ) || (guiIntfStruct.Filename[0] == 0))
     return;

   if(guiIntfStruct.Playing > 0)
   {
       mplPause();
       return;
   }
   guiIntfStruct.NewPlay = 1;
   guiGetEvent(guiCEvent, (void *) guiSetPlay);
}

void mplPause( void )
{
   if(!guiIntfStruct.Playing) return;

   if(guiIntfStruct.Playing == 1)
   {
       mp_cmd_t * cmd = calloc(1, sizeof(*cmd));
       cmd->id=MP_CMD_PAUSE;
       cmd->name=strdup("pause");
       mp_input_queue_cmd(cmd);
   } else guiIntfStruct.Playing = 1;
}

void mplNext(void)
{
    if(guiIntfStruct.Playing == 2) return;
    switch(guiIntfStruct.StreamType)
    {
#ifdef USE_DVDREAD
        case STREAMTYPE_DVD:
            if(guiIntfStruct.DVD.current_chapter == (guiIntfStruct.DVD.chapters - 1))
                return;
            guiIntfStruct.DVD.current_chapter++;
            break;
#endif
        default:
            if(mygui->playlist->current == (mygui->playlist->trackcount - 1))
                return;
            mplSetFileName(NULL, mygui->playlist->tracks[(mygui->playlist->current)++]->filename,
                           STREAMTYPE_STREAM);
            break;
    }
    mygui->startplay(mygui);
}

void mplPrev(void)
{
    if(guiIntfStruct.Playing == 2) return;
    switch(guiIntfStruct.StreamType)
    {
#ifdef USE_DVDREAD
        case STREAMTYPE_DVD:
            if(guiIntfStruct.DVD.current_chapter == 1)
                return;
            guiIntfStruct.DVD.current_chapter--;
            break;
#endif
        default:
            if(mygui->playlist->current == 0)
                return;
            mplSetFileName(NULL, mygui->playlist->tracks[(mygui->playlist->current)--]->filename,
                           STREAMTYPE_STREAM);
            break;
    }
    mygui->startplay(mygui);
}

void mplEnd( void )
{
    if(!mplGotoTheNext && guiIntfStruct.Playing)
    {
        mplGotoTheNext = 1;
        return;
    }

    if(mplGotoTheNext && guiIntfStruct.Playing &&
      (mygui->playlist->current < (mygui->playlist->trackcount - 1)) &&
      guiIntfStruct.StreamType != STREAMTYPE_DVD &&
      guiIntfStruct.StreamType != STREAMTYPE_DVDNAV)
    {
        /* we've finished this file, reset the aspect */
        if(movie_aspect >= 0)
            movie_aspect = -1;

        mplGotoTheNext = guiIntfStruct.FilenameChanged = guiIntfStruct.NewPlay = 1;
        mplSetFileName(NULL, mygui->playlist->tracks[(mygui->playlist->current)++]->filename, STREAMTYPE_STREAM);
        //sprintf(guiIntfStruct.Filename, mygui->playlist->tracks[(mygui->playlist->current)++]->filename);
    }

    if(guiIntfStruct.FilenameChanged && guiIntfStruct.NewPlay)
        return;

    guiIntfStruct.TimeSec = 0;
    guiIntfStruct.Position = 0;
    guiIntfStruct.AudioType = 0;

#ifdef USE_DVDREAD
    guiIntfStruct.DVD.current_title = 1;
    guiIntfStruct.DVD.current_chapter = 1;
    guiIntfStruct.DVD.current_angle = 1;
#endif

    if (mygui->playlist->current == (mygui->playlist->trackcount - 1))
        mygui->playlist->current = 0;

    fullscreen = 0;
    if(style == WS_VISIBLE | WS_POPUP)
    {
        style = WS_OVERLAPPEDWINDOW | WS_SIZEBOX;
        SetWindowLong(mygui->subwindow, GWL_STYLE, style);
    }
    guiGetEvent(guiCEvent, (void *) guiSetStop);
}

void mplSetFileName(char *dir, char *name, int type)
{
    if(!name) return;
    if(!dir)
        guiSetFilename(guiIntfStruct.Filename, name)
    else
        guiSetDF(guiIntfStruct.Filename, dir, name);

    guiIntfStruct.StreamType = type;
    free((void **) &guiIntfStruct.AudioFile);
    free((void **) &guiIntfStruct.Subtitlename);
}

void mplFullScreen( void )
{
    if(!guiIntfStruct.sh_video) return;

    if(sub_window)
    {
        if(!fullscreen && IsWindowVisible(mygui->subwindow) && !IsIconic(mygui->subwindow))
           GetWindowRect(mygui->subwindow, &old_rect);

        if(fullscreen)
        {
            fullscreen = 0;
            style = WS_OVERLAPPEDWINDOW | WS_SIZEBOX;
        } else {
            fullscreen = 1;
            style = WS_VISIBLE | WS_POPUP;
        }
        SetWindowLong(mygui->subwindow, GWL_STYLE, style);
        update_subwindow();
    }
    video_out->control(VOCTRL_FULLSCREEN, 0);
    if(sub_window) ShowWindow(mygui->subwindow, SW_SHOW);
}

static DWORD WINAPI GuiThread(void)
{
    MSG msg;

    if(!skinName) skinName = strdup("Blue");
    if(!mygui) mygui = create_gui(get_path("skins"), skinName, guiSetEvent);
    if(!mygui) exit_player("Unable to load gui");

    if(autosync && autosync != gtkAutoSync)
    {
       gtkAutoSyncOn = 1;
       gtkAutoSync = autosync;
    }

    while(mygui)
    {
        GetMessage(&msg, NULL, 0, 0);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    fprintf(stderr, "[GUI] Gui Thread Terminated\n");
    fflush(stderr);
    return 0;
}

void guiInit(void)
{
    DWORD threadId;
    memset(&guiIntfStruct, 0, sizeof(guiIntfStruct));
    /* Create The gui thread */
    if (!mygui)
    {
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) GuiThread, NULL, 0, &threadId);
        mp_msg(MSGT_GPLAYER, MSGL_V, "[GUI] Creating GUI Thread 0x%04x\n", threadId);
    }

    /* Wait until the gui is created */
    while(!mygui) Sleep(100);
    mp_msg(MSGT_GPLAYER, MSGL_V, "[GUI] Gui Thread started\n");
}

void guiDone(void)
{
    if(mygui)
    {
        fprintf(stderr, "[GUI] Closed by main mplayer window\n");
        fflush(stderr);
        mygui->uninit(mygui);
        free(mygui);
        TerminateThread(GuiThread, 0);
        mygui = NULL;
    }
    /* Remove tray icon */
    Shell_NotifyIcon(NIM_DELETE, &nid);
    cfg_write();
}

/* this function gets called by mplayer to update the gui */
int guiGetEvent(int type, char *arg)
{
    stream_t *stream = (stream_t *) arg;
#ifdef USE_DVDREAD
    dvd_priv_t *dvdp = (dvd_priv_t *) arg;
#endif
    if(!mygui || !mygui->skin) return 0;

    if(guiIntfStruct.mpcontext)
    {
        audio_out = mpctx_get_audio_out(guiIntfStruct.mpcontext);
        video_out = mpctx_get_video_out(guiIntfStruct.mpcontext);
        mixer = mpctx_get_mixer(guiIntfStruct.mpcontext);
        playtree = mpctx_get_playtree_iter(guiIntfStruct.mpcontext);
    }

    switch (type)
    {
        case guiSetFileFormat:
            guiIntfStruct.FileFormat = (int) arg;
            break;
        case guiSetParameters:
        {
            guiGetEvent(guiSetDefaults, NULL);
            guiIntfStruct.DiskChanged = 0;
            guiIntfStruct.FilenameChanged = 0;
            guiIntfStruct.NewPlay = 0;
            switch(guiIntfStruct.StreamType)
            {
                case STREAMTYPE_PLAYLIST:
                    break;
#ifdef USE_DVDREAD
                case STREAMTYPE_DVD:
                {
                    char tmp[512];
                    dvd_title = guiIntfStruct.DVD.current_title;
                    dvd_chapter = guiIntfStruct.DVD.current_chapter;
                    dvd_angle = guiIntfStruct.DVD.current_angle;
                    sprintf(tmp,"dvd://%d", guiIntfStruct.Title);
                    guiSetFilename(guiIntfStruct.Filename, tmp);
                    break;
                }
#endif
            }
            if(guiIntfStruct.Filename)
                filename = strdup(guiIntfStruct.Filename);
            else if(filename)
                strcpy(guiIntfStruct.Filename, filename);
            break;
        }
        case guiSetAudioOnly:
        {
            guiIntfStruct.AudioOnly = (int) arg;
            if(IsWindowVisible(mygui->subwindow))
                ShowWindow(mygui->subwindow, SW_HIDE);
            break;
        }
        case guiSetContext:
            guiIntfStruct.mpcontext = (void *) arg;
            break;
        case guiSetDemuxer:
            guiIntfStruct.demuxer = (void *) arg;
            break;
        case guiSetValues:
        {
            guiIntfStruct.sh_video = arg;
            if (arg)
            {
                sh_video_t *sh = (sh_video_t *)arg;
                codecname = sh->codec->name;
                guiIntfStruct.FPS = sh->fps;

                /* we have video, show the subwindow */
                if(!IsWindowVisible(mygui->subwindow) || IsIconic(mygui->subwindow))
                    ShowWindow(mygui->subwindow, SW_SHOWNORMAL);
                if(WinID == -1)
                    update_subwindow();

            }
            break;
        }
        case guiSetShVideo:
        {
            guiIntfStruct.MovieWidth = vo_dwidth;
            guiIntfStruct.MovieHeight = vo_dheight;

            sub_aspect = (float)guiIntfStruct.MovieWidth/guiIntfStruct.MovieHeight;
            if(WinID != -1)
               update_subwindow();
            break;
        }
        case guiSetStream:
        {
            guiIntfStruct.StreamType = stream->type;
            switch(stream->type)
            {
#ifdef USE_DVDREAD
                case STREAMTYPE_DVD:
                    guiGetEvent(guiSetDVD, (char *) stream->priv);
                    break;
#endif
            }
            break;
        }
#ifdef USE_DVDREAD
        case guiSetDVD:
        {
            guiIntfStruct.DVD.titles = dvdp->vmg_file->tt_srpt->nr_of_srpts;
            guiIntfStruct.DVD.chapters = dvdp->vmg_file->tt_srpt->title[dvd_title].nr_of_ptts;
            guiIntfStruct.DVD.angles = dvdp->vmg_file->tt_srpt->title[dvd_title].nr_of_angles;
            guiIntfStruct.DVD.nr_of_audio_channels = dvdp->nr_of_channels;
            memcpy(guiIntfStruct.DVD.audio_streams, dvdp->audio_streams, sizeof(dvdp->audio_streams));
            guiIntfStruct.DVD.nr_of_subtitles = dvdp->nr_of_subtitles;
            memcpy(guiIntfStruct.DVD.subtitles, dvdp->subtitles, sizeof(dvdp->subtitles));
            guiIntfStruct.DVD.current_title = dvd_title + 1;
            guiIntfStruct.DVD.current_chapter = dvd_chapter + 1;
            guiIntfStruct.DVD.current_angle = dvd_angle + 1;
            guiIntfStruct.Track = dvd_title + 1;
            break;
        }
#endif
        case guiReDraw:
            mygui->updatedisplay(mygui, mygui->mainwindow);
            break;
        case guiSetAfilter:
            guiIntfStruct.afilter = (void *) arg;
            break;
        case guiCEvent:
        {
            guiIntfStruct.Playing = (int) arg;
            switch (guiIntfStruct.Playing)
            {
                case guiSetPlay:
                {
                    guiIntfStruct.Playing = 1;
                    break;
                }
                case guiSetStop:
                {
                    guiIntfStruct.Playing = 0;
                    if(movie_aspect >= 0)
                        movie_aspect = -1;
                    update_subwindow();
                    break;
                }
                case guiSetPause:
                    guiIntfStruct.Playing = 2;
                    break;
            }
            break;
        }
        case guiIEvent:
        {
            mp_msg(MSGT_GPLAYER,MSGL_V, "cmd: %d\n", (int) arg);
            /* MPlayer asks us to quit */
            switch((int) arg)
            {
                case MP_CMD_GUI_FULLSCREEN:
                    mplFullScreen();
                    break;
                case MP_CMD_QUIT:
                {
                    mygui->uninit(mygui);
                    free(mygui);
                    mygui = NULL;
                    exit_player("Done");
                    return 0;
                }
                case MP_CMD_GUI_STOP:
                    guiGetEvent(guiCEvent, (void *) guiSetStop);
                    break;
                case MP_CMD_GUI_PLAY:
                    guiGetEvent(guiCEvent, (void *) guiSetPlay);
                    break;
                case MP_CMD_GUI_SKINBROWSER:
                    if(fullscreen) guiSetEvent(evFullScreen);
                    PostMessage(mygui->mainwindow, WM_COMMAND, (WPARAM) ID_SKINBROWSER, 0);
                    break;
                case MP_CMD_GUI_PLAYLIST:
                    if(fullscreen) guiSetEvent(evFullScreen);
                    PostMessage(mygui->mainwindow, WM_COMMAND, (WPARAM) ID_PLAYLIST, 0);
                    break;
                case MP_CMD_GUI_PREFERENCES:
                    if(fullscreen) guiSetEvent(evFullScreen);
                    PostMessage(mygui->mainwindow, WM_COMMAND, (WPARAM) ID_PREFS, 0);
                    break;
                case MP_CMD_GUI_LOADFILE:
                    if(fullscreen) guiSetEvent(evFullScreen);
                    PostMessage(mygui->mainwindow, WM_COMMAND, (WPARAM) IDFILE_OPEN, 0);
                    break;
                case MP_CMD_GUI_LOADSUBTITLE:
                    if(fullscreen) guiSetEvent(evFullScreen);
                    PostMessage(mygui->mainwindow, WM_COMMAND, (WPARAM) IDSUBTITLE_OPEN, 0);
                    break;
                default:
                    break;
            }
            break;
        }
        case guiSetFileName:
            if (arg) guiIntfStruct.Filename = (char *) arg;
            break;
        case guiSetDefaults:
        {
            audio_id = -1;
            video_id = -1;
            dvdsub_id = -1;
            vobsub_id = -1;
            stream_cache_size = -1;
            autosync = 0;
            vcd_track = 0;
            dvd_title = 0;
            force_fps = 0;
            if(!mygui->playlist->tracks) return 0;
            filename = guiIntfStruct.Filename = mygui->playlist->tracks[mygui->playlist->current]->filename;
            guiIntfStruct.Track = mygui->playlist->current + 1;
            if(gtkAONorm) greplace(&af_cfg.list, "volnorm", "volnorm");
            if(gtkAOExtraStereo)
            {
                char *name = malloc(12 + 20 + 1);
                snprintf(name, 12 + 20, "extrastereo=%f", gtkAOExtraStereoMul);
                name[12 + 20] = 0;
                greplace(&af_cfg.list, "extrastereo", name);
                free(name);
            }
            if(gtkCacheOn) stream_cache_size = gtkCacheSize;
            if(gtkAutoSyncOn) autosync = gtkAutoSync;
            break;
        }
        case guiSetVolume:
        {
            if(audio_out)
            {
                /* Some audio_out drivers do not support balance e.g. dsound */
                /* FIXME this algo is not correct */
                float l, r;
                mixer_getvolume(mixer, &l, &r);
                guiIntfStruct.Volume = (r > l ? r : l); /* max(r,l) */
                if (r != l)
                    guiIntfStruct.Balance = ((r-l) + 100.0f) * 0.5f;
                else
                    guiIntfStruct.Balance = 50.0f;
            }
            break;
        }
        default:
            mp_msg(MSGT_GPLAYER, MSGL_ERR, "[GUI] GOT UNHANDLED EVENT %i\n", type);
    }
    return 0;
}

/* This function adds/inserts one file into the gui playlist */
int import_file_into_gui(char *pathname, int insert)
{
    char filename[MAX_PATH];
    char *filepart = filename;

    if (strstr(pathname, "://"))
    {
        mp_msg(MSGT_GPLAYER, MSGL_V, "[GUI] Adding special %s\n", pathname);
        mygui->playlist->add_track(mygui->playlist, pathname, NULL, NULL, 0);
        return 1;
    }
    if (GetFullPathName(pathname, MAX_PATH, filename, &filepart))
    {
        if (!(GetFileAttributes(filename) & FILE_ATTRIBUTE_DIRECTORY))
        {
            mp_msg(MSGT_GPLAYER, MSGL_V, "[GUI] Adding filename: %s - fullpath: %s\n", filepart, filename);
            mygui->playlist->add_track(mygui->playlist, filename, NULL, filepart, 0);
            return 1;
        }
        else
            mp_msg(MSGT_GPLAYER, MSGL_V, "[GUI] Cannot add %s\n", filename);
    }

    return 0;
}

/*  This function imports the initial playtree (based on cmd-line files) into the gui playlist
    by either:
    - overwriting gui pl (enqueue=0) */

int import_initial_playtree_into_gui(play_tree_t *my_playtree, m_config_t *config, int enqueue)
{
    play_tree_iter_t *my_pt_iter = NULL;
    int result = 0;

    if(!mygui) guiInit();

    if((my_pt_iter = pt_iter_create(&my_playtree, config)))
    {
        while ((filename = pt_iter_get_next_file(my_pt_iter)) != NULL)
        {
            if (parse_filename(filename, my_playtree, config, 0))
                result = 1;
            else if (import_file_into_gui(filename, 0)) /* Add it to end of list */
                result = 1;
        }
    }
    mplGotoTheNext = 1;

    if (result)
    {
        mygui->playlist->current = 0;
        filename = mygui->playlist->tracks[0]->filename;
    }
    return result;
}

/* This function imports and inserts an playtree, that is created "on the fly", for example by
   parsing some MOV-Reference-File; or by loading an playlist with "File Open"
   The file which contained the playlist is thereby replaced with it's contents. */

int import_playtree_playlist_into_gui(play_tree_t *my_playtree, m_config_t *config)
{
    play_tree_iter_t *my_pt_iter = NULL;
    int result = 0;

    if((my_pt_iter = pt_iter_create(&my_playtree, config)))
    {
        while ((filename = pt_iter_get_next_file(my_pt_iter)) != NULL)
            if (import_file_into_gui(filename, 1)) /* insert it into the list and set plCurrent = new item */
                result = 1;
        pt_iter_destroy(&my_pt_iter);
    }
    filename = NULL;
    return result;
}

inline void gtkMessageBox(int type, const char *str)
{
    if (type & GTK_MB_FATAL)
        MessageBox(NULL, str, "MPlayer GUI for Windows Error", MB_OK | MB_ICONERROR);

    fprintf(stderr, "[GUI] MessageBox: %s\n", str);
    fflush(stderr);
}

void guiMessageBox(int level, char *str)
{
    switch(level)
    {
        case MSGL_FATAL:
            gtkMessageBox(GTK_MB_FATAL | GTK_MB_SIMPLE, str);
            break;
        case MSGL_ERR:
            gtkMessageBox(GTK_MB_ERROR | GTK_MB_SIMPLE, str);
            break;
    }
}

static int update_subwindow(void)
{
    int x,y;
    RECT rd;
    WINDOWPOS wp;

    if(!sub_window)
    {
        WinID = -1; // so far only directx supports WinID in windows

        if(IsWindowVisible(mygui->subwindow) && guiIntfStruct.sh_video && guiIntfStruct.Playing)
        {
            ShowWindow(mygui->subwindow, SW_HIDE);
            return 0;
        }
        else if(guiIntfStruct.AudioOnly)
            return 0;
        else ShowWindow(mygui->subwindow, SW_SHOW);
    }

    /* we've come out of fullscreen at the end of file */
    if((!IsWindowVisible(mygui->subwindow) || IsIconic(mygui->subwindow)) && !guiIntfStruct.AudioOnly)
        ShowWindow(mygui->subwindow, SW_SHOWNORMAL);

    /* get our current window coordinates */
    GetWindowRect(mygui->subwindow, &rd);

    x = rd.left;
    y = rd.top;

    /* restore sub window position when coming out of fullscreen */
    if(x <= 0) x = old_rect.left;
    if(y <= 0) y = old_rect.top;

    if(!guiIntfStruct.Playing)
    {
        window *desc = NULL;
        int i;

        for (i=0; i<mygui->skin->windowcount; i++)
            if(mygui->skin->windows[i]->type == wiSub)
                desc = mygui->skin->windows[i];

        rd.right = rd.left+desc->base->bitmap[0]->width;
        rd.bottom = rd.top+desc->base->bitmap[0]->height;
        sub_aspect = (float)(rd.right-rd.left)/(rd.bottom-rd.top);
    }
    else
    {
        rd.right = rd.left+guiIntfStruct.MovieWidth;
        rd.bottom = rd.top+guiIntfStruct.MovieHeight;

        if (movie_aspect > 0.0)       // forced aspect from the cmdline
            sub_aspect = movie_aspect;
    }


    AdjustWindowRect(&rd, WS_OVERLAPPEDWINDOW | WS_SIZEBOX, 0);
    SetWindowPos(mygui->subwindow, 0, x, y, rd.right-rd.left, rd.bottom-rd.top, SWP_NOOWNERZORDER);

    wp.hwnd = mygui->subwindow;
    wp.x = rd.left;
    wp.y = rd.top;
    wp.cx = rd.right-rd.left;
    wp.cy = rd.bottom-rd.top;
    wp.flags = SWP_NOOWNERZORDER | SWP_SHOWWINDOW;

    /* erase the bitmap image if there's video */
    if(guiIntfStruct.Playing != 0 && guiIntfStruct.sh_video)
        SendMessage(mygui->subwindow, WM_ERASEBKGND, (WPARAM)GetDC(mygui->subwindow), 0);

    /* reset the window aspect */
    SendMessage(mygui->subwindow, WM_WINDOWPOSCHANGED, 0, (LPARAM)&wp);
    return 0;
}

void guiEventHandling(void) {}
