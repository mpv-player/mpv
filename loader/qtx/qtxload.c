/* to compile: gcc -o qtxload qtxload.c ../libloader.a -lpthread -ldl -ggdb ../../cpudetect.o */

#include <stdio.h>
#include "qtxsdk/components.h"
#include "qtxsdk/select.h"

#define DEF_DISPATCHER(name) ComponentResult (*##name)(ComponentParameters *, void **)

/* ilyen egy sima komponens */
ComponentResult ComponentDummy(
    ComponentParameters *params,
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
    ComponentResult (*
dispatcher)(ComponentParameters *params, void* glob);

    Setup_LDT_Keeper();
    
    handler = expLoadLibraryA("/usr/lib/win32/QuickTime.qts");
    dispatcher = GetProcAddress(handler, "SorensonYUV9Dispatcher");
    printf("handler: %p, dispatcher: %p\n", handler, dispatcher);
    
    {
	ComponentResult ret;
	ComponentParameters *params;
	void *globals;
	
	globals = malloc(sizeof(long));
	(long)*(void **)globals = 0x2001;

	params = malloc(sizeof(ComponentParameters));

	params->flags = 0;
	params->paramSize = sizeof(params);
	params->what = kComponentVersionSelect;
	// params->what = kComponentRegisterSelect;
	// params->what = kComponentOpenSelect;
	// params->what = -5; //atoi(argv[1]);

	params->params[0] = 0x1984;

	printf("params: flags: %d, paramSize: %d, what: %d\n",
	    params->flags, params->paramSize, params->what);
	printf("params[0] = %x\n", params->params[0]);

	ret = dispatcher(params, globals);

	printf("!!! CDComponentDispatch() => %x\n",ret);

//	printf("!!! CDComponentDispatch(%p, %p) => %x\n",
//	    &params, globals, ret);
//	free(globals);
//	free(params);
    }
    
//    Restore_LDT_Keeper();
}
