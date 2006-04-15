#ifdef SYS_DARWIN
#define ASMALIGN8  ".align 3\n\t"
#define ASMALIGN16 ".align 4\n\t"
#else
#define ASMALIGN8  ".balign 8\n\t"
#define ASMALIGN16 ".balign 16\n\t"
#endif
