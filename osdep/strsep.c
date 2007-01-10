/* strsep implementation for systems that do not have it in libc */

#include <stdio.h>
#include <string.h>

#include "config.h"

char *strsep(char **stringp, const char *delim) {
  char *begin, *end;

  begin = *stringp;
  if(begin == NULL)
    return NULL;

  if(delim[0] == '\0' || delim[1] == '\0') {
    char ch = delim[0];

    if(ch == '\0')
      end = NULL;
    else {
      if(*begin == ch)
        end = begin;
      else if(*begin == '\0')
        end = NULL;
      else
        end = strchr(begin + 1, ch);
    }
  }
  else
    end = strpbrk(begin, delim);

  if(end) {
    *end++ = '\0';
    *stringp = end;
  }
  else
    *stringp = NULL;
 
  return begin;
}
