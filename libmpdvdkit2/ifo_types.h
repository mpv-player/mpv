#ifndef IFO_TYPES_H_INCLUDED
#define IFO_TYPES_H_INCLUDED

/*
 * Copyright (C) 2000, 2001 Björn Englund <d4bjorn@dtek.chalmers.se>,
 *                          Håkan Hjort <d95hjort@dtek.chalmers.se>
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

#include <inttypes.h>
#include <dvdread/dvd_reader.h>


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

#if PRAGMA_PACK
#pragma pack(1)
#endif


/**
 * Common
 *
 * The following structures are used in both the VMGI and VTSI.
 */


/**
 * DVD Time Information.
 */
typedef struct {
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t frame_u; // The two high bits are the frame rate.
} ATTRIBUTE_PACKED dvd_time_t;

/**
 * Type to store per-command data.
 */
typedef struct {
  uint8_t bytes[8];
} ATTRIBUTE_PACKED vm_cmd_t;
#define COMMAND_DATA_SIZE 8


/**
 * Video Attributes.
 */
typedef struct {
#ifdef WORDS_BIGENDIAN
  unsigned int mpeg_version         : 2;
  unsigned int video_format         : 2;
  unsigned int display_aspect_ratio : 2;
  unsigned int permitted_df         : 2;
  
  unsigned int line21_cc_1          : 1;
  unsigned int line21_cc_2          : 1;
  unsigned int unknown1             : 2;
  
  unsigned int picture_size         : 2;
  unsigned int letterboxed          : 1;
  unsigned int film_mode            : 1;
#else
  unsigned int permitted_df         : 2;
  unsigned int display_aspect_ratio : 2;
  unsigned int video_format         : 2;
  unsigned int mpeg_version         : 2;
  
  unsigned int film_mode            : 1;
  unsigned int letterboxed          : 1;
  unsigned int picture_size         : 2;
  
  unsigned int unknown1             : 2;
  unsigned int line21_cc_2          : 1;
  unsigned int line21_cc_1          : 1;
#endif
} ATTRIBUTE_PACKED video_attr_t;

/**
 * Audio Attributes. (Incomplete/Wrong?)
 */
typedef struct {
#ifdef WORDS_BIGENDIAN
  unsigned int audio_format           : 3;
  unsigned int multichannel_extension : 1;
  unsigned int lang_type              : 2;
  unsigned int application_mode       : 2;
  
  unsigned int quantization           : 2;
  unsigned int sample_frequency       : 2;
  unsigned int unknown1               : 1;
  unsigned int channels               : 3;
#else
  unsigned int application_mode       : 2;
  unsigned int lang_type              : 2;
  unsigned int multichannel_extension : 1;
  unsigned int audio_format           : 3;
  
  unsigned int channels               : 3;
  unsigned int unknown1               : 1;
  unsigned int sample_frequency       : 2;
  unsigned int quantization           : 2;
#endif
  uint16_t lang_code;
  uint8_t  lang_code2; // ??
  uint8_t  lang_extension;
  uint16_t unknown2;
} ATTRIBUTE_PACKED audio_attr_t;

/**
 * Subpicture Attributes.(Incomplete/Wrong)
 */
typedef struct {
  /*
   * type: 0 not specified
   *       1 language
   *       2 other
   * coding mode: 0 run length
   *              1 extended
   *              2 other
   * language: indicates language if type == 1
   * lang extension: if type == 1 contains the lang extension
   */
  uint8_t type;
  uint8_t zero1;
  uint16_t lang_code;
  uint8_t lang_extension;
  uint8_t zero2;
} ATTRIBUTE_PACKED subp_attr_t;



/**
 * PGC Command Table.
 */ 
typedef struct {
  uint16_t nr_of_pre;
  uint16_t nr_of_post;
  uint16_t nr_of_cell;
  uint16_t zero_1;
  vm_cmd_t *pre_cmds;
  vm_cmd_t *post_cmds;
  vm_cmd_t *cell_cmds;
} ATTRIBUTE_PACKED pgc_command_tbl_t;
#define PGC_COMMAND_TBL_SIZE 8

/**
 * PGC Program Map
 */
