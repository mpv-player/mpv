
char *get_path(char *filename){
	char *homedir;
	char *buff;
#if defined(__MINGW32__)||defined(__CYGWIN__)
    static char *config_dir = "/mplayer";
#else
	static char *config_dir = "/.mplayer";
#endif
	int len;

	if ((homedir = getenv("HOME")) == NULL)
#if defined(__MINGW32__)||defined(__CYGWIN__) /*hack to get fonts etc. loaded outside of cygwin environment*/
	{
    	int __stdcall GetModuleFileNameA(void* hModule,char* lpFilename,int nSize);
        int i,imax=0;       
        char exedir[MAX_PATH];       
        GetModuleFileNameA(NULL, exedir, MAX_PATH);
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
	mp_msg(MSGT_GLOBAL,MSGL_V,"get_path('%s') -> '%s'\n",filename,buff);
	return buff;
}
