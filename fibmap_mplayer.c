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

#ifndef FIBMAP
#define FIBMAP 1
#endif

int main ( int argc , char ** argv )
{
	int fd,lba=0;
	if (argc!=2) {
	    fprintf(stderr,"Bad usage.\n");
	    return 1;
	}
	if ((fd = open(argv[1], O_RDONLY)) == -1) {
    	    fprintf(stderr,"Cannot open file %s: %s\n",
	    argv[1] ? argv[1] : "(NULL)", strerror(errno));
    	    return 1;
	}
        if (ioctl(fd, FIBMAP, &lba) != 0) {
	    fprintf(stderr,"fibmap ioctl: %s (Hint: %s is not suid root?)\n",strerror(errno),argv[0]);
            close(fd);
            return 1;
        }
	close(fd);
	printf("%d\n",lba);
	return 0;
}
