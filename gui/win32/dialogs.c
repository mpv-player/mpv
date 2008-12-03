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

#include <windows.h>
#include <commctrl.h>
#include <get_path.h>
#include "gui/interface.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "gui.h"
#include "dialogs.h"
#include "libvo/sub.h"

WNDPROC OldUrlWndProc;
LRESULT CALLBACK SubUrlWndProc(HWND, UINT, WPARAM, LPARAM);
extern int vo_gamma_brightness;
extern int vo_gamma_saturation;
extern int vo_gamma_contrast;
extern int vo_gamma_hue;
int set_video_colors(sh_video_t *sh_video, const char *item, int value);
int get_video_colors(sh_video_t *sh_video, const char *item, int *value);

guiInterface_t guiIntfStruct;
int addurl = 0;

extern mp_osd_obj_t* vo_osd_list;
extern char **sub_name;

void guiLoadSubtitle(char *name)
{
    if (!guiIntfStruct.Playing)
    {
        guiIntfStruct.SubtitleChanged = 1;
        return;
    }
    if (subdata)
    {
        mp_msg(MSGT_GPLAYER, MSGL_INFO, MSGTR_DeletingSubtitles);
        sub_free(subdata);
        subdata = NULL;
        vo_sub = NULL;
        if (vo_osd_list)
        {
            int len;
            mp_osd_obj_t *osd = vo_osd_list;
            while (osd)
            {
                if (osd->type == OSDTYPE_SUBTITLE) break;
                osd = osd->next;
            }
            if (osd && osd->flags & OSDFLAG_VISIBLE)
            {
                len = osd->stride * (osd->bbox.y2 - osd->bbox.y1);
                memset(osd->bitmap_buffer, 0, len);
                memset(osd->alpha_buffer, 0, len);
            }
        }
    }

    if (name)
    {
        mp_msg(MSGT_GPLAYER, MSGL_INFO, MSGTR_LoadingSubtitles, name);
        subdata = sub_read_file(strdup(name), guiIntfStruct.FPS);
        if (!subdata) mp_msg(MSGT_GPLAYER, MSGL_ERR, MSGTR_CantLoadSub,name);
        sub_name = (malloc(2 * sizeof(char*))); /* when mplayer will be restarted */
        sub_name[0] = strdup(name);               /* sub_name[0] will be read */
        sub_name[1] = NULL;
    }
    update_set_of_subtitles();
}

int display_openfilewindow(gui_t *gui, int add)
{
    OPENFILENAME fileopen;
    int result = 0;
    char filelist[MAXFILE];
    char filename[MAX_PATH];
    char directory[MAX_PATH];
    char *filespec = NULL;
    char *filepart = NULL;

    memset(&fileopen, 0, sizeof(OPENFILENAME));
    memset(filelist, 0, sizeof(filelist));

    fileopen.lStructSize = sizeof(OPENFILENAME);
    fileopen.hwndOwner = gui->mainwindow;
    fileopen.hInstance = GetModuleHandle(NULL);
    fileopen.lpstrFilter = "All Files (*.*)\0*.*\0"
                           "Media Files (*.avi;*.asf;*.wmv;*.mpg;*.mpeg;*.m2v;*.mov;\
                                         *.rmvb;*.rm;*.ogm;*.mp3;*.wav;*.wma;*.ra;*.ogg)\0\
                                         *.avi;*.asf;*.wmv;*.mpg;*.mpeg;*.m2v;*.mov;\
                                         *.rmvb;*.rm;*.ogm;*.mp3;*.wav;*.wma;*.ra;*.ogg\0"
                           "Video Files (*.avi;*.mpg;*.mpeg;*.mov)\0*.avi;*.mpg;*.mpeg;*.mov\0"
                           "Avisynth Scripts (*.avs)\0*.avs\0"
                           "Audio Files (*.mp3;*.wav;*.ra)\0*.mp3;*.wav;*.ra\000";
    fileopen.nFilterIndex = 0;
    fileopen.lpstrTitle = "Add file(s)...";
    fileopen.Flags = OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST| OFN_LONGNAMES | OFN_EXPLORER| OFN_READONLY | OFN_HIDEREADONLY;
    fileopen.lpstrFile = filelist;
    fileopen.lpstrCustomFilter = NULL;
    fileopen.nMaxFile = MAXFILE;

    if(GetOpenFileName(&fileopen))
    {
        /* clear playlist */
        if(!add) gui->playlist->clear_playlist(gui->playlist);

        memcpy(directory, fileopen.lpstrFile, fileopen.nFileOffset - 1);
        directory[fileopen.nFileOffset - 1] = 0;

        do
        {
            filespec = &fileopen.lpstrFile[fileopen.nFileOffset];
            filename[0] = 0;
            strcat(filename, directory);
            strcat(filename, "\\");
            strcat(filename, filespec);

            if (GetFileAttributes(filename) & FILE_ATTRIBUTE_DIRECTORY)
                mp_msg(MSGT_GPLAYER, MSGL_V, "[GUI] %s is a directory, skipping...\n", filename);
            else
            {
                if (GetFullPathName(filename, MAX_PATH, filename, &filepart))
                {
                    mplSetFileName(NULL, filename, STREAMTYPE_FILE);
                    if(!parse_filename(filename, playtree, mconfig, 0))
                        gui->playlist->add_track(gui->playlist, filename, NULL, filepart, 0);
                    mp_msg(MSGT_GPLAYER, MSGL_V, "[GUI] Adding file: %s - path %s\n", filespec, filename);
                    result++;
                }
            }
            fileopen.nFileOffset += strlen(filespec) + 1;
        } while (*filespec);
    }
    return result;
}

