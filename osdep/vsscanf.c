#include "config.h"

/* system has no vsscanf.  try to provide one */

#include <stdio.h>
#include <stdarg.h>

int
vsscanf(const char *str, const char *format, va_list ap)
{
    /* XXX: can this be implemented in a more portable way? */
    long p1 = va_arg(ap, long);
    long p2 = va_arg(ap, long);
    long p3 = va_arg(ap, long);
    long p4 = va_arg(ap, long);
    long p5 = va_arg(ap, long);
    return sscanf(str, format, p1, p2, p3, p4, p5);
}