typedef uint8_t pgc_program_map_t; 

/**
 * Cell Playback Information.
 */
typedef struct {
#ifdef WORDS_BIGENDIAN
  unsigned int block_mode       : 2;
  unsigned int block_type       : 2;
  unsigned int seamless_play    : 1;
  unsigned int interleaved      : 1;
  unsigned int stc_discontinuity: 1;
  unsigned int seamless_angle   : 1;
  
  unsigned int unknown1         : 1;
  unsigned int restricted       : 1;
  unsigned int unknown2         : 6;
#else
  unsigned int seamless_angle   : 1;
  unsigned int stc_discontinuity: 1;
  unsigned int interleaved      : 1;
  unsigned int seamless_play    : 1;
  unsigned int block_type       : 2;
  unsigned int block_mode       : 2;
  
  unsigned int unknown2         : 6;
  unsigned int restricted       : 1;
  unsigned int unknown1         : 1;
#endif
  uint8_t still_time;
  uint8_t cell_cmd_nr;
  dvd_time_t playback_time;
  uint32_t first_sector;
  uint32_t first_ilvu_end_sector;
  uint32_t last_vobu_start_sector;
  uint32_t last_sector;
} ATTRIBUTE_PACKED cell_playback_t;

#define BLOCK_TYPE_NONE         0x0
#define BLOCK_TYPE_ANGLE_BLOCK  0x1

#define BLOCK_MODE_NOT_IN_BLOCK 0x0
#define BLOCK_MODE_FIRST_CELL   0x1
#define BLOCK_MODE_IN_BLOCK     0x2
#define BLOCK_MODE_LAST_CELL    0x3

/**
 * Cell Position Information.
 */
typedef struct {
  uint16_t vob_id_nr;
  uint8_t  zero_1;
  uint8_t  cell_nr;
} ATTRIBUTE_PACKED cell_position_t;

/**
 * User Operations.
 */
typedef struct {
#ifdef WORDS_BIGENDIAN
  unsigned int zero                           : 7; // 25-31
  unsigned int video_pres_mode_change         : 1; // 24
  
  unsigned int karaoke_audio_pres_mode_change : 1; // 23
  unsigned int angle_change                   : 1; // 22
  unsigned int subpic_stream_change           : 1; // 21
  unsigned int audio_stream_change            : 1; // 20
  unsigned int pause_on                       : 1; // 19
  unsigned int still_off                      : 1; // 18
  unsigned int button_select_or_activate      : 1; // 17
  unsigned int resume                         : 1; // 16
  
  unsigned int chapter_menu_call              : 1; // 15
  unsigned int angle_menu_call                : 1; // 14
  unsigned int audio_menu_call                : 1; // 13
  unsigned int subpic_menu_call               : 1; // 12
  unsigned int root_menu_call                 : 1; // 11
  unsigned int title_menu_call                : 1; // 10
  unsigned int backward_scan                  : 1; // 9
  unsigned int forward_scan                   : 1; // 8
  
  unsigned int next_pg_search                 : 1; // 7
  unsigned int prev_or_top_pg_search          : 1; // 6
  unsigned int time_or_chapter_search         : 1; // 5
  unsigned int go_up                          : 1; // 4
  unsigned int stop                           : 1; // 3
  unsigned int title_play                     : 1; // 2
  unsigned int chapter_search_or_play         : 1; // 1
  unsigned int title_or_time_play             : 1; // 0
#else
  unsigned int video_pres_mode_change         : 1; // 24
  unsigned int zero                           : 7; // 25-31
  
  unsigned int resume                         : 1; // 16
  unsigned int button_select_or_activate      : 1; // 17
  unsigned int still_off                      : 1; // 18
  unsigned int pause_on                       : 1; // 19
  unsigned int audio_stream_change            : 1; // 20
  unsigned int subpic_stream_change           : 1; // 21
  unsigned int angle_change                   : 1; // 22
  unsigned int karaoke_audio_pres_mode_change : 1; // 23
  
  unsigned int forward_scan                   : 1; // 8
  unsigned int backward_scan                  : 1; // 9
  unsigned int title_menu_call                : 1; // 10
  unsigned int root_menu_call                 : 1; // 11
  unsigned int subpic_menu_call               : 1; // 12
  unsigned int audio_menu_call                : 1; // 13
  unsigned int angle_menu_call                : 1; // 14
  unsigned int chapter_menu_call              : 1; // 15
  
  unsigned int title_or_time_play             : 1; // 0
  unsigned int chapter_search_or_play         : 1; // 1
  unsigned int title_play                     : 1; // 2
  unsigned int stop                           : 1; // 3
  unsigned int go_up                          : 1; // 4
  unsigned int time_or_chapter_search         : 1; // 5
  unsigned int prev_or_top_pg_search          : 1; // 6
  unsigned int next_pg_search                 : 1; // 7
#endif
} ATTRIBUTE_PACKED user_ops_t;