void display_opensubtitlewindow(gui_t *gui)
{
    OPENFILENAME subtitleopen;
    char subtitlefile[MAX_PATH];

    /* Safety check */
    if (guiIntfStruct.Playing == 0 || !guiIntfStruct.sh_video) return;

    memset(&subtitleopen, 0, sizeof(OPENFILENAME));
    memset(subtitlefile, 0, sizeof(subtitlefile));

    subtitleopen.lStructSize = sizeof(OPENFILENAME);
    subtitleopen.hwndOwner = gui->mainwindow;
    subtitleopen.hInstance = GetModuleHandle(NULL);
    subtitleopen.lpstrFilter = "All Files (*.*)\0*.*\0"
                               "Subtitle Files (*.srt;*.txt;*.vob)\0*.srt;*.txt;*.vob\0";
    subtitleopen.nFilterIndex = 0;
    subtitleopen.lpstrTitle = "Add Subtitle...";
    subtitleopen.Flags = OFN_FILEMUSTEXIST | OFN_LONGNAMES | OFN_EXPLORER | OFN_READONLY | OFN_HIDEREADONLY;
    subtitleopen.lpstrFile = subtitlefile;
    subtitleopen.lpstrCustomFilter = NULL;
    subtitleopen.nMaxFile = MAXFILE;

    if(GetOpenFileName(&subtitleopen))
        guiLoadSubtitle(subtitlefile);
}

void display_loadplaylistwindow(gui_t *gui)
{
    OPENFILENAME playlistopen;
    char playlistfile[MAX_PATH];

    memset(&playlistopen, 0, sizeof(OPENFILENAME));
    memset(playlistfile, 0, sizeof(playlistfile));

    playlistopen.lStructSize = sizeof(OPENFILENAME);
    playlistopen.hwndOwner = gui->mainwindow;
    playlistopen.hInstance = GetModuleHandle(NULL);
    playlistopen.lpstrFilter = "All Files (*.*)\0*.*\0"
                               "Playlist Files (*.m3u;*.pls;*.txt)\0*.m3u;*.pls;*.txt\0";
    playlistopen.nFilterIndex = 0;
    playlistopen.lpstrTitle = "Load Playlist...";
    playlistopen.Flags = OFN_FILEMUSTEXIST | OFN_LONGNAMES | OFN_EXPLORER | OFN_READONLY | OFN_HIDEREADONLY;
    playlistopen.lpstrFile = playlistfile;
    playlistopen.lpstrCustomFilter = NULL;
    playlistopen.nMaxFile = MAXFILE;

    if(GetOpenFileName(&playlistopen))
    {
        if(parse_filename(playlistfile, playtree, mconfig, 1))
            gui->startplay(gui);
    }
}

void display_saveplaylistwindow(gui_t* gui)
{
    OPENFILENAME playlistsave;
    static FILE *playlist_file = NULL;
    char playlistname[MAX_PATH];

    memset(&playlistsave, 0, sizeof(OPENFILENAME));
    memset(playlistname, 0, sizeof(playlistname));

    playlistsave.lStructSize = sizeof(OPENFILENAME);
    playlistsave.hwndOwner = gui->mainwindow;
    playlistsave.hInstance = GetModuleHandle(NULL);
    playlistsave.lpstrFilter = "Playlist Files (*.pls)\0*.pls\0";
    playlistsave.nFilterIndex = 0;
    playlistsave.lpstrTitle = "Save Playlist...";
    playlistsave.Flags = OFN_LONGNAMES | OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
    playlistsave.lpstrFile = playlistname;
    playlistsave.lpstrCustomFilter = NULL;
    playlistsave.nMaxFile = MAXFILE;

    if(GetSaveFileName(&playlistsave))
    {
        int i=0;
        HANDLE my_playlist;

        if(!strstr(playlistname, ".pls")) strcat(playlistname, ".pls");

        my_playlist = CreateFile(playlistname,
                                 GENERIC_WRITE,
                                 0,
                                 NULL,
                                 CREATE_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                                 NULL);

        if(my_playlist != INVALID_HANDLE_VALUE)
        {
            CloseHandle(my_playlist); /* close the file first so we can write to it */
            playlist_file = fopen(playlistsave.lpstrFile, "w");
            fprintf(playlist_file, "[playlist]\n");
            fprintf(playlist_file, "numberofentries=%d\n", gui->playlist->trackcount);

            for(i=0; i<(gui->playlist->trackcount); i++)
            {
                fprintf(playlist_file, "File%i=%s\n", i + 1, gui->playlist->tracks[i]->filename);
                fprintf(playlist_file, "Length%i=-1\n", i + 1);
            }
            fclose(playlist_file);
        }
    }
}

