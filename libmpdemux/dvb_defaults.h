/* dvb_defaults.h

   Provided by Tomi Ollila

   Copyright (C) Dave Chapman 2002

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
   Or, point your browser to http://www.gnu.org/copyleft/gpl.html

*/

#ifndef _DVB_DEFAULTS_H
#define _DVB_DEFAULTS_H

/* DVB-S */

// With a diseqc system you may need different values per LNB.  I hope
// no-one ever asks for that :-)

#define SLOF (11700*1000UL)
#define LOF1 (9750*1000UL)
#define LOF2 (10600*1000UL)



#ifdef FINLAND
    /* FINLAND settings 1 */
    #define DVB_T_LOCATION		"Suomessa"
    #define BANDWIDTH_DEFAULT           BANDWIDTH_8_MHZ
    #define HP_CODERATE_DEFAULT         FEC_2_3
    #define CONSTELLATION_DEFAULT       QAM_64
    #define TRANSMISSION_MODE_DEFAULT   TRANSMISSION_MODE_8K
    #define GUARD_INTERVAL_DEFAULT	GUARD_INTERVAL_1_8
    #define HIERARCHY_DEFAULT           HIERARCHY_NONE
#endif


#ifdef FINLAND2
    /* FINLAND settings 2 (someone verify there is such environment) */
    #define DVB_T_LOCATION		    "Suomessa II"
    #define BANDWIDTH_DEFAULT           BANDWIDTH_8_MHZ
    #define HP_CODERATE_DEFAULT         FEC_1_2
    #define CONSTELLATION_DEFAULT       QAM_64
    #define TRANSMISSION_MODE_DEFAULT   TRANSMISSION_MODE_2K
    #define GUARD_INTERVAL_DEFAULT      GUARD_INTERVAL_1_8
    #define HIERARCHY_DEFAULT           HIERARCHY_NONE
#endif

#if defined (UK) && defined (HP_CODERATE_DEFAULT)
    #error Multible countries defined
#endif



#ifndef DVB_T_LOCATION
    #ifndef UK
	#warning No DVB-T country defined in dvb_defaults.h
	#warning defaulting to UK
	#warning Ignore this if using Satellite or Cable
    #endif

    /* UNITED KINGDOM settings */
    #define DVB_T_LOCATION		"in United Kingdom"
    #define BANDWIDTH_DEFAULT           BANDWIDTH_8_MHZ
    #define HP_CODERATE_DEFAULT         FEC_2_3
    #define CONSTELLATION_DEFAULT       QAM_64
    #define TRANSMISSION_MODE_DEFAULT   TRANSMISSION_MODE_2K
    #define GUARD_INTERVAL_DEFAULT      GUARD_INTERVAL_1_32
    #define HIERARCHY_DEFAULT           HIERARCHY_NONE
#endif


#if HIERARCHY_DEFAULT == HIERARCHY_NONE && !defined (LP_CODERATE_DEFAULT)
    #define LP_CODERATE_DEFAULT (0) /* unused if HIERARCHY_NONE */
#endif

#endif
