#ifdef ARCH_X86

#define CPUTYPE_I386	0
#define CPUTYPE_I486	1
#define CPUTYPE_I586	2

typedef struct cpucaps_s {
	int cpuType;
	int hasMMX;
	int hasMMX2;
	int has3DNow;
	int has3DNowExt;
	int hasSSE;
	int hasSSE2;
} CpuCaps;

extern CpuCaps gCpuCaps;

void GetCpuCaps( CpuCaps *caps);

#endif /* ARCH_X86 */
