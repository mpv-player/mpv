#include "cfgparser.h"
#include "version.h"
#include "help_mp.h"

int cfg_func_help(struct config *conf)
{
	printf("%s", help_text);
	exit(1);
}
