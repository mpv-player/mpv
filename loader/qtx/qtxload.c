/* to compile: gcc -o qtxload qtxload.c ../libloader.a -lpthread -ldl -ggdb ../../cpudetect.o */

#include <stdio.h>
#include "qtxsdk/components.h"

#define DEF_DISPATCHER(name) ComponentResult (*##name)(struct ComponentParameters *, void **)

/* ilyen egy sima komponens */
ComponentResult ComponentDummy(
    struct ComponentParameters *params,
    void **globals,
    DEF_DISPATCHER(ComponentDispatch))
{
    printf("ComponentDummy(params: %p, globals: %p, dispatcher: %p) called!\n",
	params, globals, ComponentDispatch);
    printf(" Dummy: global datas: %p\n", *globals);
    printf(" Dummy: returning 0\n");
    return(0);
}

char *get_path()
{
    return(".");
}

main(int argc, char *argv[])
{
    void *handler;
    void *dispatcher;

    Setup_LDT_Keeper();
    
    handler = expLoadLibraryA("/usr/lib/win32/qtx/test.qtx");
    dispatcher = GetProcAddress(handler, "CDComponentDispatch");
    printf("handler: %p, dispatcher: %p\n", handler, dispatcher);
    
    {
	ComponentResult ret;
	int (*dispatcher_func)(void *, void *);
	struct ComponentParameters *params;
	void *globals;
	
	dispatcher_func = dispatcher;
	
	globals = malloc(sizeof(long));
	(long)*(void **)globals = 0x2001;

	params = malloc(sizeof(struct ComponentParameters));

	params->flags = 2;
	params->paramSize = sizeof(params);
	params->what = atoi(argv[1]);
	params->params[0] = 0x1984;
	/* 0x1000100f will load QuickTime.qts */
	/* 0x10001014 will use SendMessageA */
	/* 0x10001019 returns 0 */
	/* 0x1000101e will load QuickTime.qts */
	/* 0x10001028 returns params' addr */
	/* 0x1000102d is a dialog */
	/* 0x10001032 returns 20001 => CDVersion */
	/* 0x10001069 returns 8a */
	/* 0x100010b4 probarly init ?? */
	printf("params: flags: %d, paramSize: %d, what: %d\n",
	    params->flags, params->paramSize, params->what);
	printf("params[0] = %x\n", params->params[0]);
	ret = dispatcher_func(params, globals);
	printf("CDComponentDispatch(%p, %p) => %x\n",
	    &params, globals, ret);
	free(globals);
	free(params);
    }
    
    Restore_LDT_Keeper();
}
