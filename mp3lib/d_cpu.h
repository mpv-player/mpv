
// --------------------------------------------------------------------------
//  Cpu function detect by Pontscho/fresh!mindworkz
// --------------------------------------------------------------------------

#ifndef __MY_CPUIDENT
#define __MY_CPUIDENT

unsigned int _CpuID;
unsigned int _i586;
unsigned int _3dnow;

extern unsigned long CpuDetect( void );
extern unsigned long ipentium( void );
extern unsigned long a3dnow( void );

#endif

