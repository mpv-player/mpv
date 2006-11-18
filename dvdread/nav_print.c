/*
 * Copyright (C) 2000, 2001, 2002, 2003 Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
 * Much of the contents in this file is based on VOBDUMP.
 *
 * VOBDUMP: a program for examining DVD .VOB filse
 *
 * Copyright 1998, 1999 Eric Smith <eric@brouhaha.com>
 *
 * VOBDUMP is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.  Note that I am not
 * granting permission to redistribute or modify VOBDUMP under the
 * terms of any later version of the General Public License.
 *
 * This program is distributed in the hope that it will be useful (or
 * at least amusing), but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <stdio.h>
#include <inttypes.h>

#include "nav_types.h"
#include "nav_print.h"
#include "dvdread_internal.h"

static void print_time(dvd_time_t *dtime) {
  const char *rate;
  CHECK_VALUE((dtime->hour>>4) < 0xa && (dtime->hour&0xf) < 0xa);
  CHECK_VALUE((dtime->minute>>4) < 0x7 && (dtime->minute&0xf) < 0xa);
  CHECK_VALUE((dtime->second>>4) < 0x7 && (dtime->second&0xf) < 0xa);
  CHECK_VALUE((dtime->frame_u&0xf) < 0xa);
  
  printf("%02x:%02x:%02x.%02x", 
	 dtime->hour,
	 dtime->minute,
	 dtime->second,
	 dtime->frame_u & 0x3f);
  switch((dtime->frame_u & 0xc0) >> 6) {
  case 1:
    rate = "25.00";
    break;
  case 3:
    rate = "29.97";
    break;
  default:
    rate = "(please send a bug report)";
    break;
  } 
  printf(" @ %s fps", rate);
}


static void navPrint_PCI_GI(pci_gi_t *pci_gi) {
  int i;

  printf("pci_gi:\n");
  printf("nv_pck_lbn    0x%08x\n", pci_gi->nv_pck_lbn);
  printf("vobu_cat      0x%04x\n", pci_gi->vobu_cat);
  printf("vobu_uop_ctl  0x%08x\n", *(uint32_t*)&pci_gi->vobu_uop_ctl);
  printf("vobu_s_ptm    0x%08x\n", pci_gi->vobu_s_ptm);
  printf("vobu_e_ptm    0x%08x\n", pci_gi->vobu_e_ptm);
  printf("vobu_se_e_ptm 0x%08x\n", pci_gi->vobu_se_e_ptm);
  printf("e_eltm        ");
  print_time(&pci_gi->e_eltm);
  printf("\n");
  
  printf("vobu_isrc     \"");
  for(i = 0; i < 32; i++) {
    char c = pci_gi->vobu_isrc[i];
    if((c >= ' ') && (c <= '~'))
      printf("%c", c);
    else
      printf(".");
  }
  printf("\"\n");
}

static void navPrint_NSML_AGLI(nsml_agli_t *nsml_agli) {
  int i, j = 0;
  
  for(i = 0; i < 9; i++)
    j |= nsml_agli->nsml_agl_dsta[i];
  if(j == 0)
    return;
  
  printf("nsml_agli:\n");
  for(i = 0; i < 9; i++)
    if(nsml_agli->nsml_agl_dsta[i])
      printf("nsml_agl_c%d_dsta  0x%08x\n", i + 1, 
	     nsml_agli->nsml_agl_dsta[i]);
}

static void navPrint_HL_GI(hl_gi_t *hl_gi, int *btngr_ns, int *btn_ns) {
  
  if((hl_gi->hli_ss & 0x03) == 0)
    return;
  
  printf("hl_gi:\n");
  printf("hli_ss        0x%01x\n", hl_gi->hli_ss & 0x03);
  printf("hli_s_ptm     0x%08x\n", hl_gi->hli_s_ptm);
  printf("hli_e_ptm     0x%08x\n", hl_gi->hli_e_ptm);
  printf("btn_se_e_ptm  0x%08x\n", hl_gi->btn_se_e_ptm);

  *btngr_ns = hl_gi->btngr_ns;
  printf("btngr_ns      %d\n",  hl_gi->btngr_ns);
  printf("btngr%d_dsp_ty    0x%02x\n", 1, hl_gi->btngr1_dsp_ty);
  printf("btngr%d_dsp_ty    0x%02x\n", 2, hl_gi->btngr2_dsp_ty);
  printf("btngr%d_dsp_ty    0x%02x\n", 3, hl_gi->btngr3_dsp_ty);
  
  printf("btn_ofn       %d\n", hl_gi->btn_ofn);
  *btn_ns = hl_gi->btn_ns;
  printf("btn_ns        %d\n", hl_gi->btn_ns);
  printf("nsl_btn_ns    %d\n", hl_gi->nsl_btn_ns);
  printf("fosl_btnn     %d\n", hl_gi->fosl_btnn);
  printf("foac_btnn     %d\n", hl_gi->foac_btnn);
}

static void navPrint_BTN_COLIT(btn_colit_t *btn_colit) {
  int i, j;
  
  j = 0;
  for(i = 0; i < 6; i++)
    j |= btn_colit->btn_coli[i/2][i&1];
  if(j == 0)
    return;
  
  printf("btn_colit:\n");
  for(i = 0; i < 3; i++)
    for(j = 0; j < 2; j++)
      printf("btn_cqoli %d  %s_coli:  %08x\n",
	     i, (j == 0) ? "sl" : "ac",
	     btn_colit->btn_coli[i][j]);
}

static void navPrint_BTNIT(btni_t *btni_table, int btngr_ns, int btn_ns) {
  int i, j;
  
  printf("btnit:\n");
  printf("btngr_ns: %i\n", btngr_ns);
  printf("btn_ns: %i\n", btn_ns);
  
  if(btngr_ns == 0)
    return;
  
  for(i = 0; i < btngr_ns; i++) {
    for(j = 0; j < (36 / btngr_ns); j++) {
      if(j < btn_ns) {
	btni_t *btni = &btni_table[(36 / btngr_ns) * i + j];
	
	printf("group %d btni %d:  ", i+1, j+1);
	printf("btn_coln %d, auto_action_mode %d\n",
	       btni->btn_coln, btni->auto_action_mode);
	printf("coords   (%d, %d) .. (%d, %d)\n",
	       btni->x_start, btni->y_start, btni->x_end, btni->y_end);
	
	printf("up %d, ", btni->up);
	printf("down %d, ", btni->down);
	printf("left %d, ", btni->left);
	printf("right %d\n", btni->right);
	
	// ifoPrint_COMMAND(&btni->cmd);
	printf("\n");
      }
    }
  }
}

static void navPrint_HLI(hli_t *hli) {
  int btngr_ns = 0, btn_ns = 0;
  
  printf("hli:\n");
  navPrint_HL_GI(&hli->hl_gi, & btngr_ns, & btn_ns);
  navPrint_BTN_COLIT(&hli->btn_colit);
  navPrint_BTNIT(hli->btnit, btngr_ns, btn_ns);
}

void navPrint_PCI(pci_t *pci) {
  printf("pci packet:\n");
  navPrint_PCI_GI(&pci->pci_gi);
  navPrint_NSML_AGLI(&pci->nsml_agli);
  navPrint_HLI(&pci->hli);
}

static void navPrint_DSI_GI(dsi_gi_t *dsi_gi) {
  printf("dsi_gi:\n");
  printf("nv_pck_scr     0x%08x\n", dsi_gi->nv_pck_scr);
  printf("nv_pck_lbn     0x%08x\n", dsi_gi->nv_pck_lbn );
  printf("vobu_ea        0x%08x\n", dsi_gi->vobu_ea);
  printf("vobu_1stref_ea 0x%08x\n", dsi_gi->vobu_1stref_ea);
  printf("vobu_2ndref_ea 0x%08x\n", dsi_gi->vobu_2ndref_ea);
  printf("vobu_3rdref_ea 0x%08x\n", dsi_gi->vobu_3rdref_ea);
  printf("vobu_vob_idn   0x%04x\n", dsi_gi->vobu_vob_idn);
  printf("vobu_c_idn     0x%02x\n", dsi_gi->vobu_c_idn);
  printf("c_eltm         ");
  print_time(&dsi_gi->c_eltm);
  printf("\n");
}

static void navPrint_SML_PBI(sml_pbi_t *sml_pbi) {
  printf("sml_pbi:\n");
  printf("category 0x%04x\n", sml_pbi->category);
  if(sml_pbi->category & 0x8000)
    printf("VOBU is in preunit\n");
  if(sml_pbi->category & 0x4000)
    printf("VOBU is in ILVU\n");
  if(sml_pbi->category & 0x2000)
    printf("VOBU at the beginning of ILVU\n");
  if(sml_pbi->category & 0x1000)
    printf("VOBU at end of PREU of ILVU\n");
  
  printf("ilvu_ea       0x%08x\n", sml_pbi->ilvu_ea);
  printf("nxt_ilvu_sa   0x%08x\n", sml_pbi->ilvu_sa);
  printf("nxt_ilvu_size 0x%04x\n", sml_pbi->size);
  
  printf("vob_v_s_s_ptm 0x%08x\n", sml_pbi->vob_v_s_s_ptm);
  printf("vob_v_e_e_ptm 0x%08x\n", sml_pbi->vob_v_e_e_ptm);
  
  /* $$$ more code needed here */
}

