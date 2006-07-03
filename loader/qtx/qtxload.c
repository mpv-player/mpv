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

char *get_path(const char* x){  return strdup(x);}

void* LoadLibraryA(char* name);
void* GetProcAddress(void* handle,char* func);

#define __stdcall __attribute__((__stdcall__))
#define __cdecl   __attribute__((__cdecl__))
#define APIENTRY 

unsigned int* x_table[0x00001837];

static    OSErr (*InitializeQTML)(long flags);

int main(int argc, char *argv[]){
    void *handler;
    void *handler2;
    void* theqtdp=NULL;
    void* compcall=NULL;
    void* compcallws=NULL;
    ComponentResult (*dispatcher)(ComponentParameters *params, Globals glob);
    ComponentResult ret;
    ComponentParameters *params;
    ComponentDescription desc;
    void *globals=NULL;
    unsigned int esp=0;
    int i;

    mp_msg_init();
    mp_msg_set_level(10);

    Setup_LDT_Keeper();
    printf("loading qts\n");
//    handler = LoadLibraryA("/root/.wine/fake_windows/Windows/System/QuickTime.qts");
    handler = LoadLibraryA("QuickTime.qts");
    theqtdp = GetProcAddress(handler, "theQuickTimeDispatcher");
    compcall = GetProcAddress(handler, "_CallComponent");
    compcallws = GetProcAddress(handler, "_CallComponentFunctionWithStorage");

    InitializeQTML = 0x6299e590;//GetProcAddress(handler, "InitializeQTML");
    InitializeQTML(6+16);
    
    printf("loading svq3\n");
    handler2= LoadLibraryA("/root/.wine/fake_windows/Windows/System/QuickTime/QuickTimeEssentials.qtx");
    printf("done\n");
    dispatcher = GetProcAddress(handler2, "SMD_ComponentDispatch");
//    handler = expLoadLibraryA("/usr/lib/win32/On2_VP3.qtx");
//    dispatcher = GetProcAddress(handler, "CDComponentDispatcher");
    printf("handler: %p, dispatcher: %p  theqtdp: %p\n", handler, dispatcher, theqtdp);

//    printf("theQuickTimeDispatcher = %p\n",GetProcAddress(handler, "theQuickTimeDispatcher"));

    // patch svq3 dll:
    *((void**)0x63214c98) = NULL;
    *((void**)0x63214c9c) = theqtdp; // theQt...
    *((void**)0x63214ca0) = compcall; //0xdeadbeef; //dispatcher; // CallCOmponent_ptr
    *((void**)0x63214ca4) = compcallws; //0xdeadbef2; //dispatcher; // CallComponentWithStorage_ptr

    desc.componentType=0;
    desc.componentSubType=0;
    desc.componentManufacturer=0;
    desc.componentFlags=0;
    desc.componentFlagsMask=0;
    
    params = malloc(sizeof(ComponentParameters)+2048);

    params->flags = 0;
    params->paramSize = 4;
    params->what = kComponentOpenSelect;
    params->params[0] = 0x830000; //0x820000|i; //(i<<16)|0x24; //0x820024;
    ret = dispatcher(params, &globals);
    printf("!!! CDComponentDispatch() => 0x%X  glob=%p\n",ret,globals);

//    memset(x_table,12,4*0x00001837);

//for(i=0;i<=255;i++){

    // params->what = kComponentVersionSelect;
    // params->what = kComponentRegisterSelect;
    // params->what = kComponentOpenSelect;
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
