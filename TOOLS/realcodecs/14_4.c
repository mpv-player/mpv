/*
   GPL v2 blah blah
   
   This is a small dll that works as a wrapper for the actual 14_4.so.6.0
   dll from real player 8.0. 
*/

/*
   Assuming that RACloseCodec is the last call.
*/

#include <stddef.h>
#include <stdio.h>
#include <dlfcn.h>
#include <sys/time.h>

typedef unsigned long ulong;

ulong (*raCloseCodec)(ulong);
ulong (*raDecode)(ulong,ulong,ulong,ulong,ulong,ulong);
ulong (*raFreeDecoder)(ulong);
ulong (*raGetFlavorProperty)(ulong,ulong,ulong,ulong);
ulong (*raGetNumberOfFlavors)(void);
ulong (*raInitDecoder)(ulong,ulong);
ulong (*raOpenCodec2)(ulong);
ulong (*raSetFlavor)(ulong);

int b_dlOpened=0;
void *handle=NULL;

/* exits program when failure */
void loadSyms() {
	fputs("loadSyms()\n", stderr);
	if (!b_dlOpened) {
		char *error;

//		fputs("opening dll...\n");
		handle = dlopen ("/home/r/RealPlayer8/Codecs/real14_4.so.6.0", RTLD_LAZY);
		if (!handle) {
			fputs (dlerror(), stderr);
			exit(1);
		}

		raCloseCodec = dlsym(handle, "RACloseCodec");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(RACloseCodec): %s\n", error);
			exit(1);
		}
		raDecode = dlsym(handle, "RADecode");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(RADecode): %s\n", error);
			exit(1);
		}
		raFreeDecoder = dlsym(handle, "RAFreeDecoder");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(RAFreeDecoder): %s\n", error);
			exit(1);
		}
		raGetFlavorProperty = dlsym(handle, "RAGetFlavorProperty");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(RAGetFlavorProperty): %s\n", error);
			exit(1);
		}
		raGetNumberOfFlavors = dlsym(handle, "RAGetNumberOfFlavors");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(RAGetNumberOfFlavors): %s\n", error);
			exit(1);
			}
		raInitDecoder = dlsym(handle, "RAInitDecoder");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(RAInitDecoder): %s\n", error);
			exit(1);
		}
		raOpenCodec2 = dlsym(handle, "RAOpenCodec2");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(RAOpenCodec2): %s\n", error);
			exit(1);
		}
		raSetFlavor = dlsym(handle, "RASetFlavor");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(RASetFlavor): %s\n", error);
			exit(1);
		}
		b_dlOpened=1;
	}
}

void closeDll() {
	if (handle) {
		b_dlOpened=0;
		dlclose(handle);
		handle=NULL;
	}
}

void _init(void) {
	loadSyms();
}

struct timezone tz;
struct timeval tv1, tv2;

void tic() {
	gettimeofday(&tv1, &tz);
}

void toc() {
	long secs, usecs;
	gettimeofday(&tv2, &tz);
	secs=tv2.tv_sec-tv1.tv_sec;
	usecs=tv2.tv_usec-tv1.tv_usec;
	if (usecs<0) {
		usecs+=1000000;
		--secs;
	}
	fprintf(stderr, "Duration: %d.%0.6ds\n", secs, usecs);
}


void hexdump(void *pos, int len) {
	unsigned char *cpos=pos, *cpos1;
	int lines=(len+15)>>4;
	while(lines--) {
		int len1=len, i;
		fprintf(stderr, "%0x  ", cpos); 
		cpos1=cpos;
		for (i=0;i<16;i++) {
			if (len1>0) {
				fprintf(stderr, "%02x ", *(cpos++));
			} else {
				fprintf(stderr, "   ");
			}
			len1--;
		}
		fputs("  ", stderr);
		cpos=cpos1;
		for (i=0;i<16;i++) {
			if (len>0) {
				unsigned char ch=(*(cpos++));
				if ((ch<32)||(ch>127)) ch='.';
				fputc(ch, stderr);
			}
			len--;
		}
		fputs("\n", stderr);		
	}
	fputc('\n', stderr);
}


