#include "common1428.h"
#include "decode288.h"
#include "tables288.h"

/* Initialize internal variable structure */
Real_288 *init_288(void)
{
	Real_internal *glob;

	if ((glob=malloc(sizeof(Real_internal))))
	  memset(glob,0,sizeof(Real_internal));

	return (Real_288 *)glob;
}

/* Free internal variable structure */
void free_288(Real_288 *global)
{
	if (!global) return;
	free(global);
}

/* Unlike the 14.4 format, 28.8 blocks are interleaved */
/* to dilute the effects of transmission errors */
void deinterleave(unsigned char *in, unsigned char *out, unsigned int size)
{
  unsigned int x=0,y=0,z=0;
  if (size>=38) z=size-38;
  else return;

  while (x<=z)
  {
    memcpy(out+y,in+x,38);
    x+=38;y+=456;
    if (y>z) y-=z;
  }
}

/* Decode a block (celp) */
void decode_288(Real_288 *global, unsigned char *in, signed short int *out)
{
  int x,y;
  unsigned short int buffer[32];
  Real_internal *glob;

  if (!global) return;
  glob = (Real_internal *)global;

  unpack(buffer,in,32);
  for (x=0;x<32;x++)
  {
    glob->phasep=(glob->phase=x&7)*5;
    decode(glob,buffer[x]);
    for (y=0;y<5;*(out++)=8*glob->output[glob->phasep+(y++)]);
    if (glob->phase==3) update(glob);
  }
}

/* initial decode */
static void unpack(unsigned short *tgt, unsigned char *src, int len)
{
  int x,y,z;
  int n,temp;
  int buffer[38];

  for (x=0;x<len;tgt[x++]=0)
    buffer[x]=9+(x&1);

  for (x=y=z=0;x<38;x++) {
    n=buffer[y]-z;
    temp=src[x];
    if (n<8) temp&=255>>(8-n);
    tgt[y]+=temp<<z;
    if (n<=8) {
      tgt[++y]+=src[x]>>n;
      z=8-n;
    } else z+=8;
  }
}

static void update(Real_internal *glob)
{
  int x,y;
  float buffer1[40],temp1[37];
  float buffer2[8],temp2[11];

  for (x=0,y=glob->phasep+5;x<40;buffer1[x++]=glob->output[(y++)%40]);
  co(36,40,35,buffer1,temp1,glob->st1a,glob->st1b,table1);
  if (pred(temp1,glob->st1,36))
    colmult(glob->pr1,glob->st1,table1a,36);

  for (x=0,y=glob->phase+1;x<8;buffer2[x++]=glob->history[(y++)%8]);
  co(10,8,20,buffer2,temp2,glob->st2a,glob->st2b,table2);
  if (pred(temp2,glob->st2,10))
    colmult(glob->pr2,glob->st2,table2a,10);
}

/* Decode and produce output */
static void decode(Real_internal *glob, unsigned int input)
{
  unsigned int x,y;
  float f;
  double sum,sumsum;
  float *p1,*p2;
  float buffer[5];
  const float *table;

  for (x=36;x--;glob->sb[x+5]=glob->sb[x]);
  for (x=5;x--;) {
    p1=glob->sb+x;p2=glob->pr1;
    for (sum=0,y=36;y--;sum-=(*(++p1))*(*(p2++)));
    glob->sb[x]=sum;
  }

  f=amptable[input&7];
  table=codetable+(input>>3)*5;

  /* convert log and do rms */
  for (sum=32,x=10;x--;sum-=glob->pr2[x]*glob->lhist[x]);
  if (sum<0) sum=0; else if (sum>60) sum=60;

  sumsum=exp(sum*0.1151292546497)*f;	/* pow(10.0,sum/20)*f */
  for (sum=0,x=5;x--;) { buffer[x]=table[x]*sumsum; sum+=buffer[x]*buffer[x]; }
  if ((sum/=5)<1) sum=1;

  /* shift and store */
  for (x=10;--x;glob->lhist[x]=glob->lhist[x-1]);
  *glob->lhist=glob->history[glob->phase]=10*log10(sum)-32;

  for (x=1;x<5;x++) for (y=x;y--;buffer[x]-=glob->pr1[x-y-1]*buffer[y]);

  /* output */
  for (x=0;x<5;x++) {
    f=glob->sb[4-x]+buffer[x];
    if (f>4095) f=4095; else if (f<-4095) f=-4095;
    glob->output[glob->phasep+x]=glob->sb[4-x]=f;
  }
}

/* column multiply */
static void colmult(float *tgt, float *m1, const float *m2, int n)
{
  while (n--)
    *(tgt++)=(*(m1++))*(*(m2++));
}

static int pred(float *in, float *tgt, int n)
{
  int x,y;
  float *p1,*p2;
  double f0,f1,f2;
  float temp;

  if (in[n]==0) return 0;
  if ((f0=*in)<=0) return 0;

  for (x=1;;x++) {
    if (n<x) return 1;

    p1=in+x;
    p2=tgt;
    f1=*(p1--);
    for (y=x;--y;f1+=(*(p1--))*(*(p2++)));

    p1=tgt+x-1;
    p2=tgt;
    *(p1--)=f2=-f1/f0;
    for (y=x>>1;y--;) {
      temp=*p2+*p1*f2;
      *(p1--)+=*p2*f2;
      *(p2++)=temp;
    }
    if ((f0+=f1*f2)<0) return 0;
  }
}

static void co(int n, int i, int j, float *in, float *out, float *st1, float *st2, const float *table)
{
  int a,b,c;
  unsigned int x;
  float *fp,*fp2;
  float buffer1[37];
  float buffer2[37];
  float work[111];

  /* rotate and multiply */
  c=(b=(a=n+i)+j)-i;
  fp=st1+i;
  for (x=0;x<b;x++) {
    if (x==c) fp=in;
    work[x]=*(table++)*(*(st1++)=*(fp++));
  }
  
  prodsum(buffer1,work+n,i,n);
  prodsum(buffer2,work+a,j,n);

  for (x=0;x<=n;x++) {
    *st2=*st2*(0.5625)+buffer1[x];
    out[x]=*(st2++)+buffer2[x];
  }
  *out*=1.00390625; /* to prevent clipping */
}

/* product sum (lsf) */
static void prodsum(float *tgt, float *src, int len, int n)
{
  unsigned int x;
  float *p1,*p2;
  double sum;

  while (n>=0)
  {
    p1=(p2=src)-n;
    for (sum=0,x=len;x--;sum+=(*p1++)*(*p2++));
    tgt[n--]=sum;
  }
}
