#ifndef IFO_READ_H_INCLUDED
#define IFO_READ_H_INCLUDED

/*
 * Copyright (C) 2000, 2001, 2002 Björn Englund <d4bjorn@dtek.chalmers.se>,
 *                                Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <dvdread/ifo_types.h>
#include <dvdread/dvd_reader.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * handle = ifoOpen(dvd, title);
 *
 * Opens an IFO and reads in all the data for the IFO file corresponding to the
 * given title.  If title 0 is given, the video manager IFO file is read.
 * Returns a handle to a completely parsed structure.
 */
ifo_handle_t *ifoOpen(dvd_reader_t *, int );

/**
 * handle = ifoOpenVMGI(dvd);
 *
 * Opens an IFO and reads in _only_ the vmgi_mat data.  This call can be used
 * together with the calls below to read in each segment of the IFO file on
 * demand.
 */
ifo_handle_t *ifoOpenVMGI(dvd_reader_t *);

/**
 * handle = ifoOpenVTSI(dvd, title);
 *
 * Opens an IFO and reads in _only_ the vtsi_mat data.  This call can be used
 * together with the calls below to read in each segment of the IFO file on
 * demand.
 */
ifo_handle_t *ifoOpenVTSI(dvd_reader_t *, int);

/**
 * ifoClose(ifofile);
 * Cleans up the IFO information.  This will free all data allocated for the
 * substructures.
 */
void ifoClose(ifo_handle_t *);

/**
 * The following functions are for reading only part of the VMGI/VTSI files.
 * Returns 1 if the data was successfully read and 0 on error.
 */

/**
 * okay = ifoRead_PLT_MAIT(ifofile);
 *
 * Read in the Parental Management Information table, filling the
 * ifofile->ptl_mait structure and its substructures.  This data is only
 * located in the video manager information file.  This fills the
 * ifofile->ptl_mait structure and all its substructures.
 */
int ifoRead_PTL_MAIT(ifo_handle_t *);

/**
 * okay = ifoRead_VTS_ATRT(ifofile);
 *
 * Read in the attribute table for the main menu vob, filling the
 * ifofile->vts_atrt structure and its substructures.  Only located in the
 * video manager information file.  This fills in the ifofile->vts_atrt
 * structure and all its substructures.
 */
int ifoRead_VTS_ATRT(ifo_handle_t *);

/**
 * okay = ifoRead_TT_SRPT(ifofile);
 *
 * Reads the title info for the main menu, filling the ifofile->tt_srpt
 * structure and its substructures.  This data is only located in the video
 * manager information file.  This structure is mandatory in the IFO file.
 */
int ifoRead_TT_SRPT(ifo_handle_t *);

/**
 * okay = ifoRead_VTS_PTT_SRPT(ifofile);
 *
 * Reads in the part of title search pointer table, filling the
 * ifofile->vts_ptt_srpt structure and its substructures.  This data is only
 * located in the video title set information file.  This structure is
 * mandatory, and must be included in the VTSI file.
 */
int ifoRead_VTS_PTT_SRPT(ifo_handle_t *);

/**
 * okay = ifoRead_FP_PGC(ifofile);
 *
 * Reads in the first play program chain data, filling the
 * ifofile->first_play_pgc structure.  This data is only located in the video
 * manager information file.  This structure is mandatory, and must be included
 * in the VMGI file. **Possibly this is only optional.**
 */
int ifoRead_FP_PGC(ifo_handle_t *);

/**
 * okay = ifoRead_PGCIT(ifofile);
 *
 * Reads in the program chain information table for the video title set.  Fills
 * in the ifofile->vts_pgcit structure and its substructures, which includes
 * the data for each program chain in the set.  This data is only located in
 * the video title set information file.  This structure is mandatory, and must
 * be included in the VTSI file.
 */
int ifoRead_PGCIT(ifo_handle_t *);

