#ifndef MPLAYER_CPUDETECT_H
#define MPLAYER_CPUDETECT_H

#include "config.h"

#define CPUTYPE_I386	3
#define CPUTYPE_I486	4
#define CPUTYPE_I586	5
#define CPUTYPE_I686    6

#include "libavutil/x86_cpu.h"

typedef struct cpucaps_s {
	int cpuType;
	int cpuModel;
	int cpuStepping;
	int hasMMX;
	int hasMMX2;
	int has3DNow;
	int has3DNowExt;
	int hasSSE;
	int hasSSE2;
	int hasSSE3;
	int hasSSSE3;
	int hasSSE4a;
	int isX86;
	unsigned cl_size; /* size of cache line */
        int hasAltiVec;
	int hasTSC;
} CpuCaps;

extern CpuCaps gCpuCaps;

void GetCpuCaps(CpuCaps *caps);

/* returned value is malloc()'ed so free() it after use */
char *GetCpuFriendlyName(unsigned int regs[], unsigned int regs2[]);

#endif /* MPLAYER_CPUDETECT_H */
