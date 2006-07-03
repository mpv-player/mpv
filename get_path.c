
/*
 * Get path to config dir/file.
 *
 * Return Values:
 *   Returns the pointer to the ALLOCATED buffer containing the
 *   zero terminated path string. This buffer has to be FREED
 *   by the caller.
 *
 */
#ifdef MACOSX_BUNDLE
#include <CoreFoundation/CoreFoundation.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#endif

char *get_path(const char *filename){
	char *homedir;
	char *buff;
#if defined(__MINGW32__)
    static char *config_dir = "/mplayer";
#else
	static char *config_dir = "/.mplayer";
#endif
	int len;
#ifdef MACOSX_BUNDLE
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
#if defined(__MINGW32__)||defined(__CYGWIN__) /*hack to get fonts etc. loaded outside of cygwin environment*/
	{
        int i,imax=0;       
        char exedir[260];       
        GetModuleFileNameA(NULL, exedir, 260);
        for(i=0; i< strlen(exedir);i++)if(exedir[i] =='\\'){exedir[i]='/';imax=i;}
        exedir[imax]='\0';
	    homedir = exedir;
	}
#else
		return NULL;
#endif       
	len = strlen(homedir) + strlen(config_dir) + 1;
	if (filename == NULL) {
		if ((buff = (char *) malloc(len)) == NULL)
			return NULL;
		sprintf(buff, "%s%s", homedir, config_dir);
	} else {
		len += strlen(filename) + 1;
		if ((buff = (char *) malloc(len)) == NULL)
			return NULL;
		sprintf(buff, "%s%s/%s", homedir, config_dir, filename);
	}

#ifdef MACOSX_BUNDLE
	if(stat(buff, &dummy)) {
	
		res_url_ref=CFBundleCopyResourcesDirectoryURL(CFBundleGetMainBundle());
		bdl_url_ref=CFBundleCopyBundleURL(CFBundleGetMainBundle());
		
		if(res_url_ref&&bdl_url_ref) {

			res_url_path=malloc(maxlen);
			bdl_url_path=malloc(maxlen);

			while(!CFURLGetFileSystemRepresentation(res_url_ref, true, res_url_path, maxlen)) {
				maxlen*=2;
				res_url_path=realloc(res_url_path, maxlen);
			}
			CFRelease(res_url_ref);
			
			while(!CFURLGetFileSystemRepresentation(bdl_url_ref, true, bdl_url_path, maxlen)) {
				maxlen*=2;
				bdl_url_path=realloc(bdl_url_path, maxlen);
			}
			CFRelease(bdl_url_ref);
				
			if( strcmp(res_url_path, bdl_url_path) == 0)
				res_url_path = NULL;
		}
			
		if(res_url_path&&filename) {
			if((strlen(filename)+strlen(res_url_path)+2)>maxlen) {
				maxlen=strlen(filename)+strlen(res_url_path)+2;
			}
			free(buff);
			buff = (char *) malloc(maxlen);
			strcpy(buff, res_url_path);
				
			strcat(buff,"/");
			strcat(buff, filename);
		}
	}
#endif
	mp_msg(MSGT_GLOBAL,MSGL_V,"get_path('%s') -> '%s'\n",filename,buff);
	return buff;
}

#if defined(WIN32) && defined(USE_WIN32DLL)
void set_path_env()
{
	/*make our codec dirs available for LoadLibraryA()*/
	char tmppath[MAX_PATH*2 + 1];
	char win32path[MAX_PATH];
	char realpath[MAX_PATH];
#ifdef __CYGWIN__
	cygwin_conv_to_full_win32_path(WIN32_PATH,win32path);
	strcpy(tmppath,win32path);
#ifdef USE_REALCODECS
	cygwin_conv_to_full_win32_path(REALCODEC_PATH,realpath);
	sprintf(tmppath,"%s;%s",win32path,realpath);
#endif /*USE_REALCODECS*/
#else /*__CYGWIN__*/
	/* Expand to absolute path unless it's already absolute */
	if(!strstr(WIN32_PATH,":") && WIN32_PATH[0] != '\\'){
		GetModuleFileNameA(NULL, win32path, MAX_PATH);
		strcpy(strrchr(win32path, '\\') + 1, WIN32_PATH);
	}
	else strcpy(win32path,WIN32_PATH);
	strcpy(tmppath,win32path);
#ifdef USE_REALCODECS
	/* Expand to absolute path unless it's already absolute */
	if(!strstr(REALCODEC_PATH,":") && REALCODEC_PATH[0] != '\\'){
		GetModuleFileNameA(NULL, realpath, MAX_PATH);
		strcpy(strrchr(realpath, '\\') + 1, REALCODEC_PATH);
	}
	else strcpy(realpath,REALCODEC_PATH);
	sprintf(tmppath,"%s;%s",win32path,realpath);
#endif /*USE_REALCODECS*/
#endif /*__CYGWIN__*/
	mp_msg(MSGT_WIN32, MSGL_V,"Setting PATH to %s\n",tmppath);
	if (!SetEnvironmentVariableA("PATH", tmppath))
		mp_msg(MSGT_WIN32, MSGL_WARN, "Cannot set PATH!");
}
#endif /*WIN32 && USE_WIN32DLL*/
