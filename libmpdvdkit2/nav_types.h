#ifndef NAV_TYPES_H_INCLUDED
#define NAV_TYPES_H_INCLUDED

/*
 * Copyright (C) 2000, 2001 Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
 * The data structures in this file should represent the layout of the
 * pci and dsi packets as they are stored in the stream.  Information
 * found by reading the source to VOBDUMP is the base for the structure
 * and names of these data types.
 *
 * VOBDUMP: a program for examining DVD .VOB files.
 * Copyright 1998, 1999 Eric Smith <eric@brouhaha.com>
 *
 * VOBDUMP is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.  Note that I am not
 * granting permission to redistribute or modify VOBDUMP under the terms
 * of any later version of the General Public License.
 *
 * This program is distributed in the hope that it will be useful (or at
 * least amusing), but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 */

#include <inttypes.h>
#include <dvdread/ifo_types.h> // only dvd_time_t, vm_cmd_t and user_ops_t


#undef ATTRIBUTE_PACKED
#undef PRAGMA_PACK_BEGIN 
#undef PRAGMA_PACK_END

#if defined(__GNUC__)
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#define ATTRIBUTE_PACKED __attribute__ ((packed))
#define PRAGMA_PACK 0
#endif
#endif

#if !defined(ATTRIBUTE_PACKED)
#define ATTRIBUTE_PACKED
#define PRAGMA_PACK 1
#endif


/* The length including the substream id byte. */
#define PCI_BYTES 0x3d4
#define DSI_BYTES 0x3fa

#define PS2_PCI_SUBSTREAM_ID 0x00
#define PS2_DSI_SUBSTREAM_ID 0x01

/* Remove this */
#define DSI_START_BYTE 1031


#if PRAGMA_PACK
#pragma pack(1)
#endif


/**
 * PCI General Information 
 */
typedef struct {
  uint32_t nv_pck_lbn;
  uint16_t vobu_cat;
  uint16_t zero1;
  user_ops_t vobu_uop_ctl;
  uint32_t vobu_s_ptm;
  uint32_t vobu_e_ptm;
  uint32_t vobu_se_e_ptm;
  dvd_time_t e_eltm;
  char vobu_isrc[32];
} ATTRIBUTE_PACKED pci_gi_t;

/**
 * Non Seamless Angle Information
 */
typedef struct {
  uint32_t nsml_agl_dsta[9]; 
} ATTRIBUTE_PACKED nsml_agli_t;

/** 
 * Highlight General Information 
 */
typedef struct {
  uint16_t hli_ss; ///< only low 2 bits
  uint32_t hli_s_ptm;
  uint32_t hli_e_ptm;
  uint32_t btn_se_e_ptm;
#ifdef WORDS_BIGENDIAN
  unsigned int zero1 : 2;
  unsigned int btngr_ns : 2;
  unsigned int zero2 : 1;
  unsigned int btngr1_dsp_ty : 3;
  unsigned int zero3 : 1;
  unsigned int btngr2_dsp_ty : 3;
  unsigned int zero4 : 1;
  unsigned int btngr3_dsp_ty : 3;
#else
  unsigned int btngr1_dsp_ty : 3;
  unsigned int zero2 : 1;
  unsigned int btngr_ns : 2;
  unsigned int zero1 : 2;
  unsigned int btngr3_dsp_ty : 3;
  unsigned int zero4 : 1;
  unsigned int btngr2_dsp_ty : 3;
  unsigned int zero3 : 1;
#endif
  uint8_t btn_ofn;
  uint8_t btn_ns;     ///< only low 6 bits
  uint8_t nsl_btn_ns; ///< only low 6 bits
  uint8_t zero5;
  uint8_t fosl_btnn;  ///< only low 6 bits
  uint8_t foac_btnn;  ///< only low 6 bits
} ATTRIBUTE_PACKED hl_gi_t;


/** 
 * Button Color Information Table 
 */
typedef struct {
  uint32_t btn_coli[3][2];
} ATTRIBUTE_PACKED btn_colit_t;

/** 
 * Button Information
 */
