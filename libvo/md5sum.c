/* ========================================================================== **
 *
 *                                    MD5.c
 *
 * Copyright:
 *  Copyright (C) 2003, 2004 by Christopher R. Hertel
 *
 * Email: crh@ubiqx.mn.org
 *
 * $Id$
 *
 * -------------------------------------------------------------------------- **
 *
 * Description:
 *  Implements the MD5 hash algorithm, as described in RFC 1321.
 *
 * -------------------------------------------------------------------------- **
 *
 * License:
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * -------------------------------------------------------------------------- **
 *
 * Notes:
 *
 *  None of this will make any sense unless you're studying RFC 1321 as you
 *  read the code.
 *
 *  MD5 is described in RFC 1321.
 *  The MD*4* algorithm is described in RFC 1320 (that's 1321 - 1).
 *  MD5 is very similar to MD4, but not quite similar enough to justify
 *  putting the two into a single module.  Besides, I wanted to add a few
 *  extra functions to this one to expand its usability.
 *
 *  There are three primary motivations for this particular implementation.
 *  1) Programmer's pride.  I wanted to be able to say I'd done it, and I
 *     wanted to learn from the experience.
 *  2) Portability.  I wanted an implementation that I knew to be portable
 *     to a reasonable number platforms.  In particular, the algorithm is
 *     designed with little-endian platforms in mind, but I wanted an
 *     endian-agnostic implementation.
 *  3) Compactness.  While not an overriding goal, I thought it worth-while
 *     to see if I could reduce the overall size of the result.  This is in
 *     keeping with my hopes that this library will be suitable for use in
 *     some embedded environments.
 *  Beyond that, cleanliness and clarity are always worth pursuing.
 *
 *  As mentioned above, the code really only makes sense if you are familiar
 *  with the MD5 algorithm or are using RFC 1321 as a guide.  This code is
 *  quirky, however, so you'll want to be reading carefully.
 *
 *  Yeah...most of the comments are cut-and-paste from my MD4 implementation.
 *
 * -------------------------------------------------------------------------- **
 *
 * References:
 *  IETF RFC 1321: The MD5 Message-Digest Algorithm
 *       Ron Rivest. IETF, April, 1992
 *
 * ========================================================================== **
 */

/* #include "MD5.h"   Line of original code */

#include "md5sum.h"   /* Added this line */

/* -------------------------------------------------------------------------- **
 * Static Constants:
 *
 *  K[][] - In round one, the values of k (which are used to index
 *          particular four-byte sequences in the input) are simply
 *          sequential.  In later rounds, however, they are a bit more
 *          varied.  Rather than calculate the values of k (which may
 *          or may not be possible--I haven't though about it) the
 *          values are stored in this array.
 *
 *  S[][] - In each round there is a left rotate operation performed as
 *          part of the 16 permutations.  The number of bits varies in
 *          a repeating patter.  This array keeps track of the patterns
 *          used in each round.
 *
 *  T[][] - There are four rounds of 16 permutations for a total of 64.
 *          In each of these 64 permutation operations, a different
 *          constant value is added to the mix.  The constants are
 *          based on the sine function...read RFC 1321 for more detail.
 *          In any case, the correct constants are stored in the T[][]
 *          array.  They're divided up into four groups of 16.
 */

static const uint8_t K[3][16] =
  {
    /* Round 1: skipped (since it is simply sequential). */
    {  1,  6, 11,  0,  5, 10, 15,  4,  9, 14,  3,  8, 13,  2,  7, 12 }, /* R2 */
    {  5,  8, 11, 14,  1,  4,  7, 10, 13,  0,  3,  6,  9, 12, 15,  2 }, /* R3 */
    {  0,  7, 14,  5, 12,  3, 10,  1,  8, 15,  6, 13,  4, 11,  2,  9 }  /* R4 */
  };

static const uint8_t S[4][4] =
  {
    { 7, 12, 17, 22 },  /* Round 1 */
    { 5,  9, 14, 20 },  /* Round 2 */
    { 4, 11, 16, 23 },  /* Round 3 */
    { 6, 10, 15, 21 }   /* Round 4 */
  };


static const uint32_t T[4][16] =
  {
    { 0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,   /* Round 1 */
      0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
      0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
      0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821 },

    { 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,   /* Round 2 */
      0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
      0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
      0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a },

    { 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,   /* Round 3 */
      0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
      0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
      0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665 },

    { 0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,   /* Round 4 */
      0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
      0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
      0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 },
  };


