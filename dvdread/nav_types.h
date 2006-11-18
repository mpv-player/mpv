#ifndef NAV_TYPES_H_INCLUDED
#define NAV_TYPES_H_INCLUDED

/*
 * Copyright (C) 2000, 2001, 2002 Håkan Hjort <d95hjort@dtek.chalmers.se>
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
#include <dvdread/ifo_types.h> /* only dvd_time_t, vm_cmd_t and user_ops_t */


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
  uint32_t nv_pck_lbn;      /**< sector address of this nav pack */
  uint16_t vobu_cat;        /**< 'category' of vobu */
  uint16_t zero1;           /**< reserved */
  user_ops_t vobu_uop_ctl;  /**< UOP of vobu */
  uint32_t vobu_s_ptm;      /**< start presentation time of vobu */
  uint32_t vobu_e_ptm;      /**< end presentation time of vobu */
  uint32_t vobu_se_e_ptm;   /**< end ptm of sequence end in vobu */
  dvd_time_t e_eltm;        /**< Cell elapsed time */
  char vobu_isrc[32];
} ATTRIBUTE_PACKED pci_gi_t;

/**
 * Non Seamless Angle Information
 */
typedef struct {
  uint32_t nsml_agl_dsta[9];  /**< address of destination vobu in AGL_C#n */
} ATTRIBUTE_PACKED nsml_agli_t;

/** 
 * Highlight General Information 
 *
 * For btngrX_dsp_ty the bits have the following meaning:
 * 000b: normal 4/3 only buttons
 * XX1b: wide (16/9) buttons
 * X1Xb: letterbox buttons
 * 1XXb: pan&scan buttons
 */
typedef struct {
  uint16_t hli_ss; /**< status, only low 2 bits 0: no buttons, 1: different 2: equal 3: eual except for button cmds */
  uint32_t hli_s_ptm;              /**< start ptm of hli */
  uint32_t hli_e_ptm;              /**< end ptm of hli */
  uint32_t btn_se_e_ptm;           /**< end ptm of button select */
#ifdef WORDS_BIGENDIAN
  unsigned int zero1 : 2;          /**< reserved */
  unsigned int btngr_ns : 2;       /**< number of button groups 1, 2 or 3 with 36/18/12 buttons */
  unsigned int zero2 : 1;          /**< reserved */
  unsigned int btngr1_dsp_ty : 3;  /**< display type of subpic stream for button group 1 */
  unsigned int zero3 : 1;          /**< reserved */
  unsigned int btngr2_dsp_ty : 3;  /**< display type of subpic stream for button group 2 */
  unsigned int zero4 : 1;          /**< reserved */
  unsigned int btngr3_dsp_ty : 3;  /**< display type of subpic stream for button group 3 */
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
  uint8_t btn_ofn;     /**< button offset number range 0-255 */
  uint8_t btn_ns;      /**< number of valid buttons  <= 36/18/12 (low 6 bits) */  
  uint8_t nsl_btn_ns;  /**< number of buttons selectable by U_BTNNi (low 6 bits)   nsl_btn_ns <= btn_ns */
  uint8_t zero5;       /**< reserved */
  uint8_t fosl_btnn;   /**< forcedly selected button  (low 6 bits) */
  uint8_t foac_btnn;   /**< forcedly activated button (low 6 bits) */
} ATTRIBUTE_PACKED hl_gi_t;


/** 
 * Button Color Information Table 
 * Each entry beeing a 32bit word that contains the color indexs and alpha
 * values to use.  They are all represented by 4 bit number and stored
 * like this [Ci3, Ci2, Ci1, Ci0, A3, A2, A1, A0].   The actual palette
 * that the indexes reference is in the PGC.
 * @TODO split the uint32_t into a struct
 */
typedef struct {
  uint32_t btn_coli[3][2];  /**< [button color number-1][select:0/action:1] */
} ATTRIBUTE_PACKED btn_colit_t;

