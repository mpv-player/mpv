/*
 *      Copyright (C) 2010-2021 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *  Initial design and concept by Gabest and the MPC-HC Team, copyright under GPLv2
 */

#include "stdafx.h"
#include "ByteParser.h"

#pragma warning(push)
#pragma warning(disable : 4101)
#pragma warning(disable : 4244)
#pragma warning(disable : 5033)
extern "C"
{
#define AVCODEC_X86_MATHOPS_H
#include "libavcodec/get_bits.h"
};
#pragma warning(pop)

CByteParser::CByteParser(const BYTE *pData, size_t length)
    : m_pData(pData)
    , m_pEnd(pData + length)
{
    m_gbCtx = (GetBitContext *)av_mallocz(sizeof(GetBitContext));
    init_get_bits8(m_gbCtx, pData, (int)length);
}

CByteParser::~CByteParser()
{
    av_freep(&m_gbCtx);
}

unsigned int CByteParser::BitRead(unsigned int numBits, bool peek)
{
    if (numBits == 0)
        return 0;

    if (peek)
        return show_bits_long(m_gbCtx, numBits);
    else
        return get_bits_long(m_gbCtx, numBits);
}

void CByteParser::BitSkip(unsigned int numBits)
{
    if (numBits == 0)
        return;

    skip_bits_long(m_gbCtx, numBits);
}

size_t CByteParser::RemainingBits() const
{
    int bits = get_bits_left(m_gbCtx);
    if (bits < 0)
        return 0;
    return (size_t)bits;
}

size_t CByteParser::Pos() const
{
    return (size_t)(m_pEnd - m_pData - Remaining());
}

// Exponential Golomb Coding (with k = 0)
// As used in H.264/MPEG-4 AVC
// http://en.wikipedia.org/wiki/Exponential-Golomb_coding

unsigned CByteParser::UExpGolombRead()
{
    int n = -1;
    for (BYTE b = 0; !b && RemainingBits(); n++)
    {
        b = get_bits1(m_gbCtx);
    }
    if (!RemainingBits())
        return 0;
    return ((1 << n) | BitRead(n)) - 1;
}

int CByteParser::SExpGolombRead()
{
    int k = UExpGolombRead() + 1;
    // Negative numbers are interleaved in the series
    // unsigned: 0, 1,  2, 3,  4, 5,  6, ...
    //   signed: 0, 1, -1, 2, -2, 3, -3, ....
    // So all even numbers are negative (last bit = 0)
    // Note that we added 1 to the unsigned value already, so the check is inverted
    if (k & 1)
        return -(k >> 1);
    else
        return (k >> 1);
}

void CByteParser::BitByteAlign()
{
    align_get_bits(m_gbCtx);
}
