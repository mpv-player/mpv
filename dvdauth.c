/* (C)2001 by LGB (Gabor Lenart), based on example programs in libcss
           lgb@lgb.hu                                                        */

/* don't do anything with this source if css support was not requested */
#include "config.h"
#ifdef HAVE_LIBCSS

#include <stdio.h>
#include <stdlib.h>
//#include <string.h>      // FIXME: conflicts with fs.h
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <css.h>
#if CSS_MAJOR_VERSION > 0 || (CSS_MAJOR_VERSION == 0 && CSS_MINOR_VERSION > 1)
# include <dvd.h>
# undef	 OLD_CSS_API
#else
# if defined(__NetBSD__) || defined(__OpenBSD__)
#  include <sys/dvdio.h>
# elif defined(__linux__)
#  include <linux/cdrom.h>
# elif defined(__sun)
#  include <sun/dvdio.h>
# else
#  error "Need the DVD ioctls"
# endif
# define OLD_CSS_API 1
#endif

#include "dvdauth.h"


#if	OLD_CSS_API
/*
 * provide some backward compatibiliy macros to compile this
 * code using the old libcss-0.1
 */
#define	DVDHandle		int
#define	DVDOpenFailed		(-1)

#define	DVDAuth(hdl, s)		ioctl(hdl, DVD_AUTH, s)
#define	DVDOpenDevice(path)	open(path, O_RDONLY)
#define	DVDCloseDevice(hdl)	close(hdl)
#define	CSSDVDisEncrypted(hdl)	CSSisEncrypted(hdl)
#define	CSSDVDAuthDisc		CSSAuthDisc
#define	CSSDVDAuthTitlePath(hdl,key_title,path) \
		CSSAuthTitle(hdl,key_title,path_to_lba(path))

#else	/*OLD_CSS_API*/

#define DVDHandle		struct dvd_device *
#define	DVDOpenFailed		NULL

#endif	/*OLD_CSS_API*/


char *dvd_auth_device=NULL;
unsigned char key_disc[2048];
unsigned char key_title[5];
unsigned char *dvdimportkey=NULL;
int descrambling=0;


#if OLD_CSS_API
/*
 * With the old libcss-0.1 api, we have to find out the LBA for
 * a title for title authentication.
 */
#ifdef __linux__
#include <linux/fs.h>
#include <errno.h>

#ifndef FIBMAP
#define FIBMAP 1
#endif

static int path_to_lba (char *path)
{
    int fd, lba = 0;

    if ((fd = open(path, O_RDONLY)) == -1) {
        fprintf(stderr, "Cannot open file %s: %s",
		path ? path : "(NULL)", strerror(errno));
        return -1;
    }
    if (ioctl(fd, FIBMAP, &lba) != 0) {
        perror ("ioctl FIBMAP");
	fprintf(stderr,"Hint: run mplayer as root!\n");
        close(fd);
        return -1;
    }

    close(fd);

    return lba;
}
#else /*linux*/
static int path_to_lba (char *path)
{
#warning translating pathname to iso9660 LBA is not supported on this platform
    fprintf(stderr, "Translating pathname to iso9660 LBA is not supported on this platform\n");
    return -1;
}
#endif /*linux*/
#endif /*OLD_CSS_API*/


static void reset_agids ( DVDHandle dvd )
{
        dvd_authinfo ai;
        int i;
        for (i = 0; i < 4; i++) {
	        memset(&ai, 0, sizeof(ai));
	        ai.type = DVD_INVALIDATE_AGID;
	        ai.lsa.agid = i;
		DVDAuth(dvd, &ai);
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



int dvd_auth ( char *dev , char *filename )
{
    	DVDHandle dvd;  /* DVD device handle */

	if ((dvd=DVDOpenDevice(dev)) == DVDOpenFailed) {
		fprintf(stderr,"DVD: cannot open DVD device \"%s\".\n",dev);
		return 1;
	}
	
	if (!CSSDVDisEncrypted(dvd)) {
		printf("DVD is unencrypted! Skipping authentication!\n(note: you should not use -dvd switch for unencrypted discs!)\n");
		DVDCloseDevice(dvd);
		return 0;
	} else printf("DVD is encrypted, issuing authentication ...\n");

	/* reset AGIDs */
	reset_agids(dvd);

	/* authenticate disc */
	if (CSSDVDAuthDisc(dvd,key_disc)) {
		fprintf(stderr,"DVD: CSSDVDAuthDisc() failed.\n");
		DVDCloseDevice(dvd);
		return 1;
	}

        if (CSSDVDAuthTitlePath(dvd,key_title,filename)) {
		fprintf(stderr,"DVD: CSSDVDAuthTitle() failed.\n");
		DVDCloseDevice(dvd);
		return 1;
	}

	/* decrypting title */
        if (CSSDecryptTitleKey (key_title, key_disc) < 0) {
                fprintf(stderr,"DVD: CSSDecryptTitleKey() failed.\n");
		DVDCloseDevice(dvd);
		return 1;
	}

	DVDCloseDevice(dvd);
	printf("DVD title key is: %02X%02X%02X%02X%02X\n",key_title[0],key_title[1],key_title[2],key_title[3],key_title[4]);
	descrambling=1;
	return 0;
}


#endif
