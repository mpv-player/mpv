// File written by Michael Niedermayer and its under GPL
// simple file compare program, it finds the number of rounding errors 
// and dies if there is a larger error ( ABS(a-b)>1 )

#include <stdio.h>

// FIXME no checks but its just for debuging so who cares ;)

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
		int c0= fgetc(f0);
		int c1= fgetc(f1);
		int d= c0-c1;
		if(c0<0 && c1<0) break;
		if(c0<0 || c1<0)
		{
			printf("FATAL error, files have different size!\n");
			exit(1);
		}
		
		if(d<0) d=-d; // ABS
		if(d>1)
		{
			printf("FATAL error, too large differnce found!\n");
			exit(1);
		}
		
		if(d) dif++;
	}
	
	fclose(f0);
	fclose(f1);
	
	printf("%d (+/-1)differences found\n", dif);
	exit(0);
}