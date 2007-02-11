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
 * @file parser.c
 *
 * @short Parsing module implementation.
 *
 * This file implements the parsing routine <code>fsdp_parse</code>
 * and the <code>fsdp_get_xxxx</code> routines that allow to get the
 * session properties from a session description object build through
 * the application of <code>fsdp_parse</code> to a textual SDP session
 * description.
 **/

#include "parserpriv.h"

/**
 * \brief find the start of the next line
 * \param c pointer to current position in string
 * \return pointer to start of next line or NULL if illegal (i.e.
 *         a '\r' is not followed by a '\n'
 */
static const char *next_line(const char *c) {
  c += strcspn(c, "\n\r");
  if (*c == 0) return c;
  if (*c == '\r') c++;
  if (*c == '\n')
    return c + 1;
  return NULL;
}

/**
 * Moves the <code>c<code> pointer up to the beginning of the next
 * line.
 *
 * @param c char pointer to pointer
 * @retval FSDPE_ILLEGAL_CHARACTER, when an illegal '\r' character
 * (not followed by a '\n') is found, returns
 */
#define NEXT_LINE(c) do { if (!(c = next_line(c))) return FSDPE_ILLEGAL_CHARACTER; } while (0);

fsdp_error_t
fsdp_parse (const char *text_description, fsdp_description_t * dsc)
{
  fsdp_error_t result;
  const char *p = text_description, *p2;
  unsigned int index, j;
  /* temps for sscanf */
  const unsigned int TEMPCHARS = 6;
  char fsdp_buf[TEMPCHARS][MAXSHORTFIELDLEN];
  char longfsdp_buf[MAXLONGFIELDLEN];
  const unsigned int TEMPINTS = 2;
  unsigned long int wuint[TEMPINTS];

  if ((NULL == text_description) || (NULL == dsc))
    return FSDPE_INVALID_PARAMETER;

  /***************************************************************************/
  /* A) parse session-level description                                      */
  /***************************************************************************/

  /* `v=' line (protocol version) */
  /* according to the RFC, only `v=0' is valid */
  if (sscanf (p, "v=%1lu", &wuint[0]))
  {
    if (wuint[0] != 0)
      return FSDPE_INVALID_VERSION;
  }
  else
  {
    return FSDPE_MISSING_VERSION;
  }
  NEXT_LINE (p);

  /* `o=' line (owner/creator and session identifier) */
  /* o=<username> <session id> <version> <network type> <address type>
     <address> */
  if (!strncmp (p, "o=", 2))
  {
    p += 2;
    /* note that the following max lengths may vary in the future and
       are quite arbitary */
    if (sscanf
        (p,
         "%" MSFLENS "[\x21-\xFF] %" MSFLENS "[0-9] %" MSFLENS
         "[0-9] %2s %3s %" MSFLENS "s", fsdp_buf[0], fsdp_buf[1],
         fsdp_buf[2], fsdp_buf[3], fsdp_buf[4], fsdp_buf[5]) != 6)
      return FSDPE_INVALID_OWNER;
    dsc->o_username = strdup (fsdp_buf[0]);
    dsc->o_session_id = strdup (fsdp_buf[1]);
    dsc->o_announcement_version = strdup (fsdp_buf[2]);
    if (!strncmp (fsdp_buf[3], "IN", 2))
    {
      dsc->o_network_type = FSDP_NETWORK_TYPE_INET;
      if (!strncmp (fsdp_buf[4], "IP4", 3))
        dsc->o_address_type = FSDP_ADDRESS_TYPE_IPV4;
      else if (!strncmp (fsdp_buf[4], "IP6", 3))
        dsc->o_address_type = FSDP_ADDRESS_TYPE_IPV6;
      else
        return FSDPE_INVALID_OWNER;
    }
    else
    {
      return FSDPE_INVALID_OWNER;
    }
    /* TODO? check valid unicast address/FQDN */
    dsc->o_address = strdup (fsdp_buf[5]);
  }
  else
  {
    return FSDPE_MISSING_OWNER;
  }
  NEXT_LINE (p);

  /* `s=' line (session name) -note that the name string cannot be empty */
  /* s=<session name> */
  if (!strncmp (p, "s=", 2))
  {
    if (sscanf (p, "s=%" MLFLENS "[^\r\n]", longfsdp_buf) < 1)
      return FSDPE_EMPTY_NAME;
    dsc->s_name = strdup (longfsdp_buf);
  }
  else
  {
    return FSDPE_MISSING_NAME;
  }
  NEXT_LINE (p);

  /* `i=' line (session information) [optional] */
  /* i=<session description> */
  if (!strncmp (p, "i=", 2)
      && sscanf (p, "i=%" MLFLENS "[^\r\n]", longfsdp_buf))
  {
    dsc->i_information = strdup (longfsdp_buf);
    NEXT_LINE (p);
  }
  else
  {
    /* (optional) information absent */
  }

  /* `u=' line (URI of description)  [optional] */
  /* u=<URI> */
  if (!strncmp (p, "u=", 2)
      && sscanf (p, "u=%" MLFLENS "[^\r\n]", longfsdp_buf))
  {
    /* TODO? check valid uri */
    dsc->u_uri = strdup (longfsdp_buf);
    NEXT_LINE (p);
  }
  else
  {
    /* (optional) uri absent */
  }

  /* `e=' lines (email address) [zero or more] */
  /* e=<email address> */
  p2 = p;
  j = 0;
  while (!strncmp (p2, "e=", 2))
  {
    /* First, count how many emails are there */
    j++;
    NEXT_LINE (p2);
  }
  dsc->emails_count = j;
  if (dsc->emails_count > 0)
  {
    /* Then, build the array of emails */
    dsc->emails = calloc (j, sizeof (const char *));
    for (j = 0; j < dsc->emails_count; j++)
    {
      sscanf (p, "e=%" MLFLENS "[^\r\n]", longfsdp_buf);
      /* TODO? check valid email-address. */
      dsc->emails[j] = strdup (longfsdp_buf);
      NEXT_LINE (p);
    }
  }

  /* `p=' lines (phone number) [zero or more] */
  /*  p=<phone number> */
  j = 0;
  /* assert ( p2 == p ); */
  while (!strncmp (p2, "p=", 2))
  {
    j++;
    NEXT_LINE (p2);
  }
  dsc->phones_count = j;
  if (dsc->phones_count > 0)
  {
    dsc->phones = calloc (j, sizeof (const char *));
    for (j = 0; j < dsc->phones_count; j++)
    {
      sscanf (p, "p=%" MLFLENS "[^\r\n]", longfsdp_buf);
      /* TODO? check valid phone-number. */
      dsc->phones[j] = strdup (longfsdp_buf);
      NEXT_LINE (p);
    }
  }

  /* `c=' line (connection information - not required if included in all media) [optional] */
  /* c=<network type> <address type> <connection address> */
  result = fsdp_parse_c (&p, &(dsc->c_network_type), &(dsc->c_address_type),
			 &(dsc->c_address));
  if (FSDPE_OK != result)
    return result;

  /* `b=' lines (bandwidth information) [optional] */
  /* b=<modifier>:<bandwidth-value> */
  result =
    fsdp_parse_b (&p, &(dsc->bw_modifiers), &(dsc->bw_modifiers_count));
  if (FSDPE_OK != result)
    return result;

  /* A.1) Time descriptions: */

  /* `t=' lines (time the session is active) [1 or more] */
  /* t=<start time>  <stop time> */
  j = 0;
  p2 = p;
  while (!strncmp (p2, "t=", 2))
  {
    j++;
    NEXT_LINE (p2);
    while (!strncmp (p2, "r=", 2))
      NEXT_LINE (p2);
  }
  dsc->time_periods_count = j;
  if (dsc->time_periods_count == 0)
    return FSDPE_MISSING_TIME;
  dsc->time_periods = calloc (dsc->time_periods_count,
			      sizeof (fsdp_time_period_t *));
  index = 0;
  for (j = 0; j < dsc->time_periods_count; j++)
  {
    unsigned int h = 0;
    if (sscanf (p, "t=%10lu %10lu", &wuint[0], &wuint[1]) != 2)
    {
      /* not all periods have been successfully parsed */
      dsc->time_periods_count = j;
      return FSDPE_INVALID_TIME;
    }
    dsc->time_periods[j] = calloc (1, sizeof (fsdp_time_period_t));

    /* convert from NTP to time_t time */
    if (wuint[0] != 0)
      wuint[0] -= NTP_EPOCH_OFFSET;
    if (wuint[1] != 0)
      wuint[1] -= NTP_EPOCH_OFFSET;
    dsc->time_periods[j]->start = wuint[0];
    dsc->time_periods[j]->stop = wuint[1];
    NEXT_LINE (p);

    /* `r' lines [zero or more repeat times for each t=] */
    /*r=<repeat interval> <active duration> <list of offsets from
      start-time> */
    p2 = p;
    while (!strncmp (p2, "r=", 2))
    {
      h++;
      NEXT_LINE (p2);
    }
    dsc->time_periods[j]->repeats_count = h;
    if (h > 0)
    {
      unsigned int index2 = 0;
      dsc->time_periods[j]->repeats =
        calloc (h, sizeof (fsdp_repeat_t *));
      for (h = 0; h < dsc->time_periods[j]->repeats_count; h++)
      {
        /*
          get_repeat_values(p,&(dsc->time_periods[index].repeats[index2]));
          fsdp_error_t get_repeat_values (const char *r, fsdp_repeat_t
          *repeat);
          */
        if (sscanf (p, "r=%10s %10s %" MLFLENS "[^\r\n]",
                    fsdp_buf[0], fsdp_buf[1], longfsdp_buf) == 3)
        {
          fsdp_repeat_t *repeat;
          dsc->time_periods[j]->repeats[h] =
            calloc (1, sizeof (fsdp_repeat_t));
          repeat = dsc->time_periods[j]->repeats[h];
          /* get interval, duration and list of offsets */
          result =
            fsdp_repeat_time_to_uint (fsdp_buf[0],
                                      &(repeat->interval));
          if (result == FSDPE_OK)
          {
            result =
              fsdp_repeat_time_to_uint (fsdp_buf[1],
                                        &(repeat->duration));
            if (result == FSDPE_OK)
            {
              unsigned int k = 1;
              const char *i = longfsdp_buf;
              while (NULL != (i = strchr (i, ' ')))
              {
                k++;
                if (NULL != i)
                  i++;
              }
              repeat->offsets_count = k;
              repeat->offsets = calloc (k, sizeof (time_t));
              i = longfsdp_buf;
              for (k = 0;
                   (k < repeat->offsets_count)
                     && (result == FSDPE_OK); k++)
              {
                result =
                  fsdp_repeat_time_to_uint (i,
                                            &(repeat->
                                              offsets[k]));
                i = strchr (i, ' ');
                if (NULL != i)
                  i++;
              }
              if (k < repeat->offsets_count)
              {
                /* there where invalid repeat offsets */
                dsc->time_periods[j]->repeats_count = k;
                return FSDPE_INVALID_REPEAT;
              }
            }
          }
          if (result != FSDPE_OK)
          {
            /* not all repeats have been succesfully parsed */
            dsc->time_periods[j]->repeats_count = h;
            return FSDPE_INVALID_REPEAT;
          }
          NEXT_LINE (p);
        }
        else
        {
          /* not all repeats have been succesfully parsed */
          dsc->time_periods[j]->repeats_count = h;
          return FSDPE_INVALID_REPEAT;
        }
        index2++;
      }
    }
  }

  /* `z=' line (time zone adjustments) [zero or more] */
  /* z=<adjustment time> <offset> <adjustment time> <offset> .... */
  if (!strncmp (p, "z=", 2))
  {
    if (sscanf (p, "z=%" MLFLENS "[^\r\n]", longfsdp_buf))
    {
      /* TODO: guess how many pairs are there and process them */
      dsc->timezone_adj = strdup (longfsdp_buf);
      NEXT_LINE (p);
    }
    else
    {
      return FSDPE_INVALID_TIMEZONE;
    }
  }

  /* `k=' line (encryption key) [optional] */
  /* k=<method> 
     k=<method>:<encryption key> */
  result = fsdp_parse_k (&p, &(dsc->k_encryption_method),
			 &(dsc->k_encryption_content));
  if (result != FSDPE_OK)
    return result;

  /* A.2) Attributes */
  /* `a=' lines (session attribute) [0 or more] */
  /* a=<attribute>
     a=<attribute>:<value> */
  while (!strncmp (p, "a=", 2))
  {
    /* The "9" lenght specifier of the first string is subject to
       changes */
    if (sscanf
        (p, "a=%9[^:\r\n]:%" MSFLENS "[^\r\n]", fsdp_buf[0],
         fsdp_buf[1]) == 2)
    {
      /* session-level value attributes */
      if (!strncmp (fsdp_buf[0], "cat", 3))
        dsc->a_category = strdup (fsdp_buf[1]);
      else if (!strncmp (fsdp_buf[0], "keywds", 6))
        dsc->a_keywords = strdup (fsdp_buf[1]);
      else if (!strncmp (fsdp_buf[0], "tool", 4))
        dsc->a_keywords = strdup (fsdp_buf[1]);
      else if (!strncmp (fsdp_buf[0], "rtpmap", 6))
        fsdp_parse_rtpmap (&(dsc->a_rtpmaps),
                           &(dsc->a_rtpmaps_count), fsdp_buf[1]);
      else if (!strncmp (fsdp_buf[0], "type", 4))
      {
        if (!strncmp (fsdp_buf[1], "broadcast", 9))
          dsc->a_type = FSDP_SESSION_TYPE_BROADCAST;
        else if (!strncmp (fsdp_buf[1], "meeting", 7))
          dsc->a_type = FSDP_SESSION_TYPE_MEETING;
        else if (!strncmp (fsdp_buf[1], "moderated", 9))
          dsc->a_type = FSDP_SESSION_TYPE_MODERATED;
        else if (!strncmp (fsdp_buf[1], "test", 4))
          dsc->a_type = FSDP_SESSION_TYPE_TEST;
        else if (!strncmp (fsdp_buf[1], "H332", 4))
          dsc->a_type = FSDP_SESSION_TYPE_H332;
        else
          return FSDPE_INVALID_SESSION_TYPE;
      }
      else if (!strncmp (fsdp_buf[0], "charset", 7))
        dsc->a_charset = strdup (fsdp_buf[1]);
      else if (!strncmp (fsdp_buf[0], "sdplang", 7))
      {
        if (NULL == dsc->a_sdplangs)
        {
          dsc->a_sdplangs_count = 0;
          dsc->a_sdplangs =
            calloc (SDPLANGS_MAX_COUNT, sizeof (char *));
        }
        if (dsc->a_sdplangs_count < SDPLANGS_MAX_COUNT)
        {
          dsc->a_sdplangs[dsc->a_sdplangs_count] =
            strdup (fsdp_buf[1]);
          dsc->a_sdplangs_count++;
        }
      }
      else if (!strncmp (fsdp_buf[0], "lang", 4))
      {
        if (NULL == dsc->a_langs)
        {
          dsc->a_langs_count = 0;
          dsc->a_langs = calloc (SDPLANGS_MAX_COUNT, sizeof (char *));
        }
        if (dsc->a_langs_count < SDPLANGS_MAX_COUNT)
        {
          dsc->a_langs[dsc->a_langs_count] = strdup (fsdp_buf[1]);
          dsc->a_langs_count++;
        }
      }
      else if (!strncmp (fsdp_buf[0], "control", 7))
      {
        if (NULL == dsc->a_controls)
        {
          dsc->a_controls_count = 0;
          dsc->a_controls =
            calloc (SDPCONTROLS_MAX_COUNT, sizeof (char *));
        }
        if (dsc->a_controls_count < SDPCONTROLS_MAX_COUNT)
        {
          dsc->a_controls[dsc->a_controls_count] =
            strdup (fsdp_buf[1]);
          dsc->a_controls_count++;
        }
      }
      else if (!strncmp (fsdp_buf[0], "range", 5))
      {
        if (dsc->a_range)
          free (dsc->a_range);
        dsc->a_range = strdup (fsdp_buf[1]);
      }
      else
      {
        /* ignore unknown attributes, but provide access to them */
        *longfsdp_buf = '\0';
        strncat (longfsdp_buf, fsdp_buf[0], MAXLONGFIELDLEN-1);
        strncat (longfsdp_buf, ":", MAXLONGFIELDLEN-strlen(longfsdp_buf)-1);
        strncat (longfsdp_buf, fsdp_buf[1], MAXLONGFIELDLEN-strlen(longfsdp_buf)-1);
        if (NULL == dsc->unidentified_attributes)
        {
          dsc->unidentified_attributes_count = 0;
          dsc->unidentified_attributes =
            calloc (UNIDENTIFIED_ATTRIBUTES_MAX_COUNT,
                    sizeof (char *));
        }
        if (dsc->unidentified_attributes_count <
            UNIDENTIFIED_ATTRIBUTES_MAX_COUNT)
        {
          dsc->unidentified_attributes
            [dsc->unidentified_attributes_count] =
            strdup (longfsdp_buf);
          dsc->unidentified_attributes_count++;
        }
      }
      NEXT_LINE (p);
    }
    else if (sscanf (p, "a=%20s", fsdp_buf[0]) == 1)
    {
      /* session-level property attributes */
      if (!strncmp (fsdp_buf[0], "recvonly", 8))
        dsc->a_sendrecv_mode = FSDP_SENDRECV_RECVONLY;
      else if (!strncmp (fsdp_buf[0], "sendonly", 8))
        dsc->a_sendrecv_mode = FSDP_SENDRECV_SENDONLY;
      else if (!strncmp (fsdp_buf[0], "inactive", 8))
        dsc->a_sendrecv_mode = FSDP_SENDRECV_INACTIVE;
      else if (!strncmp (fsdp_buf[0], "sendrecv", 8))
        dsc->a_sendrecv_mode = FSDP_SENDRECV_SENDRECV;
      else
      {
        /* ignore unknown attributes, but provide access to them */
        *longfsdp_buf = '\0';
        strncat (longfsdp_buf, fsdp_buf[0], MAXLONGFIELDLEN-1);
        if (NULL == dsc->unidentified_attributes)
        {
          dsc->unidentified_attributes_count = 0;
          dsc->unidentified_attributes =
            calloc (UNIDENTIFIED_ATTRIBUTES_MAX_COUNT,
                    sizeof (char *));
        }
        if (dsc->unidentified_attributes_count <
            UNIDENTIFIED_ATTRIBUTES_MAX_COUNT)
        {
          dsc->unidentified_attributes
            [dsc->unidentified_attributes_count] =
            strdup (longfsdp_buf);
          dsc->unidentified_attributes_count++;
        }
      }
      NEXT_LINE (p);
    }
    else
      return FSDPE_INVALID_ATTRIBUTE;
  }

  /***************************************************************************/
  /* B) parse media-level descriptions                                       */
  /***************************************************************************/
  p2 = p;
  j = 0;
  while ((*p2 != '\0') && !strncmp (p2, "m=", 2))
  {
    char c;
    j++;
    NEXT_LINE (p2);
    while (sscanf (p2, "%c=", &c) == 1)
    {
      if (c == 'i' || c == 'c' || c == 'b' || c == 'k' || c == 'a')
      {
        NEXT_LINE (p2);
      }
      else if (c == 'm')
      {
        break;
      }
      else
      {
        return FSDPE_INVALID_LINE;
      }
    }
  }
  dsc->media_announcements_count = j;
  if (dsc->media_announcements_count == 0)
  {
    ;
    /*return FSDPE_MISSING_MEDIA; */
  }
  else
  {				/* dsc->media_announcements_count > 0 */
    dsc->media_announcements =
      calloc (j, sizeof (fsdp_media_announcement_t *));
    for (j = 0; j < dsc->media_announcements_count; j++)
    {
      fsdp_media_announcement_t *media = NULL;
      /* `m=' line (media name, transport address and format list) */
      /* m=<media>  <port>  <transport> <fmt list> */
      /* The max. string lengths are subject to change */
      if (sscanf (p, "m=%11s %8s %7s %" MLFLENS "[^\r\n]",
                  fsdp_buf[0], fsdp_buf[1], fsdp_buf[2],
                  longfsdp_buf) != 4)
      {
        return FSDPE_INVALID_MEDIA;
      }
      else
      {
        dsc->media_announcements[j] =
          calloc (1, sizeof (fsdp_media_announcement_t));
        media = dsc->media_announcements[j];
        if (!strncmp (fsdp_buf[0], "audio", 5))
          media->media_type = FSDP_MEDIA_AUDIO;
        else if (!strncmp (fsdp_buf[0], "video", 5))
          media->media_type = FSDP_MEDIA_VIDEO;
        else if (!strncmp (fsdp_buf[0], "application", 11))
          media->media_type = FSDP_MEDIA_APPLICATION;
        else if (!strncmp (fsdp_buf[0], "data", 4))
          media->media_type = FSDP_MEDIA_DATA;
        else if (!strncmp (fsdp_buf[0], "control", 7))
          media->media_type = FSDP_MEDIA_CONTROL;
        else
          return FSDPE_UNKNOWN_MEDIA_TYPE;
        {			/* try to get port specification as port/number */
          char *slash;
          if ((slash = strchr (fsdp_buf[1], '/')))
          {
            *slash = '\0';
            slash++;
            media->port = strtol (fsdp_buf[1], NULL, 10);
            media->port_count = strtol (slash, NULL, 10);
          }
          else
          {
            media->port = strtol (fsdp_buf[1], NULL, 10);
            media->port_count = 0;
          }
        }
        if (!strncmp (fsdp_buf[2], "RTP/AVP", 7))
          media->transport = FSDP_TP_RTP_AVP;
        else if (!strncmp (fsdp_buf[2], "udp", 3))
          media->transport = FSDP_TP_UDP;
        else if (!strncmp (fsdp_buf[2], "TCP", 3))
          media->transport = FSDP_TP_TCP;
        else if (!strncmp (fsdp_buf[2], "UDPTL", 5))
          media->transport = FSDP_TP_UDPTL;
        else if (!strncmp (fsdp_buf[2], "vat", 3))
          media->transport = FSDP_TP_VAT;
        else if (!strncmp (fsdp_buf[2], "rtp", 3))
          media->transport = FSDP_TP_OLD_RTP;
        else
          return FSDPE_UNKNOWN_MEDIA_TRANSPORT;
        {
          unsigned int k = 0;
          char *s = longfsdp_buf;
          while (NULL != (s = strchr (s, ' ')))
          {
            k++;
            if (NULL != s)
              s++;
          }
          k++;		/* when there is no space left, count the last format */
          media->formats_count = k;
          media->formats = calloc (k, sizeof (char *));
          s = longfsdp_buf;
          for (k = 0; k < media->formats_count; k++)
          {
            char *space = strchr (s, ' ');
            if (NULL != space)
              *space = '\0';
            media->formats[k] = strdup (s);
            s = space + 1;
          }
        }
        NEXT_LINE (p);
      }

      /* `i=' line (media title) [optional] */
      /* i=<media title> */
      if (!strncmp (p, "i=", 2)
          && sscanf (p, "i=%" MLFLENS "[^\r\n]", longfsdp_buf))
      {
        media->i_title = strdup (longfsdp_buf);
        NEXT_LINE (p);
      }
      else
      {
        /* (optional) information absent */
      }

      /* `c=' line (connection information - overrides session-level
         line) [optional if provided at session-level] */
      /* c=<network type> <address type> <connection address> */
      result = fsdp_parse_c (&p, &(media->c_network_type),
                             &(media->c_address_type),
                             &(media->c_address));
      if (result != FSDPE_OK)
        return result;

      /* `b=' lines (bandwidth information) [optional] */
      /* b=<modifier>:<bandwidth-value> */
      result = fsdp_parse_b (&p, &(media->bw_modifiers),
                             &(media->bw_modifiers_count));
      if (FSDPE_OK != result)
        return result;

      /* `k=' line (encryption key) [optional] */
      /* k=<method> 
         k=<method>:<encryption key> */
      result = fsdp_parse_k (&p, &(media->k_encryption_method),
                             &(media->k_encryption_content));
      if (result != FSDPE_OK)
        return result;

      /* B.1) Attributes */

      /* `a=' lines (zero or more media attribute lines) [optional] */
      /* a=<attribute>
         a=<attribute>:<value> */
      while (!strncmp (p, "a=", 2))
      {
        if (sscanf
            (p, "a=%9[^:\r\n]:%" MLFLENS "[^\r\n]", fsdp_buf[0],
             longfsdp_buf) == 2)
        {
          /* media-level value attributes */
          if (!strncmp (fsdp_buf[0], "ptime", 5))
            media->a_ptime = strtoul (longfsdp_buf, NULL, 10);
          else if (!strncmp (fsdp_buf[0], "maxptime", 8))
            media->a_maxptime = strtoul (longfsdp_buf, NULL, 10);
          else if (!strncmp (fsdp_buf[0], "rtpmap", 6))
            fsdp_parse_rtpmap (&(media->a_rtpmaps),
                               &(media->a_rtpmaps_count),
                               longfsdp_buf);
          else if (!strncmp (fsdp_buf[0], "orient", 6))
          {
            if (!strncmp (longfsdp_buf, "portrait", 8))
              media->a_orient = FSDP_ORIENT_PORTRAIT;
            else if (!strncmp (longfsdp_buf, "landscape", 9))
              media->a_orient = FSDP_ORIENT_LANDSCAPE;
            else if (!strncmp (longfsdp_buf, "seascape", 9))
              media->a_orient = FSDP_ORIENT_SEASCAPE;
          }
          else if (!strncmp (fsdp_buf[0], "sdplang", 7))
          {
            if (NULL == dsc->a_sdplangs)
            {
              media->a_sdplangs_count = 0;
              media->a_sdplangs =
                calloc (SDPLANGS_MAX_COUNT, sizeof (char *));
            }
            if (media->a_sdplangs_count < SDPLANGS_MAX_COUNT)
            {
              media->a_sdplangs[dsc->a_sdplangs_count] =
                strdup (longfsdp_buf);
              media->a_sdplangs_count++;
            }
          }
          else if (!strncmp (fsdp_buf[0], "lang", 4))
          {
            if (NULL == dsc->a_langs)
            {
              media->a_langs_count = 0;
              media->a_langs =
                calloc (SDPLANGS_MAX_COUNT, sizeof (char *));
            }
            if (media->a_langs_count < SDPLANGS_MAX_COUNT)
            {
              media->a_langs[dsc->a_langs_count] =
                strdup (longfsdp_buf);
              media->a_langs_count++;
            }
          }
          else if (!strncmp (fsdp_buf[0], "control", 7))
          {
            if (NULL == media->a_controls)
            {
              media->a_controls_count = 0;
              media->a_controls =
                calloc (SDPCONTROLS_MAX_COUNT, sizeof (char *));
            }
            if (media->a_controls_count < SDPCONTROLS_MAX_COUNT)
            {
              media->a_controls[media->a_controls_count] =
                strdup (longfsdp_buf);
              media->a_controls_count++;
            }
          }
          else if (!strncmp (fsdp_buf[0], "range", 5))
          {
            if (media->a_range)
              free (media->a_range);
            media->a_range = strdup (fsdp_buf[1]);
          }
          else if (!strncmp (fsdp_buf[0], "framerate", 9))
            media->a_framerate = strtod (longfsdp_buf, NULL);
          else if (!strncmp (fsdp_buf[0], "fmtp", 4))
          {
            if (NULL == media->a_fmtps)
            {
              media->a_fmtps_count = 0;
              media->a_fmtps =
                calloc (SDPLANGS_MAX_COUNT, sizeof (char *));
            }
            if (media->a_fmtps_count < SDPLANGS_MAX_COUNT)
            {
              media->a_fmtps[media->a_fmtps_count] =
                strdup (longfsdp_buf);
              media->a_fmtps_count++;
            }
          }
          else if (!strncmp (fsdp_buf[0], "rtcp", 4))
          {
            int opts = 0;
            /* rtcp attribute: a=rtcp:<port> <nettype> <addrtype> <address> */
            opts =
              sscanf (longfsdp_buf, "%lu %2s %3s %" MSFLENS "s",
                      &wuint[0], fsdp_buf[0], fsdp_buf[1],
                      fsdp_buf[2]);
            if (opts >= 1)
            {
              media->a_rtcp_port = wuint[0];
              if (opts >= 2)
              {
                if (!strncmp (fsdp_buf[0], "IN", 2))
                {
                  media->a_rtcp_network_type =
                    FSDP_NETWORK_TYPE_INET;
                }	/* else
                           ; TODO: define error code? */
                if (opts >= 3)
                {
                  if (!strncmp (fsdp_buf[1], "IP4", 3))
                    media->a_rtcp_address_type =
                      FSDP_ADDRESS_TYPE_IPV4;
                  else if (!strncmp (fsdp_buf[1], "IP6", 3))
                    media->a_rtcp_address_type =
                      FSDP_ADDRESS_TYPE_IPV6;
                  else
                    return FSDPE_INVALID_CONNECTION_NETTYPE;
                  /*add specific code? */
                  if (opts >= 4)
                    media->a_rtcp_address =
                      strdup (fsdp_buf[2]);
                }
              }
            }
          }
          else
          {
            /* ignore unknown attributes, but provide access to them */
            *fsdp_buf[1] = '\0';
            strncat (fsdp_buf[1], fsdp_buf[0], MAXSHORTFIELDLEN-1);
            strncat (fsdp_buf[1], ":", MAXSHORTFIELDLEN-strlen(fsdp_buf[1])-1);
            strncat (fsdp_buf[1], longfsdp_buf, MAXSHORTFIELDLEN-strlen(fsdp_buf[1])-1);
            if (NULL == media->unidentified_attributes)
            {
              media->unidentified_attributes_count = 0;
              media->unidentified_attributes =
                calloc (UNIDENTIFIED_ATTRIBUTES_MAX_COUNT,
                        sizeof (char *));
            }
            if (media->unidentified_attributes_count <
                UNIDENTIFIED_ATTRIBUTES_MAX_COUNT)
            {
              media->unidentified_attributes
                [media->unidentified_attributes_count] =
                strdup (fsdp_buf[1]);
              media->unidentified_attributes_count++;
            }
          }
          NEXT_LINE (p);
        }
        else if (sscanf (p, "a=%8s", fsdp_buf[0]) == 1)
        {
          /* media-level property attributes */
          if (!strncmp (fsdp_buf[0], "recvonly", 8))
            media->a_sendrecv_mode = FSDP_SENDRECV_RECVONLY;
          else if (!strncmp (fsdp_buf[0], "sendonly", 8))
            media->a_sendrecv_mode = FSDP_SENDRECV_SENDONLY;
          else if (!strncmp (fsdp_buf[0], "inactive", 8))
            media->a_sendrecv_mode = FSDP_SENDRECV_INACTIVE;
          else if (!strncmp (fsdp_buf[0], "sendrecv", 8))
            media->a_sendrecv_mode = FSDP_SENDRECV_SENDRECV;
          else
          {
            /* ignore unknown attributes, but provide access to them */
            *longfsdp_buf = '\0';
            strncat (longfsdp_buf, fsdp_buf[0], MAXLONGFIELDLEN-1);
            if (NULL == media->unidentified_attributes)
            {
              media->unidentified_attributes_count = 0;
              media->unidentified_attributes =
                calloc (UNIDENTIFIED_ATTRIBUTES_MAX_COUNT,
                        sizeof (char *));
            }
            if (media->unidentified_attributes_count <
                UNIDENTIFIED_ATTRIBUTES_MAX_COUNT)
            {
              media->unidentified_attributes
                [media->unidentified_attributes_count] =
                strdup (longfsdp_buf);
              media->unidentified_attributes_count++;
            }
          }
          NEXT_LINE (p);
        }
        else
          return FSDPE_INVALID_ATTRIBUTE;
      }
    }			/* end of for */
  }

  /* Check c= has been given at session level or at media level for
     all media */
  if (NULL == dsc->c_address.address)
  {
    unsigned int c;
    for (c = 0; c < dsc->media_announcements_count; c++)
      if (NULL == dsc->media_announcements[c]->c_address.address)
        return FSDPE_MISSING_CONNECTION_INFO;
  }

  /* finish */
  if (*p == '\0')
    return FSDPE_OK;
  else
    return FSDPE_OVERFILLED;
}

