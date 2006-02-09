
/*
   Written by Mark Podlipec <podlipec@ici.net>.

   Most of this code comes from a GSM 06.10 library by
   Jutta Degener and Carsten Bormann, available via
   <http://www.pobox.com/~jutta/toast.html>.

   That library is distributed with the following copyright:

    Copyright 1992 by Jutta Degener and Carsten Bormann,
    Technische Universitaet Berlin

Any use of this software is permitted provided that this notice is not
removed and that neither the authors nor the Technische Universitaet Berlin
are deemed to have made any representations as to the suitability of this
software for any purpose nor are held responsible for any defects of
this software.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.

As a matter of courtesy, the authors request to be informed about uses
this software has found, about bugs in this software, and about any
improvements that may be of general interest.

    Berlin, 15.09.1992
    Jutta Degener
    Carsten Bormann
*/


#include <stdio.h>
#include <string.h>
#include <assert.h>  /* POD optional */
#include "xa_gsm_int.h"

//void XA_MSGSM_Decoder();
static void GSM_Decode();
static void Gsm_RPE_Decoding();

//static short gsm_buf[320];
static XA_GSM_STATE gsm_state;


void GSM_Init(void)
{
  memset((char *)(&gsm_state), 0, sizeof(XA_GSM_STATE));
  gsm_state.nrp = 40;
}


/*   Table 4.3b   Quantization levels of the LTP gain quantizer
 */
/* bc                 0          1        2          3                  */
static word gsm_QLB[4] = {  3277,    11469,    21299,     32767        };

/*   Table 4.6   Normalized direct mantissa used to compute xM/xmax
 */
/* i                  0      1       2      3      4      5      6      7   */
static word gsm_FAC[8] = { 18431, 20479, 22527, 24575, 26623, 28671, 30719, 32767 };



/****************/
#define saturate(x)     \
        ((x) < MIN_WORD ? MIN_WORD : (x) > MAX_WORD ? MAX_WORD: (x))

/****************/
static word gsm_sub (a,b)
word a;
word b;
{
        longword diff = (longword)a - (longword)b;
        return saturate(diff);
}

/****************/
static word gsm_asr (a,n)
word a; 
int n;
{
        if (n >= 16) return -(a < 0);
        if (n <= -16) return 0;
        if (n < 0) return a << -n;

#       ifdef   SASR
                return a >> n;
#       else
                if (a >= 0) return a >> n;
                else return -(word)( -(uword)a >> n );
#       endif
}

/****************/
static word gsm_asl (a,n)
word a; 
int n;
{
        if (n >= 16) return 0;
        if (n <= -16) return -(a < 0);
        if (n < 0) return gsm_asr(a, -n);
        return a << n;
}


/*
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

/**** 4.2.17 */
static void RPE_grid_positioning(Mc,xMp,ep)
word            Mc;             /* grid position        IN      */
register word   * xMp;          /* [0..12]              IN      */
register word   * ep;           /* [0..39]              OUT     */
/*
 *  This procedure computes the reconstructed long term residual signal
 *  ep[0..39] for the LTP analysis filter.  The inputs are the Mc
 *  which is the grid position selection and the xMp[0..12] decoded
 *  RPE samples which are upsampled by a factor of 3 by inserting zero
 *  values.
 */
{
        int     i = 13;

        assert(0 <= Mc && Mc <= 3);

        switch (Mc) {
                case 3: *ep++ = 0;
                case 2:  do {
                                *ep++ = 0;
                case 1:         *ep++ = 0;
                case 0:         *ep++ = *xMp++;
                         } while (--i);
        }
        while (++Mc < 4) *ep++ = 0;

        /*

        int i, k;
        for (k = 0; k <= 39; k++) ep[k] = 0;
        for (i = 0; i <= 12; i++) {
                ep[ Mc + (3*i) ] = xMp[i];
        }
        */
}


/**** 4.2.16 */
static void APCM_inverse_quantization (xMc,mant,exp,xMp)
register word   * xMc;  /* [0..12]                      IN      */
word            mant;
word            exp;
register word   * xMp;  /* [0..12]                      OUT     */
/* 
 *  This part is for decoding the RPE sequence of coded xMc[0..12]
 *  samples to obtain the xMp[0..12] array.  Table 4.6 is used to get
 *  the mantissa of xmaxc (FAC[0..7]).
 */
{
        int     i;
        word    temp, temp1, temp2, temp3;
        longword        ltmp;

        assert( mant >= 0 && mant <= 7 ); 

        temp1 = gsm_FAC[ mant ];        /* see 4.2-15 for mant */
        temp2 = gsm_sub( 6, exp );      /* see 4.2-15 for exp  */
        temp3 = gsm_asl( 1, gsm_sub( temp2, 1 ));

        for (i = 13; i--;) {

                assert( *xMc <= 7 && *xMc >= 0 );       /* 3 bit unsigned */

                /* temp = gsm_sub( *xMc++ << 1, 7 ); */
                temp = (*xMc++ << 1) - 7;               /* restore sign   */
                assert( temp <= 7 && temp >= -7 );      /* 4 bit signed   */

                temp <<= 12;                            /* 16 bit signed  */
                temp = GSM_MULT_R( temp1, temp );
                temp = GSM_ADD( temp, temp3 );
                *xMp++ = gsm_asr( temp, temp2 );
        }
}


