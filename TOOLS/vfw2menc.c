/*
 * VFW Compressor Settings Tool
 *
 * Copyright (c) 2006 Gianluigi Tiesi <sherpya@netfarm.it>
 *
 * Official Website : http://oss.netfarm.it/mplayer-win32.php
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* On MinGW compile with: gcc vfw2menc.c -o vfw2menc.exe -lwinmm -lole32 */
/* Using Wine: winegcc vfw2menc.c -o vfw2menc -lwinmm -lole32 */
/* MSVC requires getopt.c and getopt.h available at the original website */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#pragma warning(disable: 4996)
#endif

#define VERSION "0.1"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <getopt.h>
#include <windows.h>
#include <vfw.h>

#define BAIL(msg) { printf("%s: %s\n", argv[0], msg); ret = -1; goto cleanup; }

typedef struct {
    UINT             uDriverSignature;
    HINSTANCE        hDriverModule;
    DRIVERPROC       DriverProc;
    DWORD            dwDriverID;
} DRVR;

typedef DRVR  *PDRVR;
typedef DRVR  *NPDRVR;
typedef DRVR  *LPDRVR;

enum
{
    MODE_NONE = 0,
    MODE_CHECK,
    MODE_SAVE,
    MODE_VIEW
};

static int save_settings(HDRVR hDriver, const char *filename)
{
    FILE *fd = NULL;
    DWORD cb = (DWORD) SendDriverMessage(hDriver, ICM_GETSTATE, 0, 0);
    char *pv = NULL;

    if (!cb)
    {
        printf("ICM_GETSTATE returned 0 size\n");
        return -1;
    }

    pv = malloc(cb);
    if (SendDriverMessage(hDriver, ICM_GETSTATE, (LPARAM) pv, (LPARAM) &cb) != ICERR_OK)
    {
        printf("ICM_GETSTATE failed\n");
        free(pv);
        return -1;
    }

    fd = fopen(filename, "wb");
    if (!fd)
    {
        printf("Cannot open file %s for writing\n", filename);
        free(pv);
        return -1;
    }

    if (fwrite(pv, cb, 1, fd) != 1)
    {
        printf("fwrite() failed on %s\n", filename);
        free(pv);
        fclose(fd);
        return -1;
    }
    fclose(fd);
    free(pv);
    return 0;
}

static int load_settings(HDRVR hDriver, const char *filename)
{
    struct stat info;
    FILE *fd = NULL;
    char *pv;

    if (stat(filename, &info) < 0)
    {
        printf("stat() on %s failed\n", filename);
        return -1;
    }

    pv = malloc(info.st_size);
    fd = fopen(filename, "rb");

    if (!fd)
    {
        printf("Cannot open file %s for reading\n", filename);
        free(pv);
        return -1;
    }

    if (fread(pv, info.st_size, 1, fd) != 1)
    {
        printf("fread() failed on %s\n", filename);
        free(pv);
        fclose(fd);
        return -1;
    }
    fclose(fd);
    if (!SendDriverMessage(hDriver, ICM_SETSTATE, (LPARAM) pv, (LPARAM) info.st_size))
    {
        printf("ICM_SETSTATE failed\n");
        free(pv);
        return -1;
    }
    free(pv);
    return 0;
}

static struct option long_options[] =
{
    { "help",   no_argument,        NULL,   'h' },
    { "driver", required_argument,  NULL,   'd' },
    { "fourcc", required_argument,  NULL,   'f' },
    { "save",   required_argument,  NULL,   's' },
    { "check",  required_argument,  NULL,   'c' },
    { "view",   no_argument,        NULL,   'v' },
    { 0, 0, 0, 0 }
};

static void help(const char *progname)
{
    printf("VFW to mencoder v"VERSION" - Copyright 2007 - Gianluigi Tiesi <sherpya@netfarm.it>\n");
    printf("This program is Free Software\n\n");
    printf("Usage: %s\n", progname);
    printf("      -h|--help            - displays this help\n");
    printf("      -d|--driver filename - dll or drv to load\n");
    printf("      -f|--fourcc fourcc   - fourcc of selected driver (look at codecs.conf)\n");
    printf("      -s|--save filename   - save settings to file\n");
    printf("      -c|--check filename  - load and show setting in filename\n");
    printf("      -v|--view            - displays the config dialog and do nothing\n");
    printf("\nExamples:\n");
    printf(" %s -f VP62 -d vp6vfw.dll -s firstpass.mcf\n", progname);
    printf(" %s -f VP62 -d vp6vfw.dll -c firstpass.mcf\n", progname);
    printf(" %s -f VP62 -d vp6vfw.dll -v\n", progname);
    printf("\nIf the driver dialog doesn't work, you can try without specifing a fourcc,\n");
    printf("but the compdata file will not work with mencoder.\n");
    printf("Driver option is required and you must specify at least -s, -c -o -v\n");
    printf("Usage with mencoder -ovc vfw -xvfwopts codec=vp6vfw.dll:compdata=settings.mcf\n");
}

