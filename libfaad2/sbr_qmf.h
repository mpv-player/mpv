/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2004 M. Bakker, Ahead Software AG, http://www.nero.com
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software 
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Ahead Software through Mpeg4AAClicense@nero.com.
**
** $Id: sbr_qmf.h,v 1.21 2004/09/08 09:43:11 gcp Exp $
**/

#ifndef __SBR_QMF_H__
#define __SBR_QMF_H__

#ifdef __cplusplus
extern "C" {
#endif

qmfa_info *qmfa_init(uint8_t channels);
void qmfa_end(qmfa_info *qmfa);
qmfs_info *qmfs_init(uint8_t channels);
void qmfs_end(qmfs_info *qmfs);

void sbr_qmf_analysis_32(sbr_info *sbr, qmfa_info *qmfa, const real_t *input,
                         qmf_t X[MAX_NTSRHFG][64], uint8_t offset, uint8_t kx);
void sbr_qmf_synthesis_32(sbr_info *sbr, qmfs_info *qmfs, qmf_t X[MAX_NTSRHFG][64],
                          real_t *output);
void sbr_qmf_synthesis_64(sbr_info *sbr, qmfs_info *qmfs, qmf_t X[MAX_NTSRHFG][64],
                          real_t *output);


#ifdef __cplusplus
}
#endif
#endif

