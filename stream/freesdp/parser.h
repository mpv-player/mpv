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
 * @file parser.h
 * @ingroup parser
 * @short Specific public header for parsing module.
 **/

#ifndef FSDP_PARSER_H
#define FSDP_PARSER_H

#include "common.h"

BEGIN_C_DECLS
/**
 * @defgroup parser FreeSDP Parsing Module
 *
 * SDP descriptions parsing routines.
 * @{
 **/
/**
 * Parse a SDP description in <code>description</code>, extracting the
 * session properties into <code>dsc</code>. These properties can be
 * obtained individually later using the <code>fsdp_get_xxxx<code>
 * functions.
 *
 * @param description a multimedia session description formatted in
 * SDP.  
 * @param dsc pointer that is updated to point to a fsdp_description_t
 * object. This fsdp_description_t object should have been previously
 * allocated using <code>fsdp_description_new()</code>; to free it,
 * <code>fsdp_description_delete()</code> should be used.
 *
 * @return FSDPE_OK when parsing completes successfully. Otherwise,
 * another error code is returned.
 **/
fsdp_error_t fsdp_parse (const char *description, fsdp_description_t * dsc);

/**
 * Get the SDP protocol version of the description.
 *
 * @return SDP protocol version number. 
 **/
unsigned int fsdp_get_version (const fsdp_description_t * dsc);

/**
 * Get the username provided by the originator of the session. 
 *
 * @param dsc SDP description object.
 * @return username of the session owner
 **/
const char *fsdp_get_owner_username (const fsdp_description_t * dsc);

/**
 * Get the id for the session described in <code>dsc</code>. 
 *
 * @param dsc SDP description object.
 * @return id string for this session.
 **/
const char *fsdp_get_session_id (const fsdp_description_t * dsc);

/**
 * Get the announcement version for the session description in
 * <code>dsc</code>.
 *
 * @param dsc SDP description object.
 * @return announcement version string for this description.
 **/
const char *fsdp_get_announcement_version (const fsdp_description_t * dsc);

/**
 * Get the the type of network the owner of the session described in
 * <code>dsc</code> is based on.
 *
 * @param dsc SDP description object.
 * @return network type for the owner of this session.
 **/
fsdp_network_type_t
fsdp_get_owner_network_type (const fsdp_description_t * dsc);

/**
 * Get the the type of address the owner of the session described in
 * <code>dsc</code> is based on.
 *
 * @param dsc SDP description object.
 * @return network address type for the owner of this session.
 **/
fsdp_address_type_t
fsdp_get_owner_address_type (const fsdp_description_t * dsc);

/**
 * Get the network address of the owner of the session described in
 * <code>dsc</code>.
 *
 * @param dsc SDP description object.
 * @return network address for the owner this session.
 **/
const char *fsdp_get_owner_address (const fsdp_description_t * dsc);

/**
 * Get the name of the session described in <code>dsc</code>.
 *
 * @param dsc SDP description object.
 * @return name of this session.
 **/
const char *fsdp_get_name (const fsdp_description_t * dsc);

/**
 * Get the information about the session provided in the description
 * <code>dsc</code>.
 *
 * @param dsc SDP description object.
 * @return information of this session.
 **/
const char *fsdp_get_information (const fsdp_description_t * dsc);

/**
 * Get an URI about the session provided in the description
 * <code>dsc</code>.
 *
 * @param dsc SDP description object.
 * @return string containing an URI about the session. NULL if the
 * session uri is missing.
 **/
const char *fsdp_get_uri (const fsdp_description_t * dsc);

/**
 * Get the number of emails specified for the session in the description
 * <code>dsc</code>.
 *
 * @param dsc SDP description object.
 * @return number of emails.
 **/
unsigned int fsdp_get_emails_count (const fsdp_description_t * dsc);

