/*
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 */

#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "codecpath.h"

char* def_path = BINARY_CODECS_PATH;

static int needs_free=0;
void SetCodecPath(const char* path)
{
    if(needs_free)free(def_path);
    if(path==0)
    {
	def_path = BINARY_CODECS_PATH;
	needs_free=0;
	return;
    }
    def_path = malloc(strlen(path)+1);
    strcpy(def_path, path);
    needs_free=1;
}
