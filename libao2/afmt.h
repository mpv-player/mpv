
/* Defines that AFMT_ stuff */

#ifdef	HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>	/* For AFMT_* on linux */
#else 
#ifdef HAVE_SOUNDCARD_H
#include <soundcard.h> /* OpenBSD have this instead of <sys/soundcard> */
#endif
#endif

/* standard, old OSS audio formats */
#ifndef AFMT_MU_LAW
#	define AFMT_MU_LAW		0x00000001
#	define AFMT_A_LAW		0x00000002
#	define AFMT_IMA_ADPCM		0x00000004
#	define AFMT_U8			0x00000008
#	define AFMT_S16_LE		0x00000010	/* Little endian signed 16*/
#	define AFMT_S16_BE		0x00000020	/* Big endian signed 16 */
#	define AFMT_S8			0x00000040
#	define AFMT_U16_LE		0x00000080	/* Little endian U16 */
#	define AFMT_U16_BE		0x00000100	/* Big endian U16 */
#endif

#ifndef AFMT_MPEG
#	define AFMT_MPEG		0x00000200	/* MPEG (2) audio */
#endif

#ifndef AFMT_AC3
#	define AFMT_AC3			0x00000400	/* Dolby Digital AC3 */
#endif

/* 32 bit formats (MSB aligned) formats */
#ifndef AFMT_S32_LE
# define AFMT_S32_LE              0x00001000
# define AFMT_S32_BE              0x00002000
#endif