/**** 4.12.15 */
static void APCM_quantization_xmaxc_to_exp_mant (xmaxc,exp_out,mant_out)
word            xmaxc;          /* IN   */
word            * exp_out;      /* OUT  */
word            * mant_out;    /* OUT  */
{
  word    exp, mant;

  /* Compute exponent and mantissa of the decoded version of xmaxc
   */

        exp = 0;
        if (xmaxc > 15) exp = SASR(xmaxc, 3) - 1;
        mant = xmaxc - (exp << 3);

        if (mant == 0) {
                exp  = -4;
                mant = 7;
        }
        else {
                while (mant <= 7) {
                        mant = mant << 1 | 1;
                        exp--;
                }
                mant -= 8;
        }

        assert( exp  >= -4 && exp <= 6 );
        assert( mant >= 0 && mant <= 7 );

        *exp_out  = exp;
        *mant_out = mant;
}

static void Gsm_RPE_Decoding (S, xmaxcr, Mcr, xMcr, erp)
XA_GSM_STATE        * S;
word            xmaxcr;
word            Mcr;
word            * xMcr;  /* [0..12], 3 bits             IN      */
word            * erp;   /* [0..39]                     OUT     */

{
        word    exp, mant;
        word    xMp[ 13 ];

        APCM_quantization_xmaxc_to_exp_mant( xmaxcr, &exp, &mant );
        APCM_inverse_quantization( xMcr, mant, exp, xMp );
        RPE_grid_positioning( Mcr, xMp, erp );

}


/*
 *  4.3 FIXED POINT IMPLEMENTATION OF THE RPE-LTP DECODER
 */

static void Postprocessing(S,s)
XA_GSM_STATE	* S;
register word 	* s;
{
  register int		k;
  register word		msr = S->msr;
  register longword	ltmp;	/* for GSM_ADD */
  register word		tmp;

  for (k = 160; k--; s++) 
  {
    tmp = GSM_MULT_R( msr, 28180 );
    msr = GSM_ADD(*s, tmp);  	   /* Deemphasis 	     */
    *s  = GSM_ADD(msr, msr) & 0xFFF8;  /* Truncation & Upscaling */
  }
  S->msr = msr;
}

/**** 4.3.2 */
void Gsm_Long_Term_Synthesis_Filtering (S,Ncr,bcr,erp,drp)
XA_GSM_STATE        * S;
word                    Ncr;
word                    bcr;
register word           * erp;     /* [0..39]                    IN */
register word           * drp;     /* [-120..-1] IN, [-120..40] OUT */

/*
 *  This procedure uses the bcr and Ncr parameter to realize the
 *  long term synthesis filtering.  The decoding of bcr needs
 *  table 4.3b.
 */
{
        register longword       ltmp;   /* for ADD */
        register int            k;
        word                    brp, drpp, Nr;

        /*  Check the limits of Nr.
         */
        Nr = Ncr < 40 || Ncr > 120 ? S->nrp : Ncr;
        S->nrp = Nr;
        assert(Nr >= 40 && Nr <= 120);

        /*  Decoding of the LTP gain bcr
         */
        brp = gsm_QLB[ bcr ];

        /*  Computation of the reconstructed short term residual 
         *  signal drp[0..39]
         */
        assert(brp != MIN_WORD);

        for (k = 0; k <= 39; k++) {
                drpp   = GSM_MULT_R( brp, drp[ k - Nr ] );
                drp[k] = GSM_ADD( erp[k], drpp );
        }

        /*
         *  Update of the reconstructed short term residual signal
         *  drp[ -1..-120 ]
         */

        for (k = 0; k <= 119; k++) drp[ -120 + k ] = drp[ -80 + k ];
}