static LRESULT CALLBACK OpenUrlWndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND url;
    HWND wdg;
    FILE *f;
    char *history = get_path("gui.url");
    gui_t *gui = (gui_t *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (iMsg)
    {
        case WM_CREATE:
            wdg = CreateWindow("button", "Ok",
                               WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               4, 43, 80, 25, hwnd,
                               (HMENU) ID_OK,
                               ((LPCREATESTRUCT) lParam) -> hInstance,
                               NULL);
            SendMessage(wdg, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT), 0);

            wdg = CreateWindow("button", "Cancel",
                               WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               90, 43, 80, 25, hwnd,
                               (HMENU) ID_CANCEL,
                               ((LPCREATESTRUCT) lParam) -> hInstance,
                               NULL);
            SendMessage(wdg, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT), 0);

            url = wdg = CreateWindowEx(WS_EX_CLIENTEDGE,
                               "edit", NULL,
                               WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL,
                               4, 10, 300, 25, hwnd,
                               (HMENU) ID_URL,
                               ((LPCREATESTRUCT) lParam) -> hInstance,
                               NULL);
            SendMessage(wdg, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT), 0);
            SendMessage(wdg, EM_SETLIMITTEXT, MAX_PATH, 0);

            /*subclass the edit box to capture the VK_RETURN key*/
            OldUrlWndProc = (WNDPROC)SetWindowLongPtr(url, GWLP_WNDPROC, (LONG_PTR)SubUrlWndProc);

            if((f = fopen(history, "r")))
            {
               char lasturl[MAX_PATH];
               fgets(lasturl, MAX_PATH, f);
               SendMessage(url, WM_SETTEXT, 0, (LPARAM) lasturl);
               SendMessage(url, EM_SETSEL, 0, -1);
               fclose(f);
            }
            break;
        case WM_KEYDOWN:
            switch (LOWORD(wParam))
            {
                case VK_RETURN:
                    SendMessage(hwnd, WM_COMMAND, (WPARAM) ID_OK, 0);
                    break;
            }
        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case ID_CANCEL:
                    DestroyWindow(hwnd);
                    return 0;
                case ID_OK:
                {
                    char file[MAX_PATH];
                    SendMessage(url, WM_GETTEXT, MAX_PATH, (LPARAM) file);
                    mplSetFileName(NULL, file, STREAMTYPE_STREAM);
                    if((f = fopen(history, "wt+")))
                    {
                        fprintf(f, file);
                        fclose(f);
                    }
                    if(!parse_filename(file, playtree, mconfig, addurl? 0 : 1))
                        gui->playlist->add_track(gui->playlist, file, NULL, NULL, 0);
                    if(!addurl)
                        gui->startplay(gui);
                    else update_playlistwindow();
                    DestroyWindow(hwnd);
                }
                break;
            }
        }
        return 0;
        case WM_DESTROY:
        {
            addurl = 0;
            return 0;
        }
    }
    return DefWindowProc(hwnd, iMsg, wParam, lParam);
}

LRESULT CALLBACK SubUrlWndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    switch(iMsg)
    {
        case WM_KEYDOWN:
            switch (LOWORD(wParam))
            {
                case VK_RETURN:
                    SendMessage(FindWindow(NULL, "MPlayer - Open URL..."), WM_COMMAND, (WPARAM) ID_OK, 0);
                    break;
            }
    }
    return CallWindowProc(OldUrlWndProc, hwnd, iMsg, wParam, lParam);
}

void display_openurlwindow(gui_t *gui, int add)
{
    HWND hWnd;
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASS wc;
    int x, y;

    if(add) addurl = 1;
    if(FindWindow(NULL, "MPlayer - Open URL...")) return;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = OpenUrlWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL,IDC_ARROW);
    wc.hIcon         = gui->icon;
    wc.hbrBackground = SOLID_GREY2;
    wc.lpszClassName = "MPlayer - URL";
    wc.lpszMenuName  = NULL;
    RegisterClass(&wc);
    x = (GetSystemMetrics(SM_CXSCREEN) / 2) - (320 / 2);
    y = (GetSystemMetrics(SM_CYSCREEN) / 2) - (100 / 2);
    hWnd = CreateWindow("MPlayer - URL",
                        "MPlayer - Open URL...",
                        WS_POPUPWINDOW | WS_CAPTION,
                        x,
                        y,
                        320,
                        100,
                        NULL,
                        NULL,
                        hInstance,
                        NULL);
   SetWindowLongPtr(hWnd, GWLP_USERDATA, (DWORD) gui);
   ShowWindow(hWnd, SW_SHOW);
   UpdateWindow(hWnd);
}

static void create_playlistmenu(gui_t *gui)
{
    gui->playlistmenu = CreatePopupMenu();
    AppendMenu(gui->playlistmenu, MF_STRING, ID_ADDFILE, "Add File...");
    AppendMenu(gui->playlistmenu, MF_STRING, ID_ADDURL, "Add Url...");
    AppendMenu(gui->playlistmenu, MF_SEPARATOR, 0, 0);
    AppendMenu(gui->playlistmenu, MF_STRING, ID_REMOVE, "Remove Selected");
    AppendMenu(gui->playlistmenu, MF_STRING, ID_CLEAR, "Clear Playlist");
    AppendMenu(gui->playlistmenu, MF_SEPARATOR, 0, 0);
    AppendMenu(gui->playlistmenu, MF_STRING, ID_CLOSE, "Close");
}