/**
 * Get the n-th email specified for the session in the description
 * <code>dsc</code>.
 *
 * @param dsc SDP description object.
 * @param index number of URI. Note that this index follows the
 * traditional C convention: from 0 to fsdp_get_emails_count() - 1.
 * @return string containing an email about the session. NULL if there
 * is no such index.
 **/
const char *fsdp_get_email (const fsdp_description_t * dsc,
			    unsigned int index);

/**
 * Get the number of phones specified for the session in the description
 * <code>dsc</code>.
 *
 * @param dsc SDP description object.
 * @return number of emails.
 **/
unsigned int fsdp_get_phones_count (const fsdp_description_t * dsc);

/**
 * Get the n-th phone specified for the session in the description
 * <code>dsc</code>.
 *
 * @param dsc SDP description object.
 * @param index number of URI. Note that this index follows the
 * traditional C convention: from 0 to fsdp_get_phones_count() - 1.
 * @return string containing a phone about the session. NULL if there
 * is no such index.
 **/
const char *fsdp_get_phone (const fsdp_description_t * dsc,
			    unsigned int index);

/**
 * Get the the global type of network of the multimedia session
 * connection.
 *
 * @param dsc SDP description object.
 * @return global network type for this
 * connection. FSDP_NETWORK_TYPE_UNDEFINED if no global network
 * address type is included in the description.
 **/
fsdp_network_type_t
fsdp_get_global_conn_network_type (const fsdp_description_t * dsc);

/**
 * Get the the global type of network address of the multimedia
 * session connection.
 *
 * @param dsc SDP description object.
 * @return global network address type for this connection.
 * FSDP_ADDRESS_TYPE_UNDEFINED if no global network address type is
 * included in the description.
 **/
fsdp_address_type_t
fsdp_get_global_conn_address_type (const fsdp_description_t * dsc);

/**
 * Get the the global address of the multimedia session connection.
 *
 * @param dsc SDP description object.
 * @return global address for this connection.
 **/
const char *fsdp_get_global_conn_address (const fsdp_description_t * dsc);

unsigned int
fsdp_get_global_conn_address_ttl (const fsdp_description_t * dsc);

unsigned int
fsdp_get_global_conn_address_count (const fsdp_description_t * dsc);

/**
 * Get the number of bandwidth modifiers specified for this session.
 *
 * @param dsc SDP description object.
 * @return number of bandwidth modifiers.
 **/
unsigned int fsdp_get_bw_modifier_count (const fsdp_description_t * dsc);

/**
 * Get the bandwidth modifier type for the session.
 *
 * @param dsc SDP description object.
 * @param index number of bandwidth modifier.
 *
 * @return global bandwidth modifier type.  
 * @retval FSDP_BW_MOD_TYPE_UNDEFINED if no global bandwith modifier
 * type is defined or invalid index.
 * @retval FSDP_BW_MOD_TYPE_UNKNOWN if an unknown bandwith modifier is
 * specified or an invalid index is provided. In this case
 * fsdp_get_bw_modifer_type_unknown() can be called to get the
 * modifier as a character string.
 **/
fsdp_bw_modifier_type_t
fsdp_get_bw_modifier_type (const fsdp_description_t * dsc,
			   unsigned int index);

/**
 * Get the textual bandwidth modifier type when it is unknown. 
 *
 * @param dsc SDP description object.
 * @param index number of bandwidth modifier.
 *
 * @return global bandwidth modifier type.
 * @retval empty string if the provided bandwidth type is not unknown,
 * the provided index is invalid or or there was a parse error.
 **/
const char *fsdp_get_bw_modifier_type_unknown (const fsdp_description_t * dsc,
					       unsigned int index);

/**
 * Get the value for the bandwidth modifier. 
 *
 * @param dsc SDP description object.
 * @param index number of bandwidth modifier.
 * @return global bandwidth value.
 * @retval 0 if no bandwidth is specified for this session or an
 * invalid index has been provided.
 **/
unsigned long int
fsdp_get_bw_value (const fsdp_description_t * dsc, unsigned int index);

/**
 * Get the number of time periods specified for this session
 *
 * @param dsc SDP description object.
 * @return number of time periods
 **/
