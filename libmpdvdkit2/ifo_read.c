/*
 * Copyright (C) 2000, 2001, 2002  Björn Englund <d4bjorn@dtek.chalmers.se>, 
 *                                 Håkan Hjort <d95hjort@dtek.chalmers.se>
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
//#include <assert.h>

#include "dvd_reader.h"

#include "config.h" // Needed for WORDS_BIGENDIAN
#include "bswap.h"
#include "ifo_types.h"
#include "ifo_read.h"

#ifndef DVD_BLOCK_LEN
#define DVD_BLOCK_LEN 2048
#endif

#ifndef NDEBUG
#define CHECK_ZERO(arg) \
  if(memcmp(my_friendly_zeros, &arg, sizeof(arg))) { \
    unsigned int i_CZ; \
    fprintf(stderr, "*** Zero check failed in %s:%i\n    for %s = 0x", \
            __FILE__, __LINE__, # arg ); \
    for(i_CZ = 0; i_CZ < sizeof(arg); i_CZ++) \
      fprintf(stderr, "%02x", *((uint8_t *)&arg + i_CZ)); \
    fprintf(stderr, "\n"); \
  }
static const uint8_t my_friendly_zeros[2048];
#else
#define CHECK_ZERO(arg) (void)(arg)
#endif


/* Prototypes for internal functions */
static int ifoRead_VMG(ifo_handle_t *ifofile);
static int ifoRead_VTS(ifo_handle_t *ifofile);
static int ifoRead_PGC(ifo_handle_t *ifofile, pgc_t *pgc, unsigned int offset);
static int ifoRead_PGC_COMMAND_TBL(ifo_handle_t *ifofile, 
                                   pgc_command_tbl_t *cmd_tbl, 
				   unsigned int offset);
static int ifoRead_PGC_PROGRAM_MAP(ifo_handle_t *ifofile, 
                                   pgc_program_map_t *program_map, 
                                   unsigned int nr, unsigned int offset);
static int ifoRead_CELL_PLAYBACK_TBL(ifo_handle_t *ifofile, 
                                     cell_playback_t *cell_playback, 
                                     unsigned int nr, unsigned int offset);
static int ifoRead_CELL_POSITION_TBL(ifo_handle_t *ifofile, 
                                     cell_position_t *cell_position, 
                                     unsigned int nr, unsigned int offset);
static int ifoRead_VTS_ATTRIBUTES(ifo_handle_t *ifofile, 
                                  vts_attributes_t *vts_attributes, 
                                  unsigned int offset);
static int ifoRead_C_ADT_internal(ifo_handle_t *ifofile, c_adt_t *c_adt, 
                                  unsigned int sector);
static int ifoRead_VOBU_ADMAP_internal(ifo_handle_t *ifofile, 
                                       vobu_admap_t *vobu_admap, 
				       unsigned int sector);
static int ifoRead_PGCIT_internal(ifo_handle_t *ifofile, pgcit_t *pgcit, 
                                  unsigned int offset);

static void ifoFree_PGC(pgc_t *pgc);
static void ifoFree_PGC_COMMAND_TBL(pgc_command_tbl_t *cmd_tbl);
static void ifoFree_PGCIT_internal(pgcit_t *pgcit);


static int DVDFileSeek_( dvd_file_t *dvd_file, uint32_t offset ) {
  return (DVDFileSeek(dvd_file, (int)offset) == (int)offset);
}


ifo_handle_t *ifoOpen(dvd_reader_t *dvd, int title) {
  ifo_handle_t *ifofile;

  ifofile = (ifo_handle_t *)malloc(sizeof(ifo_handle_t));
  if(!ifofile)
    return 0;

  memset(ifofile, 0, sizeof(ifo_handle_t));

  ifofile->file = DVDOpenFile(dvd, title, DVD_READ_INFO_FILE);
  if(!ifofile->file) {
    if(title) {
      fprintf(stderr, "libdvdread: Can't open file VTS_%02d_0.IFO.\n", title);
    } else {
      fprintf(stderr, "libdvdread: Can't open file VIDEO_TS.IFO.\n");
    }
    free(ifofile);
    return 0;
  }

  /* First check if this is a VMGI file. */
  if(ifoRead_VMG(ifofile)) {

    /* These are both mandatory. */
    if(!ifoRead_FP_PGC(ifofile) || !ifoRead_TT_SRPT(ifofile)) {
      fprintf(stderr, "libdvdread: Invalid main menu IFO (VIDEO_TS.IFO).\n");
      ifoClose(ifofile);
      return 0;
    }

    ifoRead_PGCI_UT(ifofile);
    ifoRead_PTL_MAIT(ifofile);

    /* This is also mandatory. */
    if(!ifoRead_VTS_ATRT(ifofile)) {
      fprintf(stderr, "libdvdread: Invalid main menu IFO (VIDEO_TS.IFO).\n");
      ifoClose(ifofile);
      return 0;
    }

    ifoRead_TXTDT_MGI(ifofile);
    ifoRead_C_ADT(ifofile);
    ifoRead_VOBU_ADMAP(ifofile);

    return ifofile;
  }

  if(ifoRead_VTS(ifofile)) {

    if(!ifoRead_VTS_PTT_SRPT(ifofile) || !ifoRead_PGCIT(ifofile)) {
      fprintf(stderr, "libdvdread: Invalid title IFO (VTS_%02d_0.IFO).\n",
              title);
      ifoClose(ifofile);
      return 0;
    }


    ifoRead_PGCI_UT(ifofile);
    ifoRead_C_ADT(ifofile);
    ifoRead_VOBU_ADMAP(ifofile);

    if(!ifoRead_TITLE_C_ADT(ifofile) || !ifoRead_TITLE_VOBU_ADMAP(ifofile)) {
      fprintf(stderr, "libdvdread: Invalid title IFO (VTS_%02d_0.IFO).\n",
              title);
      ifoClose(ifofile);
      return 0;
    }

    return ifofile;
  }

  if(title) {
    fprintf(stderr, "libdvdread: Invalid IFO for title %d (VTS_%02d_0.IFO).\n",
	    title, title);
  } else {
    fprintf(stderr, "libdvdread: Invalid IFO for VMGM (VIDEO_TS.IFO).\n");
  }
  ifoClose(ifofile);
  return 0;
}


ifo_handle_t *ifoOpenVMGI(dvd_reader_t *dvd) {
  ifo_handle_t *ifofile;

  ifofile = (ifo_handle_t *)malloc(sizeof(ifo_handle_t));
  if(!ifofile)
    return 0;

  memset(ifofile, 0, sizeof(ifo_handle_t));

  ifofile->file = DVDOpenFile(dvd, 0, DVD_READ_INFO_FILE);
  if(!ifofile->file) {
    fprintf(stderr, "libdvdread: Can't open file VIDEO_TS.IFO.\n");
    free(ifofile);
    return 0;
  }

  if(ifoRead_VMG(ifofile))
    return ifofile;

  fprintf(stderr, "libdvdread: Invalid main menu IFO (VIDEO_TS.IFO).\n");
  ifoClose(ifofile);
  return 0;
}


ifo_handle_t *ifoOpenVTSI(dvd_reader_t *dvd, int title) {
  ifo_handle_t *ifofile;
  
  ifofile = (ifo_handle_t *)malloc(sizeof(ifo_handle_t));
  if(!ifofile)
    return 0;

  memset(ifofile, 0, sizeof(ifo_handle_t));
  
  if(title <= 0 || title > 99) {
    fprintf(stderr, "libdvdread: ifoOpenVTSI invalid title (%d).\n", title);
    free(ifofile);
    return 0;
  }
    
  ifofile->file = DVDOpenFile(dvd, title, DVD_READ_INFO_FILE);
  if(!ifofile->file) {
    fprintf(stderr, "libdvdread: Can't open file VTS_%02d_0.IFO.\n", title);
    free(ifofile);
    return 0;
  }

  ifoRead_VTS(ifofile);
  if(ifofile->vtsi_mat)
    return ifofile;

  fprintf(stderr, "libdvdread: Invalid IFO for title %d (VTS_%02d_0.IFO).\n",
          title, title);
  ifoClose(ifofile);
  return 0;
}


void ifoClose(ifo_handle_t *ifofile) {
  if(!ifofile)
    return;
  
  ifoFree_VOBU_ADMAP(ifofile);
  ifoFree_TITLE_VOBU_ADMAP(ifofile);
  ifoFree_C_ADT(ifofile);
  ifoFree_TITLE_C_ADT(ifofile);
  ifoFree_TXTDT_MGI(ifofile);
  ifoFree_VTS_ATRT(ifofile);
  ifoFree_PTL_MAIT(ifofile);
  ifoFree_PGCI_UT(ifofile);
  ifoFree_TT_SRPT(ifofile);
  ifoFree_FP_PGC(ifofile);
  ifoFree_PGCIT(ifofile);
  ifoFree_VTS_PTT_SRPT(ifofile);

  if(ifofile->vmgi_mat)
    free(ifofile->vmgi_mat);

  if(ifofile->vtsi_mat)
    free(ifofile->vtsi_mat);

  DVDCloseFile(ifofile->file);
  ifofile->file = 0;
  free(ifofile);
  ifofile = 0;
}


