/* to compile: gcc -o qtxload qtxload.c ../libloader.a -lpthread -ldl -ggdb ../../cpudetect.o */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

char *get_path(char* x){  return strdup(x);}

void* LoadLibraryA(char* name);
void* GetProcAddress(void* handle,char* func);

#define __stdcall __attribute__((__stdcall__))
#define __cdecl   __attribute__((__cdecl__))
#define APIENTRY 

unsigned int* x_table[0x00001837];

int main(int argc, char *argv[]){
    void *handler;
    ComponentResult (*
dispatcher)(ComponentParameters *params, Globals glob);
    ComponentResult ret;
    ComponentParameters *params;
    ComponentDescription desc;
    void *globals=NULL;
    unsigned int esp=0;
    int i;

    Setup_LDT_Keeper();
    handler = LoadLibraryA("/usr/lib/win32/QuickTime.qts");
    dispatcher = GetProcAddress(handler, "SorensonYUV9Dispatcher");
//    handler = expLoadLibraryA("/usr/lib/win32/On2_VP3.qtx");
//    dispatcher = GetProcAddress(handler, "CDComponentDispatcher");
    printf("handler: %p, dispatcher: %p\n", handler, dispatcher);

    printf("theQuickTimeDispatcher = %p\n",GetProcAddress(handler, "theQuickTimeDispatcher"));

    desc.componentType=0;
    desc.componentSubType=0;
    desc.componentManufacturer=0;
    desc.componentFlags=0;
    desc.componentFlagsMask=0;
    
    params = malloc(sizeof(ComponentParameters)+2048);

    params->flags = 0;
    params->paramSize = 0;
    params->what = kComponentRegisterSelect;
    ret = dispatcher(params, &globals);
    printf("!!! CDComponentDispatch() => 0x%X  glob=%p\n",ret,globals);

//    memset(x_table,12,4*0x00001837);

//for(i=0;i<=255;i++){
    params->flags = 0;
    params->paramSize = 4; //sizeof(params->params[0]);
    params->params[0] = 0x820000; //0x820000|i; //(i<<16)|0x24; //0x820024;

    // params->what = kComponentVersionSelect;
    // params->what = kComponentRegisterSelect;
    params->what = kComponentOpenSelect;
    // params->what = kComponentCanDoSelect;

    printf("params: flags: %d, paramSize: %d, what: %d, params[0] = %x\n",
        params->flags, params->paramSize, params->what, params->params[0]);

//    __asm__ __volatile__ ("movl %%esp, %0\n\t" : "=a" (esp) :: "memory" );
//    printf("ESP=%p\n",esp);

    *((void**)0x62b7d640) = &x_table[0]; //malloc(0x00001837 * 4); // ugly hack?

    printf("params=%p  &glob=%p  x_table=%p\n",params,&globals, &x_table[0]);

    ret = dispatcher(params, &globals);

//    __asm__ __volatile__ ("movl %%esp, %0\n\t" : "=a" (esp) :: "memory" );
//    printf("ESP=%p\n",esp);

    printf("!!! CDComponentDispatch() => %d  glob=%p\n",ret,globals);
//    if(ret!=-3000) break;
//}

//    for(i=0;i<0x00001837;i++)
//	if(x_table[i]) printf("x_table[0x%X] = %p\n",i,x_table[i]);
    
    Restore_LDT_Keeper();
    exit(0);
    //return 0;
}
