#include <stdio.h>

static const short h263_format[8][2] = {
    { 0, 0 },
    { 128, 96 },
    { 176, 144 },
    { 352, 288 },
    { 704, 576 },
    { 1408, 1152 },
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

/* most is hardcoded. should extend to handle all h263 streams */
int h263_decode_picture_header(unsigned char *b_ptr)
{
    int format, width, height;
    
    buffer=b_ptr;
    bufptr=bitcnt=buf=0;

    /* picture header */
    if (get_bits(&s->gb, 22) != 0x20)
        return -1;
    skip_bits(&s->gb, 8); /* picture timestamp */

    if (get_bits1(&s->gb) != 1)
        return -1;	/* marker */
    if (get_bits1(&s->gb) != 0)
        return -1;	/* h263 id */
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
        if (!width)
            return -1;

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
        if (get_bits(&s->gb, 3) != 1)
            return -1;
        if (get_bits(&s->gb, 3) != 6) /* custom source format */
            return -1;
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
        if (height == 0)
            return -1;
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


int main(){
int c;
unsigned int head=-1;
int pos=0;

while((c=getchar())>=0){
    ++pos;
    head=(head<<8)|c;
    if((head&0xFFFFFF)==0x80){
        unsigned char buf[33];
	int i;
	buf[0]=buf[1]=0; buf[2]=0x80;
	printf("%08X: 00 00 80",pos);
	for(i=0;i<30;i++){
	    c=getchar();++pos;
	    printf(" %02X",c);
	    buf[3+i]=c;
	}
	printf("\n");
	h263_decode_picture_header(buf);
    }
}


}