static int ifoRead_VMG(ifo_handle_t *ifofile) {
  vmgi_mat_t *vmgi_mat;

  vmgi_mat = (vmgi_mat_t *)malloc(sizeof(vmgi_mat_t));
  if(!vmgi_mat)
    return 0;

  ifofile->vmgi_mat = vmgi_mat;

  if(!DVDFileSeek_(ifofile->file, 0)) {
    free(ifofile->vmgi_mat);
    ifofile->vmgi_mat = 0;
    return 0;
  }

  if(!DVDReadBytes(ifofile->file, vmgi_mat, sizeof(vmgi_mat_t))) {
    free(ifofile->vmgi_mat);
    ifofile->vmgi_mat = 0;
    return 0;
  }

  if(strncmp("DVDVIDEO-VMG", vmgi_mat->vmg_identifier, 12) != 0) {
    free(ifofile->vmgi_mat);
    ifofile->vmgi_mat = 0;
    return 0;
  }
  
  B2N_32(vmgi_mat->vmg_last_sector);
  B2N_32(vmgi_mat->vmgi_last_sector);
  B2N_32(vmgi_mat->vmg_category);
  B2N_16(vmgi_mat->vmg_nr_of_volumes);
  B2N_16(vmgi_mat->vmg_this_volume_nr);
  B2N_16(vmgi_mat->vmg_nr_of_title_sets);
  B2N_64(vmgi_mat->vmg_pos_code);
  B2N_32(vmgi_mat->vmgi_last_byte);
  B2N_32(vmgi_mat->first_play_pgc);
  B2N_32(vmgi_mat->vmgm_vobs);
  B2N_32(vmgi_mat->tt_srpt);
  B2N_32(vmgi_mat->vmgm_pgci_ut);
  B2N_32(vmgi_mat->ptl_mait);
  B2N_32(vmgi_mat->vts_atrt);
  B2N_32(vmgi_mat->txtdt_mgi);
  B2N_32(vmgi_mat->vmgm_c_adt);
  B2N_32(vmgi_mat->vmgm_vobu_admap);
  B2N_16(vmgi_mat->vmgm_audio_attr.lang_code);
  B2N_16(vmgi_mat->vmgm_subp_attr.lang_code);


  CHECK_ZERO(vmgi_mat->zero_1);
  CHECK_ZERO(vmgi_mat->zero_2);
  CHECK_ZERO(vmgi_mat->zero_3);
  CHECK_ZERO(vmgi_mat->zero_4);
  CHECK_ZERO(vmgi_mat->zero_5);
  CHECK_ZERO(vmgi_mat->zero_6);
  CHECK_ZERO(vmgi_mat->zero_7);
  CHECK_ZERO(vmgi_mat->zero_8);
  CHECK_ZERO(vmgi_mat->zero_9);
  CHECK_ZERO(vmgi_mat->zero_10);  
  assert(vmgi_mat->vmg_last_sector != 0);
  assert(vmgi_mat->vmgi_last_sector != 0);
  assert(vmgi_mat->vmgi_last_sector * 2 <= vmgi_mat->vmg_last_sector);
  assert(vmgi_mat->vmg_nr_of_volumes != 0);
  assert(vmgi_mat->vmg_this_volume_nr != 0);
  assert(vmgi_mat->vmg_this_volume_nr <= vmgi_mat->vmg_nr_of_volumes);
  assert(vmgi_mat->disc_side == 1 || vmgi_mat->disc_side == 2);
  assert(vmgi_mat->vmg_nr_of_title_sets != 0);
  assert(vmgi_mat->vmgi_last_byte >= 341);
  assert(vmgi_mat->vmgi_last_byte / DVD_BLOCK_LEN <= 
         vmgi_mat->vmgi_last_sector);
  /* It seems that first_play_pgc might be optional. */
  assert(vmgi_mat->first_play_pgc != 0 && 
         vmgi_mat->first_play_pgc < vmgi_mat->vmgi_last_byte);
  assert(vmgi_mat->vmgm_vobs == 0 || 
        (vmgi_mat->vmgm_vobs > vmgi_mat->vmgi_last_sector &&
         vmgi_mat->vmgm_vobs < vmgi_mat->vmg_last_sector));
  assert(vmgi_mat->tt_srpt <= vmgi_mat->vmgi_last_sector);
  assert(vmgi_mat->vmgm_pgci_ut <= vmgi_mat->vmgi_last_sector);
  assert(vmgi_mat->ptl_mait <= vmgi_mat->vmgi_last_sector);
  assert(vmgi_mat->vts_atrt <= vmgi_mat->vmgi_last_sector);
  assert(vmgi_mat->txtdt_mgi <= vmgi_mat->vmgi_last_sector);
  assert(vmgi_mat->vmgm_c_adt <= vmgi_mat->vmgi_last_sector);
  assert(vmgi_mat->vmgm_vobu_admap <= vmgi_mat->vmgi_last_sector);

  assert(vmgi_mat->nr_of_vmgm_audio_streams <= 1);
  assert(vmgi_mat->nr_of_vmgm_subp_streams <= 1);

  return 1;
}


static int ifoRead_VTS(ifo_handle_t *ifofile) {
  vtsi_mat_t *vtsi_mat;
  int i;

  vtsi_mat = (vtsi_mat_t *)malloc(sizeof(vtsi_mat_t));
  if(!vtsi_mat)
    return 0;
  
  ifofile->vtsi_mat = vtsi_mat;

  if(!DVDFileSeek_(ifofile->file, 0)) {
    free(ifofile->vtsi_mat);
    ifofile->vtsi_mat = 0;
    return 0;
  }

  if(!(DVDReadBytes(ifofile->file, vtsi_mat, sizeof(vtsi_mat_t)))) {
    free(ifofile->vtsi_mat);
    ifofile->vtsi_mat = 0;
    return 0;
  }

  if(strncmp("DVDVIDEO-VTS", vtsi_mat->vts_identifier, 12) != 0) {
    free(ifofile->vtsi_mat);
    ifofile->vtsi_mat = 0;
    return 0;
  }

  B2N_32(vtsi_mat->vts_last_sector);
  B2N_32(vtsi_mat->vtsi_last_sector);
  B2N_32(vtsi_mat->vts_category);
  B2N_32(vtsi_mat->vtsi_last_byte);
  B2N_32(vtsi_mat->vtsm_vobs);
  B2N_32(vtsi_mat->vtstt_vobs);
  B2N_32(vtsi_mat->vts_ptt_srpt);
  B2N_32(vtsi_mat->vts_pgcit);
  B2N_32(vtsi_mat->vtsm_pgci_ut);
  B2N_32(vtsi_mat->vts_tmapt);
  B2N_32(vtsi_mat->vtsm_c_adt);
  B2N_32(vtsi_mat->vtsm_vobu_admap);
  B2N_32(vtsi_mat->vts_c_adt);
  B2N_32(vtsi_mat->vts_vobu_admap);
  B2N_16(vtsi_mat->vtsm_audio_attr.lang_code);
  B2N_16(vtsi_mat->vtsm_subp_attr.lang_code);
  for(i = 0; i < 8; i++)
    B2N_16(vtsi_mat->vts_audio_attr[i].lang_code);
  for(i = 0; i < 32; i++)
    B2N_16(vtsi_mat->vts_subp_attr[i].lang_code);


  CHECK_ZERO(vtsi_mat->zero_1);
  CHECK_ZERO(vtsi_mat->zero_2);
  CHECK_ZERO(vtsi_mat->zero_3);
  CHECK_ZERO(vtsi_mat->zero_4);
  CHECK_ZERO(vtsi_mat->zero_5);
  CHECK_ZERO(vtsi_mat->zero_6);
  CHECK_ZERO(vtsi_mat->zero_7);
  CHECK_ZERO(vtsi_mat->zero_8);
  CHECK_ZERO(vtsi_mat->zero_9);
  CHECK_ZERO(vtsi_mat->zero_10);
  CHECK_ZERO(vtsi_mat->zero_11);
  CHECK_ZERO(vtsi_mat->zero_12);
  CHECK_ZERO(vtsi_mat->zero_13);
  CHECK_ZERO(vtsi_mat->zero_14);
  CHECK_ZERO(vtsi_mat->zero_15);
  CHECK_ZERO(vtsi_mat->zero_16);
  CHECK_ZERO(vtsi_mat->zero_17);
  CHECK_ZERO(vtsi_mat->zero_18);
  CHECK_ZERO(vtsi_mat->zero_19);
  CHECK_ZERO(vtsi_mat->zero_20);
  assert(vtsi_mat->vtsi_last_sector*2 <= vtsi_mat->vts_last_sector);
  assert(vtsi_mat->vtsi_last_byte/DVD_BLOCK_LEN <= vtsi_mat->vtsi_last_sector);
  assert(vtsi_mat->vtsm_vobs == 0 || 
        (vtsi_mat->vtsm_vobs > vtsi_mat->vtsi_last_sector &&
         vtsi_mat->vtsm_vobs < vtsi_mat->vts_last_sector));
  assert(vtsi_mat->vtstt_vobs == 0 || 
        (vtsi_mat->vtstt_vobs > vtsi_mat->vtsi_last_sector &&
         vtsi_mat->vtstt_vobs < vtsi_mat->vts_last_sector));
  assert(vtsi_mat->vts_ptt_srpt <= vtsi_mat->vtsi_last_sector);
  assert(vtsi_mat->vts_pgcit <= vtsi_mat->vtsi_last_sector);
  assert(vtsi_mat->vtsm_pgci_ut <= vtsi_mat->vtsi_last_sector);
  assert(vtsi_mat->vts_tmapt <= vtsi_mat->vtsi_last_sector);
  assert(vtsi_mat->vtsm_c_adt <= vtsi_mat->vtsi_last_sector);
  assert(vtsi_mat->vtsm_vobu_admap <= vtsi_mat->vtsi_last_sector);
  assert(vtsi_mat->vts_c_adt <= vtsi_mat->vtsi_last_sector);
  assert(vtsi_mat->vts_vobu_admap <= vtsi_mat->vtsi_last_sector);
  
  assert(vtsi_mat->nr_of_vtsm_audio_streams <= 1);
  assert(vtsi_mat->nr_of_vtsm_subp_streams <= 1);

  assert(vtsi_mat->nr_of_vts_audio_streams <= 8);
  for(i = vtsi_mat->nr_of_vts_audio_streams; i < 8; i++)
    CHECK_ZERO(vtsi_mat->vts_audio_attr[i]);

  assert(vtsi_mat->nr_of_vts_subp_streams <= 32);
  for(i = vtsi_mat->nr_of_vts_subp_streams; i < 32; i++)
    CHECK_ZERO(vtsi_mat->vts_subp_attr[i]);      

  return 1;
}


