
/* Version number of package */
#define VERSION "1.2.2"
#define HAVE_UNISTD_H 1

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
#ifdef HAVE_MPLAYER

#include "../config.h"

#else

#undef WORDS_BIGENDIAN

/* Define if you have the <stddef.h> header file.  */
#define HAVE_STDDEF_H 1

/* Define if <sys/dvdio.h> defines dvd_struct. */
#undef DVD_STRUCT_IN_SYS_DVDIO_H

/* Define if <sys/cdio.h> defines dvd_struct. */
#undef DVD_STRUCT_IN_SYS_CDIO_H

/* Define if <linux/cdrom.h> defines DVD_STRUCT. */
#define DVD_STRUCT_IN_LINUX_CDROM_H 1

/* Define if <dvd.h> defines DVD_STRUCT. */
#undef DVD_STRUCT_IN_DVD_H

/* Define if <extras/BSDI_dvdioctl/dvd.h> defines DVD_STRUCT. */
#undef DVD_STRUCT_IN_BSDI_DVDIOCTL_DVD_H

/* Have userspace SCSI headers. */
#undef SOLARIS_USCSI

/* Define if Linux-like dvd_struct is defined. */
#define HAVE_LINUX_DVD_STRUCT 1

/* Define if BSD-like dvd_struct is defined. */
#undef HAVE_BSD_DVD_STRUCT

#endif

/* assert support */
#undef HAVE_ASSERT_H

#ifndef HAVE_ASSERT_H
 #define assert( ... ) do {} while(0)
#endif
