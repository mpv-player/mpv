/* (C)2001,2002 by LGB (Gabor Lenart), based on example programs in libcss
                lgb@lgb.hu
		
   This source is part of MPlayer project. This source is copyrighted by
   the author according to rules declared in GNU/GPL license.

   2001		Inital version (LGB)
   2001		fibmap_mplayer to avoid uid=0 mplayer need (LGB)
   2001		Support for libcss with the new API (by ???)
   2002/Jan/04  Use dlopen to access libcss.so to avoid conflict with
                libdvdread [now with only libcss with old API (LGB)
   2002/Sep/21  Fix a bug which caused segmentation fault when using
		-dvdkey option, since css.so was only loaded by -dvdauth.
		(LGB, reported and suggested by "me andi" <wortelsapje@hotmail.com>)
		Also some cosmetic fix with return value of dvd_css_descramble().
   2002/Sep/21  Try to load css syms with AND without underscore at their
		names, probably not only OpenBSD requires this so it's a
		better solution (LGB).
		
   TODO:
		support for libcss libraries with new API		     */

/* don't do anything with this source if css support was not requested */
#include "config.h"
#ifdef HAVE_LIBCSS

#warning FIXME: Dynamic loading of libcss.so with newer (ver>0.1) libcss API is not supported in this version!

#include <stdio.h>
#include <stdlib.h>
//#include <string.h>      // FIXME: conflicts with fs.h
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
// #include <css.h>

#if defined(__FreeBSD__) || defined(__NetBSD__)
#  include <sys/dvdio.h>
#elif defined(__OpenBSD__)
#  include <sys/cdio.h>
#  define RTLD_NOW RTLD_LAZY
#elif defined(__linux__)
#  include <linux/cdrom.h>
#elif defined(__sun)
#  include <sun/dvdio.h>
#elif defined(hpux)
#  include <sun/scsi.h>
#elif defined(__bsdi__)
#  include <dvd.h>
#else
#  error "Need the DVD ioctls"
#endif

#include <dlfcn.h>
#include "dvdauth.h"


// #if	OLD_CSS_API
/*
 * provide some backward compatibiliy macros to compile this
 * code using the old libcss-0.1
 */
#define	DVDHandle		int
#define	DVDOpenFailed		(-1)

#define	DVDAuth(hdl, s)		ioctl(hdl, DVD_AUTH, s)
#define	DVDOpenDevice(path)	open(path, O_RDONLY)
#define	DVDCloseDevice(hdl)	close(hdl)
// #define	CSSDVDisEncrypted(hdl)	CSSisEncrypted(hdl)
// #define	CSSDVDAuthDisc		CSSAuthDisc


char *dvd_auth_device=NULL;
extern char *dvd_device;
char *dvd_raw_device=NULL;
char *css_so=NULL;
unsigned char key_disc[2048];
unsigned char key_title[5];
unsigned char *dvdimportkey=NULL;
int descrambling=0;


static int css_so_is_loaded=0;

static void *dlid;
static int (*dl_CSSisEncrypted)(int);
static int (*dl_CSSAuthDisc)(int,char *);
static int (*dl_CSSAuthTitle)(int, char *,int);
static int (*dl_CSSGetASF)(int);
static int (*dl_CSSDecryptTitleKey)(char *, char *);
static void (*dl_CSSDescramble)(u_char *, u_char *);


void dvd_css_descramble ( u_char *sec , u_char *key )
{
	(*dl_CSSDescramble)(sec,key);
}


#ifdef __linux__
#include <linux/fs.h>

#ifndef FIBMAP
#define FIBMAP 1
#endif


static int path_to_lba (char *path)
{
    int lba = 0;
    char cmd[100];
    FILE *fp;

    snprintf(cmd,sizeof(cmd),"fibmap_mplayer %s",path);
    fp=popen(cmd,"r");
    if (fp) {
	    int ret;
	    memset(cmd,0,sizeof(cmd));
	    fgets(cmd,99,fp);
//	    printf("DVD: cmd: %s\n",cmd);
	    if ((ret=pclose(fp)))
		    fprintf(stderr,"DVD: fibmap_mplayer: %s\n",*cmd?cmd:"no error info");
	    if (cmd[0]<'0'||cmd[0]>'9') fp=NULL; else {
		if(WIFEXITED(ret) && !WEXITSTATUS(ret)) {
		    lba=atoi(cmd);
		    printf("DVD: fibmap_mplayer is being used\n");
		} else
		    fp=NULL;
	    }
    }
    if (!fp) {
	int fd;
	printf("DVD: fibmap_mplayer could not run, trying with ioctl() ...\n");
	if ((fd = open(path, O_RDONLY)) == -1) {
    	    fprintf(stderr, "DVD: Cannot open file %s: %s",
	    path ? path : "(NULL)", strerror(errno));
    	    return -1;
	}
        if (ioctl(fd, FIBMAP, &lba) != 0) {
            perror("DVD: ioctl FIBMAP");
	    fprintf(stderr,"  Hint: run mplayer as root (or better to install fibmap_mplayer as suid root)!\n");
            close(fd);
            return -1;
        }
	close(fd);
    }
    printf("DVD: LBA: %d\n",lba);
    return lba;
}

