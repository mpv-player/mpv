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

#pragma once

typedef enum
{
    NALU_TYPE_UNKNOWN = 0,
    NALU_TYPE_SLICE = 1,
    NALU_TYPE_DPA = 2,
    NALU_TYPE_DPB = 3,
    NALU_TYPE_DPC = 4,
    NALU_TYPE_IDR = 5,
    NALU_TYPE_SEI = 6,
    NALU_TYPE_SPS = 7,
    NALU_TYPE_PPS = 8,
    NALU_TYPE_AUD = 9,
    NALU_TYPE_EOSEQ = 10,
    NALU_TYPE_EOSTREAM = 11,
    NALU_TYPE_FILL = 12,
    NALU_TYPE_SPS_SUB = 15,
} NALU_TYPE;

class CH264Nalu
{
  protected:
    int forbidden_bit = 0;                       //! should be always FALSE
    int nal_reference_idc = 0;                   //! NALU_PRIORITY_xxxx
    NALU_TYPE nal_unit_type = NALU_TYPE_UNKNOWN; //! NALU_TYPE_xxxx

    size_t m_nNALStartPos = 0; //! NALU start (including startcode / size)
    size_t m_nNALDataPos = 0;  //! Useful part

    const BYTE *m_pBuffer = nullptr;
    size_t m_nCurPos = 0;
    size_t m_nNextRTP = 0;
    size_t m_nSize = 0;
    int m_nNALSize = 0;

    bool MoveToNextAnnexBStartcode();
    bool MoveToNextRTPStartcode();

  public:
    CH264Nalu() { SetBuffer(nullptr, 0, 0); }
    NALU_TYPE GetType() const { return nal_unit_type; }
    bool IsRefFrame() const { return (nal_reference_idc != 0); }

    size_t GetDataLength() const { return m_nCurPos - m_nNALDataPos; }
    const BYTE *GetDataBuffer() { return m_pBuffer + m_nNALDataPos; }
    size_t GetRoundedDataLength() const
    {
        size_t nSize = m_nCurPos - m_nNALDataPos;
        return nSize + 128 - (nSize % 128);
    }

    size_t GetLength() const { return m_nCurPos - m_nNALStartPos; }
    const BYTE *GetNALBuffer() { return m_pBuffer + m_nNALStartPos; }
    size_t GetNALPos() { return m_nNALStartPos; }
    bool IsEOF() const { return m_nCurPos >= m_nSize; }

    void SetBuffer(const BYTE *pBuffer, size_t nSize, int nNALSize);
    bool ReadNext();
};

class CH265Nalu : public CH264Nalu
{
  public:
    CH265Nalu()
        : CH264Nalu(){};
    bool ReadNext();
};

class CH264NALUnescape
{
  public:
    CH264NALUnescape(const BYTE *pBuffer, size_t nSize);
    ~CH264NALUnescape();
    const BYTE *GetBuffer() const { return m_pBuffer; }
    size_t GetSize() const { return m_nSize; }

  private:
    BYTE *m_pBuffer = nullptr;
    size_t m_nSize = 0;
};
