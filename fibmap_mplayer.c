/* (C)2001,2002 by LGB (Gábor Lénárt), lgb@lgb.hu
   Part of MPlayer project, this source is copyrighted according to GNU/GPL.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include "mp_msg.h"

#ifndef FIBMAP
#define FIBMAP 1
#endif

int main ( int argc , char ** argv )
{
	int fd,ret,lba=0;
	if (geteuid()!=0) {
	    mp_msg(MSGT_CPLAYER,MSGL_FATAL, "%s must be setuid root to work\n",
	    argv[0]);
                       return 1;
	}
	if (seteuid(getuid()) == -1) {
	    mp_msg(MSGT_CPLAYER,MSGL_FATAL, "Couldn't drop privileges: %s\n",
	    strerror(errno));
	    return 1;
	}
	if (argc!=2 || argv[1]==NULL) {
	    mp_msg(MSGT_CPLAYER,MSGL_FATAL,"Usage: %s <filename>\n", argv[0]);
	    return 1;
	}
	if ((fd = open(argv[1], O_RDONLY)) == -1) {
    	    mp_msg(MSGT_CPLAYER,MSGL_FATAL,"Cannot open file %s: %s\n",
	    argv[1], strerror(errno));
    	    return 1;
	}
        if (seteuid(0) == -1) {
            mp_msg(MSGT_CPLAYER,MSGL_FATAL, "Couldn't restore root privileges: %s\n",
            strerror(errno));
            return 1;
        }
        ret = ioctl(fd, FIBMAP, &lba);
        if (seteuid(getuid()) == -1) {
            mp_msg(MSGT_CPLAYER,MSGL_FATAL, "Couldn't re-drop privileges: %s\n",
            strerror(errno));
            return 1;
        }
        close(fd);
        if (ret != 0) {
	    mp_msg(MSGT_CPLAYER,MSGL_FATAL,"fibmap ioctl failed: %s\n",
	    strerror(errno));
            return 1;
        }
	printf("%d\n",lba);
	return 0;
}
