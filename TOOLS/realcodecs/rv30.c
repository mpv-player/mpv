/*
   GPL v2 blah blah
   
   This is a small dll that works as a wrapper for the actual cook.so.6.0
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

ulong (*pncOpen)(ulong,ulong);
ulong (*pncClose)(ulong);
ulong (*pncGetUIName)(ulong,ulong);
ulong (*pncGetVersion)(ulong,ulong);
ulong (*pncQueryMediaFormat)(ulong,ulong,ulong,ulong);
ulong (*pncPreferredMediaFormat)(ulong,ulong,ulong,ulong);
ulong (*pncGetMediaFormats)(ulong,ulong,ulong,ulong);
ulong (*pncStreamOpen)(ulong,ulong,ulong);

ulong (*pnsOpenSettingsBox)(ulong,ulong);
ulong (*pnsGetIPNUnknown)(ulong);
ulong (*pnsSetDataCallback)(ulong,ulong,ulong,ulong);
ulong (*pnsSetProperty)(ulong,ulong,ulong);
ulong (*pnsGetProperty)(ulong,ulong,ulong);
ulong (*pnsClose)(ulong);
ulong (*pnsGetStreamHeaderSize)(ulong,ulong);
ulong (*pnsGetStreamHeader)(ulong,ulong);
ulong (*pnsInput)(ulong,ulong,ulong);
ulong (*pnsSetOutputPacketSize)(ulong,ulong,ulong,ulong);
ulong (*pnsGetInputBufferSize)(ulong,ulong);

void (*setDLLAccessPath)(ulong);

int b_dlOpened=0;
void *handle=NULL;

/* exits program when failure */
void loadSyms() {
	fputs("loadSyms()\n", stderr);
	if (!b_dlOpened) {
		char *error;

		fputs("opening dll...\n", stderr);
		handle = dlopen ("/usr/local/RealPlayer8/Codecs/realrv30.so.6.0", RTLD_LAZY);
		if (!handle) {
			fputs (dlerror(), stderr);
			exit(1);
		}

		pncOpen = dlsym(handle, "PNCodec_Open");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(pncOpen): %s\n", error);
			exit(1);
		}
		pncClose = dlsym(handle, "PNCodec_Close");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(pncClose): %s\n", error);
			exit(1);
		}
		pncGetUIName = dlsym(handle, "PNCodec_GetUIName");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(pncGetUIName): %s\n", error);
			exit(1);
		}
		pncGetVersion = dlsym(handle, "PNCodec_GetVersion");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(pncGetVersion): %s\n", error);
			exit(1);
		}
		pncQueryMediaFormat = dlsym(handle, "PNCodec_QueryMediaFormat");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(pncQueryMediaFormat): %s\n", error);
			exit(1);
		}
		pncPreferredMediaFormat = dlsym(handle, "PNCodec_PreferredMediaFormat");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(pncPreferredMediaFormat): %s\n", error);
			exit(1);
		}
		pncGetMediaFormats = dlsym(handle, "PNCodec_GetMediaFormats");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(pncGetMediaFormats): %s\n", error);
			exit(1);
		}
		pncStreamOpen = dlsym(handle, "PNCodec_StreamOpen");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(pncStreamOpen): %s\n", error);
			exit(1);
		}

		pnsOpenSettingsBox = dlsym(handle, "PNStream_OpenSettingsBox");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(pnsOpenSettingsBox): %s\n", error);
			exit(1);
		}
		pnsGetIPNUnknown = dlsym(handle, "PNStream_GetIPNUnknown");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(pnsGetIPNUnknown): %s\n", error);
			exit(1);
		}
		pnsSetDataCallback = dlsym(handle, "PNStream_SetDataCallback");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(pnsSetDataCallback): %s\n", error);
			exit(1);
		}
		pnsSetProperty = dlsym(handle, "PNStream_SetProperty");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(pnsSetProperty): %s\n", error);
			exit(1);
		}
		pnsGetProperty = dlsym(handle, "PNStream_GetProperty");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(pnsGetProperty): %s\n", error);
			exit(1);
		}
		pnsClose = dlsym(handle, "PNStream_Close");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(pnsClose): %s\n", error);
			exit(1);
		}
		pnsGetStreamHeaderSize = dlsym(handle, "PNStream_GetStreamHeaderSize");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(pnsGetStreamHeaderSize): %s\n", error);
			exit(1);
		}
		pnsGetStreamHeader = dlsym(handle, "PNStream_GetStreamHeader");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(pnsGetStreamHeader): %s\n", error);
			exit(1);
		}
		pnsInput = dlsym(handle, "PNStream_Input");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(pnsInput): %s\n", error);
			exit(1);
		}
		pnsSetOutputPacketSize = dlsym(handle, "PNStream_SetOutputPacketSize");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(pnsSetOutputPacketSize): %s\n", error);
			exit(1);
		}
		pnsGetInputBufferSize = dlsym(handle, "PNStream_GetInputBufferSize");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(pnsGetInputBufferSize): %s\n", error);
			exit(1);
		}
		setDLLAccessPath = dlsym(handle, "SetDLLAccessPath");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(SetDLLAccessPath): %s\n", error);
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


