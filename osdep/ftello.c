/*
 * ftello.c
 *	  64-bit version of ftello() for systems which do not have it
 */

#include "config.h"

#include <stdio.h>
#include <sys/types.h>

off_t
ftello(FILE *stream)
{
	fpos_t floc;

	if (fgetpos(stream, &floc) != 0)
		return -1;
	return floc;
}
