#ifdef ARCH_X86

#define CPUTYPE_I386	3
#define CPUTYPE_I486	4
#define CPUTYPE_I586	5
#define CPUTYPE_I686    6

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

void GetCpuCaps(CpuCaps *caps);
char *GetCpuFriendlyName(unsigned int regs[], unsigned int regs2[]);

#endif /* ARCH_X86 */

