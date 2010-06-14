/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qtxsdk/components.h"
#include "qtxsdk/select.h"
#include "loader/ldt_keeper.h"
#include "loader/wine/winbase.h"
#include "mp_msg.h"

unsigned int* x_table[0x00001837];

static    OSErr (*InitializeQTML)(long flags);

int main(void) {
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
    //unsigned int esp=0;
    //int i;

    mp_msg_init();
    //mp_msg_set_level(10);

    Setup_LDT_Keeper();
    printf("loading qts\n");
//    handler = LoadLibraryA("/root/.wine/fake_windows/Windows/System/QuickTime.qts");
    handler = LoadLibraryA("QuickTime.qts");
    theqtdp = GetProcAddress(handler, "theQuickTimeDispatcher");
    compcall = GetProcAddress(handler, "CallComponent");
    compcallws = GetProcAddress(handler, "CallComponentFunctionWithStorage");

    InitializeQTML = 0x6299e590;//GetProcAddress(handler, "InitializeQTML");
    InitializeQTML(6+16);

    printf("loading svq3\n");
    handler2= LoadLibraryA("/root/.wine/fake_windows/Windows/System/QuickTime/QuickTimeEssentials.qtx");
    printf("done\n");
    dispatcher = GetProcAddress(handler2, "SMD_ComponentDispatch");
//    handler = expLoadLibraryA("/usr/local/lib/codecs/On2_VP3.qtx");
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

//    __asm__ volatile ("movl %%esp, %0\n\t" : "=a" (esp) :: "memory" );
//    printf("ESP=%p\n",esp);

    *((void**)0x62b7d640) = &x_table[0]; //malloc(0x00001837 * 4); // ugly hack?

    printf("params=%p  &glob=%p  x_table=%p\n",params,&globals, &x_table[0]);

    ret = dispatcher(params, &globals);

//    __asm__ volatile ("movl %%esp, %0\n\t" : "=a" (esp) :: "memory" );
//    printf("ESP=%p\n",esp);

    printf("!!! CDComponentDispatch() => %d  glob=%p\n",ret,globals);
//    if(ret!=-3000) break;
//}

//    for(i=0;i<0x00001837;i++)
//	if(x_table[i]) printf("x_table[0x%X] = %p\n",i,x_table[i]);

    exit(0);
    //return 0;
}
