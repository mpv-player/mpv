/*
 * dct36_k7.c - 3DNowEx(DSP)! optimized dct36()
 *
 * This code based 'dct36_3dnow.s' by Syuuhei Kashiyama
 * <squash@mb.kcom.ne.jp>, only two types of changes have been made:
 *
 * - added new opcode PSWAPD
 * - removed PREFETCH instruction for speedup
 * - changed function name for support 3DNowEx! automatic detection
 *
 * note: because K7 processors are an aggresive out-of-order three-way
 *       superscalar ones instruction order is not significand for them.
 *
 * You can find Kashiyama's original 3dnow! support patch
 * (for mpg123-0.59o) at
 * http://user.ecc.u-tokyo.ac.jp/~g810370/linux-simd/ (Japanese).
 *
 * by KIMURA Takuhiro <kim@hannah.ipc.miyakyo-u.ac.jp> - until 31.Mar.1999
 *                    <kim@comtec.co.jp>               - after  1.Apr.1999
 *
 * Original disclaimer:
 *  The author of this program disclaim whole expressed or implied
 *  warranties with regard to this program, and in no event shall the
 *  author of this program liable to whatever resulted from the use of
 *  this program. Use it at your own risk.
 *
 * Modified by Nick Kurshev <nickols_k@mail.ru>
 *
 * 2003/06/21: Moved to GCC inline assembly - Alex Beregszaszi
 */

#define __DCT36_OPTIMIZE_FOR_K7

#include "dct36_3dnow.c"
