/* This program is licensed under the GNU Library General Public License, version 2,
 * a copy of which is included with this program (with filename LICENSE.LGPL).
 *
 * (c) 2002 John Edwards
 *
 * rand_t header.
 *
 * last modified: $ID:$
 */

#include "common.h"

#ifndef __RAND_T_H
#define __RAND_T_H

#ifdef __cplusplus
extern "C" {
#endif 

#ifndef FIXED_POINT

typedef struct {
    const float32_t*  FilterCoeff;
    uint64_t          Mask;
    double            Add;
    float32_t         Dither;
    float32_t         ErrorHistory     [2] [16];       // max. 2 channels, 16th order Noise shaping
    float32_t         DitherHistory    [2] [16];
    int32_t           LastRandomNumber [2];
} dither_t;

extern dither_t            Dither;
extern double              doubletmp;
//static const uint8_t       Parity [256];
uint32_t                   random_int ( void );
extern double              scalar16 ( const float32_t* x, const float32_t* y );
extern double              Random_Equi ( double mult );
extern double              Random_Triangular ( double mult );
void                       Init_Dither ( unsigned char bits, unsigned char shapingtype );

#endif

#ifdef __cplusplus
}
#endif 

#endif