unsigned long int fsdp_get_period_count (const fsdp_description_t * dsc);

/**
 * Get the start time for the period selected by index.
 *
 * @param dsc SDP description object.
 * @param index number of time period. Note that this index follows the
 * traditional C convention: from 0 to fsdp_get_period_count() - 1.
 * @return start time
 * @retval 0 if an invalid index is provided.
 **/
time_t
fsdp_get_period_start (const fsdp_description_t * dsc, unsigned int index);

/**
 * Get the stop time for the period selected by index.
 *
 * @param dsc SDP description object.
 * @param index number of time period. Note that this index follows the
 * traditional C convention: from 0 to fsdp_get_period_count() - 1.
 * @return stop time
 * @retval 0 if an invalid index is provided.
 **/
time_t
fsdp_get_period_stop (const fsdp_description_t * dsc, unsigned int index);

/**
 * Get the number of repeats for the period selected by index.
 *
 * @param dsc SDP description object.
 * @param index number of the period. Note that this index follows the
 * traditional C convention: from 0 to fsdp_get_period_count() - 1.
 * @return number of repeats
 * @retval 0 if an invalid index is provided.
 **/
unsigned int
fsdp_get_period_repeats_count (const fsdp_description_t * dsc,
			       unsigned int index);

/**
 * Get the interval time of the repeat selected by rindex for the
 * period selected by index.
 *
 * @param dsc SDP description object.
 * @param index number of time period. Note that this index follows the
 * traditional C convention: from 0 to fsdp_get_period_count() - 1.
 * @param rindex number of repeat
 * @return interval time
 * @retval 0 if an invalid index is provided.
 **/
unsigned long int
fsdp_get_period_repeat_interval (const fsdp_description_t * dsc,
				 unsigned int index, unsigned int rindex);

/**
 * Get the duration of the repeat selected by rindex for the period
 * selected by index.
 *
 * @param dsc SDP description object.
 * @param index number of time period. Note that this index follows the
 * traditional C convention: from 0 to fsdp_get_period_count() - 1.
 * @param rindex number of repeat
 * @return duration
 * @retval 0 if an invalid index is provided.
 **/
unsigned long int
fsdp_get_period_repeat_duration (const fsdp_description_t * dsc,
				 unsigned int index, unsigned int rindex);

/**
 * Get the offsets of the repeat selected by rindex for the period
 * selected by index.
 *
 * @param dsc SDP description object.
 * @param index number of time period. Note that this index follows the
 * traditional C convention: from 0 to fsdp_get_period_count() - 1.
 * @param rindex number of repeat
 * @return array of offsets
 * @retval NULL if an invalid index is provided.
 **/
const unsigned long int *fsdp_get_period_repeat_offsets (const
							 fsdp_description_t *
							 dsc,
							 unsigned int index,
							 unsigned int rindex);

/**
 * Get the encryption method defined for this session.
 *
 * @param dsc SDP description object.
 * @return encryption method. FSDP_ENCRYPTION_METHOD_UNDEFINED if no
 * encryption method is specified.
 **/
fsdp_encryption_method_t
fsdp_get_encryption_method (const fsdp_description_t * dsc);

/**
 * Get the encryption key or a URI pointing to the encryption key for
 * this session.
 *
 * @param dsc SDP description object.
 * @return encryption key unless FSDP_ENCRYPTION_METHOD_URI is
 * specified, in which case a URI pointing to the key is returned. If
 * the global encryption method is undefined, NULL is returned.
 **/
const char *fsdp_get_encryption_content (const fsdp_description_t * dsc);

/**
 * Get timezone adjustments.
 *
 * @param dsc SDP description object.
 * @return string with list of timezone adjustments
 * @retval NULL if no timezone adjustment list was specified or there
 * was a parse error.
 **/
const char *fsdp_get_timezone_adj (const fsdp_description_t * dsc);

/**
 *
 **/
