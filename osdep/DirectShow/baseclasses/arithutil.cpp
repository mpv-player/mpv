//------------------------------------------------------------------------------
// File: ArithUtil.cpp
//
// Desc: DirectShow base classes - implements helper classes for building
//       multimedia filters.
//
// Copyright (c) 1992-2004 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#include <streams.h>

//
//  Declare function from largeint.h we need so that PPC can build
//

//
// Enlarged integer divide - 64-bits / 32-bits > 32-bits
//

#ifndef _X86_

#define LLtoU64(x) (*(unsigned __int64 *)(void *)(&(x)))

__inline ULONG WINAPI EnlargedUnsignedDivide(IN ULARGE_INTEGER Dividend, IN ULONG Divisor, IN PULONG Remainder)
{
    // return remainder if necessary
    if (Remainder != NULL)
        *Remainder = (ULONG)(LLtoU64(Dividend) % Divisor);
    return (ULONG)(LLtoU64(Dividend) / Divisor);
}

#else
__inline ULONG WINAPI EnlargedUnsignedDivide(IN ULARGE_INTEGER Dividend, IN ULONG Divisor, IN PULONG Remainder)
{
    ULONG ulResult;
    _asm {
        mov eax,Dividend.LowPart
        mov edx,Dividend.HighPart
        mov ecx,Remainder
        div Divisor
        or  ecx,ecx
        jz  short label
        mov [ecx],edx
label:
        mov ulResult,eax
    }
    return ulResult;
}
#endif

/*  Arithmetic functions to help with time format conversions
 */

#ifdef _M_ALPHA
// work around bug in version 12.00.8385 of the alpha compiler where
// UInt32x32To64 sign-extends its arguments (?)
#undef UInt32x32To64
#define UInt32x32To64(a, b) (((ULONGLONG)((ULONG)(a)) & 0xffffffff) * ((ULONGLONG)((ULONG)(b)) & 0xffffffff))
#endif

