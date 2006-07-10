#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#include "mp_msg.h"
#include "ass_utils.h"

int mystrtoi(char** p, int base, int* res)
{
	char* start = *p;
	*res = strtol(*p, p, base);
	if (*p != start) return 1;
	else return 0;
}

int mystrtou32(char** p, int base, uint32_t* res)
{
	char* start = *p;
	*res = strtoll(*p, p, base);
	if (*p != start) return 1;
	else return 0;
}

int mystrtod(char** p, double* res)
{
	char* start = *p;
	*res = strtod(*p, p);
	if (*p != start) return 1;
	else return 0;
}

int strtocolor(char** q, uint32_t* res)
{
	uint32_t color = 0;
	int result;
	char* p = *q;
	
	if (*p == '&') ++p; 
	else mp_msg(MSGT_GLOBAL, MSGL_DBG2, "suspicious color format: \"%s\"\n", p);
	
	if (*p == 'H' || *p == 'h') { 
		++p;
		result = mystrtou32(&p, 16, &color);
	} else {
		result = mystrtou32(&p, 0, &color);
	}
	
	{
		unsigned char* tmp = (unsigned char*)(&color);
		unsigned char b;
		b = tmp[0]; tmp[0] = tmp[3]; tmp[3] = b;
		b = tmp[1]; tmp[1] = tmp[2]; tmp[2] = b;
	}
	if (*p == '&') ++p;
	*q = p;

	*res = color;
	return result;
}

