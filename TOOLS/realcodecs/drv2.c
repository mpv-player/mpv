/*
 * This is a small DLL that works as a wrapper for the actual realdrv2.so.6.0
 * DLL from RealPlayer 8.0.
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
   Assuming that RACloseCodec is the last call.
*/

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/time.h>

typedef unsigned long ulong;

ulong (*rvyuvCustomMessage)(ulong,ulong);
ulong (*rvyuvFree)(ulong);
ulong (*rvyuvHiveMessage)(ulong,ulong);
ulong (*rvyuvInit)(ulong,ulong);
ulong (*rvyuvTransform)(ulong,ulong,ulong,ulong,ulong);
ulong (*rvyuvRNFRUFree)(ulong);
ulong (*rvyuvRNFRUGetFrame)(ulong,ulong,ulong);
ulong (*rvyuvRNFRUInit)(ulong,ulong,ulong);
ulong (*rvyuvRNFRUSetup)(ulong,ulong,ulong,ulong,ulong,ulong,ulong,ulong);

//void (*setDLLAccessPath)(ulong);

int b_dlOpened=0;
void *handle=NULL;

/* exits program when failure */
void loadSyms(void) {
	fputs("loadSyms()\n", stderr);
	if (!b_dlOpened) {
		char *error;

		fputs("opening dll...\n",stderr);
		handle = dlopen ("/usr/local/RealPlayer8/Codecs/realdrv2.so.6.0", RTLD_LAZY);
		if (!handle) {
			fputs (dlerror(), stderr);
			exit(1);
		}

		rvyuvCustomMessage = dlsym(handle, "RV20toYUV420CustomMessage");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(rvyuvCustomMessage): %s\n", error);
			exit(1);
		}
		fprintf(stderr, "RV20toYUV420CustomMessage()=0x%0x\n", rvyuvCustomMessage);
		rvyuvFree = dlsym(handle, "RV20toYUV420Free");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(rvyuvFree): %s\n", error);
			exit(1);
		}
		fprintf(stderr, "RV20toYUV420Free()=0x%0x\n", rvyuvFree);
		rvyuvHiveMessage = dlsym(handle, "RV20toYUV420HiveMessage");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(rvyuvHiveMessage): %s\n", error);
			exit(1);
		}
		fprintf(stderr, "RV20toYUV420HiveMessage()=0x%0x\n", rvyuvHiveMessage);
		rvyuvInit = dlsym(handle, "RV20toYUV420Init");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(rvyuvInit): %s\n", error);
			exit(1);
		}
		fprintf(stderr, "RV20toYUV420Init()=0x%0x\n", rvyuvInit);
		rvyuvTransform = dlsym(handle, "RV20toYUV420Transform");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(rvyuvTransform): %s\n", error);
			exit(1);
		}
		fprintf(stderr, "RV20toYUV420Transform()=0x%0x\n", rvyuvTransform);
		rvyuvRNFRUFree = dlsym(handle, "RV20toYUV420_RN_FRU_Free");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(rvyuvRNFRUFree): %s\n", error);
			exit(1);
		}
		fprintf(stderr, "RV20toYUV420_RN_FRU_Free()=0x%0x\n", rvyuvRNFRUFree);
		rvyuvRNFRUGetFrame = dlsym(handle, "RV20toYUV420_RN_FRU_GetFrame");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(rvyuvRNFRUGetFrame): %s\n", error);
			exit(1);
		}
		fprintf(stderr, "RV20toYUV420_RN_FRU_GetFrame()=0x%0x\n", rvyuvRNFRUGetFrame);
		rvyuvRNFRUInit = dlsym(handle, "RV20toYUV420_RN_FRU_Init");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(rvyuvRNFRUInit): %s\n", error);
			exit(1);
		}
		fprintf(stderr, "RV20toYUV420_RN_FRU_Init()=0x%0x\n", rvyuvRNFRUInit);
		rvyuvRNFRUSetup = dlsym(handle, "RV20toYUV420_RN_FRU_Setup");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(rvyuvRNFRUSetup): %s\n", error);
			exit(1);
		}
		fprintf(stderr, "RV20toYUV420_RN_FRU_Setup()=0x%0x\n", rvyuvRNFRUSetup);

/*		setDLLAccessPath = dlsym(handle, "SetDLLAccessPath");
		if ((error = dlerror()) != NULL)  {
			fprintf (stderr, "dlsym(SetDLLAccessPath): %s\n", error);
			exit(1);
		}
*/		b_dlOpened=1;
	}
}