typedef struct {
#ifdef WORDS_BIGENDIAN
  unsigned int btn_coln         : 2;
  unsigned int x_start          : 10;
  unsigned int zero1            : 2;
  unsigned int x_end            : 10;
  unsigned int auto_action_mode : 2;
  unsigned int y_start          : 10;
  unsigned int zero2            : 2;
  unsigned int y_end            : 10;

  unsigned int zero3            : 2;
  unsigned int up               : 6;
  unsigned int zero4            : 2;
  unsigned int down             : 6;
  unsigned int zero5            : 2;
  unsigned int left             : 6;
  unsigned int zero6            : 2;
  unsigned int right            : 6;
#else
  unsigned int x_end            : 10;
  unsigned int zero1            : 2;
  unsigned int x_start          : 10;
  unsigned int btn_coln         : 2;
  unsigned int y_end            : 10;
  unsigned int zero2            : 2;
  unsigned int y_start          : 10;
  unsigned int auto_action_mode : 2;

  unsigned int up               : 6;
  unsigned int zero3            : 2;
  unsigned int down             : 6;
  unsigned int zero4            : 2;
  unsigned int left             : 6;
  unsigned int zero5            : 2;
  unsigned int right            : 6;
  unsigned int zero6            : 2;
#endif
  vm_cmd_t cmd;
} ATTRIBUTE_PACKED btni_t;

/**
 * Highlight Information 
 */
typedef struct {
  hl_gi_t     hl_gi;
  btn_colit_t btn_colit;
  btni_t      btnit[36];
} ATTRIBUTE_PACKED hli_t;

/**
 * PCI packet
 */
typedef struct {
  pci_gi_t    pci_gi;
  nsml_agli_t nsml_agli;
  hli_t       hli;
  uint8_t     zero1[189];
} ATTRIBUTE_PACKED pci_t;




/**
 * DSI General Information 
 */
typedef struct {
  uint32_t nv_pck_scr;
  uint32_t nv_pck_lbn;
  uint32_t vobu_ea;
  uint32_t vobu_1stref_ea;
  uint32_t vobu_2ndref_ea;
  uint32_t vobu_3rdref_ea;
  uint16_t vobu_vob_idn;
  uint8_t  zero1;
  uint8_t  vobu_c_idn;
  dvd_time_t c_eltm;
} ATTRIBUTE_PACKED dsi_gi_t;

/**
 * Seamless Playback Information
 */
typedef struct {
  uint16_t category; ///< category of seamless VOBU
  uint32_t ilvu_ea;  ///< end address of interleaved Unit (sectors)
  uint32_t ilvu_sa;  ///< start address of next interleaved unit (sectors)
  uint16_t size;     ///< size of next interleaved unit (sectors)
  uint32_t vob_v_s_s_ptm; ///< video start ptm in vob
  uint32_t vob_v_e_e_ptm; ///< video end ptm in vob
  struct {
    uint32_t stp_ptm1;
    uint32_t stp_ptm2;
    uint32_t gap_len1;
    uint32_t gap_len2;      
  } vob_a[8];
} ATTRIBUTE_PACKED sml_pbi_t;

/**
 * Seamless Angle Infromation for one angle
 */
typedef struct {
    uint32_t address; ///< Sector offset to next ILVU, high bit is before/after
    uint16_t size;    ///< Byte size of the ILVU poited to by address.
} ATTRIBUTE_PACKED sml_agl_data_t;

/**
 * Seamless Angle Infromation
 */
typedef struct {
  sml_agl_data_t data[9];
} ATTRIBUTE_PACKED sml_agli_t;

/**
 * VOBU Search Information 
 */
typedef struct {
  uint32_t next_video; ///< Next vobu that contains video
  uint32_t fwda[19];   ///< Forwards, time
  uint32_t next_vobu;
  uint32_t prev_vobu;
  uint32_t bwda[19];   ///< Backwards, time
  uint32_t prev_video;
} ATTRIBUTE_PACKED vobu_sri_t;

#define SRI_END_OF_CELL 0x3fffffff

/**
 * Synchronous Information
 */ 
typedef struct {
  uint16_t a_synca[8];   ///< Sector offset to first audio packet for this VOBU
  uint32_t sp_synca[32]; ///< Sector offset to first subpicture packet
} ATTRIBUTE_PACKED synci_t;

/**
 * DSI packet
 */
typedef struct {
  dsi_gi_t   dsi_gi;
  sml_pbi_t  sml_pbi;
  sml_agli_t sml_agli;
  vobu_sri_t vobu_sri;
  synci_t    synci;
  uint8_t    zero1[471];
} ATTRIBUTE_PACKED dsi_t;


#if PRAGMA_PACK
#pragma pack()
#endif

#endif /* NAV_TYPES_H_INCLUDED */
