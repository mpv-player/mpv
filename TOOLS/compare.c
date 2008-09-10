/*
 * Simple file compare program, it finds the number of rounding errors
 * and dies if there is too large an error ( ABS(a-b)>1 ).
 *
 * copyright (c) 2001 Michael Niedermayer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>

// FIXME: No checks but it is just for debugging so who cares ;)

int main(int argc, char **argv)
{
	FILE *f0, *f1;
	int dif=0;

	if(argc!=3)
	{
		printf("compare <file1> <file2>\n");
		exit(2);
	}

	f0= fopen(argv[1], "rb");
	f1= fopen(argv[2], "rb");

	for(;;)
	{
		short c0;
		short c1;
		int d;

		int e0= fread(&c0, 2, 1, f0);
		int e1= fread(&c1, 2, 1, f1);

		d=c0-c1;
		if(e0==0 && e1==0) break;
		if(e0==0 || e1==0)
		{
			printf("FATAL error, files have different size!\n");
			exit(1);
		}

		if(d<0) d=-d; // ABS
		if(d>1)
		{
			printf("FATAL error, too large a difference found (%d)!\n", d);
			exit(1);
		}

		if(d) dif++;
	}

	fclose(f0);
	fclose(f1);

	printf("%d (+/-1)differences found\n", dif);
	exit(0);
}

