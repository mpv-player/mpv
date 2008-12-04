#ifndef MPLAYER_WRAPPER_H
#define MPLAYER_WRAPPER_H

#include <inttypes.h>

typedef struct {
  uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
} reg386_t;

typedef int (*wrapper_func_t)(void *stack_base, int stack_size, reg386_t *reg,  uint32_t *flags);

extern wrapper_func_t report_entry, report_ret;

extern void (*wrapper_target)(void);

int wrapper(void);
int null_call(void);

#endif /* MPLAYER_WRAPPER_H */

