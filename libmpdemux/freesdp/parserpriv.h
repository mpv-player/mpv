/*
  This file is part of FreeSDP
  Copyright (C) 2001,2002,2003 Federico Montesino Pouzols <fedemp@suidzer0.org>

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
*/

/**
 * @file parserpriv.h
 *
 * @short Private header for parser module.
 **/

#ifndef FSDP_PARSERPRIV_H
#define FSDP_PARSERPRIV_H

#include "priv.h"
#include "parser.h"

/**
 * Parse a connection (c=<network type> <address type> <connection
 * address>) line. If the textual description in <code>p</code> begins
 * with a connection line, it is parsed. If not, nothing is done.
 * 
 * @param p fraction of textual SDP description.
 * @param ntype where to store the network type.
 * @param atype where to store the address type.
 * @param address where to store the connection address as a string.
 * 
 * @return parse error code.
 **/
static fsdp_error_t
fsdp_parse_c (const char **p, fsdp_network_type_t * ntype,
	      fsdp_address_type_t * atype,
	      fsdp_connection_address_t * address);

/**
 * Parse b (b=<modifier>:<bandwidth-value>) consecutive lines. If the
 * textual description in <code>p</code> begins with a bandwidth line,
 * it is parsed as well as all b lines inmediately after it. If not,
 * nothing is done.
 * 
 * @param p fraction of textual SDP description.
 * @param bw_modifiers pointer to empty array of bandwidth modifiers to fill.
 * @param bw_modifiers_count where to set the number of bandwidth
 *        modifiers successfully parsed.
 *
 * @return parse error code.
 **/
static fsdp_error_t
fsdp_parse_b (const char **p, fsdp_bw_modifier_t ** bw_modifiers,
	      unsigned int *bw_modifiers_count);

/**
 * Parse a k (k=<method>) or (k=<method>:<encryption key>) line. If
 * the textual description in <code>p</code> begins with an encryption
 * line, it is parsed. If not, nothing is done.
 *
 * @param p fraction of textual SDP description.
 * @param method where to store the encryption method.
 * @param content where to store the encryption key if provided.
 *
 * @return parse error code.
 **/
static fsdp_error_t
fsdp_parse_k (const char **p, fsdp_encryption_method_t * method,
	      char **content);


/**
 * Parses a string whose first token (first characters before the
 * first space or end of string) is supposed to be a time in SDP
 * syntax. Some examples of SDP times are: 2d, 5h, 3444, 7778s,
 *
 * @param time time in SDP syntax as a string.
 * @param seconds where to store the value in seconds as an integer.
 *
 * @return parse error code.
 **/
static fsdp_error_t
fsdp_repeat_time_to_uint (const char *time, unsigned long int *seconds);

static fsdp_error_t
fsdp_parse_rtpmap (fsdp_rtpmap_t *** rtpmap, unsigned int *counter,
		   const char *value);

/** 
 * Maximun default field len for "expected to be short" fields, like
 * username, session_id or inet addresses.
 *
 * MDFLENS value must be MAXSHORTFIELDLEN - 1 
 **/
#define MAXSHORTFIELDLEN 96
#define MSFLENS "95"

/**
 * Maximun default field len for "maybe very long" fields, like
 * information, attribute values. This can also be used for lines
 * where there is only a string field, like phone and email.
 *
 * MLFLENS value must be MAXLONGFIELDLEN - 1
 **/
#define MAXLONGFIELDLEN 1024
#define MLFLENS "1023"

#endif /* FSDP_PARSERPRIV_H */