static fsdp_error_t
fsdp_parse_c (const char **p, fsdp_network_type_t * ntype,
	      fsdp_address_type_t * atype,
	      fsdp_connection_address_t * address)
{
  const unsigned int TEMPCHARS = 3;
  char fsdp_buf[TEMPCHARS][MAXSHORTFIELDLEN];

  if (!strncmp (*p, "c=", 2))
  {
    if (sscanf (*p, "c=%2s %3s %" MSFLENS "s",
                fsdp_buf[0], fsdp_buf[1], fsdp_buf[2]))
    {
      if (!strncmp (fsdp_buf[0], "IN", 2))
      {
        *ntype = FSDP_NETWORK_TYPE_INET;
        if (!strncmp (fsdp_buf[1], "IP4", 3))
          *atype = FSDP_ADDRESS_TYPE_IPV4;
        else if (!strncmp (fsdp_buf[1], "IP6", 3))
          *atype = FSDP_ADDRESS_TYPE_IPV6;
        else
          return FSDPE_INVALID_CONNECTION_NETTYPE;
      }
      else
      {
        return FSDPE_INVALID_CONNECTION_ADDRTYPE;
      }
      {
        char *slash = strchr (fsdp_buf[2], '/');
        if (NULL == slash)
        {
          address->address = strdup (fsdp_buf[2]);
          address->address_ttl = 0;
          address->address_count = 0;
        }
        else
        {
          /* address is IP4 multicast */
          char *slash2;
          *slash = '\0';
          slash++;
          address->address = strdup (fsdp_buf[2]);
          slash2 = strchr (slash + 1, '/');
          if (NULL == slash2)
          {
            address->address_ttl = strtol (slash, NULL, 10);
            address->address_count = 0;
          }
          else
          {
            *slash2 = '\0';
            slash2++;
            address->address_ttl = strtol (slash, NULL, 10);
            address->address_count = strtol (slash2, NULL, 10);
          }
        }
      }
      NEXT_LINE (*p);
    }
    else
    {
      return FSDPE_INVALID_CONNECTION;
    }
  }
  return FSDPE_OK;
}

