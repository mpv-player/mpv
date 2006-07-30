/********************************************************************
 *
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 *
 * The contents of the file COPYRIGHT:
 *
 * Any use of this software is permitted provided that this notice is not
 * removed and that neither the authors nor the Technische Universitaet Berlin
 * are deemed to have made any representations as to the suitability of this
 * software for any purpose nor are held responsible for any defects of
 * this software.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 *
 * As a matter of courtesy, the authors request to be informed about uses
 * this software has found, about bugs in this software, and about any
 * improvements that may be of general interest.
 *
 *     Berlin, 15.09.1992
 *     Jutta Degener
 *     Carsten Bormann
 *
 ********************************************************************/

#include "xa_gsm.h"


typedef short			word;		/* 16 bit signed int	*/
typedef int			longword;	/* 32 bit signed int	*/

typedef unsigned short		uword;		/* unsigned word	*/
typedef unsigned int		ulongword;	/* unsigned longword	*/

typedef struct {

	word		dp0[ 280 ];

	word		z1;		/* preprocessing.c, Offset_com. */
	longword	L_z2;		/*                  Offset_com. */
	int		mp;		/*                  Preemphasis	*/

	word		u[8];		/* short_term_aly_filter.c	*/
	word		LARpp[2][8]; 	/*                              */
	word		j;		/*                              */

	word            ltp_cut;        /* long_term.c, LTP crosscorr.  */
	word		nrp; /* 40 */	/* long_term.c, synthesis	*/
	word		v[9];		/* short_term.c, synthesis	*/
	word		msr;		/* decoder.c,	Postprocessing	*/

	char		verbose;	/* only used if !NDEBUG		*/
	char		fast;		/* only used if FAST		*/

	char		wav_fmt;	/* only used if WAV49 defined	*/
	unsigned char	frame_index;	/*            odd/even chaining	*/
	unsigned char	frame_chain;	/*   half-byte to carry forward	*/
} XA_GSM_STATE;


#define	MIN_WORD	(-32767 - 1)
#define	MAX_WORD	  32767

#define	MIN_LONGWORD	(-2147483647 - 1)
#define	MAX_LONGWORD	  2147483647

#ifdef	SASR		/* flag: >> is a signed arithmetic shift right */
#undef	SASR
#define	SASR(x, by)	((x) >> (by))
#else
#define	SASR(x, by)	((x) >= 0 ? (x) >> (by) : (~(-((x) + 1) >> (by))))
#endif	/* SASR */



/*
 *  Inlined functions from add.h 
 */

/* 
 * #define GSM_MULT_R(a, b) (* word a, word b, !(a == b == MIN_WORD) *)	\
 *	(0x0FFFF & SASR(((longword)(a) * (longword)(b) + 16384), 15))
 */
#define GSM_MULT_R(a, b) /* word a, word b, !(a == b == MIN_WORD) */	\
	(SASR( ((longword)(a) * (longword)(b) + 16384), 15 ))

# define GSM_MULT(a,b)	 /* word a, word b, !(a == b == MIN_WORD) */	\
	(SASR( ((longword)(a) * (longword)(b)), 15 ))

# define GSM_L_MULT(a, b) /* word a, word b */	\
	(((longword)(a) * (longword)(b)) << 1)

# define GSM_L_ADD(a, b)	\
	( (a) <  0 ? ( (b) >= 0 ? (a) + (b)	\
		 : (utmp = (ulongword)-((a) + 1) + (ulongword)-((b) + 1)) \
		   >= MAX_LONGWORD ? MIN_LONGWORD : -(longword)utmp-2 )   \
	: ((b) <= 0 ? (a) + (b)   \
	          : (utmp = (ulongword)(a) + (ulongword)(b)) >= MAX_LONGWORD \
		    ? MAX_LONGWORD : utmp))

/*
 * # define GSM_ADD(a, b)	\
 * 	((ltmp = (longword)(a) + (longword)(b)) >= MAX_WORD \
 * 	? MAX_WORD : ltmp <= MIN_WORD ? MIN_WORD : ltmp)
 */
/* Nonportable, but faster: */

#define	GSM_ADD(a, b)	\
	((ulongword)((ltmp = (longword)(a) + (longword)(b)) - MIN_WORD) > \
		MAX_WORD - MIN_WORD ? (ltmp > 0 ? MAX_WORD : MIN_WORD) : ltmp)

# define GSM_SUB(a, b)	\
	((ltmp = (longword)(a) - (longword)(b)) >= MAX_WORD \
	? MAX_WORD : ltmp <= MIN_WORD ? MIN_WORD : ltmp)

# define GSM_ABS(a)	((a) < 0 ? ((a) == MIN_WORD ? MAX_WORD : -(a)) : (a))

/* Use these if necessary:

# define GSM_MULT_R(a, b)	gsm_mult_r(a, b)
# define GSM_MULT(a, b)		gsm_mult(a, b)
# define GSM_L_MULT(a, b)	gsm_L_mult(a, b)

# define GSM_L_ADD(a, b)	gsm_L_add(a, b)
# define GSM_ADD(a, b)		gsm_add(a, b)
# define GSM_SUB(a, b)		gsm_sub(a, b)

# define GSM_ABS(a)		gsm_abs(a)

*/

