#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wine/mmreg.h"
#include "wine/avifmt.h"
#include "wine/vfw.h"

#include "aviwrite.h"

static const short h263_format[8][2] = {
    { 0, 0 },
    { 128, 96 },
    { 176, 144 },
    { 352, 288 },
    { 704, 576 },
    { 1408, 1152 },
    { 320, 240 }
};

unsigned char* buffer;
int bufptr=0;
int bitcnt=0;
unsigned char buf=0;

unsigned int x_get_bits(int n){
    unsigned int x=0;
    while(n-->0){
	if(!bitcnt){
	    // fill buff
	    buf=buffer[bufptr++];
	    bitcnt=8;
	}
	//x=(x<<1)|(buf&1);buf>>=1;
	x=(x<<1)|(buf>>7);buf<<=1;
	--bitcnt;
    }
    return x;
}

#define get_bits(xxx,n) x_get_bits(n)
#define get_bits1(xxx) x_get_bits(1)
#define skip_bits(xxx,n) x_get_bits(n)
#define skip_bits1(xxx) x_get_bits(1)

int format;
int width=320;
int height=240;

/* most is hardcoded. should extend to handle all h263 streams */
int h263_decode_picture_header(unsigned char *b_ptr)
{
    int i;
        
    for(i=0;i<16;i++) printf(" %02X",b_ptr[i]); printf("\n");
    
    buffer=b_ptr;
    bufptr=bitcnt=buf=0;

    /* picture header */
    if (get_bits(&s->gb, 22) != 0x20){
	printf("bad picture header\n");
        return -1;
    }
    skip_bits(&s->gb, 8); /* picture timestamp */

    if (get_bits1(&s->gb) != 1){
	printf("bad marker\n");
        return -1;	/* marker */
    }
    if (get_bits1(&s->gb) != 0){
	printf("bad h263 id\n");
        return -1;	/* h263 id */
    }
    skip_bits1(&s->gb);	/* split screen off */
    skip_bits1(&s->gb);	/* camera  off */
    skip_bits1(&s->gb);	/* freeze picture release off */

    format = get_bits(&s->gb, 3);

    if (format != 7) {
        printf("h263_plus = 0  format = %d\n",format);
        /* H.263v1 */
        width = h263_format[format][0];
        height = h263_format[format][1];
	printf("%d x %d\n",width,height);
//        if (!width) return -1;

	printf("pict_type=%d\n",get_bits1(&s->gb));
	printf("unrestricted_mv=%d\n",get_bits1(&s->gb));
#if 1
	printf("SAC: %d\n",get_bits1(&s->gb));
	printf("advanced prediction mode: %d\n",get_bits1(&s->gb));
	printf("PB frame: %d\n",get_bits1(&s->gb));
#else
        if (get_bits1(&s->gb) != 0)
            return -1;	/* SAC: off */
        if (get_bits1(&s->gb) != 0)
            return -1;	/* advanced prediction mode: off */
        if (get_bits1(&s->gb) != 0)
            return -1;	/* not PB frame */
#endif
	printf("qscale=%d\n",get_bits(&s->gb, 5));
        skip_bits1(&s->gb);	/* Continuous Presence Multipoint mode: off */
    } else {
        printf("h263_plus = 1\n");
        /* H.263v2 */
        if (get_bits(&s->gb, 3) != 1){
	    printf("H.263v2 A error\n");
            return -1;
	}
        if (get_bits(&s->gb, 3) != 6){ /* custom source format */
	    printf("custom source format\n");
            return -1;
	}
        skip_bits(&s->gb, 12);
        skip_bits(&s->gb, 3);
	printf("pict_type=%d\n",get_bits(&s->gb, 3) + 1);
//        if (s->pict_type != I_TYPE &&
//            s->pict_type != P_TYPE)
//            return -1;
        skip_bits(&s->gb, 7);
        skip_bits(&s->gb, 4); /* aspect ratio */
        width = (get_bits(&s->gb, 9) + 1) * 4;
        skip_bits1(&s->gb);
        height = get_bits(&s->gb, 9) * 4;
	printf("%d x %d\n",width,height);
        //if (height == 0)
        //    return -1;
	printf("qscale=%d\n",get_bits(&s->gb, 5));
    }

    /* PEI */
    while (get_bits1(&s->gb) != 0) {
        skip_bits(&s->gb, 8);
    }
//    s->f_code = 1;
//    s->width = width;
//    s->height = height;
    return 0;
}

