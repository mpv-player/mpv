/* strl(cat|cpy) implementation for systems that do not have it in libc */
/* strl.c - strlcpy/strlcat implementation
 * Time-stamp: <2004-03-14 njk>
 * (C) 2003-2004 Nicholas J. Kain <njk@aerifal.cx>
 */

#include "../config.h"

#ifndef HAVE_STRLCPY
unsigned int strlcpy (char *dest, char *src, unsigned int size)
{
	register unsigned int i;

	for (i=0; size > 0 && src[i] != '\0'; ++i, size--)
		dest[i] = src[i];

	dest[i] = '\0';

	return i;
}
#endif

#ifndef HAVE_STRLCAT
unsigned int strlcat (char *dest, char *src, unsigned int size)
{
#if 0
	register unsigned int i, j;

	for(i=0; size > 0 && dest[i] != '\0'; size--, i++);
	for(j=0; size > 0 && src[j] != '\0'; size--, i++, j++)
		dest[i] = src[j];

	dest[i] = '\0';
	return i;
#else
	register char *d = dest, *s = src;

	for (; size > 0 && *d != '\0'; size--, d++);
	for (; size > 0 && *s != '\0'; size--, d++, s++)
		*d = *s;

	*d = '\0';
	return (d - dest) + (s - src);
#endif 
}
#endif

