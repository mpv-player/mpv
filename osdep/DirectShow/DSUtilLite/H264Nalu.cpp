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
#include "H264Nalu.h"

void CH264Nalu::SetBuffer(const BYTE *pBuffer, size_t nSize, int nNALSize)
{
    m_pBuffer = pBuffer;
    m_nSize = nSize;
    m_nNALSize = nNALSize;
    m_nCurPos = 0;
    m_nNextRTP = 0;

    m_nNALStartPos = 0;
    m_nNALDataPos = 0;

    // In AnnexB, the buffer is not guaranteed to start on a NAL boundary
    if (nNALSize == 0 && nSize > 0)
        MoveToNextAnnexBStartcode();
}

bool CH264Nalu::MoveToNextAnnexBStartcode()
{
    if (m_nSize < 4)
        goto notfound;

    size_t nBuffEnd = m_nSize - 4;

    for (size_t i = m_nCurPos; i <= nBuffEnd; i++)
    {
        if ((*((DWORD *)(m_pBuffer + i)) & 0x00FFFFFF) == 0x00010000)
        {
            // Found next AnnexB NAL
            m_nCurPos = i;
            return true;
        }
    }

notfound:
    m_nCurPos = m_nSize;
    return false;
}

bool CH264Nalu::MoveToNextRTPStartcode()
{
    if (m_nNextRTP < m_nSize)
    {
        m_nCurPos = m_nNextRTP;
        return true;
    }

    m_nCurPos = m_nSize;
    return false;
}

bool CH264Nalu::ReadNext()
{
    if (m_nCurPos >= m_nSize)
        return false;

    if ((m_nNALSize != 0) && (m_nCurPos == m_nNextRTP))
    {
        if (m_nCurPos + m_nNALSize >= m_nSize)
            return false;
        // RTP Nalu type : (XX XX) XX XX NAL..., with XX XX XX XX or XX XX equal to NAL size
        m_nNALStartPos = m_nCurPos;
        m_nNALDataPos = m_nCurPos + m_nNALSize;

        // Read Length code from the buffer
        unsigned nTemp = 0;
        for (int i = 0; i < m_nNALSize; i++)
            nTemp = (nTemp << 8) + m_pBuffer[m_nCurPos++];

        m_nNextRTP += nTemp + m_nNALSize;
        MoveToNextRTPStartcode();
    }
    else
    {
        // Remove trailing bits
        while (m_pBuffer[m_nCurPos] == 0x00 && ((*((DWORD *)(m_pBuffer + m_nCurPos)) & 0x00FFFFFF) != 0x00010000))
            m_nCurPos++;

        // AnnexB Nalu : 00 00 01 NAL...
        m_nNALStartPos = m_nCurPos;
        m_nCurPos += 3;
        m_nNALDataPos = m_nCurPos;
        MoveToNextAnnexBStartcode();
    }

    forbidden_bit = (m_pBuffer[m_nNALDataPos] >> 7) & 1;
    nal_reference_idc = (m_pBuffer[m_nNALDataPos] >> 5) & 3;
    nal_unit_type = (NALU_TYPE)(m_pBuffer[m_nNALDataPos] & 0x1f);

    return true;
}

bool CH265Nalu::ReadNext()
{
    if (CH264Nalu::ReadNext())
    {
        nal_unit_type = (NALU_TYPE)((m_pBuffer[m_nNALDataPos] >> 1) & 0x3F);
        return true;
    }

    return false;
}

CH264NALUnescape::CH264NALUnescape(const BYTE *src, size_t nSize)
{
    m_pBuffer = (BYTE *)_aligned_malloc(nSize + 16, 16);
    memset(m_pBuffer, 0, nSize + 16);

    unsigned si = 0, di = 0;
    BYTE *dst = m_pBuffer;
    while (si + 2 < nSize)
    {
        // detect and remove escapes
        if (src[si + 2] > 3)
        {
            dst[di++] = src[si++];
            dst[di++] = src[si++];
        }
        else if (src[si] == 0 && src[si + 1] == 0 && src[si + 2] != 0)
        {
            if (src[si + 2] == 3)
            { // escape
                dst[di++] = 0;
                dst[di++] = 0;
                si += 3;

                continue;
            }
        }

        dst[di++] = src[si++];
    }

    while (si < nSize)
        dst[di++] = src[si++];

    m_nSize = di;
}

CH264NALUnescape::~CH264NALUnescape()
{
    _aligned_free(m_pBuffer);
}