/* -------------------------------------------------------------------------- **
 * Macros:
 *  md5F(), md5G(), md5H(), and md5I() are described in RFC 1321.
 *  All of these operations are bitwise, and so not impacted by endian-ness.
 *
 *  GetLongByte()
 *    Extract one byte from a (32-bit) longword.  A value of 0 for <idx>
 *    indicates the lowest order byte, while 3 indicates the highest order
 *    byte.
 *    
 */

#define md5F( X, Y, Z ) ( ((X) & (Y)) | ((~(X)) & (Z)) )
#define md5G( X, Y, Z ) ( ((X) & (Z)) | ((Y) & (~(Z))) )
#define md5H( X, Y, Z ) ( (X) ^ (Y) ^ (Z) )
#define md5I( X, Y, Z ) ( (Y) ^ ((X) | (~(Z))) )

#define GetLongByte( L, idx ) ((uchar)(( L >> (((idx) & 0x03) << 3) ) & 0xFF))


/* -------------------------------------------------------------------------- **
 * Static Functions:
 */

static void Permute( uint32_t ABCD[4], const uchar block[64] )
  /* ------------------------------------------------------------------------ **
   * Permute the ABCD "registers" using the 64-byte <block> as a driver.
   *
   *  Input:  ABCD  - Pointer to an array of four unsigned longwords.
   *          block - An array of bytes, 64 bytes in size.
   *
   *  Output: none.
   *
   *  Notes:  The MD5 algorithm operates on a set of four longwords stored
   *          (conceptually) in four "registers".  It is easy to imagine a
   *          simple MD4/5 chip that would operate this way.  In any case,
   *          the mangling of the contents of those registers is driven by
   *          the input message.  The message is chopped and finally padded
   *          into 64-byte chunks and each chunk is used to manipulate the
   *          contents of the registers.
   *
   *          The MD5 Algorithm calls for padding the input to ensure that
   *          it is a multiple of 64 bytes in length.  The last 16 bytes
   *          of the padding space are used to store the message length
   *          (the length of the original message, before padding, expressed
   *          in terms of bits).  If there is not enough room for 16 bytes
   *          worth of bitcount (eg., if the original message was 122 bytes
   *          long) then the block is padded to the end with zeros and
   *          passed to this function.  Then *another* block is filled with
   *          zeros except for the last 16 bytes which contain the length.
   *
   *          Oh... and the algorithm requires that there be at least one
   *          padding byte.  The first padding byte has a value of 0x80,
   *          and any others are 0x00.
   *
   * ------------------------------------------------------------------------ **
   */
  {
  int      round;
  int      i, j;
  uint8_t  s;
  uint32_t a, b, c, d;
  uint32_t KeepABCD[4];
  uint32_t X[16];

  /* Store the current ABCD values for later re-use.
   */
  for( i = 0; i < 4; i++ )
    KeepABCD[i] = ABCD[i];

  /* Convert the input block into an array of unsigned longs, taking care
   * to read the block in Little Endian order (the algorithm assumes this).
   * The uint32_t values are then handled in host order.
   */
  for( i = 0, j = 0; i < 16; i++ )
    {
    X[i]  =  (uint32_t)block[j++];
    X[i] |= ((uint32_t)block[j++] << 8);
    X[i] |= ((uint32_t)block[j++] << 16);
    X[i] |= ((uint32_t)block[j++] << 24);
    }

  /* This loop performs the four rounds of permutations.
   * The rounds are each very similar.  The differences are in three areas:
   *   - The function (F, G, H, or I) used to perform bitwise permutations
   *     on the registers,
   *   - The order in which values from X[] are chosen.
   *   - Changes to the number of bits by which the registers are rotated.
   * This implementation uses a switch statement to deal with some of the
   * differences between rounds.  Other differences are handled by storing
   * values in arrays and using the round number to select the correct set
   * of values.
   *
   * (My implementation appears to be a poor compromise between speed, size,
   * and clarity.  Ugh.  [crh])
   */
  for( round = 0; round < 4; round++ )
    {
    for( i = 0; i < 16; i++ )
      {
      j = (4 - (i % 4)) & 0x3;  /* <j> handles the rotation of ABCD.          */
      s = S[round][i%4];        /* <s> is the bit shift for this iteration.   */

      b = ABCD[(j+1) & 0x3];    /* Copy the b,c,d values per ABCD rotation.   */
      c = ABCD[(j+2) & 0x3];    /* This isn't really necessary, it just looks */
      d = ABCD[(j+3) & 0x3];    /* clean & will hopefully be optimized away.  */

      /* The actual perumation function.
       * This is broken out to minimize the code within the switch().
       */
      switch( round )
        {
        case 0:
          /* round 1 */
          a = md5F( b, c, d ) + X[i];
          break;
        case 1:
          /* round 2 */
          a = md5G( b, c, d ) + X[ K[0][i] ];
          break;
        case 2:
          /* round 3 */
          a = md5H( b, c, d ) + X[ K[1][i] ];
          break;
        default:
          /* round 4 */
          a = md5I( b, c, d ) + X[ K[2][i] ];
          break;
        }
      a = 0xFFFFFFFF & ( ABCD[j] + a + T[round][i] );
      ABCD[j] = b + (0xFFFFFFFF & (( a << s ) | ( a >> (32 - s) )));
      }
    }

  /* Use the stored original A, B, C, D values to perform
   * one last convolution.
   */
  for( i = 0; i < 4; i++ )
    ABCD[i] = 0xFFFFFFFF & ( ABCD[i] + KeepABCD[i] );

  } /* Permute */