static void Short_term_synthesis_filtering (S,rrp,k,wt,sr)
XA_GSM_STATE *S;
register word   *rrp;  /* [0..7]       IN      */
register int    k;      /* k_end - k_start      */
register word   *wt;   /* [0..k-1]     IN      */
register word   *sr;   /* [0..k-1]     OUT     */
{
        register word           * v = S->v;
        register int            i;
        register word           sri, tmp1, tmp2;
        register longword       ltmp;   /* for GSM_ADD  & GSM_SUB */

        while (k--) {
                sri = *wt++;
                for (i = 8; i--;) {

                        /* sri = GSM_SUB( sri, gsm_mult_r( rrp[i], v[i] ) );
                         */
                        tmp1 = rrp[i];
                        tmp2 = v[i];
                        tmp2 =  ( tmp1 == MIN_WORD && tmp2 == MIN_WORD
                                ? MAX_WORD
                                : 0x0FFFF & (( (longword)tmp1 * (longword)tmp2
                                             + 16384) >> 15)) ;

                        sri  = GSM_SUB( sri, tmp2 );

                        /* v[i+1] = GSM_ADD( v[i], gsm_mult_r( rrp[i], sri ) );
                         */
                        tmp1  = ( tmp1 == MIN_WORD && sri == MIN_WORD
                                ? MAX_WORD
                                : 0x0FFFF & (( (longword)tmp1 * (longword)sri
                                             + 16384) >> 15)) ;

                        v[i+1] = GSM_ADD( v[i], tmp1);
                }
                *sr++ = v[0] = sri;
        }
}

/* 4.2.8 */

static void Decoding_of_the_coded_Log_Area_Ratios (LARc,LARpp)
word    * LARc;         /* coded log area ratio [0..7]  IN      */
word    * LARpp;        /* out: decoded ..                      */
{
        register word   temp1 /* , temp2 */;
        register long   ltmp;   /* for GSM_ADD */

        /*  This procedure requires for efficient implementation
         *  two tables.
         *
         *  INVA[1..8] = integer( (32768 * 8) / real_A[1..8])
         *  MIC[1..8]  = minimum value of the LARc[1..8]
         */

        /*  Compute the LARpp[1..8]
         */

        /*      for (i = 1; i <= 8; i++, B++, MIC++, INVA++, LARc++, LARpp++) {
         *
         *              temp1  = GSM_ADD( *LARc, *MIC ) << 10;
         *              temp2  = *B << 1;
         *              temp1  = GSM_SUB( temp1, temp2 );
         *
         *              assert(*INVA != MIN_WORD);
         *
         *              temp1  = GSM_MULT_R( *INVA, temp1 );
         *              *LARpp = GSM_ADD( temp1, temp1 );
         *      }
         */

#undef  STEP
#define STEP( B, MIC, INVA )    \
                temp1    = GSM_ADD( *LARc++, MIC ) << 10;       \
                temp1    = GSM_SUB( temp1, B << 1 );            \
                temp1    = GSM_MULT_R( INVA, temp1 );           \
                *LARpp++ = GSM_ADD( temp1, temp1 );

        STEP(      0,  -32,  13107 );
        STEP(      0,  -32,  13107 );
        STEP(   2048,  -16,  13107 );
        STEP(  -2560,  -16,  13107 );

        STEP(     94,   -8,  19223 );
        STEP(  -1792,   -8,  17476 );
        STEP(   -341,   -4,  31454 );
        STEP(  -1144,   -4,  29708 );

        /* NOTE: the addition of *MIC is used to restore
         *       the sign of *LARc.
         */
}

/* 4.2.9 */
/* Computation of the quantized reflection coefficients 
 */

/* 4.2.9.1  Interpolation of the LARpp[1..8] to get the LARp[1..8]
 */

/*
 *  Within each frame of 160 analyzed speech samples the short term
 *  analysis and synthesis filters operate with four different sets of
 *  coefficients, derived from the previous set of decoded LARs(LARpp(j-1))
 *  and the actual set of decoded LARs (LARpp(j))
 *
 * (Initial value: LARpp(j-1)[1..8] = 0.)
 */

static void Coefficients_0_12 (LARpp_j_1, LARpp_j, LARp)
register word * LARpp_j_1;
register word * LARpp_j;
register word * LARp;
{
        register int    i;
        register longword ltmp;

        for (i = 1; i <= 8; i++, LARp++, LARpp_j_1++, LARpp_j++) {
                *LARp = GSM_ADD( SASR( *LARpp_j_1, 2 ), SASR( *LARpp_j, 2 ));
                *LARp = GSM_ADD( *LARp,  SASR( *LARpp_j_1, 1));
        }
}

static void Coefficients_13_26 (LARpp_j_1, LARpp_j, LARp)
register word * LARpp_j_1;
register word * LARpp_j;
register word * LARp;
{
        register int i;
        register longword ltmp;
        for (i = 1; i <= 8; i++, LARpp_j_1++, LARpp_j++, LARp++) {
                *LARp = GSM_ADD( SASR( *LARpp_j_1, 1), SASR( *LARpp_j, 1 ));
        }
}

static void Coefficients_27_39 (LARpp_j_1, LARpp_j, LARp)
register word * LARpp_j_1;
register word * LARpp_j;
register word * LARp;
{
        register int i;
        register longword ltmp;

        for (i = 1; i <= 8; i++, LARpp_j_1++, LARpp_j++, LARp++) {
                *LARp = GSM_ADD( SASR( *LARpp_j_1, 2 ), SASR( *LARpp_j, 2 ));
                *LARp = GSM_ADD( *LARp, SASR( *LARpp_j, 1 ));
        }
}