/**
 * Program Chain Information.
 */
typedef struct {
  uint16_t zero_1;
  uint8_t  nr_of_programs;
  uint8_t  nr_of_cells;
  dvd_time_t playback_time;
  user_ops_t prohibited_ops;
  uint16_t audio_control[8]; /* New type? */
  uint32_t subp_control[32]; /* New type? */
  uint16_t next_pgc_nr;
  uint16_t prev_pgc_nr;
  uint16_t goup_pgc_nr;
  uint8_t  still_time;
  uint8_t  pg_playback_mode;
  uint32_t palette[16]; /* New type struct {zero_1, Y, Cr, Cb} ? */
  uint16_t command_tbl_offset;
  uint16_t program_map_offset;
  uint16_t cell_playback_offset;
  uint16_t cell_position_offset;
  pgc_command_tbl_t *command_tbl;
  pgc_program_map_t  *program_map;
  cell_playback_t *cell_playback;
  cell_position_t *cell_position;
} ATTRIBUTE_PACKED pgc_t;
#define PGC_SIZE 236

/**
 * Program Chain Information Search Pointer.
 */
typedef struct {
  uint8_t  entry_id;
#ifdef WORDS_BIGENDIAN
  unsigned int block_mode : 2;
  unsigned int block_type : 2;
  unsigned int unknown1   : 4;
#else
  unsigned int unknown1   : 4;
  unsigned int block_type : 2;
  unsigned int block_mode : 2;
#endif  
  uint16_t ptl_id_mask;
  uint32_t pgc_start_byte;
  pgc_t *pgc;
} ATTRIBUTE_PACKED pgci_srp_t;
#define PGCI_SRP_SIZE 8

/**
 * Program Chain Information Table.
 */
typedef struct {
  uint16_t nr_of_pgci_srp;
  uint16_t zero_1;
  uint32_t last_byte;
  pgci_srp_t *pgci_srp;
} ATTRIBUTE_PACKED pgcit_t;
#define PGCIT_SIZE 8

/**
 * Menu PGCI Language Unit.
 */
typedef struct {
  uint16_t lang_code;
  uint8_t  zero_1;
  uint8_t  exists;
  uint32_t lang_start_byte;
  pgcit_t *pgcit;
} ATTRIBUTE_PACKED pgci_lu_t;
#define PGCI_LU_SIZE 8

/**
 * Menu PGCI Unit Table.
 */
typedef struct {
  uint16_t nr_of_lus;
  uint16_t zero_1;
  uint32_t last_byte;
  pgci_lu_t *lu;
} ATTRIBUTE_PACKED pgci_ut_t;
#define PGCI_UT_SIZE 8

/**
 * Cell Address Information.
 */
typedef struct {
  uint16_t vob_id;
  uint8_t  cell_id;
  uint8_t  zero_1;
  uint32_t start_sector;
  uint32_t last_sector;
} ATTRIBUTE_PACKED cell_adr_t;

/**
 * Cell Address Table.
 */
typedef struct {
  uint16_t nr_of_vobs; /* VOBs */
  uint16_t zero_1;
  uint32_t last_byte;
  cell_adr_t *cell_adr_table;
} ATTRIBUTE_PACKED c_adt_t;
#define C_ADT_SIZE 8

/**
 * VOBU Address Map.
 */