int main(int argc, char *argv[])
{
    char *driver = NULL;
    char *fourcc = NULL;
    char *filename = NULL;
    unsigned char mode = 0;
    DWORD dwFCC = 0;
    ICOPEN icopen;
    HRESULT coinit = -1;
    /* ICINFO icinfo; */

    wchar_t drvfile[MAX_PATH];
    HDRVR hDriver = NULL;
    int ret = 0;
    int c = -1, long_options_index = -1;

    if (argc < 2)
    {
        help(argv[0]);
        ret = -1;
        goto cleanup;
    }

    while ((c = getopt_long(argc, argv, "hd:f:s:c:v", long_options, &long_options_index)) != -1)
    {
        switch (c)
        {
            case 'h':
                help(argv[0]);
                ret = 0;
                goto cleanup;
            case 'd':
                driver = strdup(optarg);
                break;
            case 'f':
                fourcc = strdup(optarg);
                if (strlen(optarg) != 4) BAIL("Fourcc must be exactly 4 chars");
                break;
            case 's':
                if (mode != MODE_NONE) BAIL("Incompatible arguments");
                filename = strdup(optarg);
                mode = MODE_SAVE;
                break;
            case 'c':
                if (mode != MODE_NONE) BAIL("Incompatible arguments");
                filename = strdup(optarg);
                mode = MODE_CHECK;
                break;
            case 'v':
                if (mode != MODE_NONE) BAIL("Incompatible arguments");
                mode = MODE_VIEW;
                break;
            default:
                printf("Wrong arguments!\n");
                help(argv[0]);
                goto cleanup;
        }
    }

    if (!(argc == optind) && (mode != MODE_NONE) &&
        driver && (filename || (mode == MODE_VIEW)))
    {
        help(argv[0]);
        goto cleanup;
    }

    if (!MultiByteToWideChar(CP_ACP, 0, driver, -1, drvfile, MAX_PATH))
        BAIL("MultiByteToWideChar() failed\n");

    if (fourcc) memcpy(&dwFCC, fourcc, 4);
    memset(&icopen, 0, sizeof(icopen));

    icopen.dwSize = sizeof(icopen);
    icopen.fccType = ICTYPE_VIDEO; /* VIDC */
    icopen.fccHandler = dwFCC;
    icopen.dwVersion  = 0x00001000; /* FIXME */
    icopen.dwFlags = ICMODE_COMPRESS;
    icopen.dwError = 0;
    icopen.pV1Reserved = NULL;
    icopen.pV2Reserved = NULL;
    icopen.dnDevNode = -1; /* FIXME */

    coinit = CoInitialize(NULL);

    if (!(hDriver = OpenDriver(drvfile, NULL, (LPARAM) &icopen)))
        BAIL("OpenDriver() failed\n");

   /*
        memset(&icinfo, 0, sizeof(ICINFO));
        icinfo.dwSize = sizeof(ICINFO);
        SendDriverMessage(hDriver, ICM_GETINFO, (LPARAM) &icinfo, sizeof(ICINFO));
    */

    if (SendDriverMessage(hDriver, ICM_CONFIGURE, -1, 0) != ICERR_OK)
        BAIL("The driver doesn't provide a configure dialog");


    switch(mode)
    {
        case MODE_CHECK:
            if (load_settings(hDriver, filename))
                BAIL("Cannot load settings from file");
            if (SendDriverMessage(hDriver, ICM_CONFIGURE, 0, 0) != ICERR_OK)
                BAIL("ICM_CONFIGURE failed");
            break;
        case MODE_SAVE:
            if (SendDriverMessage(hDriver, ICM_CONFIGURE, 0, 0) != ICERR_OK)
                BAIL("ICM_CONFIGURE failed");
            if (save_settings(hDriver, filename))
                BAIL("Cannot save settings to file");
            break;
        case MODE_VIEW:
            {
                HWND hwnd = GetDesktopWindow();
                if (SendDriverMessage(hDriver, ICM_CONFIGURE, (LPARAM) hwnd, 0) != ICERR_OK)
                    BAIL("ICM_CONFIGURE failed");
            }
            break;
        default:
            BAIL("This should not happen :)");
    }

cleanup:
    if (driver) free(driver);
    if (fourcc) free(fourcc);
    if (filename) free(filename);
    if (hDriver) CloseDriver(hDriver, 0, 0);
    if ((coinit == S_OK) || coinit == S_FALSE) CoUninitialize();
    return ret;
}
