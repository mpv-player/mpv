/*
   fastmemcpybench.c used to benchmark fastmemcpy.h code from libvo.
     
   Note: this code can not be used on PentMMX-PII because they contain
   a bug in rdtsc. For Intel processors since P6(PII) rdpmc should be used
   instead. For PIII it's disputable and seems bug was fixed but I don't
   tested it.
*/

#include <stdio.h>

#include "../libvo/fastmemcpy.h"

#define ARR_SIZE 100000
//#define ARR_SIZE 1000000

static inline unsigned long long int read_tsc( void )
{
  unsigned long long int retval;
  __asm __volatile ("rdtsc":"=A"(retval)::"memory");
  return retval;
}

unsigned char arr1[ARR_SIZE],arr2[ARR_SIZE];

int main( void )
{
  unsigned long long int v1,v2;
  unsigned char * marr1,*marr2;
  marr1 = &arr1[1];
  marr2 = &arr2[3];
  v1 = read_tsc();
  memcpy(marr1,marr2,ARR_SIZE-4);
  v2 = read_tsc();
  printf("v1 = %llu v2 = %llu v2-v1=%llu\n",v1,v2,v2-v1);
  return 0;
}
