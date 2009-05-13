/*
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

#include <sys/types.h>
#include <stdio.h>

#include "config.h"

#include <windows.h>
#include "glob.h"

int glob(const char *pattern, int flags,
          int (*errfunc)(const char *epath, int eerrno), glob_t *pglob)
{
	HANDLE searchhndl;
    WIN32_FIND_DATA found_file;
 	if(errfunc)printf("glob():ERROR:Sorry errfunc not supported by this implementation\n");
	if(flags)printf("glob():ERROR:Sorry no flags supported by this globimplementation\n");
	//printf("PATTERN \"%s\"\n",pattern);
	pglob->gl_pathc = 0;
	searchhndl = FindFirstFile( pattern,&found_file);
    if(searchhndl == INVALID_HANDLE_VALUE)
	{
		if(GetLastError() == ERROR_FILE_NOT_FOUND)
		{
			pglob->gl_pathc = 0;
		    //printf("could not find a file matching your search criteria\n");
	        return 1;
		}
		else
		{
			//printf("glob():ERROR:FindFirstFile: %i\n",GetLastError());
			return 1;
		}
	 }
    pglob->gl_pathv = malloc(sizeof(char*));
    pglob->gl_pathv[0] = strdup(found_file.cFileName);
    pglob->gl_pathc++;
    while(1)
    {
		if(!FindNextFile(searchhndl,&found_file))
		{
			if(GetLastError()==ERROR_NO_MORE_FILES)
			{
				//printf("glob(): no more files found\n");
                break;
			}
			else
			{
				//printf("glob():ERROR:FindNextFile:%i\n",GetLastError());
				return 1;
			}
		}
		else
		{
            //printf("glob: found file %s\n",found_file.cFileName);
            pglob->gl_pathc++;
            pglob->gl_pathv = realloc(pglob->gl_pathv,pglob->gl_pathc * sizeof(char*));
            pglob->gl_pathv[pglob->gl_pathc-1] = strdup(found_file.cFileName);
 		}
    }
    FindClose(searchhndl);
    return 0;
}

void globfree(glob_t *pglob)
{
	int i;
	for(i=0; i <pglob->gl_pathc ;i++)
	{
		free(pglob->gl_pathv[i]);
	}
	free(pglob->gl_pathv);
}

#if 0
int main(void){
   glob_t        gg;
   printf("globtest\n");
   glob( "*.jpeg",0,NULL,&gg );
   {
        int i;
        for(i=0;i<gg.gl_pathc;i++)printf("GLOBED:%i %s\n",i,gg.gl_pathv[i]);
    }
   globfree(&gg);

   return 0;
}
#endif