static void Coefficients_40_159 (LARpp_j, LARp)
register word * LARpp_j;
register word * LARp;
{
        register int i;

        for (i = 1; i <= 8; i++, LARp++, LARpp_j++)
                *LARp = *LARpp_j;
}
/* 4.2.9.2 */

static void LARp_to_rp (LARp)
register word * LARp;   /* [0..7] IN/OUT  */
/*
 *  The input of this procedure is the interpolated LARp[0..7] array.
 *  The reflection coefficients, rp[i], are used in the analysis
 *  filter and in the synthesis filter.
 */
{
        register int            i;
        register word           temp;
        register longword       ltmp;

        for (i = 1; i <= 8; i++, LARp++) {

                /* temp = GSM_ABS( *LARp );
                 *
                 * if (temp < 11059) temp <<= 1;
                 * else if (temp < 20070) temp += 11059;
                 * else temp = GSM_ADD( temp >> 2, 26112 );
                 *
                 * *LARp = *LARp < 0 ? -temp : temp;
                 */

                if (*LARp < 0) {
                        temp = *LARp == MIN_WORD ? MAX_WORD : -(*LARp);
                        *LARp = - ((temp < 11059) ? temp << 1
                                : ((temp < 20070) ? temp + 11059
                                :  GSM_ADD( temp >> 2, 26112 )));
                } else {
                        temp  = *LARp;
                        *LARp =    (temp < 11059) ? temp << 1
                                : ((temp < 20070) ? temp + 11059
                                :  GSM_ADD( temp >> 2, 26112 ));
                }
        }
}





/**** */
static void Gsm_Short_Term_Synthesis_Filter (S, LARcr, wt, s)
XA_GSM_STATE * S;
word    * LARcr;        /* received log area ratios [0..7] IN  */
word    * wt;           /* received d [0..159]             IN  */
word    * s;            /* signal   s [0..159]            OUT  */
{
        word            * LARpp_j       = S->LARpp[ S->j     ];
        word            * LARpp_j_1     = S->LARpp[ S->j ^=1 ];

        word            LARp[8];

#undef  FILTER
#if     defined(FAST) && defined(USE_FLOAT_MUL)

#       define  FILTER  (* (S->fast                     \
                           ? Fast_Short_term_synthesis_filtering        \
                           : Short_term_synthesis_filtering     ))
#else
#       define  FILTER  Short_term_synthesis_filtering
#endif

        Decoding_of_the_coded_Log_Area_Ratios( LARcr, LARpp_j );

        Coefficients_0_12( LARpp_j_1, LARpp_j, LARp );
        LARp_to_rp( LARp );
        FILTER( S, LARp, 13, wt, s );

        Coefficients_13_26( LARpp_j_1, LARpp_j, LARp);
        LARp_to_rp( LARp );
        FILTER( S, LARp, 14, wt + 13, s + 13 );

        Coefficients_27_39( LARpp_j_1, LARpp_j, LARp);
        LARp_to_rp( LARp );
        FILTER( S, LARp, 13, wt + 27, s + 27 );

        Coefficients_40_159( LARpp_j, LARp );
        LARp_to_rp( LARp );
        FILTER(S, LARp, 120, wt + 40, s + 40);
}




static void GSM_Decode(S,LARcr, Ncr,bcr,Mcr,xmaxcr,xMcr,s)
XA_GSM_STATE	*S;
word		*LARcr;		/* [0..7]		IN	*/
word		*Ncr;		/* [0..3] 		IN 	*/
word		*bcr;		/* [0..3]		IN	*/
word		*Mcr;		/* [0..3] 		IN 	*/
word		*xmaxcr;	/* [0..3]		IN 	*/
word		*xMcr;		/* [0..13*4]		IN	*/
word		*s;		/* [0..159]		OUT 	*/
{
  int		j, k;
  word		erp[40], wt[160];
  word		*drp = S->dp0 + 120;

  for (j=0; j <= 3; j++, xmaxcr++, bcr++, Ncr++, Mcr++, xMcr += 13) 
  {
    Gsm_RPE_Decoding( S, *xmaxcr, *Mcr, xMcr, erp );
    Gsm_Long_Term_Synthesis_Filtering( S, *Ncr, *bcr, erp, drp );
    for (k = 0; k <= 39; k++) wt[ j * 40 + k ] =  drp[ k ];
  }

  Gsm_Short_Term_Synthesis_Filter( S, LARcr, wt, s );
  Postprocessing(S, s);
}



