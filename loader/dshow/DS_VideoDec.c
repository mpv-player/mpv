#include "guids.h"
#include "interfaces.h"
#include "libwin32.h"

#include "DS_VideoDecoder.h"
#include <wine/winerror.h>
//#include <cpuinfo.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <cstdio>
#include <iostream>
#include <strstream>

#include <registry.h>
//#include <wine/winreg.h>

#include "DS_VideoDec.h"

static void* _handle; // will be parameter later...
static char** _d_ptr;  // will be parameter later...

extern "C" void Setup_LDT_Keeper();
extern "C" void setup_FS_Segment();

extern "C" int DS_VideoDecoder_Open(char* dllname, GUID* guid, BITMAPINFOHEADER* format, int flip,char** d_ptr){

    Setup_LDT_Keeper();

    CodecInfo ci;
    ci.dll=dllname;
    ci.guid=*guid;

    try {
	DS_VideoDecoder* dec=new DS_VideoDecoder(ci, *format, flip);
	_d_ptr=d_ptr;
	_handle=(void*)dec;
	return 0;
    } catch (FatalError &e) {  }

    _handle=NULL;
    return -1;

}

extern "C" void DS_VideoDecoder_Start(){
    DS_VideoDecoder* dec=(DS_VideoDecoder*) _handle;
    dec->Start();
}

extern "C" void DS_VideoDecoder_Stop(){
    DS_VideoDecoder* dec=(DS_VideoDecoder*) _handle;
    dec->Stop();
}

extern "C" void DS_VideoDecoder_Restart(){
}

extern "C" void DS_VideoDecoder_Close(){
    DS_VideoDecoder* dec=(DS_VideoDecoder*) _handle;
    _handle=NULL;
    delete dec;
}

extern "C" int DS_VideoDecoder_DecodeFrame(char* src, int size, int is_keyframe, int render){
    DS_VideoDecoder* dec=(DS_VideoDecoder*) _handle;
    CImage image;
    image.ptr=*_d_ptr;
    return dec->Decode((void*)src,(size_t)size,is_keyframe,&image);
}

extern "C" int DS_VideoDecoder_SetDestFmt(int bits, int csp){
    DS_VideoDecoder* dec=(DS_VideoDecoder*) _handle;
    return dec->SetDestFmt(bits,(fourcc_t)csp);
}

extern "C" int DS_SetValue_DivX(char* name, int value){
    DS_VideoDecoder* dec=(DS_VideoDecoder*) _handle;
    /* This printf is annoying with autoquality, *
     * should be moved into players code - atmos */
    //printf("DS_SetValue_DivX(%s),%d)\n",name,value);
    return (int) dec->SetValue(name,value);
}

extern "C" int DS_SetAttr_DivX(char* attribute, int value){
    int result, status, newkey, count;
        if(strcmp(attribute, "Quality")==0){
	    char* keyname="SOFTWARE\\Microsoft\\Scrunch";
    	    result=RegCreateKeyExA(HKEY_CURRENT_USER, keyname, 0, 0, 0, 0, 0,	   		&newkey, &status);
            if(result!=0)
	    {
	        printf("VideoDecoder::SetExtAttr: registry failure\n");
	        return -1;
	    }    
	    result=RegSetValueExA(newkey, "Current Post Process Mode", 0, REG_DWORD, &value, 4);
            if(result!=0)
	    {
	        printf("VideoDecoder::SetExtAttr: error writing value\n");
	        return -1;
	    }    
	    value=-1;
	    result=RegSetValueExA(newkey, "Force Post Process Mode", 0, REG_DWORD, &value, 4);
            if(result!=0)
	    {
		printf("VideoDecoder::SetExtAttr: error writing value\n");
	    	return -1;
	    }    
   	    RegCloseKey(newkey);
   	    return 0;
	}   	

        if(
	(strcmp(attribute, "Saturation")==0) ||
	(strcmp(attribute, "Hue")==0) ||
	(strcmp(attribute, "Contrast")==0) ||
	(strcmp(attribute, "Brightness")==0)
	)
        {
	    char* keyname="SOFTWARE\\Microsoft\\Scrunch\\Video";
    	    result=RegCreateKeyExA(HKEY_CURRENT_USER, keyname, 0, 0, 0, 0, 0,	   		&newkey, &status);
            if(result!=0)
	    {
	        printf("VideoDecoder::SetExtAttr: registry failure\n");
	        return -1;
	    }    
	    result=RegSetValueExA(newkey, attribute, 0, REG_DWORD, &value, 4);
            if(result!=0)
	    {
	        printf("VideoDecoder::SetExtAttr: error writing value\n");
	        return -1;
	    }    
   	    RegCloseKey(newkey);
   	    return 0;
	}   	

        printf("Unknown attribute!\n");
        return -200;
}

