/* This program is licensed under the GNU Library General Public License, version 2,
 * a copy of which is included with this program (with filename LICENSE.LGPL).
 *
 * (c) 2002 John Edwards
 * mostly lifted from work by Frank Klemm
 * random functions for dithering.
 *
 * last modified:
 * $Id$
 */
#include "common.h"

#ifndef FIXED_POINT

#include <string.h>
#include "dither.h"
#include "common.h"


double
Random_Equi ( double mult )                     // gives a equal distributed random number
{                                               // between -2^31*mult and +2^31*mult
	return mult * (int) random_int ();
}

double
Random_Triangular ( double mult )               // gives a triangular distributed random number
{                                               // between -2^32*mult and +2^32*mult
	return mult * ( (double) (int) random_int () + (double) (int) random_int () );
}

/*********************************************************************************************************************/

static const float32_t  F44_0 [16 + 32] = {
	(float)0, (float)0, (float)0, (float)0, (float)0, (float)0, (float)0, (float)0,
	(float)0, (float)0, (float)0, (float)0, (float)0, (float)0, (float)0, (float)0,

	(float)0, (float)0, (float)0, (float)0, (float)0, (float)0, (float)0, (float)0,
	(float)0, (float)0, (float)0, (float)0, (float)0, (float)0, (float)0, (float)0,

	(float)0, (float)0, (float)0, (float)0, (float)0, (float)0, (float)0, (float)0,
	(float)0, (float)0, (float)0, (float)0, (float)0, (float)0, (float)0, (float)0
};


static const float32_t  F44_1 [16 + 32] = {  /* SNR(w) = 4.843163 dB, SNR = -3.192134 dB */
	(float) 0.85018292704024355931, (float) 0.29089597350995344721, (float)-0.05021866022121039450, (float)-0.23545456294599161833,
	(float)-0.58362726442227032096, (float)-0.67038978965193036429, (float)-0.38566861572833459221, (float)-0.15218663390367969967,
	(float)-0.02577543084864530676, (float) 0.14119295297688728127, (float) 0.22398848581628781612, (float) 0.15401727203382084116,
	(float) 0.05216161232906000929, (float)-0.00282237820999675451, (float)-0.03042794608323867363, (float)-0.03109780942998826024,

	(float) 0.85018292704024355931, (float) 0.29089597350995344721, (float)-0.05021866022121039450, (float)-0.23545456294599161833,
	(float)-0.58362726442227032096, (float)-0.67038978965193036429, (float)-0.38566861572833459221, (float)-0.15218663390367969967,
	(float)-0.02577543084864530676, (float) 0.14119295297688728127, (float) 0.22398848581628781612, (float) 0.15401727203382084116,
	(float) 0.05216161232906000929, (float)-0.00282237820999675451, (float)-0.03042794608323867363, (float)-0.03109780942998826024,

	(float) 0.85018292704024355931, (float) 0.29089597350995344721, (float)-0.05021866022121039450, (float)-0.23545456294599161833,
	(float)-0.58362726442227032096, (float)-0.67038978965193036429, (float)-0.38566861572833459221, (float)-0.15218663390367969967,
	(float)-0.02577543084864530676, (float) 0.14119295297688728127, (float) 0.22398848581628781612, (float) 0.15401727203382084116,
	(float) 0.05216161232906000929, (float)-0.00282237820999675451, (float)-0.03042794608323867363, (float)-0.03109780942998826024,
};


static const float32_t  F44_2 [16 + 32] = {  /* SNR(w) = 10.060213 dB, SNR = -12.766730 dB */
	(float) 1.78827593892108555290, (float) 0.95508210637394326553, (float)-0.18447626783899924429, (float)-0.44198126506275016437,
	(float)-0.88404052492547413497, (float)-1.42218907262407452967, (float)-1.02037566838362314995, (float)-0.34861755756425577264,
	(float)-0.11490230170431934434, (float) 0.12498899339968611803, (float) 0.38065885268563131927, (float) 0.31883491321310506562,
	(float) 0.10486838686563442765, (float)-0.03105361685110374845, (float)-0.06450524884075370758, (float)-0.02939198261121969816,

	(float) 1.78827593892108555290, (float) 0.95508210637394326553, (float)-0.18447626783899924429, (float)-0.44198126506275016437,
	(float)-0.88404052492547413497, (float)-1.42218907262407452967, (float)-1.02037566838362314995, (float)-0.34861755756425577264,
	(float)-0.11490230170431934434, (float) 0.12498899339968611803, (float) 0.38065885268563131927, (float) 0.31883491321310506562,
	(float) 0.10486838686563442765, (float)-0.03105361685110374845, (float)-0.06450524884075370758, (float)-0.02939198261121969816,

	(float) 1.78827593892108555290, (float) 0.95508210637394326553, (float)-0.18447626783899924429, (float)-0.44198126506275016437,
	(float)-0.88404052492547413497, (float)-1.42218907262407452967, (float)-1.02037566838362314995, (float)-0.34861755756425577264,
	(float)-0.11490230170431934434, (float) 0.12498899339968611803, (float) 0.38065885268563131927, (float) 0.31883491321310506562,
	(float) 0.10486838686563442765, (float)-0.03105361685110374845, (float)-0.06450524884075370758, (float)-0.02939198261121969816,
};