/* -------------------------------------------------------------------------- **
 * Functions:
 */

auth_md5Ctx *auth_md5InitCtx( auth_md5Ctx *ctx )
  /* ------------------------------------------------------------------------ **
   * Initialize an MD5 context.
   *
   *  Input:  ctx - A pointer to the MD5 context structure to be initialized.
   *                Contexts are typically created thusly:
   *                  ctx = (auth_md5Ctx *)malloc( sizeof(auth_md5Ctx) );
   *
   *  Output: A pointer to the initialized context (same as <ctx>).
   *
   *  Notes:  The purpose of the context is to make it possible to generate
   *          an MD5 Message Digest in stages, rather than having to pass a
   *          single large block to a single MD5 function.  The context
   *          structure keeps track of various bits of state information.
   *
   *          Once the context is initialized, the blocks of message data
   *          are passed to the <auth_md5SumCtx()> function.  Once the
   *          final bit of data has been handed to <auth_md5SumCtx()> the
   *          context can be closed out by calling <auth_md5CloseCtx()>,
   *          which also calculates the final MD5 result.
   *
   *          Don't forget to free an allocated context structure when
   *          you've finished using it.
   *
   *  See Also:  <auth_md5SumCtx()>, <auth_md5CloseCtx()>
   *
   * ------------------------------------------------------------------------ **
   */
  {
  ctx->len     = 0;
  ctx->b_used  = 0;

  ctx->ABCD[0] = 0x67452301;    /* The array ABCD[] contains the four 4-byte  */
  ctx->ABCD[1] = 0xefcdab89;    /* "registers" that are manipulated to        */
  ctx->ABCD[2] = 0x98badcfe;    /* produce the MD5 digest.  The input acts    */
  ctx->ABCD[3] = 0x10325476;    /* upon the registers, not the other way      */
                                /* 'round.  The initial values are those      */
                /* given in RFC 1321 (pg. 4).  Note, however, that RFC 1321   */
                /* provides these values as bytes, not as longwords, and the  */
                /* bytes are arranged in little-endian order as if they were  */
                /* the bytes of (little endian) 32-bit ints.  That's          */
                /* confusing as all getout (to me, anyway). The values given  */
                /* here are provided as 32-bit values in C language format,   */
                /* so they are endian-agnostic.  */
  return( ctx );
  } /* auth_md5InitCtx */


auth_md5Ctx *auth_md5SumCtx( auth_md5Ctx *ctx,
                             const uchar *src,
                             const int    len )
  /* ------------------------------------------------------------------------ **
   * Build an MD5 Message Digest within the given context.
   *
   *  Input:  ctx - Pointer to the context in which the MD5 sum is being
   *                built.
   *          src - A chunk of source data.  This will be used to drive
   *                the MD5 algorithm.
   *          len - The number of bytes in <src>.
   *
   *  Output: A pointer to the updated context (same as <ctx>).
   *
   *  See Also:  <auth_md5InitCtx()>, <auth_md5CloseCtx()>, <auth_md5Sum()>
   *
   * ------------------------------------------------------------------------ **
   */
  {
  int i;

  /* Add the new block's length to the total length.
   */
  ctx->len += (uint32_t)len;

  /* Copy the new block's data into the context block.
   * Call the Permute() function whenever the context block is full.
   */
  for( i = 0; i < len; i++ )
    {
    ctx->block[ ctx->b_used ] = src[i];
    (ctx->b_used)++;
    if( 64 == ctx->b_used )
      {
      Permute( ctx->ABCD, ctx->block );
      ctx->b_used = 0;
      }
    }

  /* Return the updated context.
   */
  return( ctx );
  } /* auth_md5SumCtx */