/**
 * okay = ifoRead_PGCI_UT(ifofile);
 *
 * Reads in the menu PGCI unit table for the menu VOB.  For the video manager,
 * this corresponds to the VIDEO_TS.VOB file, and for each title set, this
 * corresponds to the VTS_XX_0.VOB file.  This data is located in both the
 * video manager and video title set information files.  For VMGI files, this
 * fills the ifofile->vmgi_pgci_ut structure and all its substructures.  For
 * VTSI files, this fills the ifofile->vtsm_pgci_ut structure.
 */
int ifoRead_PGCI_UT(ifo_handle_t *);

/**
 * okay = ifoRead_C_ADT(ifofile);
 *
 * Reads in the cell address table for the menu VOB.  For the video manager,
 * this corresponds to the VIDEO_TS.VOB file, and for each title set, this
 * corresponds to the VTS_XX_0.VOB file.  This data is located in both the
 * video manager and video title set information files.  For VMGI files, this
 * fills the ifofile->vmgm_c_adt structure and all its substructures.  For VTSI
 * files, this fills the ifofile->vtsm_c_adt structure.
 */
int ifoRead_C_ADT(ifo_handle_t *);

/**
 * okay = ifoRead_TITLE_C_ADT(ifofile);
 *
 * Reads in the cell address table for the video title set corresponding to
 * this IFO file.  This data is only located in the video title set information
 * file.  This structure is mandatory, and must be included in the VTSI file.
 * This call fills the ifofile->vts_c_adt structure and its substructures.
 */
int ifoRead_TITLE_C_ADT(ifo_handle_t *);

/**
 * okay = ifoRead_VOBU_ADMAP(ifofile);
 *
 * Reads in the VOBU address map for the menu VOB.  For the video manager, this
 * corresponds to the VIDEO_TS.VOB file, and for each title set, this
 * corresponds to the VTS_XX_0.VOB file.  This data is located in both the
 * video manager and video title set information files.  For VMGI files, this
 * fills the ifofile->vmgm_vobu_admap structure and all its substructures.  For
 * VTSI files, this fills the ifofile->vtsm_vobu_admap structure.
 */
int ifoRead_VOBU_ADMAP(ifo_handle_t *);

/**
 * okay = ifoRead_TITLE_VOBU_ADMAP(ifofile);
 *
 * Reads in the VOBU address map for the associated video title set.  This data
 * is only located in the video title set information file.  This structure is
 * mandatory, and must be included in the VTSI file.  Fills the
 * ifofile->vts_vobu_admap structure and its substructures.
 */
int ifoRead_TITLE_VOBU_ADMAP(ifo_handle_t *);

/**
 * okay = ifoRead_TXTDT_MGI(ifofile);
 *
 * Reads in the text data strings for the DVD.  Fills the ifofile->txtdt_mgi
 * structure and all its substructures.  This data is only located in the video
 * manager information file.  This structure is mandatory, and must be included
 * in the VMGI file.
 */
int ifoRead_TXTDT_MGI(ifo_handle_t *);

/**
 * The following functions are used for freeing parsed sections of the
 * ifo_handle_t structure and the allocated substructures.  The free calls
 * below are safe:  they will not mind if you attempt to free part of an IFO
 * file which was not read in or which does not exist.
 */
void ifoFree_PTL_MAIT(ifo_handle_t *);
void ifoFree_VTS_ATRT(ifo_handle_t *);
void ifoFree_TT_SRPT(ifo_handle_t *);
void ifoFree_VTS_PTT_SRPT(ifo_handle_t *);
void ifoFree_FP_PGC(ifo_handle_t *);
void ifoFree_PGCIT(ifo_handle_t *);
void ifoFree_PGCI_UT(ifo_handle_t *);
void ifoFree_C_ADT(ifo_handle_t *);
void ifoFree_TITLE_C_ADT(ifo_handle_t *);
void ifoFree_VOBU_ADMAP(ifo_handle_t *);
void ifoFree_TITLE_VOBU_ADMAP(ifo_handle_t *);
void ifoFree_TXTDT_MGI(ifo_handle_t *);

#ifdef __cplusplus
};
#endif
#endif /* IFO_READ_H_INCLUDED */
