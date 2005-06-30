/*
 * Copyright (C) 2000, 2001, 2002, 2003 Håkan Hjort <d95hjort@dtek.chalmers.se>
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

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "bswap.h"
#include "nav_types.h"
#include "nav_read.h"
#include "dvdread_internal.h"

void navRead_PCI(pci_t *pci, unsigned char *buffer) {
  int i, j;

  CHECK_VALUE(sizeof(pci_t) == PCI_BYTES - 1); // -1 for substream id
  
  memcpy(pci, buffer, sizeof(pci_t));

  /* Endian conversions  */

  /* pci pci_gi */
  B2N_32(pci->pci_gi.nv_pck_lbn);
  B2N_16(pci->pci_gi.vobu_cat);
  B2N_32(pci->pci_gi.vobu_s_ptm);
  B2N_32(pci->pci_gi.vobu_e_ptm);
  B2N_32(pci->pci_gi.vobu_se_e_ptm);

  /* pci nsml_agli */
  for(i = 0; i < 9; i++)
    B2N_32(pci->nsml_agli.nsml_agl_dsta[i]);

  /* pci hli hli_gi */
  B2N_16(pci->hli.hl_gi.hli_ss);
  B2N_32(pci->hli.hl_gi.hli_s_ptm);
  B2N_32(pci->hli.hl_gi.hli_e_ptm);
  B2N_32(pci->hli.hl_gi.btn_se_e_ptm);

  /* pci hli btn_colit */
  for(i = 0; i < 3; i++)
    for(j = 0; j < 2; j++)
      B2N_32(pci->hli.btn_colit.btn_coli[i][j]);

  /* NOTE: I've had to change the structure from the disk layout to get
   * the packing to work with Sun's Forte C compiler. */
  
  /* pci hli btni */
  for(i = 0; i < 36; i++) {
    char tmp[sizeof(pci->hli.btnit[i])], swap;
    memcpy(tmp, &(pci->hli.btnit[i]), sizeof(pci->hli.btnit[i]));
    /* Byte 4 to 7 are 'rotated' was: ABCD EFGH IJ is: ABCG DEFH IJ */
    swap   = tmp[6]; 
    tmp[6] = tmp[5];
    tmp[5] = tmp[4];
    tmp[4] = tmp[3];
    tmp[3] = swap;
    
    /* Then there are the two B2N_24(..) calls */
#ifndef WORDS_BIGENDIAN
    swap = tmp[0];
    tmp[0] = tmp[2];
    tmp[2] = swap;
    
    swap = tmp[4];
    tmp[4] = tmp[6];
    tmp[6] = swap;
#endif
    memcpy(&(pci->hli.btnit[i]), tmp, sizeof(pci->hli.btnit[i]));
  }