static fsdp_error_t
fsdp_parse_b (const char **p, fsdp_bw_modifier_t ** bw_modifiers,
	      unsigned int *bw_modifiers_count)
{
  char fsdp_buf[MAXSHORTFIELDLEN];
  unsigned long int wuint;
  unsigned int i = 0;
  const char *lp = *p;

  /* count b= lines */
  while (!strncmp (lp, "b=", 2))
  {
    NEXT_LINE (lp);
    i++;
  }
  *bw_modifiers = calloc (i, sizeof (fsdp_bw_modifier_t));
  *bw_modifiers_count = i;

  while (i > 0)
  {
    unsigned int index = *bw_modifiers_count - i;
    if (2 == sscanf (*p, "b=%20[^:\r\n]:%lu", fsdp_buf, &wuint))
    {
      if (!strncmp (fsdp_buf, "CT", 2))
        (*bw_modifiers)[index].b_mod_type =
          FSDP_BW_MOD_TYPE_CONFERENCE_TOTAL;
      else if (!strncmp (fsdp_buf, "AS", 2))
        (*bw_modifiers)[index].b_mod_type =
          FSDP_BW_MOD_TYPE_APPLICATION_SPECIFIC;
      else if (!strncmp (fsdp_buf, "RS", 2))
        (*bw_modifiers)[index].b_mod_type = FSDP_BW_MOD_TYPE_RTCP_SENDERS;
      else if (!strncmp (fsdp_buf, "RR", 2))
        (*bw_modifiers)[index].b_mod_type =
          FSDP_BW_MOD_TYPE_RTCP_RECEIVERS;
      else
      {
        (*bw_modifiers)[index].b_mod_type = FSDP_BW_MOD_TYPE_UNKNOWN;
        (*bw_modifiers)[index].b_unknown_bw_modt =
          (char *) strdup (fsdp_buf);
      }
      (*bw_modifiers)[index].b_value = wuint;
      NEXT_LINE (*p);
    }
    else
    {
      *bw_modifiers_count -= i;
      return FSDPE_INVALID_BANDWIDTH;
    }
    i--;
  }
  return FSDPE_OK;
}