static int ifoRead_PGC_COMMAND_TBL(ifo_handle_t *ifofile, 
                                   pgc_command_tbl_t *cmd_tbl, 
				   unsigned int offset) {
  
  memset(cmd_tbl, 0, sizeof(pgc_command_tbl_t));
  
  if(!DVDFileSeek_(ifofile->file, offset))
    return 0;

  if(!(DVDReadBytes(ifofile->file, cmd_tbl, PGC_COMMAND_TBL_SIZE)))
    return 0;

  B2N_16(cmd_tbl->nr_of_pre);
  B2N_16(cmd_tbl->nr_of_post);
  B2N_16(cmd_tbl->nr_of_cell);

  assert(cmd_tbl->nr_of_pre + cmd_tbl->nr_of_post + cmd_tbl->nr_of_cell<= 255);
     
  if(cmd_tbl->nr_of_pre != 0) {
    unsigned int pre_cmds_size  = cmd_tbl->nr_of_pre * COMMAND_DATA_SIZE;
    cmd_tbl->pre_cmds = (vm_cmd_t *)malloc(pre_cmds_size);
    if(!cmd_tbl->pre_cmds)
      return 0;

    if(!(DVDReadBytes(ifofile->file, cmd_tbl->pre_cmds, pre_cmds_size))) {
      free(cmd_tbl->pre_cmds);
      return 0;
    }
  }
  
  if(cmd_tbl->nr_of_post != 0) {
    unsigned int post_cmds_size = cmd_tbl->nr_of_post * COMMAND_DATA_SIZE;
    cmd_tbl->post_cmds = (vm_cmd_t *)malloc(post_cmds_size);
    if(!cmd_tbl->post_cmds) {
      if(cmd_tbl->pre_cmds) 
	free(cmd_tbl->pre_cmds);
      return 0;
    }
    if(!(DVDReadBytes(ifofile->file, cmd_tbl->post_cmds, post_cmds_size))) {
      if(cmd_tbl->pre_cmds) 
	free(cmd_tbl->pre_cmds);
      free(cmd_tbl->post_cmds);
      return 0;
    }
  }

  if(cmd_tbl->nr_of_cell != 0) {
    unsigned int cell_cmds_size = cmd_tbl->nr_of_cell * COMMAND_DATA_SIZE;
    cmd_tbl->cell_cmds = (vm_cmd_t *)malloc(cell_cmds_size);
    if(!cmd_tbl->cell_cmds) {
      if(cmd_tbl->pre_cmds)
	free(cmd_tbl->pre_cmds);
      if(cmd_tbl->post_cmds)
	free(cmd_tbl->post_cmds);
      return 0;
    }
    if(!(DVDReadBytes(ifofile->file, cmd_tbl->cell_cmds, cell_cmds_size))) {
      if(cmd_tbl->pre_cmds) 
	free(cmd_tbl->pre_cmds);
      if(cmd_tbl->post_cmds) 
	free(cmd_tbl->post_cmds);
      free(cmd_tbl->cell_cmds);
      return 0;
    }
  }
  
  /* 
   * Make a run over all the commands and see that we can interpret them all?
   */
  return 1;
}


static void ifoFree_PGC_COMMAND_TBL(pgc_command_tbl_t *cmd_tbl) {
  if(cmd_tbl) {
    if(cmd_tbl->nr_of_pre && cmd_tbl->pre_cmds)
      free(cmd_tbl->pre_cmds);
    if(cmd_tbl->nr_of_post && cmd_tbl->post_cmds)
      free(cmd_tbl->post_cmds);
    if(cmd_tbl->nr_of_cell && cmd_tbl->cell_cmds)
      free(cmd_tbl->cell_cmds);
    free(cmd_tbl);
  }
}

static int ifoRead_PGC_PROGRAM_MAP(ifo_handle_t *ifofile, 
                                   pgc_program_map_t *program_map, 
				   unsigned int nr, unsigned int offset) {
  unsigned int size = nr * sizeof(pgc_program_map_t);

  if(!DVDFileSeek_(ifofile->file, offset))
    return 0;
 
  if(!(DVDReadBytes(ifofile->file, program_map, size)))
    return 0;

  return 1;
}

static int ifoRead_CELL_PLAYBACK_TBL(ifo_handle_t *ifofile, 
                                     cell_playback_t *cell_playback,
                                     unsigned int nr, unsigned int offset) {
  unsigned int i;
  unsigned int size = nr * sizeof(cell_playback_t);

  if(!DVDFileSeek_(ifofile->file, offset))
    return 0;

  if(!(DVDReadBytes(ifofile->file, cell_playback, size)))
    return 0;

  for(i = 0; i < nr; i++) {
    B2N_32(cell_playback[i].first_sector);
    B2N_32(cell_playback[i].first_ilvu_end_sector);
    B2N_32(cell_playback[i].last_vobu_start_sector);
    B2N_32(cell_playback[i].last_sector);
    
    /* Changed < to <= because this was false in the movie 'Pi'. */
    assert(cell_playback[i].last_vobu_start_sector <= 
           cell_playback[i].last_sector);
    assert(cell_playback[i].first_sector <= 
           cell_playback[i].last_vobu_start_sector);
  }

  return 1;
}


static int ifoRead_CELL_POSITION_TBL(ifo_handle_t *ifofile, 
                                     cell_position_t *cell_position, 
                                     unsigned int nr, unsigned int offset) {
  unsigned int i;
  unsigned int size = nr * sizeof(cell_position_t);

  if(!DVDFileSeek_(ifofile->file, offset))
    return 0;

  if(!(DVDReadBytes(ifofile->file, cell_position, size)))
    return 0;

  for(i = 0; i < nr; i++) {
    B2N_16(cell_position[i].vob_id_nr);
    CHECK_ZERO(cell_position[i].zero_1);
  }

  return 1;
}