auth_md5Ctx *auth_md5CloseCtx( auth_md5Ctx *ctx, uchar *dst )
  /* ------------------------------------------------------------------------ **
   * Close an MD5 Message Digest context and generate the final MD5 sum.
   *
   *  Input:  ctx - Pointer to the context in which the MD5 sum is being
   *                built.
   *          dst - A pointer to at least 16 bytes of memory, which will
   *                receive the finished MD5 sum.
   *
   *  Output: A pointer to the closed context (same as <ctx>).
   *          You might use this to free a malloc'd context structure.  :)
   *
   *  Notes:  The context (<ctx>) is returned in an undefined state.
   *          It must be re-initialized before re-use.
   *
   *  See Also:  <auth_md5InitCtx()>, <auth_md5SumCtx()>
   *
   * ------------------------------------------------------------------------ **
   */
  {
  int      i;
  uint32_t l;

  /* Add the required 0x80 padding initiator byte.
   * The auth_md5SumCtx() function always permutes and resets the context
   * block when it gets full, so we know that there must be at least one
   * free byte in the context block.
   */
  ctx->block[ctx->b_used] = 0x80;
  (ctx->b_used)++;

  /* Zero out any remaining free bytes in the context block.
   */
  for( i = ctx->b_used; i < 64; i++ )
    ctx->block[i] = 0;

  /* We need 8 bytes to store the length field.
   * If we don't have 8, call Permute() and reset the context block.
   */
  if( 56 < ctx->b_used )
    {
    Permute( ctx->ABCD, ctx->block );
    for( i = 0; i < 64; i++ )
      ctx->block[i] = 0;
    }

  /* Add the total length and perform the final perumation.
   * Note:  The 60'th byte is read from the *original* <ctx->len> value
   *        and shifted to the correct position.  This neatly avoids
   *        any MAXINT numeric overflow issues.
   */
  l = ctx->len << 3;
  for( i = 0; i < 4; i++ )
    ctx->block[56+i] |= GetLongByte( l, i );
  ctx->block[60] = ((GetLongByte( ctx->len, 3 ) & 0xE0) >> 5);  /* See Above! */
  Permute( ctx->ABCD, ctx->block );

  /* Now copy the result into the output buffer and we're done.
   */
  for( i = 0; i < 4; i++ )
    {
    dst[ 0+i] = GetLongByte( ctx->ABCD[0], i );
    dst[ 4+i] = GetLongByte( ctx->ABCD[1], i );
    dst[ 8+i] = GetLongByte( ctx->ABCD[2], i );
    dst[12+i] = GetLongByte( ctx->ABCD[3], i );
    }

  /* Return the context.
   * This is done for compatibility with the other auth_md5*Ctx() functions.
   */
  return( ctx );
  } /* auth_md5CloseCtx */


uchar *auth_md5Sum( uchar *dst, const uchar *src, const int len )
  /* ------------------------------------------------------------------------ **
   * Compute an MD5 message digest.
   *
   *  Input:  dst - Destination buffer into which the result will be written.
   *                Must be 16 bytes, minimum.
   *          src - Source data block to be MD5'd.
   *          len - The length, in bytes, of the source block.
   *                (Note that the length is given in bytes, not bits.)
   *
   *  Output: A pointer to <dst>, which will contain the calculated 16-byte
   *          MD5 message digest.
   *
   *  Notes:  This function is a shortcut.  It takes a single input block.
   *          For more drawn-out operations, see <auth_md5InitCtx()>.
   *
   *          This function is interface-compatible with the
   *          <auth_md4Sum()> function in the MD4 module.
   *
   *          The MD5 algorithm is designed to work on data with an
   *          arbitrary *bit* length.  Most implementations, this one
   *          included, handle the input data in byte-sized chunks.
   *
   *          The MD5 algorithm does much of its work using four-byte
   *          words, and so can be tuned for speed based on the endian-ness
   *          of the host.  This implementation is intended to be
   *          endian-neutral, which may make it a teeny bit slower than
   *          others.  ...maybe.
   *
   *  See Also:  <auth_md5InitCtx()>
   *
   * ------------------------------------------------------------------------ **
   */
  {
  auth_md5Ctx ctx[1];

  (void)auth_md5InitCtx( ctx );             /* Open a context.      */
  (void)auth_md5SumCtx( ctx, src, len );    /* Pass only one block. */
  (void)auth_md5CloseCtx( ctx, dst );       /* Close the context.   */

  return( dst );                            /* Makes life easy.     */
  } /* auth_md5Sum */


/* ========================================================================== */