static void updatetracklist(HWND hwnd)
{
    int i=0;
    gui_t *gui = (gui_t *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
    /* clear listbox */
    SendDlgItemMessage(hwnd, ID_TRACKLIST, LB_RESETCONTENT, 0, 0);
    for (i=0; i < gui->playlist->trackcount; i++)
        if (gui->playlist->tracks[i]->title)
            SendDlgItemMessage(hwnd, ID_TRACKLIST, LB_ADDSTRING, 0, (LPARAM)gui->playlist->tracks[i]->title);
        else
            SendDlgItemMessage(hwnd, ID_TRACKLIST, LB_ADDSTRING, 0, (LPARAM)gui->playlist->tracks[i]->filename);
}

static LRESULT CALLBACK PlayListWndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    HWND wdg;
    POINT cursor;
    gui_t *gui = (gui_t *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
    playlist_t *pl = gui ? gui->playlist : NULL;
    switch (iMsg)
    {
        case WM_CREATE:
        {
            wdg = CreateWindow("button", "Play",
                               WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               4, 10, 80, 25, hwnd,
                               (HMENU) ID_PLAY,
                               ((LPCREATESTRUCT) lParam) -> hInstance,
                               NULL);
            SendMessage(wdg, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT), 0);

            wdg = CreateWindow ("button", "Up",
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                4, 37, 80, 25, hwnd,
                                (HMENU) ID_UP,
                                ((LPCREATESTRUCT) lParam) -> hInstance,
                                NULL);
            SendMessage(wdg, WM_SETFONT,(WPARAM) GetStockObject(DEFAULT_GUI_FONT), 0);

            wdg = CreateWindow ("button", "Down",
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                4, 64, 80, 25, hwnd,
                                (HMENU) ID_DOWN,
                                ((LPCREATESTRUCT) lParam) -> hInstance,
                                NULL);
            SendMessage(wdg, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT),0);

            wdg = CreateWindow ("button", "Remove",
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                4, 91, 80, 25, hwnd,
                                (HMENU) ID_REMOVE,
                                ((LPCREATESTRUCT) lParam) -> hInstance,
                                NULL);
            SendMessage(wdg, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT),0);

            wdg = CreateWindow ("button", "Load",
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                4, 118, 80, 25, hwnd,
                                (HMENU) ID_PLAYLISTLOAD,
                                ((LPCREATESTRUCT) lParam) -> hInstance,
                                NULL);
            SendMessage(wdg, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT),0);

            wdg = CreateWindow ("button", "Save",
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                4, 145, 80, 25, hwnd,
                                (HMENU) ID_PLAYLISTSAVE,
                                ((LPCREATESTRUCT) lParam) -> hInstance,
                                NULL);
            SendMessage(wdg, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT),0);

            wdg = CreateWindow ("button", "Close",
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                4, 193, 80, 25, hwnd,
                                (HMENU) ID_CLOSE,
                                ((LPCREATESTRUCT) lParam) -> hInstance,
                                NULL);
            SendMessage(wdg, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT),0);

            wdg = CreateWindow ("listbox", "tracklist", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL |
                               WS_HSCROLL | LBS_DISABLENOSCROLL, 92, 10, 300, 208, hwnd, (HMENU) ID_TRACKLIST,
                               ((LPCREATESTRUCT) lParam) -> hInstance, NULL);
            SendMessage(wdg, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT), 0);
            SendMessage(wdg, LB_SETHORIZONTALEXTENT, MAX_PATH*4, 0);
            break;
        }
        case WM_CONTEXTMENU:
        {
            GetCursorPos(&cursor);
            SetForegroundWindow(hwnd);
            TrackPopupMenu(gui->playlistmenu, 0, cursor.x, cursor.y, 0, hwnd, NULL);
            break;
        }
        case WM_COMMAND:
        {
            int selected = 0;
            int i;
            for (i=0; i<pl->trackcount; i++)
                if(0 < SendDlgItemMessage(hwnd, ID_TRACKLIST, LB_GETSEL, i, 0)) selected = i + 1;
            switch (LOWORD(wParam))
            {
                case ID_CLOSE:
                    DestroyWindow(hwnd);
                    return 0;
                case ID_TRACKLIST:
                    if(HIWORD(wParam) == LBN_DBLCLK)
                    {
                case ID_PLAY:
                        if(selected) pl->current = selected - 1;
                        mplSetFileName(NULL, pl->tracks[pl->current]->filename, STREAMTYPE_STREAM);
                        gui->startplay(gui);
                    }
                    return 0;
                case ID_UP:
                {
                    if(selected) pl->moveup_track(pl, selected);
                    selected--;
                    break;
                }
                case ID_DOWN:
                {
                    if(selected) pl->movedown_track(pl, selected);
                    selected++;
                    break;
                }
                case ID_REMOVE:
                    if(selected) pl->remove_track(pl, selected);
                    break;
                case ID_ADDFILE:
                {
                    if(guiIntfStruct.StreamType == STREAMTYPE_DVD ||
                       guiIntfStruct.StreamType == STREAMTYPE_DVDNAV) return 0;
                    display_openfilewindow(gui, 1);
                    break;
                }
                case ID_ADDURL:
                {
                    if(guiIntfStruct.StreamType == STREAMTYPE_DVD ||
                       guiIntfStruct.StreamType == STREAMTYPE_DVDNAV) return 0;
                    display_openurlwindow(gui, 1);
                    break;
                }
                case ID_CLEAR:
                {
                    if(!gui->playlist->trackcount) return 0;
                    gui->playlist->clear_playlist(gui->playlist);
                    break;
                }
                case ID_PLAYLISTLOAD:
                {
                    if(guiIntfStruct.StreamType == STREAMTYPE_DVD ||
                       guiIntfStruct.StreamType == STREAMTYPE_DVDNAV) return 0;
                    display_loadplaylistwindow(gui);
                    break;
                }
                case ID_PLAYLISTSAVE:
                {
                    /* no point saving an empty playlist */
                    if(!gui->playlist->trackcount ||
                        guiIntfStruct.StreamType == STREAMTYPE_DVD ||
                        guiIntfStruct.StreamType == STREAMTYPE_DVDNAV)
                        return 0;
                    display_saveplaylistwindow(gui);
                    break;
                }
            }
            updatetracklist(hwnd);
            if(selected < 1) selected = 1;
            else if(selected>pl->trackcount) selected = pl->trackcount;
            SendDlgItemMessage(hwnd, ID_TRACKLIST, LB_SETCURSEL, selected - 1, 0);
            return 0;
        }
        case WM_DROPFILES:
        {
            char file[MAX_PATH];
            int filecount = DragQueryFile((HDROP) wParam, -1, file, MAX_PATH);
            int i;
            for (i=0; i<filecount; i++)
            {
                DragQueryFile((HDROP) wParam, i, file, MAX_PATH);
                if(!parse_filename(file, playtree, mconfig, 0))
                    pl->add_track(pl, file, NULL, NULL, 0);
            }
            DragFinish((HDROP) wParam);
            updatetracklist(hwnd);
        }
            break;
    }
    return DefWindowProc(hwnd, iMsg, wParam, lParam);
}