static fsdp_error_t
fsdp_parse_k (const char **p, fsdp_encryption_method_t * method,
	      char **content)
{
  char fsdp_buf[MAXSHORTFIELDLEN];
  char longfsdp_buf[MAXLONGFIELDLEN];

  if (!strncmp (*p, "k=", 2))
  {
    if (sscanf (*p, "k=prompt"))
    {
      *method = FSDP_ENCRYPTION_METHOD_PROMPT;
      *content = NULL;
      NEXT_LINE (*p);
    }
    else
    {
      if (sscanf
          (*p, "k=%6[^:\r\n]:%" MLFLENS "s", fsdp_buf, longfsdp_buf))
      {
        if (!strncmp (fsdp_buf, "clear", 5))
          *method = FSDP_ENCRYPTION_METHOD_CLEAR;
        else if (!strncmp (fsdp_buf, "base64", 6))
          *method = FSDP_ENCRYPTION_METHOD_BASE64;
        else if (!strncmp (fsdp_buf, "uri", 3))
          *method = FSDP_ENCRYPTION_METHOD_URI;
        else
          return FSDPE_INVALID_ENCRYPTION_METHOD;
        *content = strdup (longfsdp_buf);
        NEXT_LINE (*p);
      }
    }
  }
  return FSDPE_OK;
}