ulong RACloseCodec(ulong p1) {
	ulong result;
	fprintf(stderr, "RACloseCodec(ulong p1=0x%0x(%d))\n", p1, p1);
	result=(*raCloseCodec)(p1);
//	closeDll();
	fprintf(stderr, "--> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

static int pkno=0;

ulong RADecode(ulong p1,ulong p2,ulong p3,ulong p4,ulong* p5,ulong p6) {
	ulong result;
	int x,y;
	
	fprintf(stderr, "RADecode(ulong ctx=0x%0x, ", p1);
	fprintf(stderr, "ulong src=0x%0x,\n", p2);
	fprintf(stderr, "ulong len=0x%0x,", p3);
	fprintf(stderr, "ulong dst=0x%0x,\n", p4);
	fprintf(stderr, "ulong dstcnt=0x%0x, ",p5);
	fprintf(stderr, "ulong p6=%d)\n", p6);
//	hexdump((void*)p1, 44);
	hexdump((void*)p2, p3);
//	hexdump((void*)p4, 80);
//	hexdump((void*)p5, 16);
//	tic();

	    fprintf(stderr,"\n#CRC[%3d]",pkno++);
	for(y=0;y<10;y++){
	    unsigned short crc=0;
	    unsigned char* p=p2;
	    p+=60*y;
	    for(x=0;x<60;x++){
		crc+=p[x]<<(x&7);
	    }
	    fprintf(stderr," %04X",crc);
	}
	    fprintf(stderr,"\n");

	result=(*raDecode)(p1,p2,p3,p4,p5,p6);
//	toc();
//	hexdump((void*)p1, 44);
//	hexdump((void*)p4, 80);
//	hexdump((void*)p5, 16);
	fprintf(stderr, "--> 0x%0x(%d)  decoded: %d  \n\n\n", result, result, p5[0]);
	return result;
}

ulong RAFreeDecoder(ulong p1) {
	ulong result;
	fprintf(stderr, "RAFreeDecoder(ulong p1=0x%0x(%d))\n", p1, p1);
	hexdump((void*)p1, 44);
	result=(*raFreeDecoder)(p1);
	fprintf(stderr, "--> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong RAGetFlavorProperty(ulong p1,ulong p2,ulong p3, ulong p4) {
	ulong result;
	fprintf(stderr, "RAGetFlavorProperty(ulong p1=0x%0x(%d), ", p1, p1);
	fprintf(stderr, "ulong p2=0x%0x(%d),\n", p2, p2);
	fprintf(stderr, "ulong p3=0x%0x(%d), ", p3, p3);
	fprintf(stderr, "ulong p4=0x%0x(%d))\n", p4, p4);
	hexdump((void*)p4/*(void*)(*((void**)p4))*/,p2);
	hexdump((void*)p1, 44);
	tic();
	result=(*raGetFlavorProperty)(p1,p2,p3,p4);
	toc();
	fprintf(stderr, "*p4=0x%0x\n", *((ulong*)p4));
	hexdump((void*)p4/*(void*)(*((void**)p4))*/,p2);
	hexdump((void*)p1, 44);
	fprintf(stderr, "--> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong RAGetNumberOfFlavors(void) {
	ulong result;
	fprintf(stderr, "RAGetNumberOfFlavors(void)\n");
	result=(*raGetNumberOfFlavors)();
	fprintf(stderr, "--> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong RAInitDecoder(ulong p1,ulong p2) {
	ulong result;
	int temp[256];
	unsigned char temp2[256];
	fprintf(stderr, "RAInitDecoder(ulong p1=0x%0x(%d), ", p1, p1);
	fprintf(stderr, "ulong p2=0x%0x(%d))\n", p2, p2);
	hexdump((void*)p2, 4*7);
//	hexdump((void*)p1, 44);
//	memset(temp,0x77,256*4);
//	memcpy(temp,p2,4*7);
//	hexdump((void*)temp[6], 32);

//	memset(temp2,0x77,256);
//	memcpy(temp2,temp[6],16);
//	temp[6]=temp2;
	
	result=(*raInitDecoder)(p1,/*temp*/p2);
//	hexdump((void*)temp[6], 32);
//	memcpy(p2,temp,4*11);
//	hexdump((void*)p1, 44);
	fprintf(stderr, "--> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong RAOpenCodec2(ulong p1) {
	ulong result;
//	loadSyms();
	fprintf(stderr, "RAOpenCodec2(ulong p1=0x%0x(%d)\n", p1, p1);
	hexdump((void*)p1, 44);
	result=(*raOpenCodec2)(p1);
	hexdump((void*)p1, 44);
	fprintf(stderr, "--> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong RASetFlavor(ulong p1) {
	ulong result, numprop=0, result1=0;
	ulong numflavors, flavor;
	unsigned short property;
	fprintf(stderr, "RASetFlavor(ulong p1=0x%0x(%d))\n", p1, p1);
	hexdump((void*)p1, 44);
//	hexdump((void*)p1, 44);
	result=(*raSetFlavor)(p1);
	fprintf(stderr, "--> 0x%0x(%d)\n\n\n", result, result);

#if 0
	fputs("######################## FLAVOR PROPERTIES ###################\n\n", stderr);
	numflavors=raGetNumberOfFlavors2();
	flavor=0;
	while (flavor<numflavors) {
		fprintf(stderr, "************ Flavor %d *************\n\n", flavor);
		numprop=0;
		while (numprop<32) {
			result1=raGetFlavorProperty(p1, flavor, numprop, (ulong)&property);
			fprintf(stderr, "property %d=%d, result=0x%0x\n\n",
				numprop, property, result1);
			hexdump((void*)result1, property);
			numprop++;
		}
		flavor++;
	}

	fputs("######################## FLAVOR PROPERTIES ###################\n\n", stderr);
#endif
	
	return result;
}