/** 
 * Button Information
 *
 * NOTE: I've had to change the structure from the disk layout to get
 * the packing to work with Sun's Forte C compiler.
 * The 4 and 7 bytes are 'rotated' was: ABC DEF GHIJ  is: ABCG DEFH IJ
 */
typedef struct {
#ifdef WORDS_BIGENDIAN
  unsigned int btn_coln         : 2;  /**< button color number */
  unsigned int x_start          : 10; /**< x start offset within the overlay */
  unsigned int zero1            : 2;  /**< reserved */
  unsigned int x_end            : 10; /**< x end offset within the overlay */

  unsigned int zero3            : 2;  /**< reserved */
  unsigned int up               : 6;  /**< button index when pressing up */

  unsigned int auto_action_mode : 2;  /**< 0: no, 1: activated if selected */
  unsigned int y_start          : 10; /**< y start offset within the overlay */
  unsigned int zero2            : 2;  /**< reserved */
  unsigned int y_end            : 10; /**< y end offset within the overlay */

  unsigned int zero4            : 2;  /**< reserved */
  unsigned int down             : 6;  /**< button index when pressing down */
  unsigned int zero5            : 2;  /**< reserved */
  unsigned int left             : 6;  /**< button index when pressing left */
  unsigned int zero6            : 2;  /**< reserved */
  unsigned int right            : 6;  /**< button index when pressing right */
#else
  unsigned int x_end            : 10;
  unsigned int zero1            : 2;
  unsigned int x_start          : 10;
  unsigned int btn_coln         : 2;

  unsigned int up               : 6;
  unsigned int zero3            : 2;

  unsigned int y_end            : 10;
  unsigned int zero2            : 2;
  unsigned int y_start          : 10;
  unsigned int auto_action_mode : 2;

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
  uint32_t nv_pck_lbn;      /**< sector address of this nav pack */
  uint32_t vobu_ea;         /**< end address of this VOBU */
  uint32_t vobu_1stref_ea;  /**< end address of the 1st reference image */
  uint32_t vobu_2ndref_ea;  /**< end address of the 2nd reference image */
  uint32_t vobu_3rdref_ea;  /**< end address of the 3rd reference image */
  uint16_t vobu_vob_idn;    /**< VOB Id number that this VOBU is part of */
  uint8_t  zero1;           /**< reserved */
  uint8_t  vobu_c_idn;      /**< Cell Id number that this VOBU is part of */
  dvd_time_t c_eltm;        /**< Cell elapsed time */
} ATTRIBUTE_PACKED dsi_gi_t;

/**
 * Seamless Playback Information
 */
typedef struct {
  uint16_t category;       /**< 'category' of seamless VOBU */
  uint32_t ilvu_ea;        /**< end address of interleaved Unit */
  uint32_t ilvu_sa;        /**< start address of next interleaved unit */
  uint16_t size;           /**< size of next interleaved unit */
  uint32_t vob_v_s_s_ptm;  /**< video start ptm in vob */
  uint32_t vob_v_e_e_ptm;  /**< video end ptm in vob */
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
    uint32_t address; /**< offset to next ILVU, high bit is before/after */
    uint16_t size;    /**< byte size of the ILVU pointed to by address */
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
  uint32_t next_video; /**< Next vobu that contains video */
  uint32_t fwda[19];   /**< Forwards, time */
  uint32_t next_vobu;
  uint32_t prev_vobu;
  uint32_t bwda[19];   /**< Backwards, time */
  uint32_t prev_video;
} ATTRIBUTE_PACKED vobu_sri_t;

#define SRI_END_OF_CELL 0x3fffffff

/**
 * Synchronous Information
 */ 
typedef struct {
  uint16_t a_synca[8];   /**< offset to first audio packet for this VOBU */
  uint32_t sp_synca[32]; /**< offset to first subpicture packet */
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