static fsdp_error_t
fsdp_parse_rtpmap (fsdp_rtpmap_t *** rtpmap, unsigned int *counter,
		   const char *value)
{
  fsdp_error_t result = FSDPE_OK;

  if (0 == *counter)
  {
    *counter = 0;
    *rtpmap = calloc (MEDIA_RTPMAPS_MAX_COUNT, sizeof (fsdp_rtpmap_t *));
  }
  if (*counter < MEDIA_RTPMAPS_MAX_COUNT)
  {
    unsigned int c = *counter;
    fsdp_rtpmap_t **map = *rtpmap;
    char fsdp_buf[MAXSHORTFIELDLEN];
    char longfsdp_buf[MAXLONGFIELDLEN];
    map[c] = calloc (1, sizeof (fsdp_rtpmap_t));

    /* a=rtpmap:<payload type> <encoding name>/<clock rate>[/<encoding
       parameters]> */
    if (2 == sscanf (value, "%s %s", fsdp_buf, longfsdp_buf))
    {
      char *slash1;
      map[c]->pt = strdup (fsdp_buf);
      /* parse <encoding name>/<clock rate>[/<encoding parameters>] */
      slash1 = strchr (longfsdp_buf, '/');
      if (NULL == slash1)
      {
        result = FSDPE_INVALID_ATTRIBUTE_RTPMAP;
      }
      else
      {
        char *slash2;
        *slash1 = '\0';
        slash1++;
        map[c]->encoding_name = strdup (longfsdp_buf);
        slash2 = strchr (slash1, '/');
        if (NULL != slash2)
        {
          *slash2 = '\0';
          slash2++;
          map[c]->parameters = strdup (slash2);
        }
        map[c]->clock_rate = strtol (slash1, NULL, 10);
      }
      (*counter)++;
    }
  }
  return result;
}

