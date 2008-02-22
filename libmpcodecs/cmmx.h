/*
 * x86 MMX and MMX2 packed byte operations in portable C.
 * Extra instructions: pdiffub, pcmpzb, psumbw, pcmpgtub
 * Author: Zoltan Hidvegi
 */

#ifndef MPLAYER_CMMX_H
#define MPLAYER_CMMX_H

typedef unsigned long cmmx_t;

#define ONE_BYTES (~(cmmx_t)0 / 255)
#define SIGN_BITS (ONE_BYTES << 7)
#define LOWBW_MASK (~(cmmx_t)0 / 257)

static inline cmmx_t
paddb(cmmx_t a, cmmx_t b)
{
    return ((a & ~SIGN_BITS) + (b & ~SIGN_BITS)) ^ ((a^b) & SIGN_BITS);
}

static inline cmmx_t
psubb(cmmx_t a, cmmx_t b)
{
    return ((a | SIGN_BITS) - (b & ~SIGN_BITS)) ^ (~(a^b) & SIGN_BITS);
}

static inline cmmx_t
paddusb(cmmx_t a, cmmx_t b)
{
    cmmx_t s = (a & ~SIGN_BITS) + (b & ~SIGN_BITS);
    cmmx_t abs = (a | b) & SIGN_BITS;
    cmmx_t c = abs & (s | (a & b));
    return s | abs | (abs - (c >> 7));
}

static inline cmmx_t
paddusb_s(cmmx_t a, cmmx_t b)
{
    cmmx_t sum = a+b;
    cmmx_t ov = sum & SIGN_BITS;
    return sum + (sum ^ (ov - (ov>>7)));
}

static inline cmmx_t
psubusb(cmmx_t a, cmmx_t b)
{
    cmmx_t s = (a | SIGN_BITS) - (b & ~SIGN_BITS);
    cmmx_t anb = a & ~b;
    cmmx_t c = (anb | (s & ~(a^b))) & SIGN_BITS;
    return s & ((c & anb) | (c - (c >> 7)));
}

static inline cmmx_t
psubusb_s(cmmx_t a, cmmx_t b)
{
    cmmx_t d = (a|SIGN_BITS) - b;
    cmmx_t m = d & SIGN_BITS;
    return d & (m - (m>>7));
}

static inline cmmx_t
pcmpgtub(cmmx_t b, cmmx_t a)
{
    cmmx_t s = (a | SIGN_BITS) - (b & ~SIGN_BITS);
    cmmx_t ret = ((~a & b) | (~s & ~(a ^ b))) & SIGN_BITS;
    return ret | (ret - (ret >> 7));
}

static inline cmmx_t
pdiffub(cmmx_t a, cmmx_t b)
{
    cmmx_t xs = (~a ^ b) & SIGN_BITS;
    cmmx_t s = ((a | SIGN_BITS) - (b & ~SIGN_BITS)) ^ xs;
    cmmx_t gt = ((~a & b) | (s & xs)) & SIGN_BITS;
    cmmx_t gt7 = gt >> 7;
    return (s ^ gt ^ (gt - gt7)) + gt7;
}

static inline cmmx_t
pdiffub_s(cmmx_t a, cmmx_t b)
{
    cmmx_t d = (a|SIGN_BITS) - b;
    cmmx_t g = (~d & SIGN_BITS) >> 7;
    return (d ^ (SIGN_BITS-g)) + g;
}

static inline cmmx_t
pmaxub(cmmx_t a, cmmx_t b)
{
    return psubusb(a,b) + b;
}

static inline cmmx_t
pminub(cmmx_t a, cmmx_t b)
{
    return paddusb(a,~b) - ~b;
}

static inline cmmx_t
pminub_s(cmmx_t a, cmmx_t b)
{
    cmmx_t d = (a|SIGN_BITS) - b;
    cmmx_t m = ~SIGN_BITS + ((d&SIGN_BITS)>>7);
    return ((d&m) + b) & ~SIGN_BITS;
}

static inline cmmx_t
pavgb(cmmx_t a, cmmx_t b)
{
    cmmx_t ao = a & ONE_BYTES;
    cmmx_t bo = b & ONE_BYTES;
    return ((a^ao)>>1) + ((b^bo)>>1) + (ao|bo);
}

static inline cmmx_t
pavgb_s(cmmx_t a, cmmx_t b)
{
    return ((a+b+ONE_BYTES)>>1) & ~SIGN_BITS;
}

static inline cmmx_t
p31avgb(cmmx_t a, cmmx_t b)
{
    cmmx_t ao = a & (3*ONE_BYTES);
    cmmx_t bo = b & (3*ONE_BYTES);
    return 3*((a^ao)>>2) + ((b^bo)>>2) +
	(((3*ao+bo+2*ONE_BYTES)>>2) & (3*ONE_BYTES));
}

static inline cmmx_t
p31avgb_s(cmmx_t a, cmmx_t b)
{
    cmmx_t avg = ((a+b)>>1) & ~SIGN_BITS;
    return pavgb_s(avg, a);
}

static inline unsigned long
psumbw(cmmx_t a)
{
    cmmx_t t = (a & LOWBW_MASK) + ((a>>8) & LOWBW_MASK);
    unsigned long ret =
	(unsigned long)t + (unsigned long)(t >> (4*sizeof(cmmx_t)));
    if (sizeof(cmmx_t) > 4)
	ret += ret >> 16;
    return ret & 0xffff;
}

static inline unsigned long
psumbw_s(cmmx_t a)
{
    unsigned long ret =
	(unsigned long)a + (unsigned long)(a >> (4*sizeof(cmmx_t)));
    if (sizeof(cmmx_t) <= 4)
	return (ret & 0xff) + ((ret>>8) & 0xff);
    ret = (ret & 0xff00ff) + ((ret>>8) & 0xff00ff);
    ret += ret >> 16;
    return ret & 0xffff;
}

static inline unsigned long
psadbw(cmmx_t a, cmmx_t b)
{
    return psumbw(pdiffub(a,b));
}

static inline unsigned long
psadbw_s(cmmx_t a, cmmx_t b)
{
    return psumbw_s(pdiffub_s(a,b));
}

static inline cmmx_t
pcmpzb(cmmx_t a)
{
    cmmx_t ret = (((a | SIGN_BITS) - ONE_BYTES) | a) & SIGN_BITS;
    return ~(ret | (ret - (ret >> 7)));
}

static inline cmmx_t
pcmpeqb(cmmx_t a, cmmx_t b)
{
    return pcmpzb(a ^ b);
}

#endif /* MPLAYER_CMMX_H */
