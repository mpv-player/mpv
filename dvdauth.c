/* (C)2001 by LGB (Gabor Lenart), based on example programs in libcss
           lgb@lgb.hu                                                        */

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

char *dvd_auth_device=NULL;
unsigned char key_disc[2048];
unsigned char key_title[5];
unsigned char *dvdimportkey=NULL;
int descrambling=0;


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


int dvd_import_key ( unsigned char *hexkey )
{
	unsigned char *t=key_title;
	int digit=4,len;
	bzero(key_title,sizeof(key_title));
//	printf("DVD key: %s\n",hexkey);
	for (len=0;len<10;len++) {
//		printf("-> %c\n",*hexkey);
		if (!*hexkey) return 1;
		if (*hexkey>='A'&&*hexkey<='F') *t|=(*hexkey-'A'+10)<<digit;
			else if (*hexkey>='0'&&*hexkey<='9') *t|=(*hexkey-'0')<<digit;
				else return 1;
		if (digit) digit=0; else {
			digit=4;
			t++;
		}
		hexkey++;
	}
	if (*hexkey) return 1;
	printf("DVD key (requested): %02X%02X%02X%02X%02X\n",key_title[0],key_title[1],key_title[2],key_title[3],key_title[4]);
	descrambling=1;
	return 0;
}



int dvd_auth ( char *dev , int fd )
{
        int devfd;  /* FD of DVD device */
        int lba;


	if ((devfd=open(dev,O_RDONLY))<0) {
		fprintf(stderr,"DVD: cannot open DVD device \"%s\".\n",dev);
		return 1;
	}
	
	if (!CSSisEncrypted(devfd)) {
		printf("DVD is unencrypted! Skipping authentication!\n(note: you should not use -dvd switch for unencrypted discs!)\n");
		return 0;
	} else printf("DVD is encrypted, issuing authentication ...\n");

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
	printf("DVD title key is: %02X%02X%02X%02X%02X\n",key_title[0],key_title[1],key_title[2],key_title[3],key_title[4]);
	descrambling=1;
	return 0;
}


#endif
