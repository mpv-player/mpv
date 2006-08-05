/*
 *  Copyright (C) 2001 Bertrand BAUDET, 2006 Benjamin Zores
 *   Network helpers for TCP connections
 *   (originally borrowed from network.c,
 *      by Bertrand BAUDET <bertrand_baudet@yahoo.com>).
 *
 *   This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef _TCP_H_
#define _TCP_H_

/* Connect to a server using a TCP connection */
int connect2Server (char *host, int port, int verb);

#define TCP_ERROR_TIMEOUT -3     /* connection timeout */
#define TCP_ERROR_FATAL   -2     /* unable to resolve name */
#define TCP_ERROR_PORT    -1     /* unable to connect to a particular port */

#endif /* _TCP_H_ */
