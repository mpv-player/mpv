/* (C)2001 by LGB (Gabor Lenart), based on example programs in libcss
   Some TODO: root privilegies really needed??  */

/* don't do anything with this source if css support was not requested */
#include "config.h"
#ifdef HAVE_LIBCSS

#include <stdio.h>
#include <stdlib.h>
#include <linux/cdrom.h>
// FIXME #include <string.h> conflicts with #include <linux/fs.h> (below)
//#include <string.h>  // FIXME this conflicts with #include <linux/fs.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <css.h>

#include "dvdauth.h"

char *dvd_device=NULL;
unsigned char key_disc[2048];
unsigned char key_title[5];


#include <linux/fs.h>

#ifndef FIBMAP
#define FIBMAP 1
#endif


static int path_to_lba ( int fd )
{
        int lba = 0;
        if (ioctl(fd, FIBMAP, &lba) < 0) {
	        perror ("ioctl FIBMAP");
		fprintf(stderr,"Hint: run mplayer as root!\n");
//	        close(fd);
	        return -1;
	}
	return lba;
}



static void reset_agids ( int fd )
{
        dvd_authinfo ai;
        int i;
        for (i = 0; i < 4; i++) {
	        memset(&ai, 0, sizeof(ai));
	        ai.type = DVD_INVALIDATE_AGID;
	        ai.lsa.agid = i;
	        ioctl(fd, DVD_AUTH, &ai);
	}
}



int dvd_auth ( char *dev , int fd )
{
        int devfd;  /* FD of DVD device */
        int lba;

//	printf("DVD: auth fd=%d on %s.\n",fd,dev);

	if ((devfd=open(dev,O_RDONLY))<0) {
		fprintf(stderr,"DVD: cannot open DVD device \"%s\".\n",dev);
		return 1;
	}

	/* reset AGIDs */
	reset_agids(devfd);

	/* authenticate disc */
	if (CSSAuthDisc(devfd,key_disc)) {
		fprintf(stderr,"DVD: CSSAuthDisc() failed.\n");
		return 1;
	}

	/* authenticate title */
        lba=path_to_lba(fd);
	if (lba==-1) {
		fprintf(stderr,"DVD: path_to_lba() failed.\n");
		return 1;
	}
        if (CSSAuthTitle(devfd,key_title,lba)) {
		fprintf(stderr,"DVD: CSSAuthTitle() failed.\n");
		return 1;
	}

	/* decrypting title */
        if (CSSDecryptTitleKey (key_title, key_disc) < 0) {
                fprintf(stderr,"DVD: CSSDecryptTitleKey() failed.\n");
		return 1;
	}

	close(devfd);
	return 0;
}


#endif
