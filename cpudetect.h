#ifndef CPUDETECT_H
#define CPUDETECT_H

#define CPUTYPE_I386	3
#define CPUTYPE_I486	4
#define CPUTYPE_I586	5
#define CPUTYPE_I686    6

typedef struct cpucaps_s {
	int cpuType;
	int cpuStepping;
	int hasMMX;
	int hasMMX2;
	int has3DNow;
	int has3DNowExt;
	int hasSSE;
	int hasSSE2;
	int isX86;
} CpuCaps;

extern CpuCaps gCpuCaps;

void GetCpuCaps(CpuCaps *caps);

/* returned value is malloc()'ed so free() it after use */
char *GetCpuFriendlyName(unsigned int regs[], unsigned int regs2[]);

#endif /* !CPUDETECT_H */

