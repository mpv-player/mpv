/* to compile:
   gcc -o list list.c ../libloader.a -lpthread -ldl -lm -ggdb ../../cpudetect.o
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qtxsdk/components.h"
#include "qtxsdk/select.h"

char* get_path(char* x){  return strdup(x);}
void* LoadLibraryA(char* name);
void* GetProcAddress(void* handle,char* func);

#define __stdcall __attribute__((__stdcall__))
#define __cdecl   __attribute__((__cdecl__))
#define APIENTRY 

int main(int argc, char *argv[]){
    void *handler;
    ComponentDescription desc;
    Component (*FindNextComponent)(Component prev,ComponentDescription* desc);
    long (*CountComponents)(ComponentDescription* desc);

    Setup_LDT_Keeper();
    handler = LoadLibraryA("/usr/lib/win32/qtmlClient.dll");
    FindNextComponent = GetProcAddress(handler, "FindNextComponent");
    CountComponents = GetProcAddress(handler, "CountComponents");
    printf("handler: %p, funcs: %p, %p\n", handler, FindNextComponent,CountComponents);

    memset(&desc,0,sizeof(desc));
    desc.componentType=0;
    desc.componentSubType=0;
    desc.componentManufacturer=0;
    desc.componentFlags=0;
    desc.componentFlagsMask=0;
    
    printf("Count = %d\n",CountComponents(&desc));
    
    Restore_LDT_Keeper();
    exit(0);
}
