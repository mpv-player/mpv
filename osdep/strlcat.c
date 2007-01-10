/* strlcat implementation for systems that do not have it in libc
 * Time-stamp: <2004-03-14 njk>
 * (C) 2003-2004 Nicholas J. Kain <njk@aerifal.cx>
 */

#include "config.h"

unsigned int strlcat (char *dest, const char *src, unsigned int size)
{
	register char *d = dest;

	for (; size > 0 && *d != '\0'; size--, d++);
	return (d - dest) + strlcpy(d, src, size);
}

