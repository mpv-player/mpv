#ifndef __ASS_UTILS_H__
#define __ASS_UTILS_H__

int mystrtoi(char** p, int base, int* res);
int mystrtou32(char** p, int base, uint32_t* res);
int mystrtod(char** p, double* res);
int strtocolor(char** q, uint32_t* res);
#endif

