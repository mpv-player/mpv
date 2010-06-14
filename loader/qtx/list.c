/*
 * To compile edit loader/win32.c and change the #if 0 to 1 at line 1326
 * to enable quicktime fix!
 *
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

int main(void) {
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