int postable[32768];

int main(){
int c;
unsigned int head=-1;
int pos=0;
int frames=0;
FILE *f=fopen("paulvandykforanangel.viv","rb");
FILE *f2=fopen("GB1.avi","wb");
aviwrite_t* avi=aviwrite_new_muxer();
aviwrite_stream_t* mux=aviwrite_new_stream(avi,AVIWRITE_TYPE_VIDEO);
//unsigned char* buffer=malloc(0x200000);
int i,len;
int v_id=0;
int flag=0;
int flag2=0;
int prefix=0;

mux->buffer_size=0x200000;
mux->buffer=malloc(mux->buffer_size);

mux->h.dwScale=1; 
mux->h.dwRate=10; 

mux->bih=malloc(sizeof(BITMAPINFOHEADER));
mux->bih->biSize=sizeof(BITMAPINFOHEADER);
mux->bih->biPlanes=1;
mux->bih->biBitCount=24;
mux->bih->biCompression=0x6f766976;//      7669766f;
aviwrite_write_header(avi,f2);

/*
c=fgetc(f); if(c) printf("error! not vivo file?\n");
len=0;
while((c=fgetc(f))>=0x80) len+=0x80*(c&0x0F);
len+=c;
printf("hdr1: %d\n",len);
for(i=0;i<len;i++) fgetc(f);
*/

while((c=fgetc(f))>=0){

    printf("%08X  %02X\n",ftell(f),c);

    prefix=0;
    if(c==0x82){
	prefix=1;
	//continue;
	c=fgetc(f);
	printf("%08X  %02X\n",ftell(f),c);
    }

    if(c==0x00){
	// header
	int len=0;
	while((c=fgetc(f))>=0x80) len+=0x80*(c&0x0F);
	len+=c;
	printf("header: 00 (%d)\n",len);
	for(i=0;i<len;i++) fgetc(f);
	continue;
    }

    if((c&0xF0)==0x40){
	// audio
	len=24;
	if(prefix) len=fgetc(f);
	printf("audio: %02X (%d)\n",c,len);
	for(i=0;i<len;i++) fgetc(f);
	continue;
    }
    if((c&0xF0)==0x30){
	// audio
	len=40;
	if(prefix) len=fgetc(f);
	printf("audio: %02X (%d)\n",c,len);
	for(i=0;i<len;i++) fgetc(f);
	continue;
    }
    if(flag2 || (((c&0xF0)==0x10 || (c&0xF0)==0x20) && (c&0x0F)!=(v_id&0xF))){
	// end of frame:
	printf("Frame size: %d\n",mux->buffer_len);
	h263_decode_picture_header(mux->buffer);
	aviwrite_write_chunk(avi,mux,f2,mux->buffer_len,0x10);
	mux->buffer_len=0;
	
	if((v_id&0xF0)==0x10) fprintf(stderr,"hmm. last video packet %02X\n",v_id);
    }
    flag2=0;
    if((c&0xF0)==0x10){
	// 128 byte
	len=128;
	if(prefix) len=fgetc(f);
	printf("video: %02X (%d)\n",c,len);
	fread(mux->buffer+mux->buffer_len,len,1,f);
	mux->buffer_len+=len;
    v_id=c;
	continue;
    }
    if((c&0xF0)==0x20){
	int len=fgetc(f);
	printf("video: %02X (%d)\n",c,len);
	fread(mux->buffer+mux->buffer_len,len,1,f);
	mux->buffer_len+=len;
	flag2=1;
    v_id=c;
	continue;
    }
    printf("error: %02X!\n",c);
    exit(1);
}

if(!width) width=320;
if(!height) height=240;

mux->bih->biWidth=width;
mux->bih->biHeight=height;
mux->bih->biSizeImage=3*width*height;

aviwrite_write_index(avi,f2);
fseek(f2,0,SEEK_SET);
aviwrite_write_header(avi,f2);

}