static int ifoRead_PGC(ifo_handle_t *ifofile, pgc_t *pgc, unsigned int offset) {
  unsigned int i;

  if(!DVDFileSeek_(ifofile->file, offset))
    return 0;
 
  if(!(DVDReadBytes(ifofile->file, pgc, PGC_SIZE)))
    return 0;

  B2N_16(pgc->next_pgc_nr);
  B2N_16(pgc->prev_pgc_nr);
  B2N_16(pgc->goup_pgc_nr);
  B2N_16(pgc->command_tbl_offset);
  B2N_16(pgc->program_map_offset);
  B2N_16(pgc->cell_playback_offset);
  B2N_16(pgc->cell_position_offset);

  for(i = 0; i < 8; i++)
    B2N_16(pgc->audio_control[i]);
  for(i = 0; i < 32; i++)
    B2N_32(pgc->subp_control[i]);
  for(i = 0; i < 16; i++)
    B2N_32(pgc->palette[i]);
  
  CHECK_ZERO(pgc->zero_1);
  assert(pgc->nr_of_programs <= pgc->nr_of_cells);

  /* verify time (look at print_time) */
  for(i = 0; i < 8; i++)
    if(!pgc->audio_control[i] & 0x8000) /* The 'is present' bit */
      CHECK_ZERO(pgc->audio_control[i]);
  for(i = 0; i < 32; i++)
    if(!pgc->subp_control[i] & 0x80000000) /* The 'is present' bit */
      CHECK_ZERO(pgc->subp_control[i]);
  
  /* Check that time is 0:0:0:0 also if nr_of_programs == 0 */
  if(pgc->nr_of_programs == 0) {
    CHECK_ZERO(pgc->still_time);
    CHECK_ZERO(pgc->pg_playback_mode); // ??
    assert(pgc->program_map_offset == 0);
    assert(pgc->cell_playback_offset == 0);
    assert(pgc->cell_position_offset == 0);
  } else {
    assert(pgc->program_map_offset != 0);
    assert(pgc->cell_playback_offset != 0);
    assert(pgc->cell_position_offset != 0);
  }
  
  if(pgc->command_tbl_offset != 0) {
    pgc->command_tbl = malloc(sizeof(pgc_command_tbl_t));
    if(!pgc->command_tbl)
      return 0;

    if(!ifoRead_PGC_COMMAND_TBL(ifofile, pgc->command_tbl, 
                                offset + pgc->command_tbl_offset)) {
      free(pgc->command_tbl);
      return 0;
    }
  } else {
    pgc->command_tbl = NULL;
  }
  
  if(pgc->program_map_offset != 0) {
    pgc->program_map = malloc(pgc->nr_of_programs * sizeof(pgc_program_map_t));
    if(!pgc->program_map) {
      ifoFree_PGC_COMMAND_TBL(pgc->command_tbl);
      return 0;
    }
    if(!ifoRead_PGC_PROGRAM_MAP(ifofile, pgc->program_map,pgc->nr_of_programs,
                                offset + pgc->program_map_offset)) {
      ifoFree_PGC_COMMAND_TBL(pgc->command_tbl);
      free(pgc->program_map);
      return 0;
    }
  } else {
    pgc->program_map = NULL;
  }
  
  if(pgc->cell_playback_offset != 0) {
    pgc->cell_playback = malloc(pgc->nr_of_cells * sizeof(cell_playback_t));
    if(!pgc->cell_playback) {
      ifoFree_PGC_COMMAND_TBL(pgc->command_tbl);
      if(pgc->program_map)
	free(pgc->program_map);
      return 0;
    }
    if(!ifoRead_CELL_PLAYBACK_TBL(ifofile, pgc->cell_playback, 
				  pgc->nr_of_cells,
                                  offset + pgc->cell_playback_offset)) {
      ifoFree_PGC_COMMAND_TBL(pgc->command_tbl);
      if(pgc->program_map)
	free(pgc->program_map);
      free(pgc->cell_playback);
      return 0;
    }
  } else {
    pgc->cell_playback = NULL;
  }
  
  if(pgc->cell_position_offset != 0) {
    pgc->cell_position = malloc(pgc->nr_of_cells * sizeof(cell_position_t));
    if(!pgc->cell_position) {
      ifoFree_PGC(pgc);
      return 0;
    }
    if(!ifoRead_CELL_POSITION_TBL(ifofile, pgc->cell_position, 
				  pgc->nr_of_cells,
                                  offset + pgc->cell_position_offset)) {
      ifoFree_PGC(pgc);
      return 0;
    }
  } else {
    pgc->cell_position = NULL;
  }

  return 1;
}

int ifoRead_FP_PGC(ifo_handle_t *ifofile) {

  if(!ifofile)
    return 0;

  if(!ifofile->vmgi_mat)
    return 0;
  
  /* It seems that first_play_pgc might be optional after all. */
  if(ifofile->vmgi_mat->first_play_pgc == 0) { /* mandatory */
    ifofile->first_play_pgc = 0;
    return 0; /* change this to a 1 if it's optional. */
  }
  
  ifofile->first_play_pgc = (pgc_t *)malloc(sizeof(pgc_t));
  if(!ifofile->first_play_pgc)
    return 0;
  
  if(!ifoRead_PGC(ifofile, ifofile->first_play_pgc, 
                  ifofile->vmgi_mat->first_play_pgc)) {
    free(ifofile->first_play_pgc);
    ifofile->first_play_pgc = 0;
    return 0;
  }

  return 1;
}

static void ifoFree_PGC(pgc_t *pgc) {
  if(pgc) {
    ifoFree_PGC_COMMAND_TBL(pgc->command_tbl);
    if(pgc->program_map)
      free(pgc->program_map);
    if(pgc->cell_playback)
      free(pgc->cell_playback);
    if(pgc->cell_position)
      free(pgc->cell_position);
  }
}

void ifoFree_FP_PGC(ifo_handle_t *ifofile) {
  if(!ifofile)
    return;
  
  if(ifofile->first_play_pgc) {
    ifoFree_PGC(ifofile->first_play_pgc);
    free(ifofile->first_play_pgc);
    ifofile->first_play_pgc = 0;
  }
}


int ifoRead_TT_SRPT(ifo_handle_t *ifofile) {
  tt_srpt_t *tt_srpt;
  int i, info_length;

  if(!ifofile)
    return 0;

  if(!ifofile->vmgi_mat)
    return 0;

  if(ifofile->vmgi_mat->tt_srpt == 0) /* mandatory */
    return 0;

  if(!DVDFileSeek_(ifofile->file, ifofile->vmgi_mat->tt_srpt * DVD_BLOCK_LEN))
    return 0;

  tt_srpt = (tt_srpt_t *)malloc(sizeof(tt_srpt_t));
  if(!tt_srpt)
    return 0;

  ifofile->tt_srpt = tt_srpt;
  
  if(!(DVDReadBytes(ifofile->file, tt_srpt, TT_SRPT_SIZE))) {
    fprintf(stderr, "libdvdread: Unable to read read TT_SRPT.\n");
    free(tt_srpt);
    return 0;
  }

  B2N_16(tt_srpt->nr_of_srpts);
  B2N_32(tt_srpt->last_byte);
  
  info_length = tt_srpt->last_byte + 1 - TT_SRPT_SIZE;

  tt_srpt->title = (title_info_t *)malloc(info_length); 
  if(!tt_srpt->title) {
    free(tt_srpt);
    ifofile->tt_srpt = 0;
    return 0;
  }
  if(!(DVDReadBytes(ifofile->file, tt_srpt->title, info_length))) {
    fprintf(stderr, "libdvdread: Unable to read read TT_SRPT.\n");
    ifoFree_TT_SRPT(ifofile);
    return 0;
  }

  for(i =  0; i < tt_srpt->nr_of_srpts; i++) {
    B2N_16(tt_srpt->title[i].nr_of_ptts);
    B2N_16(tt_srpt->title[i].parental_id);
    B2N_32(tt_srpt->title[i].title_set_sector);
  }
  

  CHECK_ZERO(tt_srpt->zero_1);
  assert(tt_srpt->nr_of_srpts != 0);
  assert(tt_srpt->nr_of_srpts < 100); // ??
  assert((int)tt_srpt->nr_of_srpts * sizeof(title_info_t) <= info_length);
  
  for(i = 0; i < tt_srpt->nr_of_srpts; i++) {
    assert(tt_srpt->title[i].pb_ty.zero_1 == 0);
    assert(tt_srpt->title[i].nr_of_angles != 0);
    assert(tt_srpt->title[i].nr_of_angles < 10);
    //assert(tt_srpt->title[i].nr_of_ptts != 0);
    // XXX: this assertion breaks Ghostbusters:
    assert(tt_srpt->title[i].nr_of_ptts < 1000); // ??
    assert(tt_srpt->title[i].title_set_nr != 0);
    assert(tt_srpt->title[i].title_set_nr < 100); // ??
    assert(tt_srpt->title[i].vts_ttn != 0);
    assert(tt_srpt->title[i].vts_ttn < 100); // ??
    //assert(tt_srpt->title[i].title_set_sector != 0);
  }
  
  // Make this a function
#if 0
  if(memcmp((uint8_t *)tt_srpt->title + 
            tt_srpt->nr_of_srpts * sizeof(title_info_t), 
            my_friendly_zeros, 
            info_length - tt_srpt->nr_of_srpts * sizeof(title_info_t))) {
    fprintf(stderr, "VMG_PTT_SRPT slack is != 0, ");
    hexdump((uint8_t *)tt_srpt->title + 
            tt_srpt->nr_of_srpts * sizeof(title_info_t), 
            info_length - tt_srpt->nr_of_srpts * sizeof(title_info_t));
  }
#endif

  return 1;
}


