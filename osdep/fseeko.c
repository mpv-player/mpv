/*
 * fseeko.c
 *	  64-bit versions of fseeko/ftello() for systems which do not have them
 */

#include "config.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef WIN32
#define flockfile
#define funlockfile
#endif

/*
 *	On BSD/OS and NetBSD (and perhaps others), off_t and fpos_t are the 
 *      same.  Standards say off_t is an arithmetic type, but not necessarily 
 *      integral, while fpos_t might be neither.
 *
 *	This is thread-safe on BSD/OS using flockfile/funlockfile.
 */

int
fseeko(FILE *stream, off_t offset, int whence)
{
	fpos_t floc;
	struct stat filestat;

	switch (whence)
	{
		case SEEK_CUR:
			flockfile(stream);
			if (fgetpos(stream, &floc) != 0)
				goto failure;
			floc += offset;
			if (fsetpos(stream, &floc) != 0)
				goto failure;
			funlockfile(stream);
			return 0;
			break;
		case SEEK_SET:
			if (fsetpos(stream, &offset) != 0)
				return -1;
			return 0;
			break;
		case SEEK_END:
			flockfile(stream);
			if (fstat(fileno(stream), &filestat) != 0)
				goto failure;
			floc = filestat.st_size;
			if (fsetpos(stream, &floc) != 0)
				goto failure;
			funlockfile(stream);
			return 0;
			break;
		default:
			errno =	EINVAL;
			return -1;
	}

failure:
	funlockfile(stream);
	return -1;
}
