
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

char *get_path(char *filename){
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
#endif

	if ((homedir = getenv("HOME")) == NULL)
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
	CFIndex maxlen=64;
	CFURLRef resources=NULL;
	
		free(buff);
		buff=NULL;
		
		resources=CFBundleCopyResourcesDirectoryURL(CFBundleGetMainBundle());
		if(resources) {

			buff=malloc(maxlen);
			*buff=0;
		
			while(!CFURLGetFileSystemRepresentation(resources, true, buff, maxlen)) {
				maxlen*=2;
				buff=realloc(buff, maxlen);
			}
			CFRelease(resources);
		}
			
		if(buff&&filename) {
			if((strlen(filename)+strlen(buff)+2)>maxlen) {
				maxlen=strlen(filename)+strlen(buff)+2;
				buff=realloc(buff, maxlen);
			}
			strcat(buff,"/");
			strcat(buff, filename);
		}
	}
#endif
	mp_msg(MSGT_GLOBAL,MSGL_V,"get_path('%s') -> '%s'\n",filename,buff);
	return buff;
}