/****-------------------------------------------------------------------****
 **** Podlipec:  For AVI/WAV files GSM 6.10 combines two 33 bytes frames
 **** into one 65 byte frame.
 ****-------------------------------------------------------------------****/
void XA_MSGSM_Decoder(unsigned char *ibuf,unsigned short *obuf)
{ word sr;
  word  LARc[8], Nc[4], Mc[4], bc[4], xmaxc[4], xmc[13*4];	

  sr = *ibuf++;

  LARc[0] = sr & 0x3f;  sr >>= 6;
  sr |= (word)*ibuf++ << 2;
  LARc[1] = sr & 0x3f;  sr >>= 6;
  sr |= (word)*ibuf++ << 4;
  LARc[2] = sr & 0x1f;  sr >>= 5;
  LARc[3] = sr & 0x1f;  sr >>= 5;
  sr |= (word)*ibuf++ << 2;
  LARc[4] = sr & 0xf;  sr >>= 4;
  LARc[5] = sr & 0xf;  sr >>= 4;
  sr |= (word)*ibuf++ << 2;			/* 5 */
  LARc[6] = sr & 0x7;  sr >>= 3;
  LARc[7] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 4;
  Nc[0] = sr & 0x7f;  sr >>= 7;
  bc[0] = sr & 0x3;  sr >>= 2;
  Mc[0] = sr & 0x3;  sr >>= 2;
  sr |= (word)*ibuf++ << 1;
  xmaxc[0] = sr & 0x3f;  sr >>= 6;
  xmc[0] = sr & 0x7;  sr >>= 3;
  sr = *ibuf++;
  xmc[1] = sr & 0x7;  sr >>= 3;
  xmc[2] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 2;
  xmc[3] = sr & 0x7;  sr >>= 3;
  xmc[4] = sr & 0x7;  sr >>= 3;
  xmc[5] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 1;			/* 10 */
  xmc[6] = sr & 0x7;  sr >>= 3;
  xmc[7] = sr & 0x7;  sr >>= 3;
  xmc[8] = sr & 0x7;  sr >>= 3;
  sr = *ibuf++;
  xmc[9] = sr & 0x7;  sr >>= 3;
  xmc[10] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 2;
  xmc[11] = sr & 0x7;  sr >>= 3;
  xmc[12] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 4;
  Nc[1] = sr & 0x7f;  sr >>= 7;
  bc[1] = sr & 0x3;  sr >>= 2;
  Mc[1] = sr & 0x3;  sr >>= 2;
  sr |= (word)*ibuf++ << 1;
  xmaxc[1] = sr & 0x3f;  sr >>= 6;
  xmc[13] = sr & 0x7;  sr >>= 3;
  sr = *ibuf++;				/* 15 */
  xmc[14] = sr & 0x7;  sr >>= 3;
  xmc[15] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 2;
  xmc[16] = sr & 0x7;  sr >>= 3;
  xmc[17] = sr & 0x7;  sr >>= 3;
  xmc[18] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 1;
  xmc[19] = sr & 0x7;  sr >>= 3;
  xmc[20] = sr & 0x7;  sr >>= 3;
  xmc[21] = sr & 0x7;  sr >>= 3;
  sr = *ibuf++;
  xmc[22] = sr & 0x7;  sr >>= 3;
  xmc[23] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 2;
  xmc[24] = sr & 0x7;  sr >>= 3;
  xmc[25] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 4;			/* 20 */
  Nc[2] = sr & 0x7f;  sr >>= 7;
  bc[2] = sr & 0x3;  sr >>= 2;
  Mc[2] = sr & 0x3;  sr >>= 2;
  sr |= (word)*ibuf++ << 1;
  xmaxc[2] = sr & 0x3f;  sr >>= 6;
  xmc[26] = sr & 0x7;  sr >>= 3;
  sr = *ibuf++;
  xmc[27] = sr & 0x7;  sr >>= 3;
  xmc[28] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 2;
  xmc[29] = sr & 0x7;  sr >>= 3;
  xmc[30] = sr & 0x7;  sr >>= 3;
  xmc[31] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 1;
  xmc[32] = sr & 0x7;  sr >>= 3;
  xmc[33] = sr & 0x7;  sr >>= 3;
  xmc[34] = sr & 0x7;  sr >>= 3;
  sr = *ibuf++;				/* 25 */
  xmc[35] = sr & 0x7;  sr >>= 3;
  xmc[36] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 2;
  xmc[37] = sr & 0x7;  sr >>= 3;
  xmc[38] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 4;
  Nc[3] = sr & 0x7f;  sr >>= 7;
  bc[3] = sr & 0x3;  sr >>= 2;
  Mc[3] = sr & 0x3;  sr >>= 2;
  sr |= (word)*ibuf++ << 1;
  xmaxc[3] = sr & 0x3f;  sr >>= 6;
  xmc[39] = sr & 0x7;  sr >>= 3;
  sr = *ibuf++;
  xmc[40] = sr & 0x7;  sr >>= 3;
  xmc[41] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 2;			/* 30 */
  xmc[42] = sr & 0x7;  sr >>= 3;
  xmc[43] = sr & 0x7;  sr >>= 3;
  xmc[44] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 1;
  xmc[45] = sr & 0x7;  sr >>= 3;
  xmc[46] = sr & 0x7;  sr >>= 3;
  xmc[47] = sr & 0x7;  sr >>= 3;
  sr = *ibuf++;
  xmc[48] = sr & 0x7;  sr >>= 3;
  xmc[49] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 2;
  xmc[50] = sr & 0x7;  sr >>= 3;
  xmc[51] = sr & 0x7;  sr >>= 3;

  GSM_Decode(&gsm_state, LARc, Nc, bc, Mc, xmaxc, xmc, obuf);

/*
  carry = sr & 0xf;
  sr = carry;
*/
  /* 2nd frame */
  sr &= 0xf;
  sr |= (word)*ibuf++ << 4;			/* 1 */
  LARc[0] = sr & 0x3f;  sr >>= 6;
  LARc[1] = sr & 0x3f;  sr >>= 6;
  sr = *ibuf++;
  LARc[2] = sr & 0x1f;  sr >>= 5;
  sr |= (word)*ibuf++ << 3;
  LARc[3] = sr & 0x1f;  sr >>= 5;
  LARc[4] = sr & 0xf;  sr >>= 4;
  sr |= (word)*ibuf++ << 2;
  LARc[5] = sr & 0xf;  sr >>= 4;
  LARc[6] = sr & 0x7;  sr >>= 3;
  LARc[7] = sr & 0x7;  sr >>= 3;
  sr = *ibuf++;				/* 5 */
  Nc[0] = sr & 0x7f;  sr >>= 7;
  sr |= (word)*ibuf++ << 1;
  bc[0] = sr & 0x3;  sr >>= 2;
  Mc[0] = sr & 0x3;  sr >>= 2;
  sr |= (word)*ibuf++ << 5;
  xmaxc[0] = sr & 0x3f;  sr >>= 6;
  xmc[0] = sr & 0x7;  sr >>= 3;
  xmc[1] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 1;
  xmc[2] = sr & 0x7;  sr >>= 3;
  xmc[3] = sr & 0x7;  sr >>= 3;
  xmc[4] = sr & 0x7;  sr >>= 3;
  sr = *ibuf++;
  xmc[5] = sr & 0x7;  sr >>= 3;
  xmc[6] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 2;			/* 10 */
  xmc[7] = sr & 0x7;  sr >>= 3;
  xmc[8] = sr & 0x7;  sr >>= 3;
  xmc[9] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 1;
  xmc[10] = sr & 0x7;  sr >>= 3;
  xmc[11] = sr & 0x7;  sr >>= 3;
  xmc[12] = sr & 0x7;  sr >>= 3;
  sr = *ibuf++;
  Nc[1] = sr & 0x7f;  sr >>= 7;
  sr |= (word)*ibuf++ << 1;
  bc[1] = sr & 0x3;  sr >>= 2;
  Mc[1] = sr & 0x3;  sr >>= 2;
  sr |= (word)*ibuf++ << 5;
  xmaxc[1] = sr & 0x3f;  sr >>= 6;
  xmc[13] = sr & 0x7;  sr >>= 3;
  xmc[14] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 1;			/* 15 */
  xmc[15] = sr & 0x7;  sr >>= 3;
  xmc[16] = sr & 0x7;  sr >>= 3;
  xmc[17] = sr & 0x7;  sr >>= 3;
  sr = *ibuf++;
  xmc[18] = sr & 0x7;  sr >>= 3;
  xmc[19] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 2;
  xmc[20] = sr & 0x7;  sr >>= 3;
  xmc[21] = sr & 0x7;  sr >>= 3;
  xmc[22] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 1;
  xmc[23] = sr & 0x7;  sr >>= 3;
  xmc[24] = sr & 0x7;  sr >>= 3;
  xmc[25] = sr & 0x7;  sr >>= 3;
  sr = *ibuf++;
  Nc[2] = sr & 0x7f;  sr >>= 7;
  sr |= (word)*ibuf++ << 1;			/* 20 */
  bc[2] = sr & 0x3;  sr >>= 2;
  Mc[2] = sr & 0x3;  sr >>= 2;
  sr |= (word)*ibuf++ << 5;
  xmaxc[2] = sr & 0x3f;  sr >>= 6;
  xmc[26] = sr & 0x7;  sr >>= 3;
  xmc[27] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 1;	
  xmc[28] = sr & 0x7;  sr >>= 3;
  xmc[29] = sr & 0x7;  sr >>= 3;
  xmc[30] = sr & 0x7;  sr >>= 3;
  sr = *ibuf++;
  xmc[31] = sr & 0x7;  sr >>= 3;
  xmc[32] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 2;
  xmc[33] = sr & 0x7;  sr >>= 3;
  xmc[34] = sr & 0x7;  sr >>= 3;
  xmc[35] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 1;			/* 25 */
  xmc[36] = sr & 0x7;  sr >>= 3;
  xmc[37] = sr & 0x7;  sr >>= 3;
  xmc[38] = sr & 0x7;  sr >>= 3;
  sr = *ibuf++;
  Nc[3] = sr & 0x7f;  sr >>= 7;
  sr |= (word)*ibuf++ << 1;		
  bc[3] = sr & 0x3;  sr >>= 2;
  Mc[3] = sr & 0x3;  sr >>= 2;
  sr |= (word)*ibuf++ << 5;
  xmaxc[3] = sr & 0x3f;  sr >>= 6;
  xmc[39] = sr & 0x7;  sr >>= 3;
  xmc[40] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 1;
  xmc[41] = sr & 0x7;  sr >>= 3;
  xmc[42] = sr & 0x7;  sr >>= 3;
  xmc[43] = sr & 0x7;  sr >>= 3;
  sr = (word)*ibuf++;				/* 30 */
  xmc[44] = sr & 0x7;  sr >>= 3;
  xmc[45] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 2;
  xmc[46] = sr & 0x7;  sr >>= 3;
  xmc[47] = sr & 0x7;  sr >>= 3;
  xmc[48] = sr & 0x7;  sr >>= 3;
  sr |= (word)*ibuf++ << 1;
  xmc[49] = sr & 0x7;  sr >>= 3;
  xmc[50] = sr & 0x7;  sr >>= 3;
  xmc[51] = sr & 0x7;  sr >>= 3;

  GSM_Decode(&gsm_state, LARc, Nc, bc, Mc, xmaxc, xmc, &obuf[160]);

  /* Return number of source bytes consumed and output samples produced */
//  *icnt = 65;		
//  *ocnt = 320;
  return;
}

