/* strl(cat|cpy) implementation for systems that do not have it in libc */
/* strl.c - strlcpy/strlcat implementation
 * Time-stamp: <2004-03-14 njk>
 * (C) 2003-2004 Nicholas J. Kain <njk@aerifal.cx>
 */

#include "config.h"

#ifndef HAVE_STRLCPY
unsigned int strlcpy (char *dest, const char *src, unsigned int size)
{
	register unsigned int i = 0;

	if (size > 0) {
	size--;
	for (i=0; size > 0 && src[i] != '\0'; ++i, size--)
		dest[i] = src[i];

	dest[i] = '\0';
	}
	while (src[i++]);

	return i;
}
#endif

#ifndef HAVE_STRLCAT
unsigned int strlcat (char *dest, const char *src, unsigned int size)
{
	register char *d = dest;

	for (; size > 0 && *d != '\0'; size--, d++);
	return (d - dest) + strlcpy(d, src, size);
}
#endif