ulong PNCodec_Open(ulong p1,ulong p2) {
	ulong result;
	fprintf(stderr, "PNCodec_Open(ulong fourcc=0x%0x(%d), ", p1, p1);
	fprintf(stderr, "PNCMain **pncMain=0x%0x(%d))\n", p2, p2);
//	hexdump((void*)p1, 44);
	tic();
	result=(*pncOpen)(p1,p2);
	toc();
	hexdump((void*)p2, 4);
//	hexdump(*((void**)p2), 0x1278);
	fprintf(stderr, "PNCodec_Open --> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong PNCodec_Close(ulong p1) {
	ulong result;
	fprintf(stderr, "PNCodec_Close(PNCMain *pncMain=0x%0x(%d))\n", p1, p1);
//	hexdump((void*)p1, 44);
	tic();
	result=(*pncClose)(p1);
	toc();
//	hexdump((void*)p1, 44);
	fprintf(stderr, "PNCodec_Close --> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong PNCodec_GetUIName(ulong p1,ulong p2) {
	ulong result;
	fprintf(stderr, "PNCodec_GetUIName(PNCMain *pncMain=0x%0x(%d), ", p1, p1);
	fprintf(stderr, "char **appname=0x%0x(%d))\n", p2, p2);
//	hexdump((void*)p1, 0x1278);
//	hexdump((void*)p2, 128);
	tic();
	result=(*pncGetUIName)(p1,p2);
	toc();
//	hexdump((void*)p1, 0x1278);
//	hexdump((void*)p2, 128);
	fprintf(stderr, "PNCodec_GetUIName --> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong PNCodec_GetVersion(ulong p1,ulong p2) {
	ulong result;
	fprintf(stderr, "PNCodec_GetVersion(ulong p1=0x%0x(%d), ", p1, p1);
	fprintf(stderr, "ulong p2=0x%0x(%d))\n", p2, p2);
//	hexdump((void*)p1, 44);
	tic();
	result=(*pncGetVersion)(p1,p2);
	toc();
//	hexdump((void*)p1, 44);
	fprintf(stderr, "PNCodec_GetVersion --> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong PNCodec_QueryMediaFormat(ulong p1,ulong p2,ulong p3,ulong p4) {
	ulong result;
	fprintf(stderr, "PNCodec_QueryMediaFormat(ulong p1=0x%0x(%d), ", p1, p1);
	fprintf(stderr, "ulong p2=0x%0x(%d),\n\t", p2, p2);
	fprintf(stderr, "ulong p3=0x%0x(%d),", p3, p3);
	fprintf(stderr, "ulong p4=0x%0x(%d),\n", p4, p4);
//	hexdump((void*)p1, 44);
	tic();
	result=(*pncQueryMediaFormat)(p1,p2,p3,p4);
	toc();
//	hexdump((void*)p1, 44);
	fprintf(stderr, "PNCodec_QueryMediaFormat --> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong PNCodec_PreferredMediaFormat(ulong p1,ulong p2,ulong p3,ulong p4) {
	ulong result;
	fprintf(stderr, "PNCodec_PreferredMediaFormat(ulong p1=0x%0x(%d), ", p1, p1);
	fprintf(stderr, "ulong p2=0x%0x(%d),\n\t", p2, p2);
	fprintf(stderr, "ulong p3=0x%0x(%d),", p3, p3);
	fprintf(stderr, "ulong p4=0x%0x(%d),\n", p4, p4);
//	hexdump((void*)p1, 44);
	tic();
	result=(*pncPreferredMediaFormat)(p1,p2,p3,p4);
	toc();
//	hexdump((void*)p1, 44);
	fprintf(stderr, "PNCodec_PreferredMediaFormat --> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong PNCodec_GetMediaFormats(ulong p1,ulong p2,ulong p3,ulong p4) {
	ulong result;
	fprintf(stderr, "PNCodec_GetMediaFormats(ulong p1=0x%0x(%d), ", p1, p1);
	fprintf(stderr, "ulong p2=0x%0x(%d),\n\t", p2, p2);
	fprintf(stderr, "ulong p3=0x%0x(%d),", p3, p3);
	fprintf(stderr, "ulong p4=0x%0x(%d),\n", p4, p4);
//	hexdump((void*)p1, 44);
	tic();
	result=(*pncGetMediaFormats)(p1,p2,p3,p4);
	toc();
//	hexdump((void*)p1, 44);
	fprintf(stderr, "PNCodec_GetMediaFormats --> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong PNCodec_StreamOpen(ulong p1,ulong p2,ulong p3) {
	ulong result;
	fprintf(stderr, "PNCodec_StreamOpen(PNCMain *pncMain=0x%0x(%d), ", p1, p1);
	fprintf(stderr, "PNSMain **pnsMain=0x%0x(%d),\n\t", p2, p2);
	fprintf(stderr, "ulong **p3=0x%0x(%d),\n", p3, p3);
//	hexdump((void*)p1, 0x1278);
//	hexdump((void*)p2, 128);
//	hexdump((void*)p3, 4);
	hexdump(*((void**)p3), 4);
	tic();
	result=(*pncStreamOpen)(p1,p2,p3);
	toc();
//	hexdump((void*)p1, 0x1278);
	hexdump((void*)p2, 4);
//	hexdump((void*)p3, 128);
	hexdump(*((void**)p2), 128);
	hexdump(**((void***)p2), 128);
	fprintf(stderr, "PNCodec_StreamOpen --> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong PNStream_OpenSettingsBox(ulong p1,ulong p2) {
	ulong result;
	fprintf(stderr, "PNStream_OpenSettingsBox(ulong p1=0x%0x(%d), ", p1, p1);
	fprintf(stderr, "ulong p2=0x%0x(%d),\n", p2, p2);
//	hexdump((void*)p1, 44);
	tic();
	result=(*pnsOpenSettingsBox)(p1,p2);
	toc();
//	hexdump((void*)p1, 44);
	fprintf(stderr, "PNStream_OpenSettingsBox --> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong PNStream_GetIPNUnknown(ulong p1) {
	ulong result;
	fprintf(stderr, "PNStream_GetIPNUnknown(ulong p1=0x%0x(%d))\n", p1, p1);
//	hexdump((void*)p1, 44);
	tic();
	result=(*pnsGetIPNUnknown)(p1);
	toc();
//	hexdump((void*)p1, 44);
	fprintf(stderr, "PNStream_GetIPNUnknown --> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong PNStream_SetDataCallback(ulong p1,ulong p2,ulong p3,ulong p4) {
	ulong result;
	int i=0;
	void **pp;
	fprintf(stderr, "PNStream_SetDataCallback(ulong p1=0x%0x(%d), ", p1, p1);
	fprintf(stderr, "ulong p2=0x%0x(%d),\n\t", p2, p2);
	fprintf(stderr, "ulong p3=0x%0x(%d),", p3, p3);
	fprintf(stderr, "ulong p4=0x%0x(%d))\n", p4, p4);
	hexdump((void*)p1, 0x24);
	hexdump((void*)p2, 32);
	hexdump((void*)p3, 4);
	hexdump((void*)p4, 32);
	fprintf(stderr, "content of the callback functions:\n\n");
	while(i<8) {
		hexdump(*((void**)p2+i), (i==0)?32*4:16);
		i++;
	}
	i=0;
	pp=(*(void***)p2);
	fprintf(stderr, "content of the callback functions (first entry):\n\n");
	while(i<15) {
		hexdump(*((void**)pp+i), 32);
		i++;
	}

	tic();
	result=(*pnsSetDataCallback)(p1,p2,p3,p4);
	toc();
	hexdump((void*)p1, 0x24);
//	hexdump((void*)p2, 256);
//	hexdump((void*)p3, 4);
	hexdump(*((void**)p3), 256);
	fprintf(stderr, "PNStream_SetDataCallback --> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong PNStream_SetProperty(ulong p1,ulong p2,ulong p3) {
	ulong result;
	fprintf(stderr, "PNStream_SetProperty(ulong p1=0x%0x(%d), ", p1, p1);
	fprintf(stderr, "ulong p2=0x%0x(%d),\n\t", p2, p2);
	fprintf(stderr, "ulong p3=0x%0x(%d))\n", p3, p3);
	hexdump((void*)p3, 4);
	tic();
	result=(*pnsSetProperty)(p1,p2,p3);
	toc();
//	hexdump((void*)p3, 44);
	fprintf(stderr, "PNStream_SetProperty --> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong PNStream_GetProperty(ulong p1,ulong p2,ulong p3) {
	ulong result;
	fprintf(stderr, "PNStream_GetProperty(ulong p1=0x%0x(%d), ", p1, p1);
	fprintf(stderr, "ulong p2=0x%0x(%d),\n\t", p2, p2);
	fprintf(stderr, "ulong p3=0x%0x(%d))\n", p3, p3);
//	hexdump((void*)p3, 44);
	tic();
	result=(*pnsGetProperty)(p1,p2,p3);
	toc();
	hexdump((void*)p3, 4);
	fprintf(stderr, "PNStream_GetProperty --> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong PNStream_Close(ulong p1) {
	ulong result;
	fprintf(stderr, "PNStream_Close(ulong p1=0x%0x(%d))\n", p1, p1);
//	hexdump((void*)p1, 44);
	tic();
	result=(*pnsClose)(p1);
	toc();
//	hexdump((void*)p1, 44);
	fprintf(stderr, "PNStream_Close --> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong streamHeaderSize=0;

ulong PNStream_GetStreamHeaderSize(ulong p1,ulong p2) {
	ulong result;
	fprintf(stderr, "PNStream_GetStreamHeaderSize(ulong p1=0x%0x(%d), ", p1, p1);
	fprintf(stderr, "ulong p2=0x%0x(%d),\n", p2, p2);
//	hexdump((void*)p2, 44);
	tic();
	result=(*pnsGetStreamHeaderSize)(p1,p2);
	toc();
	hexdump((void*)p2, 4);
	streamHeaderSize=*((ulong *)p2);
	fprintf(stderr, "PNStream_GetStreamHeaderSize --> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong PNStream_GetStreamHeader(ulong p1,ulong p2) {
	ulong result;
	fprintf(stderr, "PNStream_GetStreamHeader(ulong p1=0x%0x(%d), ", p1, p1);
	fprintf(stderr, "ulong p2=0x%0x(%d),\n", p2, p2);
//	hexdump((void*)p2, 44);
	tic();
	result=(*pnsGetStreamHeader)(p1,p2);
	toc();
	hexdump((void*)p2, streamHeaderSize);
	fprintf(stderr, "PNStream_GetStreamHeader --> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong PNStream_Input(ulong p1,ulong p2,ulong p3) {
	ulong result;
	fprintf(stderr, "PNStream_Input(ulong p1=0x%0x(%d), ", p1, p1);
	fprintf(stderr, "ulong p2=0x%0x(%d),\n\t", p2, p2);
	fprintf(stderr, "ulong p3=0x%0x(%d))\n", p3, p3);
	hexdump((void*)p3, 4);
	tic();
	result=(*pnsInput)(p1,p2,p3);
	toc();
//	hexdump((void*)p3, 44);
	fprintf(stderr, "PNStream_Input --> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong PNStream_SetOutputPacketSize(ulong p1,ulong p2,ulong p3,ulong p4) {
	ulong result;
	fprintf(stderr, "PNStream_SetOutputPacketSize(ulong p1=0x%0x(%d), ", p1, p1);
	fprintf(stderr, "ulong p2=0x%0x(%d),\n\t", p2, p2);
	fprintf(stderr, "ulong p3=0x%0x(%d),", p3, p3);
	fprintf(stderr, "ulong p4=0x%0x(%d))\n", p4, p4);
//	hexdump((void*)p1, 44);
	tic();
	result=(*pnsSetOutputPacketSize)(p1,p2,p3,p4);
	toc();
//	hexdump((void*)p1, 44);
	fprintf(stderr, "PNStream_SetOutputPacketSize --> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

ulong PNStream_GetInputBufferSize(ulong p1,ulong p2) {
	ulong result;
	fprintf(stderr, "PNStream_GetInputBufferSize(ulong p1=0x%0x(%d), ", p1, p1);
	fprintf(stderr, "ulong p2=0x%0x(%d))\n", p2, p2);
//	hexdump((void*)p1, 44);
	tic();
	result=(*pnsGetInputBufferSize)(p1,p2);
	toc();
//	hexdump((void*)p1, 44);
	fprintf(stderr, "PNStream_GetInputBufferSize --> 0x%0x(%d)\n\n\n", result, result);
	return result;
}

void  SetDLLAccessPath(ulong p1) {
	fprintf(stderr, "SetDLLAccessPath(ulong p1=0x%0x(%d))\n", p1, p1);
//	hexdump((void*)p1, 44);
	(*setDLLAccessPath)(p1);
//	hexdump((void*)p1, 44);
	fprintf(stderr, "--> void\n\n\n");
}