static fsdp_error_t
fsdp_repeat_time_to_uint (const char *time, unsigned long int *seconds)
{
  const unsigned long SECONDS_PER_DAY = 86400;
  const unsigned long SECONDS_PER_HOUR = 3600;
  const unsigned long SECONDS_PER_MINUTE = 60;
  char c;
  unsigned long int wuint;

  if (sscanf (time, "%lu%c", &wuint, &c) == 2)
  {
    /* time with unit specification character */
    switch (c)
    {
    case 'd':
      *seconds = wuint * SECONDS_PER_DAY;
      break;
    case 'h':
      *seconds = wuint * SECONDS_PER_HOUR;
      break;
    case 'm':
      *seconds = wuint * SECONDS_PER_MINUTE;
      break;
    case 's':
      *seconds = wuint;
      break;
    default:
      return FSDPE_INVALID_REPEAT;
      break;
    }
  }
  else if (sscanf (time, "%lu", &wuint) == 1)
  {
    /* time without unit specification character */
    *seconds = wuint;
  }
  else
  {
    return FSDPE_INVALID_REPEAT;
  }
  return FSDPE_OK;
}

unsigned int
fsdp_get_version (const fsdp_description_t * dsc)
{
  if (!dsc)
    return 0;
  return dsc->version;
}

const char *
fsdp_get_owner_username (const fsdp_description_t * dsc)
{
  if (!dsc)
    return NULL;
  return dsc->o_username;
}

const char *
fsdp_get_session_id (const fsdp_description_t * dsc)
{
  if (!dsc)
    return NULL;
  return dsc->o_session_id;
}

const char *
fsdp_get_announcement_version (const fsdp_description_t * dsc)
{
  if (!dsc)
    return NULL;
  return dsc->o_announcement_version;
}

fsdp_network_type_t
fsdp_get_owner_network_type (const fsdp_description_t * dsc)
{
  if (!dsc)
    return FSDP_NETWORK_TYPE_UNDEFINED;
  return dsc->o_network_type;
}

fsdp_address_type_t
fsdp_get_owner_address_type (const fsdp_description_t * dsc)
{
  if (!dsc)
    return FSDP_ADDRESS_TYPE_UNDEFINED;
  return dsc->o_address_type;
}

const char *
fsdp_get_owner_address (const fsdp_description_t * dsc)
{
  if (!dsc)
    return NULL;
  return dsc->o_address;
}

const char *
fsdp_get_name (const fsdp_description_t * dsc)
{
  if (!dsc)
    return NULL;
  return dsc->s_name;
}

const char *
fsdp_get_information (const fsdp_description_t * dsc)
{
  if (!dsc)
    return NULL;
  return dsc->i_information;
}

const char *
fsdp_get_uri (const fsdp_description_t * dsc)
{
  if (!dsc)
    return NULL;
  return dsc->u_uri;
}

unsigned int
fsdp_get_emails_count (const fsdp_description_t * dsc)
{
  if (!dsc)
    return 0;
  return dsc->emails_count;
}

const char *
fsdp_get_email (const fsdp_description_t * dsc, unsigned int index)
{
  if ((!dsc) || (index >= dsc->emails_count))
    return NULL;
  return dsc->emails[index];
}

unsigned int
fsdp_get_phones_count (const fsdp_description_t * dsc)
{
  if (!dsc)
    return 0;
  return dsc->phones_count;
}

const char *
fsdp_get_phone (const fsdp_description_t * dsc, unsigned int index)
{
  if ((!dsc) || (index >= dsc->phones_count))
    return NULL;
  return dsc->phones[index];
}

fsdp_network_type_t
fsdp_get_global_conn_network_type (const fsdp_description_t * dsc)
{
  if (!dsc)
    return FSDP_NETWORK_TYPE_UNDEFINED;
  return dsc->c_network_type;
}

fsdp_address_type_t
fsdp_get_global_conn_address_type (const fsdp_description_t * dsc)
{
  if (!dsc)
    return FSDP_ADDRESS_TYPE_UNDEFINED;
  return dsc->c_address_type;
}

const char *
fsdp_get_global_conn_address (const fsdp_description_t * dsc)
{
  if (!dsc)
    return NULL;
  return dsc->c_address.address;
}

unsigned int
fsdp_get_global_conn_address_ttl (const fsdp_description_t * dsc)
{
  if (!dsc)
    return 0;
  return dsc->c_address.address_ttl;
}

unsigned int
fsdp_get_global_conn_address_count (const fsdp_description_t * dsc)
{
  if (!dsc)
    return 0;
  return dsc->c_address.address_count;
}

unsigned int
fsdp_get_bw_modifier_count (const fsdp_description_t * dsc)
{
  if (!dsc)
    return 0;
  return dsc->bw_modifiers_count;
}

fsdp_bw_modifier_type_t
fsdp_get_bw_modifier_type (const fsdp_description_t * dsc, unsigned int index)
{
  if ((!dsc) || (index >= dsc->bw_modifiers_count))
    return FSDP_BW_MOD_TYPE_UNDEFINED;
  return dsc->bw_modifiers[index].b_mod_type;
}

const char *
fsdp_get_bw_modifier_type_unknown (const fsdp_description_t * dsc,
				   unsigned int index)
{
  if ((!dsc) || (index >= dsc->bw_modifiers_count) ||
      (dsc->bw_modifiers[index].b_mod_type != FSDP_BW_MOD_TYPE_UNKNOWN))
    return NULL;
  return dsc->bw_modifiers[index].b_unknown_bw_modt;
}

