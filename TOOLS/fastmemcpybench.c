/*
   fastmemcpybench.c used to benchmark fastmemcpy.h code from libvo.
     
   Note: this code can not be used on PentMMX-PII because they contain
   a bug in rdtsc. For Intel processors since P6(PII) rdpmc should be used
   instead. For PIII it's disputable and seems bug was fixed but I don't
   tested it.
*/

/* According to Uoti this code is broken. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <inttypes.h>

//#define ARR_SIZE 100000
#define ARR_SIZE (1024*768*2)

#ifdef CONFIG_MGA

#include "drivers/mga_vid.h"

static mga_vid_config_t mga_vid_config;
static unsigned char* frame=NULL;
static int f;

static int mga_init(void)
{
	f = open("/dev/mga_vid",O_RDWR);
	if(f == -1)
	{
		fprintf(stderr,"Couldn't open /dev/mga_vid\n"); 
		return -1;
	}

	mga_vid_config.num_frames=1;
        mga_vid_config.frame_size=ARR_SIZE;
        mga_vid_config.format=MGA_VID_FORMAT_YUY2;

        mga_vid_config.colkey_on=0;
	mga_vid_config.src_width = 640;
	mga_vid_config.src_height= 480;
	mga_vid_config.dest_width = 320;
	mga_vid_config.dest_height= 200;
	mga_vid_config.x_org= 0;
	mga_vid_config.y_org= 0;
	
	mga_vid_config.version=MGA_VID_VERSION;
	if (ioctl(f,MGA_VID_CONFIG,&mga_vid_config))
	{
		perror("Error in mga_vid_config ioctl()");
                printf("Your mga_vid driver version is incompatible with this MPlayer version!\n");
                exit(1);
	}
	ioctl(f,MGA_VID_ON,0);

	frame = (char*)mmap(0,mga_vid_config.frame_size*mga_vid_config.num_frames,PROT_WRITE,MAP_SHARED,f,0);
        if(!frame){
          printf("Can't mmap mga frame\n");
          exit(1);
        }

	//clear the buffer
	//memset(frames[0],0x80,mga_vid_config.frame_size*mga_vid_config.num_frames);

  return 0;

}

#endif

// Returns current time in microseconds
static unsigned int GetTimer(void)
{
  struct timeval tv;
  struct timezone tz;
//  float s;
  gettimeofday(&tv,&tz);
//  s=tv.tv_usec;s*=0.000001;s+=tv.tv_sec;
  return tv.tv_sec * 1000000 + tv.tv_usec;
}  

static inline unsigned long long int read_tsc( void )
{
  unsigned long long int retval;
  __asm__ volatile ("rdtsc":"=A"(retval)::"memory");
  return retval;
}

unsigned char  __attribute__((aligned(4096)))arr1[ARR_SIZE],arr2[ARR_SIZE];

int main( void )
{
  unsigned long long int v1,v2;
  unsigned char * marr1,*marr2;
  int i;
  unsigned int t;
#ifdef CONFIG_MGA
  mga_init();
  marr1 = &frame[3];
#else
  marr1 = &arr1[3];
#endif
  marr2 = &arr2[9];

  for(i=0; i<ARR_SIZE-16; i++) marr1[i] = marr2[i] = i;

  t=GetTimer();
  v1 = read_tsc();
  for(i=0;i<100;i++) memcpy(marr1,marr2,ARR_SIZE-16);
  v2 = read_tsc();
  t=GetTimer()-t;
  // ARR_SIZE*100/(1024*1024)/(t/1000000) = ARR_SIZE*95.36743/t
  printf(NAME": cpu clocks=%llu = %dus  (%5.3ffps)  %5.1fMB/s\n",v2-v1,t,100000000.0f/(float)t,(float)ARR_SIZE*95.36743f/(float)t);
  return 0;
}