void ifoFree_TT_SRPT(ifo_handle_t *ifofile) {
  if(!ifofile)
    return;
  
  if(ifofile->tt_srpt) {
    free(ifofile->tt_srpt->title);
    free(ifofile->tt_srpt);
    ifofile->tt_srpt = 0;
  }
}


int ifoRead_VTS_PTT_SRPT(ifo_handle_t *ifofile) {
  vts_ptt_srpt_t *vts_ptt_srpt;
  int info_length, i, j;
  uint32_t *data;

  if(!ifofile)
    return 0;
  
  if(!ifofile->vtsi_mat)
    return 0;

  if(ifofile->vtsi_mat->vts_ptt_srpt == 0) /* mandatory */
    return 0;
    
  if(!DVDFileSeek_(ifofile->file,
		   ifofile->vtsi_mat->vts_ptt_srpt * DVD_BLOCK_LEN))
    return 0;

  vts_ptt_srpt = (vts_ptt_srpt_t *)malloc(sizeof(vts_ptt_srpt_t));
  if(!vts_ptt_srpt)
    return 0;

  ifofile->vts_ptt_srpt = vts_ptt_srpt;

  if(!(DVDReadBytes(ifofile->file, vts_ptt_srpt, VTS_PTT_SRPT_SIZE))) {
    fprintf(stderr, "libdvdread: Unable to read PTT search table.\n");
    free(vts_ptt_srpt);
    return 0;
  }

  B2N_16(vts_ptt_srpt->nr_of_srpts);
  B2N_32(vts_ptt_srpt->last_byte);

  CHECK_ZERO(vts_ptt_srpt->zero_1);
  assert(vts_ptt_srpt->nr_of_srpts != 0);
  assert(vts_ptt_srpt->nr_of_srpts < 100); // ??
  
  info_length = vts_ptt_srpt->last_byte + 1 - VTS_PTT_SRPT_SIZE;
  
  data = (uint32_t *)malloc(info_length); 
  if(!data) {
    free(vts_ptt_srpt);
    ifofile->vts_ptt_srpt = 0;
    return 0;
  }
  if(!(DVDReadBytes(ifofile->file, data, info_length))) {
    fprintf(stderr, "libdvdread: Unable to read PTT search table.\n");
    free(vts_ptt_srpt);
    free(data);
    ifofile->vts_ptt_srpt = 0;
    return 0;
  }

  for(i = 0; i < vts_ptt_srpt->nr_of_srpts; i++) {
    B2N_32(data[i]);
    /* assert(data[i] + sizeof(ptt_info_t) <= vts_ptt_srpt->last_byte + 1);
       Magic Knight Rayearth Daybreak is mastered very strange and has 
       Titles with 0 PTTs. They all have a data[i] offsets beyond the end of
       of the vts_ptt_srpt structure. */
    assert(data[i] + sizeof(ptt_info_t) <= vts_ptt_srpt->last_byte + 1 + 4);
  }
  
  vts_ptt_srpt->title = malloc(vts_ptt_srpt->nr_of_srpts * sizeof(ttu_t));
  if(!vts_ptt_srpt->title) {
    free(vts_ptt_srpt);
    free(data);
    ifofile->vts_ptt_srpt = 0;
    return 0;
  }
  for(i = 0; i < vts_ptt_srpt->nr_of_srpts; i++) {
    int n;
    if(i < vts_ptt_srpt->nr_of_srpts - 1)
      n = (data[i+1] - data[i]);
    else
      n = (vts_ptt_srpt->last_byte + 1 - data[i]);
    /* assert(n > 0 && (n % 4) == 0);
       Magic Knight Rayearth Daybreak is mastered very strange and has 
       Titles with 0 PTTs. */
    if(n < 0) n = 0;
    assert(n % 4 == 0);
    
    vts_ptt_srpt->title[i].nr_of_ptts = n / 4;
    vts_ptt_srpt->title[i].ptt = malloc(n * sizeof(ptt_info_t));
    if(!vts_ptt_srpt->title[i].ptt) {
      for(n = 0; n < i; n++)
        free(vts_ptt_srpt->title[n].ptt);
      free(vts_ptt_srpt);
      free(data);
      ifofile->vts_ptt_srpt = 0;
      return 0;
    }
    for(j = 0; j < vts_ptt_srpt->title[i].nr_of_ptts; j++) {
      /* The assert placed here because of Magic Knight Rayearth Daybreak */
      assert(data[i] + sizeof(ptt_info_t) <= vts_ptt_srpt->last_byte + 1);
      vts_ptt_srpt->title[i].ptt[j].pgcn 
        = *(uint16_t*)(((char *)data) + data[i] + 4*j - VTS_PTT_SRPT_SIZE);
      vts_ptt_srpt->title[i].ptt[j].pgn 
        = *(uint16_t*)(((char *)data) + data[i] + 4*j + 2 - VTS_PTT_SRPT_SIZE);
    }
  }
  free(data);
  
  for(i = 0; i < vts_ptt_srpt->nr_of_srpts; i++) {
    for(j = 0; j < vts_ptt_srpt->title[i].nr_of_ptts; j++) {
      B2N_16(vts_ptt_srpt->title[i].ptt[j].pgcn);
      B2N_16(vts_ptt_srpt->title[i].ptt[j].pgn);
    }
  }
  
  for(i = 0; i < vts_ptt_srpt->nr_of_srpts; i++) {
    assert(vts_ptt_srpt->title[i].nr_of_ptts < 1000); // ??
    for(j = 0; j < vts_ptt_srpt->title[i].nr_of_ptts; j++) {
      assert(vts_ptt_srpt->title[i].ptt[j].pgcn != 0 );
      assert(vts_ptt_srpt->title[i].ptt[j].pgcn < 1000); // ??
      assert(vts_ptt_srpt->title[i].ptt[j].pgn != 0);
      assert(vts_ptt_srpt->title[i].ptt[j].pgn < 100); // ??
    }
  }

  return 1;
}


void ifoFree_VTS_PTT_SRPT(ifo_handle_t *ifofile) {
  if(!ifofile)
    return;
  
  if(ifofile->vts_ptt_srpt) {
    int i;
    for(i = 0; i < ifofile->vts_ptt_srpt->nr_of_srpts; i++)
      free(ifofile->vts_ptt_srpt->title[i].ptt);
    free(ifofile->vts_ptt_srpt->title);
    free(ifofile->vts_ptt_srpt);
    ifofile->vts_ptt_srpt = 0;
  }
}


int ifoRead_PTL_MAIT(ifo_handle_t *ifofile) {
  ptl_mait_t *ptl_mait;
  int info_length;
  unsigned int i;

  if(!ifofile)
    return 0;
  
  if(!ifofile->vmgi_mat)
    return 0;
  
  if(ifofile->vmgi_mat->ptl_mait == 0)
    return 1;

  if(!DVDFileSeek_(ifofile->file, 
		  ifofile->vmgi_mat->ptl_mait * DVD_BLOCK_LEN))
    return 0;

  ptl_mait = (ptl_mait_t *)malloc(sizeof(ptl_mait_t));
  if(!ptl_mait)
    return 0;

  ifofile->ptl_mait = ptl_mait;

  if(!(DVDReadBytes(ifofile->file, ptl_mait, PTL_MAIT_SIZE))) {
    free(ptl_mait);
    ifofile->ptl_mait = 0;
    return 0;
  }

  B2N_16(ptl_mait->nr_of_countries);
  B2N_16(ptl_mait->nr_of_vtss);
  B2N_32(ptl_mait->last_byte);
  
  info_length = ptl_mait->last_byte + 1 - PTL_MAIT_SIZE;
  
  assert(ptl_mait->nr_of_countries != 0);
  assert(ptl_mait->nr_of_countries < 100); // ??
  assert(ptl_mait->nr_of_vtss != 0);
  assert(ptl_mait->nr_of_vtss < 100); // ??  
  assert(ptl_mait->nr_of_countries * PTL_MAIT_COUNTRY_SIZE <= info_length);
  
  /* Change this to read and 'translate' the tables too. 
     I.e don't read so much here */
  ptl_mait->countries = (ptl_mait_country_t *)malloc(info_length);
  if(!ptl_mait->countries) {
    free(ptl_mait);
    ifofile->ptl_mait = 0;
    return 0;
  }
  if(!(DVDReadBytes(ifofile->file, ptl_mait->countries, info_length))) {
    fprintf(stderr, "libdvdread: Unable to read PTL_MAIT.\n");
    ifoFree_PTL_MAIT(ifofile);
    return 0;
  }

  for(i = 0; i < ptl_mait->nr_of_countries; i++) {
    B2N_16(ptl_mait->countries[i].country_code);
    B2N_16(ptl_mait->countries[i].pf_ptl_mai_start_byte);
  }
  
  for(i = 0; i < ptl_mait->nr_of_countries; i++) {
    CHECK_ZERO(ptl_mait->countries[i].zero_1);
    CHECK_ZERO(ptl_mait->countries[i].zero_2);    
    assert(ptl_mait->countries[i].pf_ptl_mai_start_byte + 
           8 * (ptl_mait->nr_of_vtss + 1) * 2 <= ptl_mait->last_byte + 1);
  }

  return 1;
}