typedef struct {
  uint32_t last_byte;
  uint32_t *vobu_start_sectors;
} ATTRIBUTE_PACKED vobu_admap_t;
#define VOBU_ADMAP_SIZE 4




/**
 * VMGI
 *
 * The following structures relate to the Video Manager.
 */

/**
 * Video Manager Information Management Table.
 */
typedef struct {
  char     vmg_identifier[12];
  uint32_t vmg_last_sector;
  uint8_t  zero_1[12];
  uint32_t vmgi_last_sector;
  uint8_t  zero_2;
  uint8_t  specification_version;
  uint32_t vmg_category;
  uint16_t vmg_nr_of_volumes;
  uint16_t vmg_this_volume_nr;
  uint8_t  disc_side;
  uint8_t  zero_3[19];
  uint16_t vmg_nr_of_title_sets;  /* Number of VTSs. */
  char     provider_identifier[32];
  uint64_t vmg_pos_code;
  uint8_t  zero_4[24];
  uint32_t vmgi_last_byte;
  uint32_t first_play_pgc;
  uint8_t  zero_5[56];
  uint32_t vmgm_vobs;             /* sector */
  uint32_t tt_srpt;               /* sector */
  uint32_t vmgm_pgci_ut;          /* sector */
  uint32_t ptl_mait;              /* sector */
  uint32_t vts_atrt;              /* sector */
  uint32_t txtdt_mgi;             /* sector */
  uint32_t vmgm_c_adt;            /* sector */
  uint32_t vmgm_vobu_admap;       /* sector */
  uint8_t  zero_6[32];
  
  video_attr_t vmgm_video_attr;
  uint8_t  zero_7;
  uint8_t  nr_of_vmgm_audio_streams; // should be 0 or 1
  audio_attr_t vmgm_audio_attr;
  audio_attr_t zero_8[7];
  uint8_t  zero_9[17];
  uint8_t  nr_of_vmgm_subp_streams; // should be 0 or 1
  subp_attr_t  vmgm_subp_attr;
  subp_attr_t  zero_10[27];  /* XXX: how much 'padding' here? */
} ATTRIBUTE_PACKED vmgi_mat_t;

typedef struct {
#ifdef WORDS_BIGENDIAN
  unsigned int zero_1                    : 1;
  unsigned int multi_or_random_pgc_title : 1; // 0 == one sequential pgc title
  unsigned int jlc_exists_in_cell_cmd    : 1;
  unsigned int jlc_exists_in_prepost_cmd : 1;
  unsigned int jlc_exists_in_button_cmd  : 1;
  unsigned int jlc_exists_in_tt_dom      : 1;
  unsigned int chapter_search_or_play    : 1; // UOP 1
  unsigned int title_or_time_play        : 1; // UOP 0
#else
  unsigned int title_or_time_play        : 1; // UOP 0
  unsigned int chapter_search_or_play    : 1; // UOP 1
  unsigned int jlc_exists_in_tt_dom      : 1;
  unsigned int jlc_exists_in_button_cmd  : 1;
  unsigned int jlc_exists_in_prepost_cmd : 1;
  unsigned int jlc_exists_in_cell_cmd    : 1;
  unsigned int multi_or_random_pgc_title : 1; // 0 == one sequential pgc title
  unsigned int zero_1                    : 1;
#endif
} ATTRIBUTE_PACKED playback_type_t;

/**
 * Title Information.
 */
typedef struct {
  playback_type_t pb_ty;
  uint8_t  nr_of_angles;
  uint16_t nr_of_ptts;
  uint16_t parental_id;
  uint8_t  title_set_nr;
  uint8_t  vts_ttn;
  uint32_t title_set_sector;
} ATTRIBUTE_PACKED title_info_t;

/**
 * PartOfTitle Search Pointer Table.
 */
typedef struct {
  uint16_t nr_of_srpts;
  uint16_t zero_1;
  uint32_t last_byte;
  title_info_t *title;
} ATTRIBUTE_PACKED tt_srpt_t;
#define TT_SRPT_SIZE 8

/**
 * Parental Management Information Unit Table.
 */
