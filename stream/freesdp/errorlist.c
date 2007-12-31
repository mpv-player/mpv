/*
  This file is part of FreeSDP
  Copyright (C) 2001, 2002 Federico Montesino Pouzols <fedemp@suidzer0.org>

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
 * @file errorlist.c
 *
 * @short Translation table for error numbers
 *
 */

#ifndef FSDP_ERRORLIST_C
#define FSDP_ERRORLIST_C

#include "common.h"

const char *fsdp_error_t_s[] = {
  "No error",/** FSDPE_OK **/
  "Illegal character detected",/** FSDPE_ILLEGAL_CHARACTER **/
  "Missing version item", /** FSDPE_MISSING_VERSION **/
  "Invalid version item", /** FSDPE_INVALID_VERSION **/
  "Owner item not present", /** FSDPE_MISSING_OWNER **/
  "Parse error in owner item", /** FSDPE_INVALID_OWNER **/
  "Session name not present", /** FSDPE_MISSING_NAME **/
  "Empty session name item", /** FSDPE_EMPTY_NAME **/
  "Syntax error in connection item", /** FSDPE_INVALID_CONNECTION **/
  "Unrecognized address type in connection item", /** FSDPE_INVALID_CONNECTION_ADDRTYPE **/
  "Unrecognized network type in connection item", /** FSDPE_INVALID_CONNECTION_NETTYPE **/
  "Parse error in bandwith item", /** FSDPE_INVALID_BANDWIDTH **/
  "No time period for the session", /** FSDPE_MISSING_TIME **/
  "Parse error in time item", /** FSDPE_INVALID_TIME **/
  "Parse error in repeat time item", /** FSDPE_INVALID_REPEAT **/
  "Parse error in timezone item", /** FSDPE_INVALID_TIMEZONE **/
  "Unknown encryption method", /** FSDPE_INVALID_ENCRYPTION_METHOD **/
  "Syntax error in an attribute item", /** FSDPE_INVALID_ATTRIBUTE **/
  "Syntax error in an rtpmap attribute item", /** FSDPE_INVALID_ATTRIBUTE_RTPMAP **/
  "Unknown session type in a session-level attribute", /** FSDPE_INVALID_SESSION_TYPE **/
  "Parse error in media item", /** FSDPE_INVALID_MEDIA **/
  "Unknown media type in media item", /** FSDPE_UNKNOWN_MEDIA_TYPE **/
  "Unknown media transport", /** FSDPE_UNKNOWN_MEDIA_TRANSPORT **/
  "Unknown extra lines in description item", /** FSDPE_OVERFILLED **/
  "Unknown line found",	/** FSDPE_INVALID_LINE **/
  "No connection information provided",	/** FSDPE_MISSING_CONNECTION_INFO **/
  "Description item does not fit in MAXSIZE", /** FSDPE_INVALID_INDEX **/
  "Internal error", /** FSDPE_INTERNAL_ERROR **/
  "Invalid function parameters", /** FSDPE_INVALID_PARAMETER **/
  "Buffer overflow" /** FSDPE_BUFFER_OVERFLOW **/
};


const char *
fsdp_strerror (fsdp_error_t err_no)
{
  return (fsdp_error_t_s[err_no]);
}

#endif /* FSDP_ERRORLIST_C */