void ifoFree_PTL_MAIT(ifo_handle_t *ifofile) {
  if(!ifofile)
    return;
  
  if(ifofile->ptl_mait) {
    free(ifofile->ptl_mait->countries);
    free(ifofile->ptl_mait);
    ifofile->ptl_mait = 0;
  }
}

int ifoRead_TITLE_C_ADT(ifo_handle_t *ifofile) {

  if(!ifofile)
    return 0;

  if(!ifofile->vtsi_mat)
    return 0;

  if(ifofile->vtsi_mat->vts_c_adt == 0) /* mandatory */
    return 0;

  ifofile->vts_c_adt = (c_adt_t *)malloc(sizeof(c_adt_t));
  if(!ifofile->vts_c_adt)
    return 0;

  if(!ifoRead_C_ADT_internal(ifofile, ifofile->vts_c_adt, 
                             ifofile->vtsi_mat->vts_c_adt)) {
    free(ifofile->vts_c_adt);
    ifofile->vts_c_adt = 0;
    return 0;
  }

  return 1;
}

int ifoRead_C_ADT(ifo_handle_t *ifofile) {
  unsigned int sector;

  if(!ifofile)
    return 0;
  
  if(ifofile->vmgi_mat) {
    if(ifofile->vmgi_mat->vmgm_c_adt == 0)
      return 1;
    sector = ifofile->vmgi_mat->vmgm_c_adt;
  } else if(ifofile->vtsi_mat) {
    if(ifofile->vtsi_mat->vtsm_c_adt == 0)
      return 1;
    sector = ifofile->vtsi_mat->vtsm_c_adt;
  } else {
    return 0;
  }
  
  ifofile->menu_c_adt = (c_adt_t *)malloc(sizeof(c_adt_t));
  if(!ifofile->menu_c_adt)
    return 0;

  if(!ifoRead_C_ADT_internal(ifofile, ifofile->menu_c_adt, sector)) {
    free(ifofile->menu_c_adt);
    ifofile->menu_c_adt = 0;
    return 0;
  }

  return 1;
}

static int ifoRead_C_ADT_internal(ifo_handle_t *ifofile, 
                                  c_adt_t *c_adt, unsigned int sector) {
  int i, info_length;

  if(!DVDFileSeek_(ifofile->file, sector * DVD_BLOCK_LEN))
    return 0;

  if(!(DVDReadBytes(ifofile->file, c_adt, C_ADT_SIZE)))
    return 0;

  B2N_16(c_adt->nr_of_vobs);
  B2N_32(c_adt->last_byte);
  
  info_length = c_adt->last_byte + 1 - C_ADT_SIZE;
  
  CHECK_ZERO(c_adt->zero_1);
  /* assert(c_adt->nr_of_vobs > 0);  
     Magic Knight Rayearth Daybreak is mastered very strange and has 
     Titles with a VOBS that has no cells. */
  assert(info_length % sizeof(cell_adr_t) == 0);
  
  /* assert(info_length / sizeof(cell_adr_t) >= c_adt->nr_of_vobs);
     Enemy of the State region 2 (de) has Titles where nr_of_vobs field
     is to high, they high ones are never referenced though. */
  if(info_length / sizeof(cell_adr_t) < c_adt->nr_of_vobs) {
    fprintf(stderr, "libdvdread: *C_ADT nr_of_vobs > avaiable info entries\n");
    c_adt->nr_of_vobs = info_length / sizeof(cell_adr_t);
  }
  
  c_adt->cell_adr_table = (cell_adr_t *)malloc(info_length);
  if(!c_adt->cell_adr_table)
    return 0;

  if(info_length && 
     !(DVDReadBytes(ifofile->file, c_adt->cell_adr_table, info_length))) {
    free(c_adt->cell_adr_table);
    return 0;
  }

  for(i = 0; i < info_length/sizeof(cell_adr_t); i++) {
    B2N_16(c_adt->cell_adr_table[i].vob_id);
    B2N_32(c_adt->cell_adr_table[i].start_sector);
    B2N_32(c_adt->cell_adr_table[i].last_sector);

    CHECK_ZERO(c_adt->cell_adr_table[i].zero_1);
    assert(c_adt->cell_adr_table[i].vob_id > 0);
    assert(c_adt->cell_adr_table[i].vob_id <= c_adt->nr_of_vobs);
    assert(c_adt->cell_adr_table[i].cell_id > 0);
    assert(c_adt->cell_adr_table[i].start_sector < 
	   c_adt->cell_adr_table[i].last_sector);
  }

  return 1;
}


static void ifoFree_C_ADT_internal(c_adt_t *c_adt) {
  if(c_adt) {
    free(c_adt->cell_adr_table);
    free(c_adt);
  }
}

void ifoFree_C_ADT(ifo_handle_t *ifofile) {
  if(!ifofile)
    return;
  
  ifoFree_C_ADT_internal(ifofile->menu_c_adt);
  ifofile->menu_c_adt = 0;
}

void ifoFree_TITLE_C_ADT(ifo_handle_t *ifofile) {
  if(!ifofile)
    return;
  
  ifoFree_C_ADT_internal(ifofile->vts_c_adt);
  ifofile->vts_c_adt = 0;
}

int ifoRead_TITLE_VOBU_ADMAP(ifo_handle_t *ifofile) {
  if(!ifofile)
    return 0;

  if(!ifofile->vtsi_mat)
    return 0;
  
  if(ifofile->vtsi_mat->vts_vobu_admap == 0) /* mandatory */
    return 0;
  
  ifofile->vts_vobu_admap = (vobu_admap_t *)malloc(sizeof(vobu_admap_t));
  if(!ifofile->vts_vobu_admap)
    return 0;

  if(!ifoRead_VOBU_ADMAP_internal(ifofile, ifofile->vts_vobu_admap,
                                  ifofile->vtsi_mat->vts_vobu_admap)) {
    free(ifofile->vts_vobu_admap);
    ifofile->vts_vobu_admap = 0;
    return 0;
  }

  return 1;
}

int ifoRead_VOBU_ADMAP(ifo_handle_t *ifofile) {
  unsigned int sector;

  if(!ifofile)
    return 0;
     
  if(ifofile->vmgi_mat) {
    if(ifofile->vmgi_mat->vmgm_vobu_admap == 0)
      return 1;
    sector = ifofile->vmgi_mat->vmgm_vobu_admap;
  } else if(ifofile->vtsi_mat) {
    if(ifofile->vtsi_mat->vtsm_vobu_admap == 0)
      return 1;
    sector = ifofile->vtsi_mat->vtsm_vobu_admap;
  } else {
    return 0;
  }
  
  ifofile->menu_vobu_admap = (vobu_admap_t *)malloc(sizeof(vobu_admap_t));
  if(!ifofile->menu_vobu_admap)
    return 0;
  
  if(!ifoRead_VOBU_ADMAP_internal(ifofile, ifofile->menu_vobu_admap, sector)) {
    free(ifofile->menu_vobu_admap);
    ifofile->menu_vobu_admap = 0;
    return 0;
  }

  return 1;
}

static int ifoRead_VOBU_ADMAP_internal(ifo_handle_t *ifofile, 
                                       vobu_admap_t *vobu_admap, 
				       unsigned int sector) {
  unsigned int i;
  int info_length;

  if(!DVDFileSeek_(ifofile->file, sector * DVD_BLOCK_LEN))
    return 0;

  if(!(DVDReadBytes(ifofile->file, vobu_admap, VOBU_ADMAP_SIZE)))
    return 0;

  B2N_32(vobu_admap->last_byte);
  
  info_length = vobu_admap->last_byte + 1 - VOBU_ADMAP_SIZE;
  /* assert(info_length > 0);
     Magic Knight Rayearth Daybreak is mastered very strange and has 
     Titles with a VOBS that has no VOBUs. */
  assert(info_length % sizeof(uint32_t) == 0);
  
  vobu_admap->vobu_start_sectors = (uint32_t *)malloc(info_length); 
  if(!vobu_admap->vobu_start_sectors) {
    return 0;
  }
  if(info_length && 
     !(DVDReadBytes(ifofile->file, 
		    vobu_admap->vobu_start_sectors, info_length))) {
    free(vobu_admap->vobu_start_sectors);
    return 0;
  }

  for(i = 0; i < info_length/sizeof(uint32_t); i++)
    B2N_32(vobu_admap->vobu_start_sectors[i]);

  return 1;
}


static void ifoFree_VOBU_ADMAP_internal(vobu_admap_t *vobu_admap) {
  if(vobu_admap) {
    free(vobu_admap->vobu_start_sectors);
    free(vobu_admap);
  }
}