/*   Compute (a * b + d) / c */
LONGLONG WINAPI llMulDiv(LONGLONG a, LONGLONG b, LONGLONG c, LONGLONG d)
{
    /*  Compute the absolute values to avoid signed arithmetic problems */
    ULARGE_INTEGER ua, ub;
    DWORDLONG uc;

    ua.QuadPart = (DWORDLONG)(a >= 0 ? a : -a);
    ub.QuadPart = (DWORDLONG)(b >= 0 ? b : -b);
    uc = (DWORDLONG)(c >= 0 ? c : -c);
    BOOL bSign = (a < 0) ^ (b < 0);

    /*  Do long multiplication */
    ULARGE_INTEGER p[2];
    p[0].QuadPart = UInt32x32To64(ua.LowPart, ub.LowPart);

    /*  This next computation cannot overflow into p[1].HighPart because
        the max number we can compute here is:

                 (2 ** 32 - 1) * (2 ** 32 - 1) +  // ua.LowPart * ub.LowPart
    (2 ** 32) *  (2 ** 31) * (2 ** 32 - 1) * 2    // x.LowPart * y.HighPart * 2

    == 2 ** 96 - 2 ** 64 + (2 ** 64 - 2 ** 33 + 1)
    == 2 ** 96 - 2 ** 33 + 1
    < 2 ** 96
    */

    ULARGE_INTEGER x;
    x.QuadPart = UInt32x32To64(ua.LowPart, ub.HighPart) + UInt32x32To64(ua.HighPart, ub.LowPart) + p[0].HighPart;
    p[0].HighPart = x.LowPart;
    p[1].QuadPart = UInt32x32To64(ua.HighPart, ub.HighPart) + x.HighPart;

    if (d != 0)
    {
        ULARGE_INTEGER ud[2];
        if (bSign)
        {
            ud[0].QuadPart = (DWORDLONG)(-d);
            if (d > 0)
            {
                /*  -d < 0 */
                ud[1].QuadPart = (DWORDLONG)(LONGLONG)-1;
            }
            else
            {
                ud[1].QuadPart = (DWORDLONG)0;
            }
        }
        else
        {
            ud[0].QuadPart = (DWORDLONG)d;
            if (d < 0)
            {
                ud[1].QuadPart = (DWORDLONG)(LONGLONG)-1;
            }
            else
            {
                ud[1].QuadPart = (DWORDLONG)0;
            }
        }
        /*  Now do extended addition */
        ULARGE_INTEGER uliTotal;

        /*  Add ls DWORDs */
        uliTotal.QuadPart = (DWORDLONG)ud[0].LowPart + p[0].LowPart;
        p[0].LowPart = uliTotal.LowPart;

        /*  Propagate carry */
        uliTotal.LowPart = uliTotal.HighPart;
        uliTotal.HighPart = 0;

        /*  Add 2nd most ls DWORDs */
        uliTotal.QuadPart += (DWORDLONG)ud[0].HighPart + p[0].HighPart;
        p[0].HighPart = uliTotal.LowPart;

        /*  Propagate carry */
        uliTotal.LowPart = uliTotal.HighPart;
        uliTotal.HighPart = 0;

        /*  Add MS DWORDLONGs - no carry expected */
        p[1].QuadPart += ud[1].QuadPart + uliTotal.QuadPart;

        /*  Now see if we got a sign change from the addition */
        if ((LONG)p[1].HighPart < 0)
        {
            bSign = !bSign;

            /*  Negate the current value (ugh!) */
            p[0].QuadPart = ~p[0].QuadPart;
            p[1].QuadPart = ~p[1].QuadPart;
            p[0].QuadPart += 1;
            p[1].QuadPart += (p[0].QuadPart == 0);
        }
    }

    /*  Now for the division */
    if (c < 0)
    {
        bSign = !bSign;
    }

    /*  This will catch c == 0 and overflow */
    if (uc <= p[1].QuadPart)
    {
        return bSign ? (LONGLONG)0x8000000000000000 : (LONGLONG)0x7FFFFFFFFFFFFFFF;
    }

    DWORDLONG ullResult;

    /*  Do the division */
    /*  If the dividend is a DWORD_LONG use the compiler */
    if (p[1].QuadPart == 0)
    {
        ullResult = p[0].QuadPart / uc;
        return bSign ? -(LONGLONG)ullResult : (LONGLONG)ullResult;
    }

    /*  If the divisor is a DWORD then its simpler */
    ULARGE_INTEGER ulic;
    ulic.QuadPart = uc;
    if (ulic.HighPart == 0)
    {
        ULARGE_INTEGER uliDividend;
        ULARGE_INTEGER uliResult;
        DWORD dwDivisor = (DWORD)uc;
        // ASSERT(p[1].HighPart == 0 && p[1].LowPart < dwDivisor);
        uliDividend.HighPart = p[1].LowPart;
        uliDividend.LowPart = p[0].HighPart;
#ifndef USE_LARGEINT
        uliResult.HighPart = (DWORD)(uliDividend.QuadPart / dwDivisor);
        p[0].HighPart = (DWORD)(uliDividend.QuadPart % dwDivisor);
        uliResult.LowPart = 0;
        uliResult.QuadPart = p[0].QuadPart / dwDivisor + uliResult.QuadPart;
#else
        /*  NOTE - this routine will take exceptions if
            the result does not fit in a DWORD
        */
        if (uliDividend.QuadPart >= (DWORDLONG)dwDivisor)
        {
            uliResult.HighPart = EnlargedUnsignedDivide(uliDividend, dwDivisor, &p[0].HighPart);
        }
        else
        {
            uliResult.HighPart = 0;
        }
        uliResult.LowPart = EnlargedUnsignedDivide(p[0], dwDivisor, NULL);
#endif
        return bSign ? -(LONGLONG)uliResult.QuadPart : (LONGLONG)uliResult.QuadPart;
    }

    ullResult = 0;

    /*  OK - do long division */
    for (int i = 0; i < 64; i++)
    {
        ullResult <<= 1;

        /*  Shift 128 bit p left 1 */
        p[1].QuadPart <<= 1;
        if ((p[0].HighPart & 0x80000000) != 0)
        {
            p[1].LowPart++;
        }
        p[0].QuadPart <<= 1;

        /*  Compare */
        if (uc <= p[1].QuadPart)
        {
            p[1].QuadPart -= uc;
            ullResult += 1;
        }
    }

    return bSign ? -(LONGLONG)ullResult : (LONGLONG)ullResult;
}