static const float32_t  F44_3 [16 + 32] = {  /* SNR(w) = 15.382598 dB, SNR = -29.402334 dB */
	(float) 2.89072132015058161445, (float) 2.68932810943698754106, (float) 0.21083359339410251227, (float)-0.98385073324997617515,
	(float)-1.11047823227097316719, (float)-2.18954076314139673147, (float)-2.36498032881953056225, (float)-0.95484132880101140785,
	(float)-0.23924057925542965158, (float)-0.13865235703915925642, (float) 0.43587843191057992846, (float) 0.65903257226026665927,
	(float) 0.24361815372443152787, (float)-0.00235974960154720097, (float) 0.01844166574603346289, (float) 0.01722945988740875099,

	(float) 2.89072132015058161445, (float) 2.68932810943698754106, (float) 0.21083359339410251227, (float)-0.98385073324997617515,
	(float)-1.11047823227097316719, (float)-2.18954076314139673147, (float)-2.36498032881953056225, (float)-0.95484132880101140785,
	(float)-0.23924057925542965158, (float)-0.13865235703915925642, (float) 0.43587843191057992846, (float) 0.65903257226026665927,
	(float) 0.24361815372443152787, (float)-0.00235974960154720097, (float) 0.01844166574603346289, (float) 0.01722945988740875099,

	(float) 2.89072132015058161445, (float) 2.68932810943698754106, (float) 0.21083359339410251227, (float)-0.98385073324997617515,
	(float)-1.11047823227097316719, (float)-2.18954076314139673147, (float)-2.36498032881953056225, (float)-0.95484132880101140785,
	(float)-0.23924057925542965158, (float)-0.13865235703915925642, (float) 0.43587843191057992846, (float) 0.65903257226026665927,
	(float) 0.24361815372443152787, (float)-0.00235974960154720097, (float) 0.01844166574603346289, (float) 0.01722945988740875099
};


double
scalar16 ( const float32_t* x, const float32_t* y )
{
	return x[ 0]*y[ 0] + x[ 1]*y[ 1] + x[ 2]*y[ 2] + x[ 3]*y[ 3]
	     + x[ 4]*y[ 4] + x[ 5]*y[ 5] + x[ 6]*y[ 6] + x[ 7]*y[ 7]
	     + x[ 8]*y[ 8] + x[ 9]*y[ 9] + x[10]*y[10] + x[11]*y[11]
	     + x[12]*y[12] + x[13]*y[13] + x[14]*y[14] + x[15]*y[15];
}


void
Init_Dither ( unsigned char bits, unsigned char shapingtype )
{
	static uint8_t          default_dither [] = { 92, 92, 88, 84, 81, 78, 74, 67,  0,  0 };
	static const float32_t*              F [] = { F44_0, F44_1, F44_2, F44_3 };
	uint8_t                 index;

	if (shapingtype > 3) shapingtype = 3;
	index = bits - 11 - shapingtype;
	if (index > 9) index = 9;

	memset ( Dither.ErrorHistory , 0, sizeof (Dither.ErrorHistory ) );
	memset ( Dither.DitherHistory, 0, sizeof (Dither.DitherHistory) );

	Dither.FilterCoeff = F [shapingtype];
	Dither.Mask   = ((uint64_t)-1) << (32 - bits);
	Dither.Add    = 0.5     * ((1L << (32 - bits)) - 1);
	Dither.Dither = 0.01*default_dither[index] / (((int64_t)1) << bits);
}

#endif