void ifoFree_VOBU_ADMAP(ifo_handle_t *ifofile) {
  if(!ifofile)
    return;
  
  ifoFree_VOBU_ADMAP_internal(ifofile->menu_vobu_admap);
  ifofile->menu_vobu_admap = 0;
}

void ifoFree_TITLE_VOBU_ADMAP(ifo_handle_t *ifofile) {
  if(!ifofile)
    return;
  
  ifoFree_VOBU_ADMAP_internal(ifofile->vts_vobu_admap);
  ifofile->vts_vobu_admap = 0;
}

int ifoRead_PGCIT(ifo_handle_t *ifofile) {

  if(!ifofile)
    return 0;
  
  if(!ifofile->vtsi_mat)
    return 0;
  
  if(ifofile->vtsi_mat->vts_pgcit == 0) /* mandatory */
    return 0;
  
  ifofile->vts_pgcit = (pgcit_t *)malloc(sizeof(pgcit_t));
  if(!ifofile->vts_pgcit)
    return 0;

  if(!ifoRead_PGCIT_internal(ifofile, ifofile->vts_pgcit, 
                             ifofile->vtsi_mat->vts_pgcit * DVD_BLOCK_LEN)) {
    free(ifofile->vts_pgcit);
    ifofile->vts_pgcit = 0;
    return 0;
  }

  return 1;
}

static int ifoRead_PGCIT_internal(ifo_handle_t *ifofile, pgcit_t *pgcit, 
                                  unsigned int offset) {
  int i, info_length;
  uint8_t *data, *ptr;
  
  if(!DVDFileSeek_(ifofile->file, offset))
    return 0;

  if(!(DVDReadBytes(ifofile->file, pgcit, PGCIT_SIZE)))
    return 0;

  B2N_16(pgcit->nr_of_pgci_srp);
  B2N_32(pgcit->last_byte);
  
  CHECK_ZERO(pgcit->zero_1);
  /* assert(pgcit->nr_of_pgci_srp != 0);
     Magic Knight Rayearth Daybreak is mastered very strange and has 
     Titles with 0 PTTs. */
  assert(pgcit->nr_of_pgci_srp < 10000); // ?? seen max of 1338
  
  info_length = pgcit->nr_of_pgci_srp * PGCI_SRP_SIZE;
  data = malloc(info_length);
  if(!data)
    return 0;

  if(info_length && !(DVDReadBytes(ifofile->file, data, info_length))) {
    free(data);
    return 0;
  }

  pgcit->pgci_srp = malloc(pgcit->nr_of_pgci_srp * sizeof(pgci_srp_t));
  if(!pgcit->pgci_srp) {
    free(data);
    return 0;
  }
  ptr = data;
  for(i = 0; i < pgcit->nr_of_pgci_srp; i++) {
    memcpy(&pgcit->pgci_srp[i], ptr, PGCI_LU_SIZE);
    ptr += PGCI_LU_SIZE;
    B2N_16(pgcit->pgci_srp[i].ptl_id_mask);
    B2N_32(pgcit->pgci_srp[i].pgc_start_byte);
    assert(pgcit->pgci_srp[i].unknown1 == 0);
  }
  free(data);
  
  for(i = 0; i < pgcit->nr_of_pgci_srp; i++)
    assert(pgcit->pgci_srp[i].pgc_start_byte + PGC_SIZE <= pgcit->last_byte+1);
  
  for(i = 0; i < pgcit->nr_of_pgci_srp; i++) {
    pgcit->pgci_srp[i].pgc = malloc(sizeof(pgc_t));
    if(!pgcit->pgci_srp[i].pgc) {
      int j;
      for(j = 0; j < i; j++) {
        ifoFree_PGC(pgcit->pgci_srp[j].pgc);
        free(pgcit->pgci_srp[j].pgc);
      }
      return 0;
    }
    if(!ifoRead_PGC(ifofile, pgcit->pgci_srp[i].pgc, 
                    offset + pgcit->pgci_srp[i].pgc_start_byte)) {
      int j;
      for(j = 0; j < i; j++) {
        ifoFree_PGC(pgcit->pgci_srp[j].pgc);
        free(pgcit->pgci_srp[j].pgc);
      }
      free(pgcit->pgci_srp);
      return 0;
    }
  }

  return 1;
}

static void ifoFree_PGCIT_internal(pgcit_t *pgcit) {
  if(pgcit) {
    int i;
    for(i = 0; i < pgcit->nr_of_pgci_srp; i++)
      ifoFree_PGC(pgcit->pgci_srp[i].pgc);
    free(pgcit->pgci_srp);
  }
}

void ifoFree_PGCIT(ifo_handle_t *ifofile) {
  if(!ifofile)
    return;
  
  if(ifofile->vts_pgcit) {
    ifoFree_PGCIT_internal(ifofile->vts_pgcit);
    free(ifofile->vts_pgcit);
    ifofile->vts_pgcit = 0;
  }
}


int ifoRead_PGCI_UT(ifo_handle_t *ifofile) {
  pgci_ut_t *pgci_ut;
  unsigned int sector;
  unsigned int i;  
  int info_length;
  uint8_t *data, *ptr;

  if(!ifofile)
    return 0;
  
  if(ifofile->vmgi_mat) {
    if(ifofile->vmgi_mat->vmgm_pgci_ut == 0)
      return 1;
    sector = ifofile->vmgi_mat->vmgm_pgci_ut;
  } else if(ifofile->vtsi_mat) {
    if(ifofile->vtsi_mat->vtsm_pgci_ut == 0)
      return 1;
    sector = ifofile->vtsi_mat->vtsm_pgci_ut;
  } else {
    return 0;
  }
  
  ifofile->pgci_ut = (pgci_ut_t *)malloc(sizeof(pgci_ut_t));
  if(!ifofile->pgci_ut)
    return 0;
  
  if(!DVDFileSeek_(ifofile->file, sector * DVD_BLOCK_LEN)) {
    free(ifofile->pgci_ut);
    ifofile->pgci_ut = 0;
    return 0;
  }
  
  if(!(DVDReadBytes(ifofile->file, ifofile->pgci_ut, PGCI_UT_SIZE))) {
    free(ifofile->pgci_ut);
    ifofile->pgci_ut = 0;
    return 0;
  }
  
  pgci_ut = ifofile->pgci_ut;
  
  B2N_16(pgci_ut->nr_of_lus);
  B2N_32(pgci_ut->last_byte);
  
  CHECK_ZERO(pgci_ut->zero_1);
  assert(pgci_ut->nr_of_lus != 0);
  assert(pgci_ut->nr_of_lus < 100); // ?? 3-4 ?
  assert((uint32_t)pgci_ut->nr_of_lus * PGCI_LU_SIZE < pgci_ut->last_byte);

  info_length = pgci_ut->nr_of_lus * PGCI_LU_SIZE;
  data = malloc(info_length);
  if(!data) {
    free(pgci_ut);
    ifofile->pgci_ut = 0;
    return 0;
  }
  if(!(DVDReadBytes(ifofile->file, data, info_length))) {
    free(data);
    free(pgci_ut);
    ifofile->pgci_ut = 0;
    return 0;
  }

  pgci_ut->lu = malloc(pgci_ut->nr_of_lus * sizeof(pgci_lu_t));
  if(!pgci_ut->lu) {
    free(data);
    free(pgci_ut);
    ifofile->pgci_ut = 0;
   return 0;
  }
  ptr = data;
  for(i = 0; i < pgci_ut->nr_of_lus; i++) {
    memcpy(&pgci_ut->lu[i], ptr, PGCI_LU_SIZE);
    ptr += PGCI_LU_SIZE;
    B2N_16(pgci_ut->lu[i].lang_code); 
    B2N_32(pgci_ut->lu[i].lang_start_byte); 
  }
  free(data);
  
  for(i = 0; i < pgci_ut->nr_of_lus; i++) {
    CHECK_ZERO(pgci_ut->lu[i].zero_1);
    // Maybe this is only defined for v1.1 and later titles?
    /* If the bits in 'lu[i].exists' are enumerated abcd efgh then:
            VTS_x_yy.IFO        VIDEO_TS.IFO
       a == 0x83 "Root"         0x82 "Title"
       b == 0x84 "Subpicture"
       c == 0x85 "Audio"
       d == 0x86 "Angle"
       e == 0x87 "PTT"
    */
    assert((pgci_ut->lu[i].exists & 0x07) == 0);
  }

  for(i = 0; i < pgci_ut->nr_of_lus; i++) {
    pgci_ut->lu[i].pgcit = malloc(sizeof(pgcit_t));
    if(!pgci_ut->lu[i].pgcit) {
      unsigned int j;
      for(j = 0; j < i; j++) {
        ifoFree_PGCIT_internal(pgci_ut->lu[j].pgcit);
        free(pgci_ut->lu[j].pgcit);
      }
      free(pgci_ut->lu);
      free(pgci_ut);
      ifofile->pgci_ut = 0;
      return 0;
    }
    if(!ifoRead_PGCIT_internal(ifofile, pgci_ut->lu[i].pgcit, 
                               sector * DVD_BLOCK_LEN 
                               + pgci_ut->lu[i].lang_start_byte)) {
      unsigned int j;
      for(j = 0; j < i; j++) {
        ifoFree_PGCIT_internal(pgci_ut->lu[j].pgcit);
        free(pgci_ut->lu[j].pgcit);
      }
      free(pgci_ut->lu[i].pgcit);
      free(pgci_ut->lu);
      free(pgci_ut);
      ifofile->pgci_ut = 0;
      return 0;
    }
    // FIXME: Iterate and verify that all menus that should exists accordingly
    //        to pgci_ut->lu[i].exists really do?
  }

  return 1;
}