void update_playlistwindow(void)
{
    HWND hWnd = FindWindow(NULL, "MPlayer Playlist");
    if (hWnd) updatetracklist(hWnd);
}

void display_playlistwindow(gui_t *gui)
{
    HWND hWnd = FindWindow(NULL, "MPlayer Playlist");
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASS wc;
    int x, y;

    if (hWnd)
    {
        SendMessage(hWnd, WM_CLOSE, 0, 0);
        return;
    }

    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = PlayListWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL,IDC_ARROW);
    wc.hIcon         = gui->icon;
    wc.hbrBackground = SOLID_GREY2;
    wc.lpszClassName = "MPlayer - Playlist";
    wc.lpszMenuName  = NULL;
    RegisterClass(&wc);
    create_playlistmenu(gui);
    x = (GetSystemMetrics(SM_CXSCREEN) / 2) - (400 / 2);   /* Erik: center popup window on screen */
    y = (GetSystemMetrics(SM_CYSCREEN) / 2) - (254 / 2);
    hWnd = CreateWindow("MPlayer - Playlist",
                        "MPlayer Playlist",
                        WS_POPUPWINDOW | WS_CAPTION | WS_MINIMIZEBOX,
                        x,
                        y,
                        400,
                        254,
                        NULL,
                        NULL,
                        hInstance,
                        NULL);
   SetWindowLongPtr(hWnd, GWLP_USERDATA, (DWORD)gui);
   updatetracklist(hWnd);
   DragAcceptFiles(hWnd,TRUE);
   ShowWindow(hWnd, SW_SHOW);
   UpdateWindow(hWnd);
}

static LRESULT CALLBACK SkinBrowserWndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND listbox;
    static char skinspath[MAX_PATH];
    gui_t* gui = (gui_t*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (iMsg)
    {
        case WM_CREATE:
        {
            HANDLE skinHandle = INVALID_HANDLE_VALUE;
            WIN32_FIND_DATA finddata;

            listbox = CreateWindow("listbox", NULL,
                                   WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT |
                                   LBS_DISABLENOSCROLL | WS_VSCROLL | WS_TABSTOP,
                                   4, 5, 166, 60, hwnd,
                                   (HMENU) ID_DIR,
                                   ((LPCREATESTRUCT) lParam) -> hInstance,
                                   NULL);
            SendMessage(listbox, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT), 0);

            /* This opens the skins directory, lists the directory names, and adds them to the listbox */
            sprintf(skinspath, "%s/*.", get_path("skins"));

            skinHandle = FindFirstFile(skinspath, &finddata);
            if (skinHandle != INVALID_HANDLE_VALUE)
            {
                do
                {
                    if (finddata.cFileName[0] == '.') continue;
                    /* populate the listbox */
                    capitalize(finddata.cFileName);
                    SendDlgItemMessage(hwnd, ID_DIR, LB_ADDSTRING, 0, (LPARAM) finddata.cFileName);
                } while (FindNextFile(skinHandle, &finddata));
                FindClose(skinHandle);
            }
            else
                mp_msg(MSGT_GPLAYER, MSGL_FATAL, "Error opening %s\n", get_path("skins"));
            break;
        }
        case WM_COMMAND:
        {
            if ((HWND) lParam == listbox)
            {
                if(HIWORD(wParam) == LBN_DBLCLK)
                {
                    int index = SendMessage(listbox, LB_GETCURSEL, 0, 0);
                    int len = SendMessage(listbox, LB_GETTEXTLEN, index, 0);
                    if (len)
                    {
                        if (guiIntfStruct.Playing) guiGetEvent(guiCEvent, (void *) guiSetStop);
                        if (skinName) free(skinName);
                        skinName = (char *) malloc(len+1);
                        SendMessage(listbox, LB_GETTEXT, (WPARAM) index, (LPARAM) skinName);
                        /* fill out the full pathname to the skin */
                        strcpy(skinspath, get_path("skins"));
                        strcat(skinspath, "\\");
                        strcat(skinspath, skinName);
                        ShowWindow(hwnd, SW_HIDE);
                        Shell_NotifyIcon(NIM_DELETE, &nid);
                        destroy_window(gui);
                        create_window(gui, skinspath);
                        create_subwindow(gui, skinspath);
                        SendMessage(hwnd, WM_CLOSE, 0, 0); /* Avoid crashing when switching skin */
                    }
                }
            }
        }
            return 0;
    }
    return DefWindowProc(hwnd, iMsg, wParam, lParam);
}

