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

main()
{
    void *handler;
    void *dispatcher;

    Setup_LDT_Keeper();
    Setup_FS_Segment();
    
    handler = expLoadLibraryA("/usr/lib/win32/qtx/test.qtx");
    dispatcher = GetProcAddress(handler, "CDComponentDispatch");
    printf("handler: %p, dispatcher: %p\n", handler, dispatcher);
    
    {
	ComponentResult ret;
	int (*dispatcher_func)(void *, void *);
	struct ComponentParameters params;
	void *globals;
	
	globals = malloc(sizeof(long));
	(long)*(void **)globals = 0x2001;

	params.flags = 0;
	params.paramSize = sizeof(params);
	params.what = 2; /* probarly register :p */
	params.params[0] = -1;
	params.params[1] = -1;
	memset(&params.params[0], 0x77, sizeof(params.params)*2);
//	params.params[1] = 0x1000100f;
	/* 0x1000100f will load QuickTime.qts */
	/* 0x10001014 will use SendMessageA */
	/* 0x10001019 returns 0 */
	/* 0x1000101e will load QuickTime.qts */
	/* 0x10001028 returns params' addr */
	/* 0x1000102d is a dialog */
	/* 0x10001032 returns 20001 => CDVersion */
	/* 0x10001069 returns 8a */
//	params.params[0] = 0x1984;
//	params.params[1] = 0x1337;
//	params.params[1] = ComponentDummy;
	printf("params: flags: %d, paramSize: %d, what: %d\n",
	    params.flags, params.paramSize, params.what);
	printf("params[0] = %x, params[1] = %x\n", params.params[0],
	    params.params[1]);
	ret = dispatcher_func(&params, globals);
	printf("CDComponentDispatch(%p, %p) => %x\n",
	    &params, globals, ret);
	free(globals);
	printf("params: flags: %d, paramSize: %d, what: %d\n",
	    params.flags, params.paramSize, params.what);
	printf("params[0] = %x, params[1] = %x\n", params.params[0],
	    params.params[1]);
    }
    
    Restore_LDT_Keeper();
}