#define GSM_MAGIC 0xd

void XA_GSM_Decoder(unsigned char *ibuf,unsigned short *obuf)
{ word  LARc[8], Nc[4], Mc[4], bc[4], xmaxc[4], xmc[13*4];	

	/* Sanity */
  if (((*ibuf >> 4) & 0x0F) != GSM_MAGIC)
  { int i;
    for(i=0;i<160;i++) obuf[i] = 0;
//    *icnt = 33;
//    *ocnt = 160;
    return;
  }

  LARc[0]  = (*ibuf++ & 0xF) << 2;           /* 1 */
  LARc[0] |= (*ibuf >> 6) & 0x3;
  LARc[1]  = *ibuf++ & 0x3F;
  LARc[2]  = (*ibuf >> 3) & 0x1F;
  LARc[3]  = (*ibuf++ & 0x7) << 2;
  LARc[3] |= (*ibuf >> 6) & 0x3;
  LARc[4]  = (*ibuf >> 2) & 0xF;
  LARc[5]  = (*ibuf++ & 0x3) << 2;
  LARc[5] |= (*ibuf >> 6) & 0x3;
  LARc[6]  = (*ibuf >> 3) & 0x7;
  LARc[7]  = *ibuf++ & 0x7;

  Nc[0]  = (*ibuf >> 1) & 0x7F;

  bc[0]  = (*ibuf++ & 0x1) << 1;
  bc[0] |= (*ibuf >> 7) & 0x1;

  Mc[0]  = (*ibuf >> 5) & 0x3;

  xmaxc[0]  = (*ibuf++ & 0x1F) << 1;
  xmaxc[0] |= (*ibuf >> 7) & 0x1;

  xmc[0]  = (*ibuf >> 4) & 0x7;
  xmc[1]  = (*ibuf >> 1) & 0x7;
  xmc[2]  = (*ibuf++ & 0x1) << 2;
  xmc[2] |= (*ibuf >> 6) & 0x3;
  xmc[3]  = (*ibuf >> 3) & 0x7;
  xmc[4]  = *ibuf++ & 0x7;
  xmc[5]  = (*ibuf >> 5) & 0x7;
  xmc[6]  = (*ibuf >> 2) & 0x7;
  xmc[7]  = (*ibuf++ & 0x3) << 1;            /* 10 */
  xmc[7] |= (*ibuf >> 7) & 0x1;
  xmc[8]  = (*ibuf >> 4) & 0x7;
  xmc[9]  = (*ibuf >> 1) & 0x7;
  xmc[10]  = (*ibuf++ & 0x1) << 2;
  xmc[10] |= (*ibuf >> 6) & 0x3;
  xmc[11]  = (*ibuf >> 3) & 0x7;
  xmc[12]  = *ibuf++ & 0x7;

  Nc[1]  = (*ibuf >> 1) & 0x7F;

  bc[1]  = (*ibuf++ & 0x1) << 1;
  bc[1] |= (*ibuf >> 7) & 0x1;

  Mc[1]  = (*ibuf >> 5) & 0x3;

  xmaxc[1]  = (*ibuf++ & 0x1F) << 1;
  xmaxc[1] |= (*ibuf >> 7) & 0x1;


  xmc[13]  = (*ibuf >> 4) & 0x7;
  xmc[14]  = (*ibuf >> 1) & 0x7;
  xmc[15]  = (*ibuf++ & 0x1) << 2;
  xmc[15] |= (*ibuf >> 6) & 0x3;
  xmc[16]  = (*ibuf >> 3) & 0x7;
  xmc[17]  = *ibuf++ & 0x7;
  xmc[18]  = (*ibuf >> 5) & 0x7;
  xmc[19]  = (*ibuf >> 2) & 0x7;
  xmc[20]  = (*ibuf++ & 0x3) << 1;
  xmc[20] |= (*ibuf >> 7) & 0x1;
  xmc[21]  = (*ibuf >> 4) & 0x7;
  xmc[22]  = (*ibuf >> 1) & 0x7;
  xmc[23]  = (*ibuf++ & 0x1) << 2;
  xmc[23] |= (*ibuf >> 6) & 0x3;
  xmc[24]  = (*ibuf >> 3) & 0x7;
  xmc[25]  = *ibuf++ & 0x7;

  Nc[2]  = (*ibuf >> 1) & 0x7F;

  bc[2]  = (*ibuf++ & 0x1) << 1;             /* 20 */
  bc[2] |= (*ibuf >> 7) & 0x1;

  Mc[2]  = (*ibuf >> 5) & 0x3;

  xmaxc[2]  = (*ibuf++ & 0x1F) << 1;
  xmaxc[2] |= (*ibuf >> 7) & 0x1;


  xmc[26]  = (*ibuf >> 4) & 0x7;
  xmc[27]  = (*ibuf >> 1) & 0x7;
  xmc[28]  = (*ibuf++ & 0x1) << 2;
  xmc[28] |= (*ibuf >> 6) & 0x3;
  xmc[29]  = (*ibuf >> 3) & 0x7;
  xmc[30]  = *ibuf++ & 0x7;
  xmc[31]  = (*ibuf >> 5) & 0x7;
  xmc[32]  = (*ibuf >> 2) & 0x7;
  xmc[33]  = (*ibuf++ & 0x3) << 1;
  xmc[33] |= (*ibuf >> 7) & 0x1;
  xmc[34]  = (*ibuf >> 4) & 0x7;
  xmc[35]  = (*ibuf >> 1) & 0x7;
  xmc[36]  = (*ibuf++ & 0x1) << 2;
  xmc[36] |= (*ibuf >> 6) & 0x3;
  xmc[37]  = (*ibuf >> 3) & 0x7;
  xmc[38]  = *ibuf++ & 0x7;

  Nc[3]  = (*ibuf >> 1) & 0x7F;

  bc[3]  = (*ibuf++ & 0x1) << 1;
  bc[3] |= (*ibuf >> 7) & 0x1;

  Mc[3]  = (*ibuf >> 5) & 0x3;

  xmaxc[3]  = (*ibuf++ & 0x1F) << 1;
  xmaxc[3] |= (*ibuf >> 7) & 0x1;

  xmc[39]  = (*ibuf >> 4) & 0x7;
  xmc[40]  = (*ibuf >> 1) & 0x7;
  xmc[41]  = (*ibuf++ & 0x1) << 2;
  xmc[41] |= (*ibuf >> 6) & 0x3;
  xmc[42]  = (*ibuf >> 3) & 0x7;
  xmc[43]  = *ibuf++ & 0x7;                  /* 30  */
  xmc[44]  = (*ibuf >> 5) & 0x7;
  xmc[45]  = (*ibuf >> 2) & 0x7;
  xmc[46]  = (*ibuf++ & 0x3) << 1;
  xmc[46] |= (*ibuf >> 7) & 0x1;
  xmc[47]  = (*ibuf >> 4) & 0x7;
  xmc[48]  = (*ibuf >> 1) & 0x7;
  xmc[49]  = (*ibuf++ & 0x1) << 2;
  xmc[49] |= (*ibuf >> 6) & 0x3;
  xmc[50]  = (*ibuf >> 3) & 0x7;
  xmc[51]  = *ibuf & 0x7;                    /* 33 */

  GSM_Decode(&gsm_state, LARc, Nc, bc, Mc, xmaxc, xmc, obuf);

  /* Return number of source bytes consumed and output samples produced */
//  *icnt = 33;
//  *ocnt = 160;
}