void display_skinbrowser(gui_t* gui)
{
    HWND hWnd = FindWindow(NULL, "Skin Browser");
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASS wc;
    int x, y;

    if (hWnd)
    {
        SendMessage(hWnd, WM_CLOSE, 0, 0);
        return;
    }

    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = SkinBrowserWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = gui->icon;
    wc.hbrBackground = SOLID_GREY2;
    wc.lpszClassName = "Skin Browser";
    wc.lpszMenuName  = NULL;
    RegisterClass(&wc);
    x = (GetSystemMetrics(SM_CXSCREEN) / 2) - (180 / 2);
    y = (GetSystemMetrics(SM_CYSCREEN) / 2) - (102 / 2);
    hWnd = CreateWindow("Skin Browser",
                        "Skin Browser",
                        WS_POPUPWINDOW |WS_CAPTION,
                        x,
                        y,
                        180,
                        102,
                        NULL,
                        NULL,
                        hInstance,
                        NULL);
   SetWindowLongPtr(hWnd, GWLP_USERDATA, (DWORD) gui);
   ShowWindow(hWnd, SW_SHOW);
   UpdateWindow(hWnd);
}

#ifdef CONFIG_DVDREAD
static LRESULT CALLBACK TitleChapterWndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND title;
    static HWND chapter;
    HWND wdg;
    int i=0, j=0;
    char titles[MAX_PATH] = "";
    char chapters[MAX_PATH] = "";
    gui_t *gui = (gui_t *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (iMsg)
    {
        case WM_CREATE:
            wdg = CreateWindow("button", "Ok",
                               WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               4, 43, 80, 25, hwnd,
                               (HMENU) ID_OK,
                               ((LPCREATESTRUCT) lParam) -> hInstance,
                               NULL);
                              SendMessage(wdg, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT), 0);

            wdg = CreateWindow("button", "Cancel",
                               WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               90, 43, 80, 25, hwnd,
                               (HMENU) ID_CANCEL,
                               ((LPCREATESTRUCT) lParam) -> hInstance,
                               NULL);
                              SendMessage(wdg, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT), 0);

            title = CreateWindow("combobox", NULL,
                                 CBS_DROPDOWNLIST | CB_SHOWDROPDOWN | CBS_NOINTEGRALHEIGHT | CBS_HASSTRINGS |
                                 WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP,
                                 4, 10, 80, 160, hwnd,
                                 (HMENU) ID_TITLESEL,
                                 ((LPCREATESTRUCT) lParam) -> hInstance,
                                 NULL);

            SendMessage(title, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT), 0);

            chapter = CreateWindow("combobox", NULL,
                                 CBS_DROPDOWNLIST | CB_SHOWDROPDOWN | CBS_NOINTEGRALHEIGHT | CBS_HASSTRINGS |
                                 WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP,
                                 90, 10, 80, 160, hwnd,
                                 (HMENU) ID_CHAPTERSEL,
                                 ((LPCREATESTRUCT) lParam) -> hInstance,
                                 NULL);
            SendMessage(chapter, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT), 0);

            for (i=0; i<guiIntfStruct.DVD.titles; i++)
            {
                /* we have to reverse the order here because of the way CB_INSERTSTRING adds items */
                sprintf(&titles[i], "%d", guiIntfStruct.DVD.titles - i);
                SendDlgItemMessage(hwnd, ID_TITLESEL, CB_INSERTSTRING, 0, (LPARAM) &titles[i]);
            }
            SendDlgItemMessage(hwnd, ID_TITLESEL, CB_SETCURSEL, dvd_title, 0);

            for (j=0; j<guiIntfStruct.DVD.chapters; j++)
            {
                sprintf(&chapters[j], "%d", guiIntfStruct.DVD.chapters - j);
                SendDlgItemMessage(hwnd, ID_CHAPTERSEL, CB_INSERTSTRING, 0, (LPARAM) &chapters[j]);
            }
            SendDlgItemMessage(hwnd, ID_CHAPTERSEL, CB_SETCURSEL, dvd_chapter, 0);

            break;
        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case ID_CANCEL:
                    DestroyWindow(hwnd);
                    return 0;
                case ID_OK:
                {
                    guiIntfStruct.DVD.current_title = SendMessage(title, CB_GETCURSEL, 0, 0) + 1;
                    guiIntfStruct.DVD.current_chapter = SendMessage(chapter, CB_GETCURSEL, 0, 0) + 1;

                    if((guiIntfStruct.DVD.current_title != 0 || guiIntfStruct.DVD.current_chapter != 0))
                    {
                        gui->startplay(gui);
                        DestroyWindow(hwnd);
                    }
                }
                break;
            }
        }
        return 0;
    }
    return DefWindowProc(hwnd, iMsg, wParam, lParam);
}

