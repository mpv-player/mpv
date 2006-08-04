/* Imported from the dvbstream project
 *
 * Modified for use with MPlayer, for details see the changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 */

#ifndef _RTP_H
#define _RTP_H

#include "config.h"
#ifndef HAVE_WINSOCK2
#include <sys/socket.h>
#else
#include <winsock2.h>
#endif

int read_rtp_from_server(int fd, char *buffer, int length);

#endif