#else /*linux*/

static int path_to_lba (char *path)
{
#warning translating pathname to iso9660 LBA is not supported on this platform
    fprintf(stderr, "DVD: Translating pathname to iso9660 LBA is not supported on this platform\n");
    return -1;
}

#endif /*linux*/


static int CSSDVDAuthTitlePath(DVDHandle hdl,unsigned char *key_title,char *path)
{
	int lba=path_to_lba(path);
	if (lba==-1) return -1;
	return (*dl_CSSAuthTitle)(hdl,key_title,lba);
}		
		


static void reset_agids ( DVDHandle dvd )
{
#if !defined(DVD_AUTH) && defined(DVDIOCREPORTKEY)
	struct dvd_authinfo ai;
	int i;
	for (i = 0; i < 4; i++) {
		memset(&ai, 0, sizeof(ai));
		ai.format = DVD_INVALIDATE_AGID;
		ai.agid = i;
		ioctl(dvd, DVDIOCREPORTKEY, &ai);
	}
#else
#if defined(__OpenBSD__)
        union
#endif
        dvd_authinfo ai;
        int i;
        for (i = 0; i < 4; i++) {
	        memset(&ai, 0, sizeof(ai));
	        ai.type = DVD_INVALIDATE_AGID;
	        ai.lsa.agid = i;
		DVDAuth(dvd, &ai);
	}
#endif
}


static int dvd_load_css_so ( void )
{
	if (css_so_is_loaded) {
		printf("DVD: warning: attempt to load css.so twice, ignoring.\n");
		return 0;
	}
	if (!css_so) css_so=strdup("libcss.so");
	printf("DVD: opening libcss.so as %s ...\n",css_so);
	dlid=dlopen(css_so,RTLD_NOW);
	if (!dlid) {
		printf("DVD: dlopen: %s\n",dlerror());
		return 1;
	} printf("DVD: dlopen OK!\n");

/* #ifdef __OpenBSD__
#define CSS_DLSYM(v,s) if (!(v=dlsym(dlid,"_" s))) {\
fprintf(stderr,"DVD: %s\n  Hint: use libcss version 0.1!\n",dlerror());\
return 1; }
#else
#define CSS_DLSYM(v,s) if (!(v=dlsym(dlid,s))) {\
fprintf(stderr,"DVD: %s\n  Hint: use libcss version 0.1!\n",dlerror());\
return 1; }
#endif */

#define CSS_DLSYM(v,s) \
if (!(v=dlsym(dlid,s))) {\
	if (!(v=dlsym(dlid,"_" s))) {\
		fprintf(stderr,"DVD: %s\n Hint: use libcss version 0.1!\n",dlerror());\
		return 1;\
	}\
}


	CSS_DLSYM(dl_CSSisEncrypted,"CSSisEncrypted");
	CSS_DLSYM(dl_CSSAuthDisc,"CSSAuthDisc");
	CSS_DLSYM(dl_CSSAuthTitle,"CSSAuthTitle");
	CSS_DLSYM(dl_CSSGetASF,"CSSGetASF");
	CSS_DLSYM(dl_CSSDecryptTitleKey,"CSSDecryptTitleKey");
	CSS_DLSYM(dl_CSSDescramble,"CSSDescramble");

#undef CSS_DLSYM

	css_so_is_loaded=1;
	return 0;
}


int dvd_import_key ( unsigned char *hexkey )
{
	unsigned char *t=key_title;
	int digit=4,len;
	memset(key_title,0,sizeof(key_title));
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
	printf("DVD: DVD key (requested): %02X%02X%02X%02X%02X\n",key_title[0],key_title[1],key_title[2],key_title[3],key_title[4]);
	if (dvd_load_css_so()) return 1;
	descrambling=1;
	return 0;
}



int dvd_auth ( char *dev , char *filename )
{
    	DVDHandle dvd;  /* DVD device handle */

	if (dvd_load_css_so()) return 1;

	if ((dvd=DVDOpenDevice(dev)) == DVDOpenFailed) {
		fprintf(stderr,"DVD: cannot open DVD device \"%s\": %s.\n",
			dev, strerror(errno));
		return 1;
	}

	if (!(*dl_CSSisEncrypted)(dvd)) {
		printf("DVD: DVD is unencrypted! Skipping authentication!\n(note: you should not use -dvd switch for unencrypted discs!)\n");
		DVDCloseDevice(dvd);
		return 0;
	} else printf("DVD: DVD is encrypted, issuing authentication ...\n");
	/* reset AGIDs */
	reset_agids(dvd);
	/* authenticate disc */
	if ((*dl_CSSAuthDisc)(dvd,key_disc)) {
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
        if ((*dl_CSSDecryptTitleKey)(key_title, key_disc) < 0) {
                fprintf(stderr,"DVD: CSSDecryptTitleKey() failed.\n");
		DVDCloseDevice(dvd);
		return 1;
	}

	DVDCloseDevice(dvd);
	printf("DVD: DVD title key is: %02X%02X%02X%02X%02X\n",key_title[0],key_title[1],key_title[2],key_title[3],key_title[4]);
	descrambling=1;
	return 0;
}


#endif