void display_chapterselwindow(gui_t *gui)
{
    HWND hWnd;
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASS wc;
    int x, y;

    if (guiIntfStruct.StreamType != STREAMTYPE_DVD) return;
    if (FindWindow(NULL, "Select Title/Chapter...")) return;

    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = TitleChapterWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL,IDC_ARROW);
    wc.hIcon         = gui->icon;
    wc.hbrBackground = SOLID_GREY2;
    wc.lpszClassName = "Select Title/Chapter...";
    wc.lpszMenuName  = NULL;
    RegisterClass(&wc);
    x = (GetSystemMetrics(SM_CXSCREEN) / 2) - (180 / 2);
    y = (GetSystemMetrics(SM_CYSCREEN) / 2) - (100 / 2);
    hWnd = CreateWindow("Select Title/Chapter...",
                        "Select Title/Chapter...",
                        WS_POPUPWINDOW | WS_CAPTION,
                        x,
                        y,
                        180,
                        100,
                        NULL,
                        NULL,
                        hInstance,
                        NULL);
   SetWindowLongPtr(hWnd, GWLP_USERDATA, (DWORD) gui);
   ShowWindow(hWnd, SW_SHOW);
   UpdateWindow(hWnd);
}
#endif

static LRESULT CALLBACK EqWndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    HWND btn, label, eq0, eq1, eq2, eq3;

    switch (iMsg)
    {
        case WM_CREATE:
        {
            btn = CreateWindow("button", "Reset",
                               WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               157, 143, 80, 25, hwnd,
                               (HMENU) ID_DEFAULTS,
                               ((LPCREATESTRUCT) lParam) -> hInstance,
                               NULL);
            SendMessage(btn, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT), 0);

            btn = CreateWindow("button", "Close",
                               WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               243, 143, 80, 25, hwnd,
                               (HMENU) ID_CLOSE,
                               ((LPCREATESTRUCT) lParam) -> hInstance,
                               NULL);
            SendMessage(btn, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT), 0);

            label = CreateWindow("static", "Brightness",
                                 WS_CHILD | WS_VISIBLE,
                                 12, 122, 70, 15, hwnd,
                                 NULL,
                                 ((LPCREATESTRUCT) lParam) -> hInstance,
                                 NULL);
            SendMessage(label, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT), 0);

            label = CreateWindow("static", "Contrast",
                                 WS_CHILD | WS_VISIBLE,
                                 99, 122, 70, 15, hwnd,
                                 NULL,
                                 ((LPCREATESTRUCT) lParam) -> hInstance,
                                 NULL);
            SendMessage(label, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT), 0);

            label = CreateWindow("static", "Hue",
                                 WS_CHILD | WS_VISIBLE,
                                 191, 122, 70, 15, hwnd,
                                 NULL,
                                 ((LPCREATESTRUCT) lParam) -> hInstance, NULL);
            SendMessage(label, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT), 0);

            label = CreateWindow("static", "Saturation",
                                 WS_CHILD | WS_VISIBLE,
                                 260, 122, 70, 15, hwnd,
                                 NULL,
                                 ((LPCREATESTRUCT) lParam) -> hInstance,
                                 NULL);
            SendMessage(label, WM_SETFONT, (WPARAM) GetStockObject(DEFAULT_GUI_FONT), 0);

            eq0 = CreateWindow(TRACKBAR_CLASS, "brightness",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_DISABLED |
                               TBS_VERT | TBS_NOTICKS,
                               30, 0, 20, 120, hwnd,
                               (HMENU) ID_EQ0,
                               ((LPCREATESTRUCT) lParam) -> hInstance,
                               NULL);
            SendDlgItemMessage(hwnd, ID_EQ0, TBM_SETRANGE, 1, MAKELONG(0, 200));

            eq1 = CreateWindow(TRACKBAR_CLASS, "contrast",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_DISABLED |
                               TBS_VERT | TBS_NOTICKS,
                               112, 0, 20, 120, hwnd,
                               (HMENU) ID_EQ1,
                               ((LPCREATESTRUCT) lParam) -> hInstance,
                               NULL);
            SendDlgItemMessage(hwnd, ID_EQ1, TBM_SETRANGE, 1, MAKELONG(0, 200));

            eq2 = CreateWindow(TRACKBAR_CLASS, "hue",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_DISABLED |
                               TBS_VERT | TBS_NOTICKS,
                               194, 0, 20, 120, hwnd,
                               (HMENU) ID_EQ2,
                               ((LPCREATESTRUCT) lParam) -> hInstance,
                               NULL);
            SendDlgItemMessage(hwnd, ID_EQ2, TBM_SETRANGE, 1, MAKELONG(0, 200));

            eq3 = CreateWindow(TRACKBAR_CLASS, "saturation",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_DISABLED |
                               TBS_VERT | TBS_NOTICKS,
                               276, 0, 20, 120, hwnd,
                               (HMENU) ID_EQ3,
                               ((LPCREATESTRUCT) lParam) -> hInstance, NULL);
            SendDlgItemMessage(hwnd, ID_EQ3, TBM_SETRANGE, 1, MAKELONG(0, 200));

            if(guiIntfStruct.sh_video && guiIntfStruct.Playing)
            {
                EnableWindow(eq0, 1); EnableWindow(eq1, 1); EnableWindow(eq2, 1); EnableWindow(eq3, 1);
                get_video_colors(guiIntfStruct.sh_video, "brightness", &vo_gamma_brightness);
                get_video_colors(guiIntfStruct.sh_video, "contrast", &vo_gamma_contrast);
                get_video_colors(guiIntfStruct.sh_video, "hue", &vo_gamma_hue);
                get_video_colors(guiIntfStruct.sh_video, "saturation", &vo_gamma_saturation);
            }
            SendDlgItemMessage(hwnd, ID_EQ0, TBM_SETPOS, 1, (LPARAM)100 - vo_gamma_brightness);
            SendDlgItemMessage(hwnd, ID_EQ1, TBM_SETPOS, 1, (LPARAM)100 - vo_gamma_contrast);
            SendDlgItemMessage(hwnd, ID_EQ2, TBM_SETPOS, 1, (LPARAM)100 - vo_gamma_hue);
            SendDlgItemMessage(hwnd, ID_EQ3, TBM_SETPOS, 1, (LPARAM)100 - vo_gamma_saturation);
            break;
        }
        case WM_VSCROLL:
        {
            switch (LOWORD(wParam))
            {
                case TB_THUMBTRACK:
                    if(guiIntfStruct.sh_video && guiIntfStruct.Playing)
                    {
                        vo_gamma_brightness = 100 - SendDlgItemMessage(hwnd, ID_EQ0, TBM_GETPOS, 0, 0);
                        set_video_colors(guiIntfStruct.sh_video, "brightness", vo_gamma_brightness);

                        vo_gamma_contrast = 100 - SendDlgItemMessage(hwnd, ID_EQ1, TBM_GETPOS, 0, 0);
                        set_video_colors(guiIntfStruct.sh_video, "contrast", vo_gamma_contrast);

                        vo_gamma_hue = 100 - SendDlgItemMessage(hwnd, ID_EQ2, TBM_GETPOS, 0, 0);
                        set_video_colors(guiIntfStruct.sh_video, "hue", vo_gamma_hue);

                        vo_gamma_saturation = 100 - SendDlgItemMessage(hwnd, ID_EQ3, TBM_GETPOS, 0, 0);
                        set_video_colors(guiIntfStruct.sh_video, "saturation", vo_gamma_saturation);
                    }
                    else
                    {
                        EnableWindow(GetDlgItem(hwnd, ID_EQ0), 0);
                        EnableWindow(GetDlgItem(hwnd, ID_EQ1), 0);
                        EnableWindow(GetDlgItem(hwnd, ID_EQ2), 0);
                        EnableWindow(GetDlgItem(hwnd, ID_EQ3), 0);
                    }
                    break;
            }
        }
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            return (INT_PTR)SOLID_GREY2;
        }
        break;
        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case ID_CLOSE:
                    DestroyWindow(hwnd);
                    return 0;
                case ID_DEFAULTS:
                    if(guiIntfStruct.sh_video && guiIntfStruct.Playing)
                    {
                        vo_gamma_brightness=0;
                        SendDlgItemMessage(hwnd, ID_EQ0, TBM_SETPOS, 1, (LPARAM)100);
                        set_video_colors(guiIntfStruct.sh_video, "brightness", vo_gamma_brightness);

                        vo_gamma_contrast=0;
                        SendDlgItemMessage(hwnd, ID_EQ1, TBM_SETPOS, 1, (LPARAM)100);
                        set_video_colors(guiIntfStruct.sh_video, "contrast", vo_gamma_contrast);

                        vo_gamma_hue=0;
                        SendDlgItemMessage(hwnd, ID_EQ2, TBM_SETPOS, 1, (LPARAM)100);
                        set_video_colors(guiIntfStruct.sh_video, "hue", vo_gamma_hue);

                        vo_gamma_saturation=0;
                        SendDlgItemMessage(hwnd, ID_EQ3, TBM_SETPOS, 1, (LPARAM)100);
                        set_video_colors(guiIntfStruct.sh_video, "saturation", vo_gamma_saturation);
                    }
                    break;
            }
        }
        return 0;
    }
    return DefWindowProc(hwnd, iMsg, wParam, lParam);
}

void display_eqwindow(gui_t *gui)
{
    HWND hWnd;
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASS wc;
    int x, y;

    if(!guiIntfStruct.sh_video) return;
    if(FindWindow(NULL, "MPlayer - Equalizer")) return;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = EqWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL,IDC_ARROW);
    wc.hIcon         = gui->icon;
    wc.hbrBackground = SOLID_GREY2;
    wc.lpszClassName = "MPlayer - Equalizer";
    wc.lpszMenuName  = NULL;
    RegisterClass(&wc);
    x = (GetSystemMetrics(SM_CXSCREEN) / 2) - (332 / 2);
    y = (GetSystemMetrics(SM_CYSCREEN) / 2) - (200 / 2);
    hWnd = CreateWindow("MPlayer - Equalizer",
                        "MPlayer - Equalizer",
                        WS_POPUPWINDOW | WS_CAPTION,
                        x,
                        y,
                        332,
                        200,
                        NULL,
                        NULL,
                        hInstance,
                        NULL);
   SetWindowLongPtr(hWnd, GWLP_USERDATA, (DWORD) gui);
   ShowWindow(hWnd, SW_SHOW);
   UpdateWindow(hWnd);
}
