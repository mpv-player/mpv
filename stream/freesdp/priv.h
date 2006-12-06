/*
  This file is part of FreeSDP
  Copyright (C) 2001,2002,2003 Federico Montesino Pouzols <fedemp@altern.org>

  FreeSDP is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  Benjamin Zores, (C) 2006
    added support in parser for the a=control: lines.
    added support in parser for the a=range: lines.
*/

/**
 * @file priv.h
 *
 * @short Common private header for both formatting and parsing modules.
 **/

#ifndef FSDP_PRIV_H
#define FSDP_PRIV_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define NTP_EPOCH_OFFSET 2208988800UL

#define FSDP_MAX_LENGTH 2000

/* Tags for doxygen documentation */

/**
 * @mainpage FreeSDP Library Reference Manual
 * @section overview Overview (README)
 * @verbinclude ../../README
 *
 **/

/**
 * @example formatdemo.c
 * 
 * A basic SDP descriptions formatter based on FreeSDP.
 **/

/**
 * @example parsedemo.c
 * 
 * A basic SDP descriptions parser based on FreeSDP.
 **/

/* Private routines declarations */

BEGIN_C_DECLS
/**
 * @short bandwidth modifier
 *
 * Holds type of modifier and value. Also holds the literal bandwidth
 * modifier if unknown.
 **/
  typedef struct
{
  fsdp_bw_modifier_type_t b_mod_type;
  unsigned long int b_value;
  char *b_unknown_bw_modt;
} fsdp_bw_modifier_t;

/**
 * @short a=rtpmap: attribute
 *
 * Holds payload type, enconding name, RTP clock rate, and encofing
 * parameters.
 **/
typedef struct
{
  char *pt;
  char *encoding_name;
  unsigned int clock_rate;
  char *parameters;
} fsdp_rtpmap_t;

/**
 * @short Connection address specification
 *
 * Holds address (unicast or multicast) as well as TTL and number of
 * ports, when it is an IP4 multicast address.
 **/
typedef struct fsdp_connection_address_t_s
{
  char *address;
  unsigned int address_ttl;
  unsigned int address_count;
} fsdp_connection_address_t;

/**
 * @short Struct for each media in a session description.
 **/
struct fsdp_media_description_t_s
{
  /* from `m=<media>  <port>  <transport> <fmt list>' line */
  fsdp_media_t media_type;
  unsigned int port;
  unsigned int port_count;
  fsdp_transport_protocol_t transport;
  char **formats;
  unsigned int formats_count;
  /* from i=<media title> */
  char *i_title;
  /* from `c=<network type> <address type> <connection address>' line
     (optional) */
  fsdp_network_type_t c_network_type;
  fsdp_address_type_t c_address_type;
  fsdp_connection_address_t c_address;
  /* from `b=<modifier>:<bandwidth-value>' lines (optional) */
  fsdp_bw_modifier_t *bw_modifiers;
  unsigned int bw_modifiers_count;
  /* from `k=<method>' or `k=<method>:<encryption key>' line
     (optional) */
  fsdp_encryption_method_t k_encryption_method;
  char *k_encryption_content;
  /* from `a=<attribute>' or `a=<attribute>:<value>' lines (opt) */
  unsigned long int a_ptime;
  unsigned long int a_maxptime;
  /* rtpmap */
  fsdp_rtpmap_t **a_rtpmaps;
  unsigned int a_rtpmaps_count;
  fsdp_orient_t a_orient;
  fsdp_sendrecv_mode_t a_sendrecv_mode;

  char **a_sdplangs;
  unsigned int a_sdplangs_count;
  char **a_langs;
  unsigned int a_langs_count;

  char **a_controls;
  unsigned int a_controls_count;

  char *a_range;

  float a_framerate;
  unsigned int a_quality;
  char **a_fmtps;
  unsigned int a_fmtps_count;
  /* rtcp attribute */
  unsigned int a_rtcp_port;
  fsdp_network_type_t a_rtcp_network_type;
  fsdp_address_type_t a_rtcp_address_type;
  char *a_rtcp_address;
  /* media attributes that are not directly supported */
  char **unidentified_attributes;
  unsigned int unidentified_attributes_count;
};

typedef struct fsdp_media_description_t_s fsdp_media_announcement_t;

/**
 * @short Information for a repeat (struct for r= lines)
 **/
typedef struct
{
  /* times in seconds */
  unsigned long int interval;
  unsigned long int duration;
  unsigned long int *offsets;
  unsigned int offsets_count;
} fsdp_repeat_t;

/**
 * @short Information about a time period
 *
 * The start and stop times as well as the information from the r=
 * lines for a t= line are stored in this structures.
 **/
typedef struct
{
  time_t start;
  time_t stop;
  fsdp_repeat_t **repeats;
  unsigned int repeats_count;
} fsdp_time_period_t;

/**
 * @short Struct for session descriptions.
 **/
struct fsdp_description_t_s
{
  /* from v=... line */
  unsigned int version;
  /* from o=... line */
  char *o_username;
  char *o_session_id;
  char *o_announcement_version;
  fsdp_network_type_t o_network_type;
  fsdp_address_type_t o_address_type;
  char *o_address;
  /* from s=... line */
  char *s_name;
  /* from i=... line (opt) */
  char *i_information;
  /* from u=... line (opt) */
  char *u_uri;
  /* from e=... lines (0 or more) */
  const char **emails;
  unsigned int emails_count;
  /* from p=... lines (0 or more) */
  const char **phones;
  unsigned int phones_count;
  /* from `c=<network type> <address type> <connection address>' line */
  fsdp_network_type_t c_network_type;
  fsdp_address_type_t c_address_type;
  fsdp_connection_address_t c_address;
  /* from `b=<modifier>:<bandwidth-value>' lines (optional) */
  fsdp_bw_modifier_t *bw_modifiers;
  unsigned int bw_modifiers_count;
  /* from `t=<start time>  <stop time>' lines (1 or more) */
  /* from `r=<repeat interval> <active duration> <list of offsets from
     start-time>' */
  fsdp_time_period_t **time_periods;
  unsigned int time_periods_count;
  /* from `z=<adjustment time> <offset> <adjustment time> <offset>
     ....' lines */
  char *timezone_adj;
  /* from `k=<method>' or `k=<method>:<encryption key>' line (opt) */
  fsdp_encryption_method_t k_encryption_method;
  char *k_encryption_content;
  /* from `a=<attribute>' or `a=<attribute>:<value>' lines (opt) */
  char *a_category;
  char *a_keywords;
  char *a_tool;
  char *a_range;
  /* rtpmap */
  fsdp_rtpmap_t **a_rtpmaps;
  unsigned int a_rtpmaps_count;
  fsdp_sendrecv_mode_t a_sendrecv_mode;
  fsdp_session_type_t a_type;
  char *a_charset;

  char **a_sdplangs;
  unsigned int a_sdplangs_count;
  char **a_langs;
  unsigned int a_langs_count;

  char **a_controls;
  unsigned int a_controls_count;
  /* from `m=<media> <port>/<number of ports> <transport> <fmt list>'
     lines [one or more] */
  fsdp_media_announcement_t **media_announcements;
  unsigned int media_announcements_count;
  /* session attributes that are not directly supported */
  char **unidentified_attributes;
  unsigned int unidentified_attributes_count;
};

#define MEDIA_RTPMAPS_MAX_COUNT 5
#define SDPLANGS_MAX_COUNT 5
#define SDPCONTROLS_MAX_COUNT 10
#define UNIDENTIFIED_ATTRIBUTES_MAX_COUNT 5

END_C_DECLS
#endif /* FSDP_PRIV_H */