unsigned int
fsdp_get_unidentified_attribute_count (const fsdp_description_t * dsc);

/**
 *
 **/
const char *fsdp_get_unidentified_attribute (const fsdp_description_t * dsc,
					     unsigned int index);

/**
 *
 **/
unsigned int
fsdp_get_media_rtpmap_count (const fsdp_media_description_t * mdsc);

/**
 *
 **/
const char *fsdp_get_media_rtpmap_payload_type (const fsdp_media_description_t
						* mdsc, unsigned int index);

/**
 *
 **/
const char *fsdp_get_media_rtpmap_encoding_name (const
						 fsdp_media_description_t *
						 mdsc, unsigned int index);

/**
 *
 **/
unsigned int
fsdp_get_media_rtpmap_clock_rate (const fsdp_media_description_t * mdsc,
				  unsigned int index);

/**
 *
 **/
const char *fsdp_get_media_rtpmap_encoding_parameters (const
						       fsdp_description_t *
						       mdsc,
						       unsigned int index);

/**
 * Get the value of the session attribute specified in
 * <code>att</code>. This function works for all the session
 * attributes whose value is a character string. These attributes are
 * defined in the session_string_attribute_t enumerated type.
 *
 * @param dsc SDP description object.
 * @param att attribute to get.
 *
 * @return value of the attribute <code>att</code>.
 * @retval NULL if the attribute was not specified or there was a
 * parse error or an invalid att is given.
 **/
const char *fsdp_get_str_att (const fsdp_description_t * dsc,
			      fsdp_session_str_att_t att);

/**
 *
 **/
unsigned int fsdp_get_sdplang_count (const fsdp_description_t * dsc);

/**
 *
 **/
const char *fsdp_get_sdplang (const fsdp_description_t * dsc,
			      unsigned int index);

/** 
 * Get the mode of the conference, specified with attributes sendrecv,
 * sendonly, recvonly and inactive.
 *
 * @param dsc SDP description object.
 * @return send/rec conference mode.
 * @retval FSDP_SENDRECV_UNDEFINED if conference mode not provided.
 **/
fsdp_sendrecv_mode_t fsdp_get_sendrecv_mode (const fsdp_description_t * dsc);

/**
 * Get the type of conference, such as broadcast, meeting, moderated,
 * test or H332.
 *
 * @param dsc SDP description object.
 * @return conference type.
 * @retval FSDP_SESSION_TYPE_UNDEFINED if conference type not provided.
 **/
fsdp_session_type_t fsdp_get_session_type (const fsdp_description_t * dsc);

/**
 *
 **/
unsigned int fsdp_get_media_count (const fsdp_description_t * dsc);

/**
 *
 **/
const fsdp_media_description_t *fsdp_get_media (const fsdp_description_t *
						dsc, unsigned int index);

/**
 *
 **/
fsdp_media_t fsdp_get_media_type (const fsdp_media_description_t * dsc);

/**
 *
 **/
unsigned int fsdp_get_media_port (const fsdp_media_description_t * dsc);

unsigned int fsdp_get_media_port_count (const fsdp_media_description_t * dsc);

/**
 *
 **/
fsdp_transport_protocol_t
fsdp_get_media_transport_protocol (const fsdp_media_description_t * dsc);

/**
 *
 **/
const char *fsdp_get_media_formats (const fsdp_media_description_t * dsc);

/**
 *
 **/
unsigned int
fsdp_get_media_formats_count (const fsdp_media_description_t * dsc);

/**
 *
 **/
const char *fsdp_get_media_format (const fsdp_media_description_t * dsc,
				   unsigned int index);

/**
 *
 **/
const char *fsdp_get_media_title (const fsdp_media_description_t * dsc);

/**
 *
 **/
fsdp_network_type_t
fsdp_get_media_network_type (const fsdp_media_description_t * dsc);

/**
 *
 **/
fsdp_address_type_t
fsdp_get_media_address_type (const fsdp_media_description_t * dsc);

/**
 *
 **/
