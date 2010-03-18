/*
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 */

#ifndef MPLAYER_DRV_H
#define MPLAYER_DRV_H

#include "wine/windef.h"
#include "wine/driver.h"

void CodecAlloc(void);
void CodecRelease(void);

HDRVR DrvOpen(LPARAM lParam2);
void DrvClose(HDRVR hdrvr);

#endif /* MPLAYER_DRV_H */
