// code from xanim sources...
// (I hope that not hurt copyright :o)

#define xaLONG long
#define xaULONG unsigned long
#define xaBYTE char
#define xaUBYTE unsigned char

xaULONG long xa_alaw_2_sign[256];

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

