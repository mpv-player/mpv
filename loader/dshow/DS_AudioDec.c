
#include "guids.h"
#include "interfaces.h"

#include "DS_AudioDecoder.h"
#include <wine/winerror.h>
#include <libwin32.h>
//#include <cpuinfo.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <cstdio>
#include <iostream>
#include <strstream>

#include "DS_AudioDec.h"

#include "ldt_keeper.h"

//    DS_Decoder(const CodecInfo& info, const WAVEFORMATEX*);
//    virtual ~DS_Decoder();
//    virtual int Convert(const void*, size_t, void*, size_t, size_t*, size_t*);
//    virtual int GetSrcSize(int);

static void* _handle;

extern "C" int DS_AudioDecoder_Open(char* dllname, GUID* guid, WAVEFORMATEX* wf){

    Setup_LDT_Keeper();
    Setup_FS_Segment();

    CodecInfo ci;
    ci.dll=dllname;
    ci.guid=*guid;

    DS_AudioDecoder* dec=new DS_AudioDecoder(ci, wf);
    _handle=(void*)dec;

    return 0;
}

extern "C" void DS_AudioDecoder_Close(){
}

extern "C" int DS_AudioDecoder_GetSrcSize(int dest_size){
    DS_AudioDecoder* dec=(DS_AudioDecoder*)_handle;
    return dec->GetSrcSize(dest_size);
}

extern "C" int DS_AudioDecoder_Convert(unsigned char* in_data, unsigned in_size,
	     unsigned char* out_data, unsigned out_size,
	     unsigned* size_read, unsigned* size_written){
    DS_AudioDecoder* dec=(DS_AudioDecoder*)_handle;
    Setup_FS_Segment();
    return dec->Convert( (void*)in_data,(size_t)in_size,
			 (void*)out_data,(size_t)out_size,
			 (size_t*)size_read, (size_t*)size_written );
}