typedef struct {
  uint16_t country_code;
  uint16_t zero_1;
  uint16_t pf_ptl_mai_start_byte;
  uint16_t zero_2;
  /* uint16_t *pf_ptl_mai // table of nr_of_vtss+1 x 8 */
} ATTRIBUTE_PACKED ptl_mait_country_t;
#define PTL_MAIT_COUNTRY_SIZE 8

/**
 * Parental Management Information Table.
 */
typedef struct {
  uint16_t nr_of_countries;
  uint16_t nr_of_vtss;
  uint32_t last_byte;
  ptl_mait_country_t *countries;
} ATTRIBUTE_PACKED ptl_mait_t;
#define PTL_MAIT_SIZE 8

/**
 * Video Title Set Attributes.
 */
typedef struct {
  uint32_t last_byte;
  uint32_t vts_cat;
  
  video_attr_t vtsm_vobs_attr;
  uint8_t  zero_1;
  uint8_t  nr_of_vtsm_audio_streams; // should be 0 or 1
  audio_attr_t vtsm_audio_attr;
  audio_attr_t zero_2[7];  
  uint8_t  zero_3[16];
  uint8_t  zero_4;
  uint8_t  nr_of_vtsm_subp_streams; // should be 0 or 1
  subp_attr_t vtsm_subp_attr;
  subp_attr_t zero_5[27];
  
  uint8_t  zero_6[2];
  
  video_attr_t vtstt_vobs_video_attr;
  uint8_t  zero_7;
  uint8_t  nr_of_vtstt_audio_streams;
  audio_attr_t vtstt_audio_attr[8];
  uint8_t  zero_8[16];
  uint8_t  zero_9;
  uint8_t  nr_of_vtstt_subp_streams;
  subp_attr_t vtstt_subp_attr[32];
} ATTRIBUTE_PACKED vts_attributes_t;
#define VTS_ATTRIBUTES_SIZE 542
#define VTS_ATTRIBUTES_MIN_SIZE 356

/**
 * Video Title Set Attribute Table.
 */
typedef struct {
  uint16_t nr_of_vtss;
  uint16_t zero_1;
  uint32_t last_byte;
  vts_attributes_t *vts;
} ATTRIBUTE_PACKED vts_atrt_t;
#define VTS_ATRT_SIZE 8

/**
 * Text Data. (Incomplete)
 */
typedef struct {
  uint32_t last_byte;    /* offsets are relative here */
  uint16_t offsets[100]; /* == nr_of_srpts + 1 (first is disc title) */
#if 0  
  uint16_t unknown; // 0x48 ?? 0x48 words (16bit) info following
  uint16_t zero_1;
  
  uint8_t type_of_info;//?? 01 == disc, 02 == Title, 04 == Title part 
  uint8_t unknown1;
  uint8_t unknown2;
  uint8_t unknown3;
  uint8_t unknown4;//?? allways 0x30 language?, text format?
  uint8_t unknown5;
  uint16_t offset; // from first 
  
  char text[12]; // ended by 0x09
#endif
} ATTRIBUTE_PACKED txtdt_t;

/**
 * Text Data Language Unit. (Incomplete)
 */ 
typedef struct {
  uint16_t lang_code;
  uint16_t unknown;      /* 0x0001, title 1? disc 1? side 1? */
  uint32_t txtdt_start_byte;  /* prt, rel start of vmg_txtdt_mgi  */
  txtdt_t  *txtdt;
} ATTRIBUTE_PACKED txtdt_lu_t;
#define TXTDT_LU_SIZE 8

/**
 * Text Data Manager Information. (Incomplete)
 */
typedef struct {
  char disc_name[14];            /* how many bytes?? */
  uint16_t nr_of_language_units; /* 32bit??          */
  uint32_t last_byte;
  txtdt_lu_t *lu;
} ATTRIBUTE_PACKED txtdt_mgi_t;
#define TXTDT_MGI_SIZE 20


/**
 * VTS
 *
 * Structures relating to the Video Title Set (VTS).
 */

/**
 * Video Title Set Information Management Table.
 */
