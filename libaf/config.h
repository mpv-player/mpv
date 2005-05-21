/*=============================================================================
//	
//  This software has been released under the terms of the GNU General Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright 2002 Anders Johansson ajh@watri.uwa.edu.au
//
//=============================================================================
*/

#ifndef __af_config_h__
#define __af_config_h__

#include "../config.h" // WORDS_BIGENDIAN

#ifndef MPLAYER_CONFIG_H
#error Mandatory WORDS_BIGENDIAN does not contain 0 nor 1
#endif

// Number of channels
#ifndef AF_NCH
#define AF_NCH 6
#endif

#endif /* __af_config_h__ */
