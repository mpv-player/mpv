// code from xanim sources...
// (I hope that not hurt copyright :o)

#define xaLONG long
#define xaULONG unsigned long
#define xaBYTE char
#define xaUBYTE unsigned char

//xaULONG long xa_alaw_2_sign[256];
xaULONG xa_alaw_2_sign[256];
xaULONG xa_ulaw_2_sign[256];

/*
** This routine converts from ulaw to 16 bit linear.
**
** Craig Reese: IDA/Supercomputing Research Center
** 29 September 1989
**
** References:
** 1) CCITT Recommendation G.711  (very difficult to follow)
** 2) MIL-STD-188-113,"Interoperability and Performance Standards
**     for Analog-to_Digital Conversion Techniques,"
**     17 February 1987
**
** Input: 8 bit ulaw sample
** Output: signed 16 bit linear sample
*/

xaLONG XA_uLaw_to_Signed( ulawbyte )
xaUBYTE ulawbyte;
{
  static int exp_lut[8] = { 0, 132, 396, 924, 1980, 4092, 8316, 16764 };
  int sign, exponent, mantissa, sample;
 
  ulawbyte = ~ ulawbyte;
  sign = ( ulawbyte & 0x80 );
  exponent = ( ulawbyte >> 4 ) & 0x07;
  mantissa = ulawbyte & 0x0F;
  sample = exp_lut[exponent] + ( mantissa << ( exponent + 3 ) );
  if ( sign != 0 ) sample = -sample;
 
  return sample;
}

void Gen_uLaw_2_Signed()
{ xaULONG i;
  for(i=0;i<256;i++)
  { xaUBYTE data = (xaUBYTE)(i);
    xaLONG d = XA_uLaw_to_Signed( data );
    xa_ulaw_2_sign[i] = (xaULONG)((xaULONG)(d) & 0xffff);
  }
}



void Gen_aLaw_2_Signed()
{ xaULONG i;
  for(i=0;i<256;i++)
  { xaUBYTE data = (xaUBYTE)(i);
    xaLONG d, t, seg;

    data ^= 0x55;

    t = (data & 0xf) << 4;
    seg = (data & 0x70) >> 4;
    if (seg == 0)	t += 8;
    else if (seg == 1)	t += 0x108;
    else	{ t += 108; t <<= seg - 1; }

    d =  (data & 0x80)?(t):(-t);
    xa_alaw_2_sign[i] = (xaULONG)((xaULONG)(d) & 0xffff);
  }
}