LONGLONG WINAPI Int64x32Div32(LONGLONG a, LONG b, LONG c, LONG d)
{
    ULARGE_INTEGER ua;
    DWORD ub;
    DWORD uc;

    /*  Compute the absolute values to avoid signed arithmetic problems */
    ua.QuadPart = (DWORDLONG)(a >= 0 ? a : -a);
    ub = (DWORD)(b >= 0 ? b : -b);
    uc = (DWORD)(c >= 0 ? c : -c);
    BOOL bSign = (a < 0) ^ (b < 0);

    /*  Do long multiplication */
    ULARGE_INTEGER p0;
    DWORD p1;
    p0.QuadPart = UInt32x32To64(ua.LowPart, ub);

    if (ua.HighPart != 0)
    {
        ULARGE_INTEGER x;
        x.QuadPart = UInt32x32To64(ua.HighPart, ub) + p0.HighPart;
        p0.HighPart = x.LowPart;
        p1 = x.HighPart;
    }
    else
    {
        p1 = 0;
    }

    if (d != 0)
    {
        ULARGE_INTEGER ud0;
        DWORD ud1;

        if (bSign)
        {
            //
            //  Cast d to LONGLONG first otherwise -0x80000000 sign extends
            //  incorrectly
            //
            ud0.QuadPart = (DWORDLONG)(-(LONGLONG)d);
            if (d > 0)
            {
                /*  -d < 0 */
                ud1 = (DWORD)-1;
            }
            else
            {
                ud1 = (DWORD)0;
            }
        }
        else
        {
            ud0.QuadPart = (DWORDLONG)d;
            if (d < 0)
            {
                ud1 = (DWORD)-1;
            }
            else
            {
                ud1 = (DWORD)0;
            }
        }
        /*  Now do extended addition */
        ULARGE_INTEGER uliTotal;

        /*  Add ls DWORDs */
        uliTotal.QuadPart = (DWORDLONG)ud0.LowPart + p0.LowPart;
        p0.LowPart = uliTotal.LowPart;

        /*  Propagate carry */
        uliTotal.LowPart = uliTotal.HighPart;
        uliTotal.HighPart = 0;

        /*  Add 2nd most ls DWORDs */
        uliTotal.QuadPart += (DWORDLONG)ud0.HighPart + p0.HighPart;
        p0.HighPart = uliTotal.LowPart;

        /*  Add MS DWORDLONGs - no carry expected */
        p1 += ud1 + uliTotal.HighPart;

        /*  Now see if we got a sign change from the addition */
        if ((LONG)p1 < 0)
        {
            bSign = !bSign;

            /*  Negate the current value (ugh!) */
            p0.QuadPart = ~p0.QuadPart;
            p1 = ~p1;
            p0.QuadPart += 1;
            p1 += (p0.QuadPart == 0);
        }
    }

    /*  Now for the division */
    if (c < 0)
    {
        bSign = !bSign;
    }

    /*  This will catch c == 0 and overflow */
    if (uc <= p1)
    {
        return bSign ? (LONGLONG)0x8000000000000000 : (LONGLONG)0x7FFFFFFFFFFFFFFF;
    }

    /*  Do the division */

    /*  If the divisor is a DWORD then its simpler */
    ULARGE_INTEGER uliDividend;
    ULARGE_INTEGER uliResult;
    DWORD dwDivisor = uc;
    uliDividend.HighPart = p1;
    uliDividend.LowPart = p0.HighPart;
    /*  NOTE - this routine will take exceptions if
        the result does not fit in a DWORD
    */
    if (uliDividend.QuadPart >= (DWORDLONG)dwDivisor)
    {
        uliResult.HighPart = EnlargedUnsignedDivide(uliDividend, dwDivisor, &p0.HighPart);
    }
    else
    {
        uliResult.HighPart = 0;
    }
    uliResult.LowPart = EnlargedUnsignedDivide(p0, dwDivisor, NULL);
    return bSign ? -(LONGLONG)uliResult.QuadPart : (LONGLONG)uliResult.QuadPart;
}