static void navPrint_SML_AGLI(sml_agli_t *sml_agli) {
  int i;
  printf("sml_agli:\n");
  for(i = 0; i < 9; i++) {
    printf("agl_c%d address: 0x%08x size 0x%04x\n", i,
	   sml_agli->data[i].address, sml_agli->data[i].size);
  }
}

static void navPrint_VOBU_SRI(vobu_sri_t *vobu_sri) {
  int i;
  int stime[19] = { 240, 120, 60, 20, 15, 14, 13, 12, 11, 
		     10,   9,  8,  7,  6,  5,  4,  3,  2, 1};
  printf("vobu_sri:\n");
  printf("Next VOBU with Video %08x\n", vobu_sri->next_video);
  for(i = 0; i < 19; i++) {
    printf("%3.1f %08x ", stime[i]/2.0, vobu_sri->fwda[i]);
  }
  printf("\n");
  printf("Next VOBU %08x\n", vobu_sri->next_vobu);
  printf("--\n");
  printf("Prev VOBU %08x\n", vobu_sri->prev_vobu);
  for(i = 0; i < 19; i++) {
    printf("%3.1f %08x ", stime[18 - i]/2.0, vobu_sri->bwda[i]);
  }
  printf("\n");
  printf("Prev VOBU with Video %08x\n", vobu_sri->prev_video);
}

static void navPrint_SYNCI(synci_t *synci) {
  int i;
  
  printf("synci:\n");
  /* $$$ more code needed here */
  for(i = 0; i < 8; i++)
    printf("%04x ", synci->a_synca[i]);
  for(i = 0; i < 32; i++)
    printf("%08x ", synci->sp_synca[i]);
}

void navPrint_DSI(dsi_t *dsi) {
  printf("dsi packet:\n");
  navPrint_DSI_GI(&dsi->dsi_gi);
  navPrint_SML_PBI(&dsi->sml_pbi);
  navPrint_SML_AGLI(&dsi->sml_agli);
  navPrint_VOBU_SRI(&dsi->vobu_sri);
  navPrint_SYNCI(&dsi->synci);
}