typedef struct {
  char vts_identifier[12];
  uint32_t vts_last_sector;
  uint8_t  zero_1[12];
  uint32_t vtsi_last_sector;
  uint8_t  zero_2;
  uint8_t  specification_version;
  uint32_t vts_category;
  uint16_t zero_3;
  uint16_t zero_4;
  uint8_t  zero_5;
  uint8_t  zero_6[19];
  uint16_t zero_7;
  uint8_t  zero_8[32];
  uint64_t zero_9;
  uint8_t  zero_10[24];
  uint32_t vtsi_last_byte;
  uint32_t zero_11;
  uint8_t  zero_12[56];
  uint32_t vtsm_vobs;       /* sector */
  uint32_t vtstt_vobs;      /* sector */
  uint32_t vts_ptt_srpt;    /* sector */
  uint32_t vts_pgcit;       /* sector */
  uint32_t vtsm_pgci_ut;    /* sector */
  uint32_t vts_tmapt;       /* sector */  // XXX: FIXME TODO Implement
  uint32_t vtsm_c_adt;      /* sector */
  uint32_t vtsm_vobu_admap; /* sector */
  uint32_t vts_c_adt;       /* sector */
  uint32_t vts_vobu_admap;  /* sector */
  uint8_t  zero_13[24];
  
  video_attr_t vtsm_video_attr;
  uint8_t  zero_14;
  uint8_t  nr_of_vtsm_audio_streams; // should be 0 or 1
  audio_attr_t vtsm_audio_attr;
  audio_attr_t zero_15[7];
  uint8_t  zero_16[17];
  uint8_t  nr_of_vtsm_subp_streams; // should be 0 or 1
  subp_attr_t vtsm_subp_attr;
  subp_attr_t zero_17[27];
  uint8_t  zero_18[2];
  
  video_attr_t vts_video_attr;
  uint8_t  zero_19;
  uint8_t  nr_of_vts_audio_streams;
  audio_attr_t vts_audio_attr[8];
  uint8_t  zero_20[17];
  uint8_t  nr_of_vts_subp_streams;
  subp_attr_t vts_subp_attr[32];
  /* XXX: how much 'padding' here, if any? */
} ATTRIBUTE_PACKED vtsi_mat_t;

/**
 * PartOfTitle Unit Information.
 */
typedef struct {
  uint16_t pgcn;
  uint16_t pgn;
} ATTRIBUTE_PACKED ptt_info_t;

/**
 * PartOfTitle Information.
 */
typedef struct {
  uint16_t nr_of_ptts;
  ptt_info_t *ptt;
} ATTRIBUTE_PACKED ttu_t;

/**
 * PartOfTitle Search Pointer Table.
 */
typedef struct {
  uint16_t nr_of_srpts;
  uint16_t zero_1;
  uint32_t last_byte;
  ttu_t  *title;
} ATTRIBUTE_PACKED vts_ptt_srpt_t;
#define VTS_PTT_SRPT_SIZE 8


#if PRAGMA_PACK
#pragma pack()
#endif


/**
 * The following structure defines an IFO file.  The structure is divided into
 * two parts, the VMGI, or Video Manager Information, which is read from the
 * VIDEO_TS.[IFO,BUP] file, and the VTSI, or Video Title Set Information, which
 * is read in from the VTS_XX_0.[IFO,BUP] files.
 */
typedef struct {
  dvd_file_t *file;
  
  /* VMGI */
  vmgi_mat_t     *vmgi_mat;
  tt_srpt_t      *tt_srpt;
  pgc_t          *first_play_pgc;    
  ptl_mait_t     *ptl_mait;
  vts_atrt_t     *vts_atrt;
  txtdt_mgi_t    *txtdt_mgi;
  
  /* Common */
  pgci_ut_t      *pgci_ut;
  c_adt_t        *menu_c_adt;
  vobu_admap_t   *menu_vobu_admap;
  
  /* VTSI */
  vtsi_mat_t     *vtsi_mat;
  vts_ptt_srpt_t *vts_ptt_srpt;
  pgcit_t        *vts_pgcit;
  int            *vts_tmapt; // FIXME add/correct the type
  c_adt_t        *vts_c_adt;
  vobu_admap_t   *vts_vobu_admap;
} ifo_handle_t;

#endif /* IFO_TYPES_H_INCLUDED */