void ifoFree_PGCI_UT(ifo_handle_t *ifofile) {
  unsigned int i;

  if(!ifofile)
    return;
  
  if(ifofile->pgci_ut) {
    for(i = 0; i < ifofile->pgci_ut->nr_of_lus; i++) {
      ifoFree_PGCIT_internal(ifofile->pgci_ut->lu[i].pgcit);
      free(ifofile->pgci_ut->lu[i].pgcit);
    }
    free(ifofile->pgci_ut->lu);
    free(ifofile->pgci_ut);
    ifofile->pgci_ut = 0;
  }
}

static int ifoRead_VTS_ATTRIBUTES(ifo_handle_t *ifofile, 
                                  vts_attributes_t *vts_attributes, 
                                  unsigned int offset) {
  unsigned int i;

  if(!DVDFileSeek_(ifofile->file, offset))
    return 0;

  if(!(DVDReadBytes(ifofile->file, vts_attributes, sizeof(vts_attributes_t))))
    return 0;

  B2N_32(vts_attributes->last_byte);
  B2N_32(vts_attributes->vts_cat);
  B2N_16(vts_attributes->vtsm_audio_attr.lang_code);
  B2N_16(vts_attributes->vtsm_subp_attr.lang_code);
  for(i = 0; i < 8; i++)
    B2N_16(vts_attributes->vtstt_audio_attr[i].lang_code);
  for(i = 0; i < 32; i++)
    B2N_16(vts_attributes->vtstt_subp_attr[i].lang_code);
  
  CHECK_ZERO(vts_attributes->zero_1);
  CHECK_ZERO(vts_attributes->zero_2);
  CHECK_ZERO(vts_attributes->zero_3);
  CHECK_ZERO(vts_attributes->zero_4);
  CHECK_ZERO(vts_attributes->zero_5);
  CHECK_ZERO(vts_attributes->zero_6);
  CHECK_ZERO(vts_attributes->zero_7);
  assert(vts_attributes->nr_of_vtsm_audio_streams <= 1);
  assert(vts_attributes->nr_of_vtsm_subp_streams <= 1);
  assert(vts_attributes->nr_of_vtstt_audio_streams <= 8);
  for(i = vts_attributes->nr_of_vtstt_audio_streams; i < 8; i++)
    CHECK_ZERO(vts_attributes->vtstt_audio_attr[i]);
  assert(vts_attributes->nr_of_vtstt_subp_streams <= 32);
  {
    unsigned int nr_coded;
    assert(vts_attributes->last_byte + 1 >= VTS_ATTRIBUTES_MIN_SIZE);  
    nr_coded = (vts_attributes->last_byte + 1 - VTS_ATTRIBUTES_MIN_SIZE)/6;
    // This is often nr_coded = 70, how do you know how many there really are?
    if(nr_coded > 32) { // We haven't read more from disk/file anyway
      nr_coded = 32;
    }
    assert(vts_attributes->nr_of_vtstt_subp_streams <= nr_coded);
    for(i = vts_attributes->nr_of_vtstt_subp_streams; i < nr_coded; i++)
      CHECK_ZERO(vts_attributes->vtstt_subp_attr[i]);
  }

  return 1;
}



int ifoRead_VTS_ATRT(ifo_handle_t *ifofile) {
  vts_atrt_t *vts_atrt;
  unsigned int i, info_length, sector;
  uint32_t *data;

  if(!ifofile)
    return 0;
  
  if(!ifofile->vmgi_mat)
    return 0;
  
  if(ifofile->vmgi_mat->vts_atrt == 0) /* mandatory */
    return 0;
  
  sector = ifofile->vmgi_mat->vts_atrt;
  if(!DVDFileSeek_(ifofile->file, sector * DVD_BLOCK_LEN))
    return 0;

  vts_atrt = (vts_atrt_t *)malloc(sizeof(vts_atrt_t));
  if(!vts_atrt)
    return 0;

  ifofile->vts_atrt = vts_atrt;
  
  if(!(DVDReadBytes(ifofile->file, vts_atrt, VTS_ATRT_SIZE))) {
    free(vts_atrt);
    ifofile->vts_atrt = 0;
    return 0;
  }

  B2N_16(vts_atrt->nr_of_vtss);
  B2N_32(vts_atrt->last_byte);

  CHECK_ZERO(vts_atrt->zero_1);
  assert(vts_atrt->nr_of_vtss != 0);
  assert(vts_atrt->nr_of_vtss < 100); //??
  assert((uint32_t)vts_atrt->nr_of_vtss * (4 + VTS_ATTRIBUTES_MIN_SIZE) + 
         VTS_ATRT_SIZE < vts_atrt->last_byte + 1);

  info_length = vts_atrt->nr_of_vtss * sizeof(uint32_t);
  data = (uint32_t *)malloc(info_length);
  if(!data) {
    free(vts_atrt);
    ifofile->vts_atrt = 0;
    return 0;
  }
  if(!(DVDReadBytes(ifofile->file, data, info_length))) {
    free(data);
    free(vts_atrt);
    ifofile->vts_atrt = 0;
    return 0;
  }
  
  for(i = 0; i < vts_atrt->nr_of_vtss; i++) {
    B2N_32(data[i]);
    assert(data[i] + VTS_ATTRIBUTES_MIN_SIZE < vts_atrt->last_byte + 1);
  }
  
  info_length = vts_atrt->nr_of_vtss * sizeof(vts_attributes_t);
  vts_atrt->vts = (vts_attributes_t *)malloc(info_length);
  if(!vts_atrt->vts) {
    free(data);
    free(vts_atrt);
    ifofile->vts_atrt = 0;
    return 0;
  }
  for(i = 0; i < vts_atrt->nr_of_vtss; i++) {
    unsigned int offset = data[i];
    if(!ifoRead_VTS_ATTRIBUTES(ifofile, &(vts_atrt->vts[i]),
                               (sector * DVD_BLOCK_LEN) + offset)) {
      free(data);
      free(vts_atrt);
      ifofile->vts_atrt = 0;
      return 0;
    }

    // This assert cant be in ifoRead_VTS_ATTRIBUTES
    assert(offset + vts_atrt->vts[i].last_byte <= vts_atrt->last_byte + 1);
    // Is this check correct?
  }
  free(data);

  return 1;
}


void ifoFree_VTS_ATRT(ifo_handle_t *ifofile) {
  if(!ifofile)
    return;
  
  if(ifofile->vts_atrt) {
    free(ifofile->vts_atrt->vts);
    free(ifofile->vts_atrt);
    ifofile->vts_atrt = 0;
  }
}


int ifoRead_TXTDT_MGI(ifo_handle_t *ifofile) {
  txtdt_mgi_t *txtdt_mgi;

  if(!ifofile)
    return 0;
  
  if(!ifofile->vmgi_mat)
    return 0;
 
  /* Return successfully if there is nothing to read. */ 
  if(ifofile->vmgi_mat->txtdt_mgi == 0)
    return 1;

  if(!DVDFileSeek_(ifofile->file, 
		   ifofile->vmgi_mat->txtdt_mgi * DVD_BLOCK_LEN))
    return 0;
  
  txtdt_mgi = (txtdt_mgi_t *)malloc(sizeof(txtdt_mgi_t));
  if(!txtdt_mgi) {
    return 0;
  }
  ifofile->txtdt_mgi = txtdt_mgi;

  if(!(DVDReadBytes(ifofile->file, txtdt_mgi, TXTDT_MGI_SIZE))) {
    fprintf(stderr, "libdvdread: Unable to read TXTDT_MGI.\n");
    free(txtdt_mgi);
    ifofile->txtdt_mgi = 0;
    return 0;
  }

  // fprintf(stderr, "-- Not done yet --\n");
  return 1;
}

void ifoFree_TXTDT_MGI(ifo_handle_t *ifofile) {
  if(!ifofile)
    return;
  
  if(ifofile->txtdt_mgi) {
    free(ifofile->txtdt_mgi);
    ifofile->txtdt_mgi = 0;
  }
}