unsigned long int
fsdp_get_bw_value (const fsdp_description_t * dsc, unsigned int index)
{
  if ((!dsc) || (index >= dsc->bw_modifiers_count))
    return 0;
  return dsc->bw_modifiers[index].b_value;
}

time_t
fsdp_get_period_start (const fsdp_description_t * dsc, unsigned int index)
{
  if ((!dsc) || (index >= dsc->time_periods_count))
    return 0;
  return dsc->time_periods[index]->start;
}

time_t
fsdp_get_period_stop (const fsdp_description_t * dsc, unsigned int index)
{
  if ((!dsc) || (index >= dsc->time_periods_count))
    return 0;
  return dsc->time_periods[index]->stop;
}

unsigned int
fsdp_get_period_repeats_count (const fsdp_description_t * dsc,
			       unsigned int index)
{
  if ((!dsc) || (index >= dsc->time_periods_count))
    return 0;
  return dsc->time_periods[index]->repeats_count;
}

unsigned long int
fsdp_get_period_repeat_interval (const fsdp_description_t * dsc,
				 unsigned int index, unsigned int rindex)
{
  if ((!dsc) || (index >= dsc->time_periods_count))
    return 0;
  return dsc->time_periods[index]->repeats[rindex]->interval;
}

unsigned long int
fsdp_get_period_repeat_duration (const fsdp_description_t * dsc,
				 unsigned int index, unsigned int rindex)
{
  if ((!dsc) || (index >= dsc->time_periods_count))
    return 0;
  return dsc->time_periods[index]->repeats[rindex]->duration;
}

const unsigned long int *
fsdp_get_period_repeat_offsets (const fsdp_description_t * dsc,
				unsigned int index, unsigned int rindex)
{
  if ((!dsc) || (index >= dsc->time_periods_count))
    return NULL;
  return dsc->time_periods[index]->repeats[rindex]->offsets;
}

const char *
fsdp_get_timezone_adj (const fsdp_description_t * dsc)
{
  if (!dsc)
    return NULL;
  return dsc->timezone_adj;
}

unsigned int
fsdp_get_unidentified_attribute_count (const fsdp_description_t * dsc)
{
  if (!dsc)
    return 0;
  return dsc->unidentified_attributes_count;
}

const char *
fsdp_get_unidentified_attribute (const fsdp_description_t * dsc,
				 unsigned int index)
{
  if (!dsc || (index < dsc->unidentified_attributes_count))
    return NULL;
  return dsc->unidentified_attributes[index];
}

fsdp_encryption_method_t
fsdp_get_encryption_method (const fsdp_description_t * dsc)
{
  if (!dsc)
    return FSDP_ENCRYPTION_METHOD_UNDEFINED;
  return dsc->k_encryption_method;
}

const char *
fsdp_get_encryption_content (const fsdp_description_t * dsc)
{
  if (!dsc || (dsc->k_encryption_method == FSDP_ENCRYPTION_METHOD_UNDEFINED))
    return NULL;
  return dsc->k_encryption_content;
}

unsigned int
fsdp_get_rtpmap_count (const fsdp_description_t * dsc)
{
  if (!dsc)
    return 0;
  return dsc->a_rtpmaps_count;
}

const char *
fsdp_get_rtpmap_payload_type (const fsdp_description_t * dsc,
			      unsigned int index)
{
  if ((!dsc) || (index >= dsc->a_rtpmaps_count))
    return NULL;
  return dsc->a_rtpmaps[index]->pt;
}

const char *
fsdp_get_rtpmap_encoding_name (const fsdp_description_t * dsc,
			       unsigned int index)
{
  if ((!dsc) || (index >= dsc->a_rtpmaps_count))
    return NULL;
  return dsc->a_rtpmaps[index]->encoding_name;
}

unsigned int
fsdp_get_rtpmap_clock_rate (const fsdp_description_t * dsc,
			    unsigned int index)
{
  if ((!dsc) || (index >= dsc->a_rtpmaps_count))
    return 0;
  return dsc->a_rtpmaps[index]->clock_rate;
}

const char *
fsdp_get_rtpmap_encoding_parameters (const fsdp_description_t * dsc,
				     unsigned int index)
{
  if ((!dsc) || (index >= dsc->a_rtpmaps_count))
    return NULL;
  return dsc->a_rtpmaps[index]->parameters;
}

const char *
fsdp_get_str_att (const fsdp_description_t * dsc, fsdp_session_str_att_t att)
{
  /*TODO: change these individual attributes with a table, thus
    avoiding this slow switch */
  char *result;

  if (!dsc)
    return NULL;

  switch (att)
  {
  case FSDP_SESSION_STR_ATT_CATEGORY:
    result = dsc->a_category;
    break;
  case FSDP_SESSION_STR_ATT_KEYWORDS:
    result = dsc->a_keywords;
    break;
  case FSDP_SESSION_STR_ATT_TOOL:
    result = dsc->a_tool;
    break;
  case FSDP_SESSION_STR_ATT_CHARSET:
    result = dsc->a_charset;
    break;
  default:
    result = NULL;
  }
  return result;
}

unsigned int
fsdp_get_sdplang_count (const fsdp_description_t * dsc)
{
  if (!dsc)
    return 0;
  return dsc->a_sdplangs_count;
}

const char *
fsdp_get_sdplang (const fsdp_description_t * dsc, unsigned int index)
{
  if ((!dsc) || (index >= dsc->a_sdplangs_count))
    return NULL;
  return dsc->a_sdplangs[index];
}

unsigned int
fsdp_get_lang_count (const fsdp_description_t * dsc)
{
  if (!dsc)
    return 0;
  return dsc->a_langs_count;
}

const char *
fsdp_get_lang (const fsdp_description_t * dsc, unsigned int index)
{
  if ((!dsc) || (index >= dsc->a_langs_count))
    return NULL;
  return dsc->a_langs[index];
}

unsigned int
fsdp_get_control_count (const fsdp_description_t * dsc)
{
  if (!dsc)
    return 0;
  return dsc->a_controls_count;
}

const char *
fsdp_get_control (const fsdp_description_t * dsc, unsigned int index)
{
  if ((!dsc) || (index >= dsc->a_controls_count))
    return NULL;
  return dsc->a_controls[index];
}

const char *
fsdp_get_range (const fsdp_description_t * dsc)
{
  return dsc->a_range;
}

fsdp_sendrecv_mode_t
fsdp_get_sendrecv_mode (const fsdp_description_t * dsc)
{
  if (!dsc)
    return FSDP_SENDRECV_UNDEFINED;
  return dsc->a_sendrecv_mode;
}

fsdp_session_type_t
fsdp_get_session_type (const fsdp_description_t * dsc)
{
  if (!dsc)
    return FSDP_SESSION_TYPE_UNDEFINED;
  return dsc->a_type;
}

unsigned int
fsdp_get_media_count (const fsdp_description_t * dsc)
{
  if (!dsc)
    return 0;
  return dsc->media_announcements_count;
}

const fsdp_media_description_t *
fsdp_get_media (const fsdp_description_t * dsc, unsigned int index)
{
  if ((index >= dsc->media_announcements_count))
    return NULL;
  return dsc->media_announcements[index];
}

fsdp_media_t
fsdp_get_media_type (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return FSDP_MEDIA_UNDEFINED;
  return dsc->media_type;
}

unsigned int
fsdp_get_media_port (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return 0;
  return dsc->port;
}

unsigned int
fsdp_get_media_port_count (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return 0;
  return dsc->port_count;
}

fsdp_transport_protocol_t
fsdp_get_media_transport_protocol (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return FSDP_TP_UNDEFINED;
  return dsc->transport;
}

