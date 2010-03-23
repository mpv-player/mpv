/*
 * Get path to config dir/file.
 *
 * Return Values:
 *   Returns the pointer to the ALLOCATED buffer containing the
 *   zero terminated path string. This buffer has to be FREED
 *   by the caller.
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "mp_msg.h"
#include "path.h"

#ifdef CONFIG_MACOSX_BUNDLE
#include <CoreFoundation/CoreFoundation.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#elif defined(__MINGW32__)
#include <windows.h>
#elif defined(__CYGWIN__)
#include <windows.h>
#include <sys/cygwin.h>
#endif

#include "osdep/osdep.h"

char *get_path(const char *filename){
	char *homedir;
	char *buff;
#ifdef __MINGW32__
	static char *config_dir = "/mplayer";
#else
	static char *config_dir = "/.mplayer";
#endif
	int len;
#ifdef CONFIG_MACOSX_BUNDLE
	struct stat dummy;
	CFIndex maxlen=256;
	CFURLRef res_url_ref=NULL;
	CFURLRef bdl_url_ref=NULL;
	char *res_url_path = NULL;
	char *bdl_url_path = NULL;
#endif

	if ((homedir = getenv("MPLAYER_HOME")) != NULL)
		config_dir = "";
	else if ((homedir = getenv("HOME")) == NULL)
#if defined(__MINGW32__) || defined(__CYGWIN__)
	/* Hack to get fonts etc. loaded outside of Cygwin environment. */
	{
		int i,imax=0;
		char exedir[260];
		GetModuleFileNameA(NULL, exedir, 260);
		for (i=0; i< strlen(exedir); i++)
			if (exedir[i] =='\\')
				{exedir[i]='/'; imax=i;}
		exedir[imax]='\0';
		homedir = exedir;
	}
#elif defined(__OS2__)
    {
        PPIB ppib;
        char path[260];

        // Get process info blocks
        DosGetInfoBlocks(NULL, &ppib);

        // Get full path of the executable
        DosQueryModuleName(ppib->pib_hmte, sizeof( path ), path);

        // Truncate name part including last backslash
        *strrchr(path, '\\') = 0;

        // Convert backslash to slash
        _fnslashify(path);

        homedir = path;
    }
#else
	return NULL;
#endif
	len = strlen(homedir) + strlen(config_dir) + 1;
	if (filename == NULL) {
		if ((buff = malloc(len)) == NULL)
			return NULL;
		sprintf(buff, "%s%s", homedir, config_dir);
	} else {
		len += strlen(filename) + 1;
		if ((buff = malloc(len)) == NULL)
			return NULL;
		sprintf(buff, "%s%s/%s", homedir, config_dir, filename);
	}

#ifdef CONFIG_MACOSX_BUNDLE
	if (stat(buff, &dummy)) {

		res_url_ref=CFBundleCopyResourcesDirectoryURL(CFBundleGetMainBundle());
		bdl_url_ref=CFBundleCopyBundleURL(CFBundleGetMainBundle());

		if (res_url_ref&&bdl_url_ref) {

			res_url_path=malloc(maxlen);
			bdl_url_path=malloc(maxlen);

			while (!CFURLGetFileSystemRepresentation(res_url_ref, true, res_url_path, maxlen)) {
				maxlen*=2;
				res_url_path=realloc(res_url_path, maxlen);
			}
			CFRelease(res_url_ref);

			while (!CFURLGetFileSystemRepresentation(bdl_url_ref, true, bdl_url_path, maxlen)) {
				maxlen*=2;
				bdl_url_path=realloc(bdl_url_path, maxlen);
			}
			CFRelease(bdl_url_ref);

			if (strcmp(res_url_path, bdl_url_path) == 0)
				res_url_path = NULL;
		}

		if (res_url_path&&filename) {
			if ((strlen(filename)+strlen(res_url_path)+2)>maxlen) {
				maxlen=strlen(filename)+strlen(res_url_path)+2;
			}
			free(buff);
			buff = malloc(maxlen);
			strcpy(buff, res_url_path);

			strcat(buff,"/");
			strcat(buff, filename);
		}
	}
#endif
	mp_msg(MSGT_GLOBAL,MSGL_V,"get_path('%s') -> '%s'\n",filename,buff);
	return buff;
}

#if (defined(__MINGW32__) || defined(__CYGWIN__)) && defined(CONFIG_WIN32DLL)
void set_path_env(void)
{
	/*make our codec dirs available for LoadLibraryA()*/
	char win32path[MAX_PATH];
#ifdef __CYGWIN__
	cygwin_conv_to_full_win32_path(BINARY_CODECS_PATH, win32path);
#else /*__CYGWIN__*/
	/* Expand to absolute path unless it's already absolute */
	if (!strstr(BINARY_CODECS_PATH,":") && BINARY_CODECS_PATH[0] != '\\') {
		GetModuleFileNameA(NULL, win32path, MAX_PATH);
		strcpy(strrchr(win32path, '\\') + 1, BINARY_CODECS_PATH);
	}
	else strcpy(win32path, BINARY_CODECS_PATH);
#endif /*__CYGWIN__*/
	mp_msg(MSGT_WIN32, MSGL_V, "Setting PATH to %s\n", win32path);
	if (!SetEnvironmentVariableA("PATH", win32path))
		mp_msg(MSGT_WIN32, MSGL_WARN, "Cannot set PATH!");
}
#endif /* (defined(__MINGW32__) || defined(__CYGWIN__)) && defined(CONFIG_WIN32DLL) */

char *codec_path = BINARY_CODECS_PATH;

static int needs_free = 0;

void set_codec_path(const char *path)
{
    if (needs_free)
        free(codec_path);
    if (path == 0) {
        codec_path = BINARY_CODECS_PATH;
        needs_free = 0;
        return;
    }
    codec_path = malloc(strlen(path) + 1);
    strcpy(codec_path, path);
    needs_free = 1;
}