#ifndef NDEBUG
  /* Asserts */

  /* pci pci gi */ 
  CHECK_VALUE(pci->pci_gi.zero1 == 0);

  /* pci hli hli_gi */
  CHECK_VALUE(pci->hli.hl_gi.zero1 == 0);
  CHECK_VALUE(pci->hli.hl_gi.zero2 == 0);
  CHECK_VALUE(pci->hli.hl_gi.zero3 == 0);
  CHECK_VALUE(pci->hli.hl_gi.zero4 == 0);
  CHECK_VALUE(pci->hli.hl_gi.zero5 == 0);

  /* Are there buttons defined here? */
  if((pci->hli.hl_gi.hli_ss & 0x03) != 0) {
    CHECK_VALUE(pci->hli.hl_gi.btn_ns != 0); 
    CHECK_VALUE(pci->hli.hl_gi.btngr_ns != 0); 
  } else {
    CHECK_VALUE((pci->hli.hl_gi.btn_ns != 0 && pci->hli.hl_gi.btngr_ns != 0) 
	   || (pci->hli.hl_gi.btn_ns == 0 && pci->hli.hl_gi.btngr_ns == 0));
  }

  /* pci hli btnit */
  for(i = 0; i < pci->hli.hl_gi.btngr_ns; i++) {
    for(j = 0; j < (36 / pci->hli.hl_gi.btngr_ns); j++) {
      int n = (36 / pci->hli.hl_gi.btngr_ns) * i + j;
      CHECK_VALUE(pci->hli.btnit[n].zero1 == 0);
      CHECK_VALUE(pci->hli.btnit[n].zero2 == 0);
      CHECK_VALUE(pci->hli.btnit[n].zero3 == 0);
      CHECK_VALUE(pci->hli.btnit[n].zero4 == 0);
      CHECK_VALUE(pci->hli.btnit[n].zero5 == 0);
      CHECK_VALUE(pci->hli.btnit[n].zero6 == 0);
      
      if (j < pci->hli.hl_gi.btn_ns) {	
	CHECK_VALUE(pci->hli.btnit[n].x_start <= pci->hli.btnit[n].x_end);
	CHECK_VALUE(pci->hli.btnit[n].y_start <= pci->hli.btnit[n].y_end);
	CHECK_VALUE(pci->hli.btnit[n].up <= pci->hli.hl_gi.btn_ns);
	CHECK_VALUE(pci->hli.btnit[n].down <= pci->hli.hl_gi.btn_ns);
	CHECK_VALUE(pci->hli.btnit[n].left <= pci->hli.hl_gi.btn_ns);
	CHECK_VALUE(pci->hli.btnit[n].right <= pci->hli.hl_gi.btn_ns);
	//vmcmd_verify(pci->hli.btnit[n].cmd);
      } else {
	int k;
	CHECK_VALUE(pci->hli.btnit[n].btn_coln == 0);
	CHECK_VALUE(pci->hli.btnit[n].auto_action_mode == 0);
	CHECK_VALUE(pci->hli.btnit[n].x_start == 0);
	CHECK_VALUE(pci->hli.btnit[n].y_start == 0);
	CHECK_VALUE(pci->hli.btnit[n].x_end == 0);
	CHECK_VALUE(pci->hli.btnit[n].y_end == 0);
	CHECK_VALUE(pci->hli.btnit[n].up == 0);
	CHECK_VALUE(pci->hli.btnit[n].down == 0);
	CHECK_VALUE(pci->hli.btnit[n].left == 0);
	CHECK_VALUE(pci->hli.btnit[n].right == 0);
	for (k = 0; k < 8; k++)
	  CHECK_VALUE(pci->hli.btnit[n].cmd.bytes[k] == 0); //CHECK_ZERO?
      }
    }
  }
#endif /* !NDEBUG */
}

void navRead_DSI(dsi_t *dsi, unsigned char *buffer) {
  int i;

  CHECK_VALUE(sizeof(dsi_t) == DSI_BYTES - 1); // -1 for substream id
  
  memcpy(dsi, buffer, sizeof(dsi_t));

  /* Endian conversions */

  /* dsi dsi gi */
  B2N_32(dsi->dsi_gi.nv_pck_scr);
  B2N_32(dsi->dsi_gi.nv_pck_lbn);
  B2N_32(dsi->dsi_gi.vobu_ea);
  B2N_32(dsi->dsi_gi.vobu_1stref_ea);
  B2N_32(dsi->dsi_gi.vobu_2ndref_ea);
  B2N_32(dsi->dsi_gi.vobu_3rdref_ea);
  B2N_16(dsi->dsi_gi.vobu_vob_idn);

  /* dsi sml pbi */
  B2N_16(dsi->sml_pbi.category);
  B2N_32(dsi->sml_pbi.ilvu_ea);
  B2N_32(dsi->sml_pbi.ilvu_sa);
  B2N_16(dsi->sml_pbi.size);
  B2N_32(dsi->sml_pbi.vob_v_s_s_ptm);
  B2N_32(dsi->sml_pbi.vob_v_e_e_ptm);

  /* dsi sml agli */
  for(i = 0; i < 9; i++) {
    B2N_32(dsi->sml_agli.data[ i ].address);
    B2N_16(dsi->sml_agli.data[ i ].size);
  }

  /* dsi vobu sri */
  B2N_32(dsi->vobu_sri.next_video);
  for(i = 0; i < 19; i++)
    B2N_32(dsi->vobu_sri.fwda[i]);
  B2N_32(dsi->vobu_sri.next_vobu);
  B2N_32(dsi->vobu_sri.prev_vobu);
  for(i = 0; i < 19; i++)
    B2N_32(dsi->vobu_sri.bwda[i]);
  B2N_32(dsi->vobu_sri.prev_video);

  /* dsi synci */
  for(i = 0; i < 8; i++)
    B2N_16(dsi->synci.a_synca[i]);
  for(i = 0; i < 32; i++)
    B2N_32(dsi->synci.sp_synca[i]);

  
  /* Asserts */

  /* dsi dsi gi */
  CHECK_VALUE(dsi->dsi_gi.zero1 == 0);
}

