/* setenv implementation for systems lacking it. */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#ifndef MP_DEBUG
  #define NDEBUG
#endif
#include <assert.h>

int setenv(const char *name, const char *val, int overwrite)
{
  int len  = strlen(name) + strlen(val) + 2;
  char *env = malloc(len);
  if (!env) { return -1; }

  assert(overwrite != 0);

  strcpy(env, name);
  strcat(env, "=");
  strcat(env, val);
  putenv(env);

  return 0;
}