void closeDll(void) {
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

void tic(void) {
	gettimeofday(&tv1, &tz);
}

void toc(void) {
	long secs, usecs;
	gettimeofday(&tv2, &tz);
	secs=tv2.tv_sec-tv1.tv_sec;
	usecs=tv2.tv_usec-tv1.tv_usec;
	if (usecs<0) {
		usecs+=1000000;
		--secs;
	}
//	fprintf(stderr, "Duration: %ld.%.6lds\n", secs, usecs);
}


static void hexdump(void *pos, int len) {
	unsigned char *cpos=pos, *cpos1;
	int lines=(len+15)>>4;
	while(lines--) {
		int len1=len, i;
		fprintf(stderr, "#R# %0x  ", (int)cpos-(int)pos);
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


ulong RV20toYUV420CustomMessage(ulong* p1,ulong p2) {
	ulong result;
//	ulong *pp1=p1;
	ulong temp[16];
	fprintf(stderr, "#R# => RV20toYUV420CustomMessage(%p,%p) [%ld,%ld,%ld] \n", p1, p2, p1[0],p1[1],p1[2]);

/*
	if(p1[0]!=17) return 0;

	if(p1[1]!=1) return 0;

	if(p1[0]==105) return 0;
	if(p1[0]==3) return 0;
	if(p1[0]==18) return 0;
	if(p1[0]==30) return 0;
*/

	if(p1[0]==0x24){
	    hexdump(p1[2],64);
	    memset(temp,0x77,16*4);
	    memcpy(temp,p1[2],16);
	    p1[2]=temp;
	} else {
	    switch(p1[0]){
	    case 17:
		if(p1[1]==1)
		break;
	    case 18:
	    case 30:
	    case 3:
	    default:
		return 0;
	    }
	}
//	fprintf(stderr, "ulong p2=0x%0lx(%ld))\n", p2, p2);
//	hexdump((void*)p1, 12);
//	if (pp1[0]==0x24) {
//		hexdump((void*)(pp1[2]),128);
//	}
//	tic();
	result=(*rvyuvCustomMessage)(p1,p2);
//	toc();
	fprintf(stderr, "#R# <= RV20toYUV420CustomMessage --> 0x%0lx(%ld)\n", result, result);
	return result;
}

ulong RV20toYUV420Free(ulong p1) {
	ulong result;
	fprintf(stderr, "RV20toYUV420Free(ulong p1=0x%0lx(%ld))\n", p1, p1);
//	hexdump((void*)p1, 44);
	tic();
	result=(*rvyuvFree)(p1);
	toc();
//	hexdump((void*)p1, 44);
	fprintf(stderr, "RV20toYUV420Free --> 0x%0lx(%ld)\n\n\n", result, result);
	return result;
}

char h_temp[32768];

ulong RV20toYUV420HiveMessage(ulong *p1,ulong p2) {
	ulong result;
	fprintf(stderr, "#R# RV20toYUV420HiveMessage(%p,%p)\n", p1, p2);
//	    p1->constant,p1->width,p1->height,p1->format1,p1->format2);
//	fprintf(stderr, "ulong p2=0x%0lx(%ld))\n", p2, p2);
//	hexdump((void*)p1, sizeof(struct init_data));

	fprintf(stderr,">HIVE %ld %p\n",p1[0],p1[1]);

	if(p1[0]!=0) return 0;

	p1[1]=0x10000000;
	return 0;

//	fprintf(stderr,"COPY INIT DATA!\n");
	memset(h_temp,0x77,1000);
	memcpy(h_temp,p1,4);
//	fprintf(stderr,"COPY OK!\n");

//	tic();
//	result=(*rvyuvHiveMessage)(p1,p2);
	result=(*rvyuvHiveMessage)(h_temp,p2);
//	toc();

	fprintf(stderr,"COPY INIT DATA!\n");
	memcpy(p1,h_temp,8);
	fprintf(stderr,"COPY OK!\n");

	memset(h_temp,0x77,1000);

//	p1[0]=0;
//	p1[1]=0x20000000;

	fprintf(stderr,"<HIVE %ld %p\n",p1[0],p1[1]);

//	hexdump((void*)p1, sizeof(struct init_data));
//	hexdump((void*)p1, 8);
	fprintf(stderr, "#R# RV20toYUV420HiveMessage --> 0x%0lx(%ld)\n\n", result, result);
	return result;
}

struct init_data {
	short constant; //=0xb;
	short width, height;
	short x1,x2,x3;
	// 12
	ulong format1;
	long  x4;
	ulong format2;
//	long unknown[32];
};

static char i_temp[32768];

ulong RV20toYUV420Init(ulong p1,ulong p2) {
	ulong result;
	fprintf(stderr, "#R# RV20toYUV420Init(ulong p1=0x%0lx(%ld), ", p1, p1);
	fprintf(stderr, "ulong p2=0x%0lx(%ld))\n", p2, p2);

	fprintf(stderr,"COPY INIT DATA!\n");
	memcpy(i_temp,p1,24);
	p1=i_temp;
	fprintf(stderr,"COPY OK!\n");

	hexdump((void*)p1, 24);
	tic();
	result=(*rvyuvInit)(p1,p2);
	toc();
	hexdump((void*)p1, 24);

	memset(i_temp,0x77,1000);

//	hexdump(*((void**)p2), 512);
	fprintf(stderr, "#R# RV20toYUV420Init --> 0x%0lx(%ld)\n\n\n", result, result);
	return result;
}

unsigned long build_crc(unsigned char *pch, unsigned long len) {
	unsigned long crc=0, a;
//	unsigned long b;
	// it's not the real crc function, but so what...
	while (len--) {
		a=*(pch++);
//		a=a+(a<<6);
//		a^=0x555;
//		b=(crc>>29)&7;
//		crc=((crc<<3)+b)^a;
		crc^=a;
	}
	return crc;
}

#define MIN(a,b) ((a)<(b)?(a):(b))

// p1=input data (stream)
// p2=output buffer
// p3=input struct
// p4=output struct
// p5=rvyuv_main
ulong RV20toYUV420Transform(ulong p1,ulong p2,ulong p3,ulong p4,ulong p5) {

//result=RV20toYUV420Transform(char *input_stream, char *output_data,
//	struct transin *, struct transout *, struct rvyuvMain *);

	ulong result;
	ulong *pp3=p3;
	ulong *pp4=p4;
	void *v;
	ulong temp[128];
	int i;

	unsigned long crc_src, crc0;
//	unsigned long len, crc1, crc2;
	unsigned char *pch=(char *)p1;
	fprintf(stderr, "#R# RV20toYUV420Transform(in=%p,out=%p,tin=%p,tout=%p,yuv=%p)\n",p1,p2,p3,p4,p5);
	// input data, length=*p3
//	hexdump((void*)p1, /*MIN(64,*/ *((ulong*)p3) /*)*/ );
//	v=p5;
//	v+=0x3c;
//	v=*((void **)v);
//	pp3=v;
//	len=pp3[3]*pp3[4]*3/2;
//	pch=p2;
//	while(--len) *(pch++)=0;
//	hexdump((char*)p2, 64);
//	hexdump((void*)p3, 32);
//	hexdump((void*)p5, 64);
//	pp3=p3;
//	if (pp3[3]>1024) {
//		hexdump((void*)(pp3[3]),32);
//		pp3=pp3[3];
//	}

	pp3=p3;
	// it's not the real crc function, but so what...
	pch=p1;
	crc_src=build_crc(pch, pp3[0]);

	pp4=pp3[3];
	fprintf(stderr,"transin1[%p]: {%ld/%ld} ",pp4,pp3[2],pp3[0]);
//	pp4[0],pp4[1],pp4[2],pp4[3],
//	pp4[4],pp4[5],pp4[6],pp4[7]);

	memset(temp,0x77,128*4);

	memcpy(temp,pp4,8*(pp3[2]+1));
	for(i=0;i<=pp3[2];i++){
	    fprintf(stderr," %p(%ld)",temp[i*2],temp[i*2+1]);
	}
        fprintf(stderr,"\n");


	pp3[3]=pp4=temp;

//	pp4[2]=
//	pp4[3]=
//	pp4[4]=NULL;

	//pp4[6]=pp4[5];

	v=p5;
/*	fprintf(stderr, "rvyuvMain=0x%0x\n", v);
	v+=0x3c;
	v=*((void **)v);
	fprintf(stderr, "[$+3ch]=0x%0x\n", v);
	hexdump(v, 512);
	v+=0x60;
	v=*((void **)v);
	fprintf(stderr, "[$+60h]=0x%0x\n", v);
	hexdump(v, 512);
	v+=0x28;
	v=*((void **)v);
	fprintf(stderr, "[$+28h]=0x%0x\n", v);
	hexdump(v, 512);
*/
/*	v+=0x178;
	hexdump(v, 16);
	v=*((void **)v);
	if (v>0x8000000) {
		fprintf(stderr, "[$+178h]=0x%0x\n", v);
		hexdump(v, 128);
	}
*/
//	tic();
	result=(*rvyuvTransform)(p1,p2,p3,p4,p5);
//	toc();

	crc0=build_crc(p2, 176*144);
//	crc1=build_crc(p2+pp4[3]*pp4[4]/2, pp4[3]*pp4[4]/2);
//	crc2=build_crc(p2+pp4[3]*pp4[4], pp4[3]*pp4[4]/2);

//	pp3=p3;
	// TRANSFORM: <timestamp> <numblocks> <len> <crc_src> <crc_dest> <p4[4]>
//	fprintf(stderr, "TRAFO:\t%ld\t%ld\t%ld\t%.8lX\t%.8lX\t%ld\n",
//		pp3[5], pp3[2], pp3[0], crc_src, crc0, pp3[4]);
	fprintf(stderr, "#R# Decode: %ld(%ld) [%08lX] pts=%ld -> %ld [%08lX]\n",
	    pp3[0],pp3[2],crc_src,pp3[5],
	    result,crc0);

	// output
//	hexdump((char*)p2, /*64*/ pp4[3]*pp4[4]/2);
//	hexdump((void*)p4, 20);
//	hexdump((void*)p5, 512);
//	fprintf(stderr, "RV20toYUV420Transform --> 0x%0lx(%ld)\n\n\n", result, result);
	return result;
}

ulong RV20toYUV420_RN_FRU_Free(ulong p1) {
	ulong result;
	fprintf(stderr, "RV20toYUV420_RN_FRU_Free(ulong p1=0x%0lx(%ld))\n", p1, p1);
	tic();
	result=(*rvyuvRNFRUFree)(p1);
	toc();
	fprintf(stderr, "RV20toYUV420_RN_FRU_Free --> 0x%0lx(%ld)\n\n\n", result, result);
	return result;
}

ulong RV20toYUV420_RN_FRU_GetFrame(ulong p1,ulong p2,ulong p3) {
	ulong result;
//	fprintf(stderr, "RV20toYUV420_RN_FRU_GetFrame(ulong p1=0x%0lx(%ld), ", p1, p1);
//	fprintf(stderr, "ulong p2=0x%0lx(%ld),\n\t", p2, p2);
//	fprintf(stderr, "ulong p3=0x%0lx(%ld))\n", p3, p3);
//	hexdump((void*)p1, 44);
	tic();
	result=(*rvyuvRNFRUGetFrame)(p1,p2,p3);
	toc();
//	hexdump((void*)p1, 44);
//	fprintf(stderr, "RV20toYUV420_RN_FRU_GetFrame --> 0x%0lx(%ld)\n\n\n", result, result);
	return result;
}

ulong RV20toYUV420_RN_FRU_Init(ulong p1,ulong p2,ulong p3) {
	ulong result;
//	fprintf(stderr, "RV20toYUV420_RN_FRU_Init(ulong p1=0x%0lx(%ld), ", p1, p1);
//	fprintf(stderr, "ulong p2=0x%0lx(%ld),\n\t", p2, p2);
//	fprintf(stderr, "ulong p3=0x%0lx(%ld))\n", p3, p3);
//	hexdump((void*)p1, 44);
	tic();
	result=(*rvyuvRNFRUInit)(p1,p2,p3);
	toc();
//	hexdump((void*)p1, 44);
//	fprintf(stderr, "RV20toYUV420_RN_FRU_Init --> 0x%0lx(%ld)\n\n\n", result, result);
	return result;
}

ulong RV20toYUV420_RN_FRU_Setup(ulong p1,ulong p2,ulong p3,ulong p4,
	ulong p5,ulong p6,ulong p7,ulong p8) {
	ulong result;
//	fprintf(stderr, "RV20toYUV420_RN_FRU_Setup(ulong p1=0x%0lx(%ld), ", p1, p1);
//	fprintf(stderr, "ulong p2=0x%0lx(%ld),\n\t", p2, p2);
//	fprintf(stderr, "ulong p3=0x%0lx(%ld), ", p3, p3);
//	fprintf(stderr, "ulong p4=0x%0lx(%ld),\n\t", p4, p4);
//	fprintf(stderr, "ulong p5=0x%0lx(%ld), ", p5, p5);
//	fprintf(stderr, "ulong p6=0x%0lx(%ld),\n\t", p6, p6);
//	fprintf(stderr, "ulong p7=0x%0lx(%ld), ", p7, p7);
//	fprintf(stderr, "ulong p8=0x%0lx(%ld))\n", p8, p8);
//	hexdump((void*)p1, 44);
	tic();
	result=(*rvyuvRNFRUSetup)(p1,p2,p3,p4,p5,p6,p7,p8);
	toc();
//	hexdump((void*)p1, 44);
//	fprintf(stderr, "RV20toYUV420_RN_FRU_Setup --> 0x%0lx(%ld)\n\n\n", result, result);
	return result;
}

/*void  SetDLLAccessPath(ulong p1) {
	fprintf(stderr, "SetDLLAccessPath(ulong p1=0x%0lx(%ld))\n", p1, p1);
	hexdump((void*)p1, 44);
	(*setDLLAccessPath)(p1);
	hexdump((void*)p1, 44);
	fprintf(stderr, "--> void\n\n\n");
}
*/