unsigned int
fsdp_get_media_formats_count (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return 0;
  return dsc->formats_count;
}

const char *
fsdp_get_media_format (const fsdp_media_description_t * dsc,
		       unsigned int index)
{
  if (!dsc && (index < dsc->formats_count))
    return NULL;
  return dsc->formats[index];
}

const char *
fsdp_get_media_title (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return NULL;
  return dsc->i_title;
}

fsdp_network_type_t
fsdp_get_media_network_type (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return FSDP_NETWORK_TYPE_UNDEFINED;
  return dsc->c_network_type;
}

fsdp_address_type_t
fsdp_get_media_address_type (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return FSDP_ADDRESS_TYPE_UNDEFINED;
  return dsc->c_address_type;
}

const char *
fsdp_get_media_address (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return NULL;
  return dsc->c_address.address;
}

unsigned int
fsdp_get_media_address_ttl (const fsdp_media_description_t * mdsc)
{
  if (!mdsc)
    return 0;
  return mdsc->c_address.address_ttl;
}

unsigned int
fsdp_get_media_address_count (const fsdp_media_description_t * mdsc)
{
  if (!mdsc)
    return 0;
  return mdsc->c_address.address_count;
}

fsdp_bw_modifier_type_t
fsdp_get_media_bw_modifier_type (const fsdp_media_description_t * dsc,
				 unsigned int index)
{
  if (!dsc || (index >= dsc->bw_modifiers_count))
    return FSDP_BW_MOD_TYPE_UNDEFINED;
  return dsc->bw_modifiers[index].b_mod_type;
}

const char *
fsdp_get_media_bw_modifier_type_unknown (const fsdp_media_description_t * dsc,
					 unsigned int index)
{
  if (!dsc || (index >= dsc->bw_modifiers_count) ||
      (FSDP_BW_MOD_TYPE_UNKNOWN != dsc->bw_modifiers[index].b_mod_type))
    return NULL;
  return dsc->bw_modifiers[index].b_unknown_bw_modt;
}

unsigned long int
fsdp_get_media_bw_value (const fsdp_media_description_t * dsc,
			 unsigned int index)
{
  if (!dsc || (index >= dsc->bw_modifiers_count))
    return 0;
  return dsc->bw_modifiers[index].b_value;
}

fsdp_encryption_method_t
fsdp_get_media_encryption_method (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return FSDP_ENCRYPTION_METHOD_UNDEFINED;
  return dsc->k_encryption_method;
}

const char *
fsdp_get_media_encryption_content (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return NULL;
  return dsc->k_encryption_content;
}

unsigned int
fsdp_get_media_ptime (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return 0;
  return dsc->a_ptime;
}

unsigned int
fsdp_get_media_maxptime (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return 0;
  return dsc->a_maxptime;
}

unsigned int
fsdp_get_media_rtpmap_count (const fsdp_media_description_t * mdsc)
{
  if (!mdsc)
    return 0;
  return mdsc->a_rtpmaps_count;
}

const char *
fsdp_get_media_rtpmap_payload_type (const fsdp_media_description_t * mdsc,
				    unsigned int index)
{
  if (!mdsc || (index >= mdsc->a_rtpmaps_count))
    return NULL;
  return mdsc->a_rtpmaps[index]->pt;
}

const char *
fsdp_get_media_rtpmap_encoding_name (const fsdp_media_description_t * mdsc,
				     unsigned int index)
{
  if (!mdsc || (index >= mdsc->a_rtpmaps_count))
    return NULL;
  return mdsc->a_rtpmaps[index]->encoding_name;
}

unsigned int
fsdp_get_media_rtpmap_clock_rate (const fsdp_media_description_t * mdsc,
				  unsigned int index)
{
  if (!mdsc || (index >= mdsc->a_rtpmaps_count))
    return 0;
  return mdsc->a_rtpmaps[index]->clock_rate;
}

const char *
fsdp_get_media_rtpmap_encoding_parameters (const fsdp_description_t * mdsc,
					   unsigned int index)
{
  if (!mdsc || (index >= mdsc->a_rtpmaps_count))
    return NULL;
  return mdsc->a_rtpmaps[index]->parameters;
}

unsigned int
fsdp_get_media_sdplang_count (const fsdp_media_description_t * mdsc)
{
  if (!mdsc)
    return 0;
  return mdsc->a_sdplangs_count;
}

const char *
fsdp_get_media_sdplang (const fsdp_media_description_t * mdsc,
			unsigned int index)
{
  if (!mdsc || (index >= mdsc->a_sdplangs_count))
    return NULL;
  return mdsc->a_sdplangs[index];
}

unsigned int
fsdp_get_media_lang_count (const fsdp_media_description_t * mdsc)
{
  if (!mdsc)
    return 0;
  return mdsc->a_langs_count;
}

const char *
fsdp_get_media_lang (const fsdp_media_description_t * mdsc,
		     unsigned int index)
{
  if (!mdsc || (index >= mdsc->a_langs_count))
    return NULL;
  return mdsc->a_langs[index];
}

unsigned int
fsdp_get_media_control_count (const fsdp_media_description_t * mdsc)
{
  if (!mdsc)
    return 0;
  return mdsc->a_controls_count;
}

char *
fsdp_get_media_control (const fsdp_media_description_t * mdsc,
			unsigned int index)
{
  if (!mdsc || (index >= mdsc->a_controls_count))
    return NULL;
  return mdsc->a_controls[index];
}

char *
fsdp_get_media_range (const fsdp_media_description_t * mdsc)
{
  return mdsc->a_range;
}

unsigned int
fsdp_get_media_fmtp_count (const fsdp_media_description_t * mdsc)
{
  if (!mdsc)
    return 0;
  return mdsc->a_fmtps_count;
}

const char *
fsdp_get_media_fmtp (const fsdp_media_description_t * mdsc,
		     unsigned int index)
{
  if (!mdsc || (index >= mdsc->a_fmtps_count))
    return NULL;
  return mdsc->a_fmtps[index];
}

fsdp_orient_t
fsdp_get_media_orient (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return FSDP_ORIENT_UNDEFINED;
  return dsc->a_orient;
}

fsdp_sendrecv_mode_t
fsdp_get_media_sendrecv (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return FSDP_SENDRECV_UNDEFINED;
  return dsc->a_sendrecv_mode;
}

float
fsdp_get_media_framerate (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return 0;
  return dsc->a_framerate;
}

unsigned int
fsdp_get_media_quality (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return 0;
  return dsc->a_quality;
}

unsigned int
fsdp_get_media_rtcp_port (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return 0;
  return dsc->a_rtcp_port;
}

fsdp_network_type_t
fsdp_get_media_rtcp_network_type (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return FSDP_NETWORK_TYPE_UNDEFINED;
  return dsc->a_rtcp_network_type;
}

fsdp_address_type_t
fsdp_get_media_rtcp_address_type (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return FSDP_ADDRESS_TYPE_UNDEFINED;
  return dsc->a_rtcp_address_type;
}

const char *
fsdp_get_media_rtcp_address (const fsdp_media_description_t * dsc)
{
  if (!dsc)
    return NULL;
  return dsc->a_rtcp_address;
}

unsigned int
fsdp_get_media_unidentified_attribute_count (const fsdp_media_description_t
					     * mdsc)
{
  if (!mdsc)
    return 0;
  return mdsc->unidentified_attributes_count;
}

const char *
fsdp_get_media_unidentified_attribute (const fsdp_media_description_t * mdsc,
				       unsigned int index)
{
  if (!mdsc || (index < mdsc->unidentified_attributes_count))
    return NULL;
  return mdsc->unidentified_attributes[index];
}
