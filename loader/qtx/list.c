/* to compile:
   edit ../win32.c, change the #if 0 to 1 at line 1326 to enabel quicktime fix!
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qtxsdk/components.h"
#include "qtxsdk/select.h"
#include "ldt_keeper.h"

char* get_path(const char* x){  return strdup(x);}
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
    OSErr (*InitializeQTML)(long flags);
    OSErr (*EnterMovies)(void);
    OSErr ret;

    Setup_LDT_Keeper();
    handler = LoadLibraryA("/usr/local/lib/codecs/qtmlClient.dll");
    printf("***************************\n");
    InitializeQTML = 0x1000c870; //GetProcAddress(handler, "InitializeQTML");
    EnterMovies = 0x10003ac0; //GetProcAddress(handler, "EnterMovies");
    FindNextComponent = 0x1000d5f0; //GetProcAddress(handler, "FindNextComponent");
    CountComponents = 0x1000d5d0; //GetProcAddress(handler, "CountComponents");
//     = GetProcAddress(handler, "");
    printf("handler: %p, funcs: %p %p %p, %p\n", handler, InitializeQTML, EnterMovies, FindNextComponent,CountComponents);

    ret=InitializeQTML(0);
    printf("InitializeQTML->%d\n",ret);
    ret=EnterMovies();
    printf("EnterMovies->%d\n",ret);

    memset(&desc,0,sizeof(desc));
    desc.componentType= (((unsigned char)'S')<<24)|
			(((unsigned char)'V')<<16)|
			(((unsigned char)'Q')<<8)|
			(((unsigned char)'5'));
    desc.componentSubType=0;
    desc.componentManufacturer=0;
    desc.componentFlags=0;
    desc.componentFlagsMask=0;
    
    printf("Count = %ld\n",CountComponents(&desc));

    exit(0);
}