const char *fsdp_get_media_address (const fsdp_media_description_t * dsc);

unsigned int
fsdp_get_media_address_ttl (const fsdp_media_description_t * mdsc);

unsigned int
fsdp_get_media_address_count (const fsdp_media_description_t * mdsc);

/**
 *
 **/
fsdp_bw_modifier_type_t
fsdp_get_media_bw_modifier_type (const fsdp_media_description_t * dsc,
				 unsigned int index);

/**
 *
 **/
const char *fsdp_get_media_bw_modifier_type_unknown (const
						     fsdp_media_description_t
						     * dsc,
						     unsigned int index);

/**
 *
 **/
unsigned long int
fsdp_get_media_bw_value (const fsdp_media_description_t * dsc,
			 unsigned int index);

/**
 *
 **/
fsdp_encryption_method_t
fsdp_get_media_encryption_method (const fsdp_media_description_t * dsc);

/**
 *
 **/
const char *fsdp_get_media_encryption_content (const fsdp_media_description_t
					       * dsc);

/**
 *
 **/
unsigned int fsdp_get_media_ptime (const fsdp_media_description_t * dsc);

/**
 *
 **/
unsigned int fsdp_get_media_maxptime (const fsdp_media_description_t * dsc);

/**
 *
 **/
unsigned int
fsdp_get_media_fmtp_count (const fsdp_media_description_t * mdsc);

/**
 *
 **/
const char *fsdp_get_media_fmtp (const fsdp_media_description_t * mdsc,
				 unsigned int index);

/**
 *
 **/
unsigned int
fsdp_get_media_sdplang_count (const fsdp_media_description_t * dsc);

/**
 *
 **/
const char *fsdp_get_media_sdplang (const fsdp_media_description_t * dsc,
				    unsigned int index);

/**
 *
 **/
unsigned int fsdp_get_media_lang_count (const fsdp_media_description_t * dsc);

/**
 *
 **/
const char *fsdp_get_media_lang (const fsdp_media_description_t * dsc,
				 unsigned int index);


unsigned int fsdp_get_control_count (const fsdp_description_t * dsc);

const char *fsdp_get_control (const fsdp_description_t * dsc,
			      unsigned int index);

const char *fsdp_get_range (const fsdp_description_t * dsc);

unsigned int
fsdp_get_media_control_count (const fsdp_media_description_t * mdsc);

char *fsdp_get_media_control (const fsdp_media_description_t * mdsc,
			      unsigned int index);

char *fsdp_get_media_range (const fsdp_media_description_t * mdsc);

/**
 *
 **/
fsdp_orient_t fsdp_get_media_orient (const fsdp_media_description_t * dsc);

/**
 *
 **/
fsdp_sendrecv_mode_t
fsdp_get_media_sendrecv (const fsdp_media_description_t * dsc);

/**
 *
 **/
float fsdp_get_media_framerate (const fsdp_media_description_t * dsc);

/**
 *
 **/
unsigned int fsdp_get_media_quality (const fsdp_media_description_t * dsc);

/**
 *
 **/
unsigned int fsdp_get_media_rtcp_port (const fsdp_media_description_t * dsc);

/**
 *
 **/
fsdp_network_type_t
fsdp_get_media_rtcp_network_type (const fsdp_media_description_t * dsc);

/**
 *
 **/
fsdp_address_type_t
fsdp_get_media_rtcp_address_type (const fsdp_media_description_t * dsc);

/**
 *
 **/
const char *fsdp_get_media_rtcp_address (const fsdp_media_description_t *
					 dsc);

/**
 *
 **/
unsigned int
fsdp_get_media_unidentified_attribute_count (const fsdp_media_description_t
					     * mdsc);

/**
 *
 **/
const char *fsdp_get_media_unidentified_attribute (const
						   fsdp_media_description_t *
						   mdsc, unsigned int index);


	  /** @} *//* closes parser group */

END_C_DECLS
#endif /* FSDP_PARSER_H */
