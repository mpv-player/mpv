#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#ifdef USE_QTX_CODECS

#include "mp_msg.h"
#include "vd_internal.h"

static vd_info_t info = {
	"Quicktime Video decoder",
	"qtvideo",
	"A'rpi",
	"Faust3",
	"win32"
};

LIBVD_EXTERN(qtvideo)

#include "components.h"

//#include "wine/windef.h"

HMODULE   WINAPI LoadLibraryA(LPCSTR);
FARPROC   WINAPI GetProcAddress(HMODULE,LPCSTR);
int       WINAPI FreeLibrary(HMODULE);

//static ComponentDescription desc; // for FindNextComponent()
static ComponentInstance ci=NULL; // codec handle
//static CodecInfo cinfo;	// for ImageCodecGetCodecInfo()
//Component prev=NULL;
//ComponentResult cres; // 
static CodecCapabilities codeccap; // for decpar
static CodecDecompressParams decpar; // for ImageCodecPreDecompress()
//static ImageSubCodecDecompressCapabilities icap; // for ImageCodecInitialize()
static Rect OutBufferRect;              //the dimensions of our GWorld

static GWorldPtr OutBufferGWorld = NULL;//a GWorld is some kind of description for a drawing environment
static ImageDescriptionHandle framedescHandle;
//static HINSTANCE qtml_dll;
static HMODULE handler;

static    Component (*FindNextComponent)(Component prev,ComponentDescription* desc);
static    OSErr (*GetComponentInfo)(Component prev,ComponentDescription* desc,Handle h1,Handle h2,Handle h3);
static    long (*CountComponents)(ComponentDescription* desc);
static    OSErr (*InitializeQTML)(long flags);
static    OSErr (*EnterMovies)(void);
static    ComponentInstance (*OpenComponent)(Component c);
static    ComponentResult (*ImageCodecInitialize)(ComponentInstance ci,
                                 ImageSubCodecDecompressCapabilities * cap);
static    ComponentResult (*ImageCodecBeginBand)(ComponentInstance      ci,
                                 CodecDecompressParams * params,
                                 ImageSubCodecDecompressRecord * drp,
                                 long                   flags);
static    ComponentResult (*ImageCodecDrawBand)(ComponentInstance      ci,
                                 ImageSubCodecDecompressRecord * drp);
static    ComponentResult (*ImageCodecEndBand)(ComponentInstance      ci,
                                 ImageSubCodecDecompressRecord * drp,
                                 OSErr                  result,
                                 long                   flags);
static    ComponentResult (*ImageCodecGetCodecInfo)(ComponentInstance      ci,
                                 CodecInfo *            info);
static    ComponentResult (*ImageCodecPreDecompress)(ComponentInstance      ci,
                                 CodecDecompressParams * params);
static    ComponentResult (*ImageCodecBandDecompress)(ComponentInstance      ci,
                                 CodecDecompressParams * params);
static    PixMapHandle    (*GetGWorldPixMap)(GWorldPtr offscreenGWorld);                  
static    OSErr           (*QTNewGWorldFromPtr)(GWorldPtr *gw,
			       OSType pixelFormat,
			       const Rect *boundsRect,
			       CTabHandle cTable,
                               /*GDHandle*/void* aGDevice, //unused anyway
                               GWorldFlags flags,
                               void *baseAddr,
                               long rowBytes); 
static    OSErr           (*NewHandleClear)(Size byteCount);                          


// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

static int codec_inited=0;

// init driver
static int init(sh_video_t *sh){
    long result = 1;
    ComponentResult cres;
    ComponentDescription desc;
    Component prev=NULL;
    CodecInfo cinfo;	// for ImageCodecGetCodecInfo()
    ImageSubCodecDecompressCapabilities icap; // for ImageCodecInitialize()

    handler = LoadLibraryA("qtmlClient.dll");

    InitializeQTML = GetProcAddress(handler, "InitializeQTML");
    EnterMovies = GetProcAddress(handler, "EnterMovies");
    FindNextComponent = GetProcAddress(handler, "FindNextComponent");
    CountComponents = GetProcAddress(handler, "CountComponents");
    GetComponentInfo = GetProcAddress(handler, "GetComponentInfo");
    OpenComponent = GetProcAddress(handler, "OpenComponent");
    ImageCodecInitialize = GetProcAddress(handler, "ImageCodecInitialize");
    ImageCodecGetCodecInfo = GetProcAddress(handler, "ImageCodecGetCodecInfo");
    ImageCodecBeginBand = GetProcAddress(handler, "ImageCodecBeginBand");
    ImageCodecPreDecompress = GetProcAddress(handler, "ImageCodecPreDecompress");
    ImageCodecBandDecompress = GetProcAddress(handler, "ImageCodecBandDecompress");
    GetGWorldPixMap = GetProcAddress(handler, "GetGWorldPixMap");
    QTNewGWorldFromPtr = GetProcAddress(handler, "QTNewGWorldFromPtr");
    NewHandleClear = GetProcAddress(handler, "NewHandleClear");
    //     = GetProcAddress(handler, "");
    
    if(!InitializeQTML || !EnterMovies || !FindNextComponent || !ImageCodecBandDecompress){
	printf("invalid qt DLL!\n");
	return 0;
    }

//    result=InitializeQTML(6+16);
    result=InitializeQTML(0);
    printf("InitializeQTML returned %i\n",result);
    result=EnterMovies();
    printf("EnterMovies->%d\n",result);

    memset(&desc,0,sizeof(desc));
    desc.componentType= (((unsigned char)'i')<<24)|
			(((unsigned char)'m')<<16)|
			(((unsigned char)'d')<<8)|
			(((unsigned char)'c'));
    desc.componentSubType= 
		    (((unsigned char)'S'<<24))|
			(((unsigned char)'V')<<16)|
			(((unsigned char)'Q')<<8)|
			(((unsigned char)'3'));
    desc.componentManufacturer=0;
    desc.componentFlags=0;
    desc.componentFlagsMask=0;

    printf("Count = %d\n",CountComponents(&desc));
#if 0
    memset(&desc,0,sizeof(desc));
    while((prev=FindNextComponent(prev,&desc))){
	unsigned char* c1=&desc2.componentType;
	unsigned char* c2=&desc2.componentSubType;
	memset(&desc2,0,sizeof(desc2));
	printf("juhee %p (%p)\n",prev,&desc);
	GetComponentInfo(prev,&desc2,NULL,NULL,NULL);
	printf("DESC: %c%c%c%c/%c%c%c%c [0x%X/0x%X] 0x%X\n",
	    c1[3],c1[2],c1[1],c1[0],
	    c2[3],c2[2],c2[1],c2[0],
	    desc2.componentType,desc2.componentSubType,
	    desc2.componentFlags);
    }
#endif

    prev=FindNextComponent(NULL,&desc);
    if(!prev){
	printf("Cannot find requested component\n");
	exit(1);
    }
    printf("Found it! ID = 0x%X\n",prev);

    ci=OpenComponent(prev);
    printf("ci=%p\n",ci);

    memset(&icap,0,sizeof(icap));
    cres=ImageCodecInitialize(ci,&icap);
    printf("ImageCodecInitialize->%p  size=%d (%d)\n",cres,icap.recordSize,icap.decompressRecordSize);
    
    memset(&cinfo,0,sizeof(cinfo));
    cres=ImageCodecGetCodecInfo(ci,&cinfo);
    printf("Flags: compr: 0x%X  decomp: 0x%X format: 0x%X\n",
	cinfo.compressFlags, cinfo.decompressFlags, cinfo.formatFlags);
    printf("Codec name: %.*s\n",((unsigned char*)&cinfo.typeName)[0],
	((unsigned char*)&cinfo.typeName)+1);

    //make a yuy2 gworld
    OutBufferRect.top=0;
    OutBufferRect.left=0;
    OutBufferRect.right=sh->disp_w;
    OutBufferRect.bottom=sh->disp_h;

    //Fill the imagedescription for our SVQ3 frame
    //we can probably get this from Demuxer
#if 0
    framedescHandle=(ImageDescriptionHandle)NewHandleClear(sizeof(ImageDescription)+200);
    printf("framedescHandle=%p  *p=%p\n",framedescHandle,*framedescHandle);
{ FILE* f=fopen("/root/.wine/fake_windows/IDesc","r");
  if(!f) printf("filenot found: IDesc\n");
  fread(*framedescHandle,sizeof(ImageDescription)+200,1,f);
  fclose(f);
}
#else
    framedescHandle=&(sh->ImageDesc);
#endif
//Find codecscomponent for video decompression
//    result = FindCodec ('SVQ1',anyCodec,&compressor,&decompressor );                 
//    printf("FindCodec SVQ1 returned:%i compressor: 0x%X decompressor: 0x%X\n",result,compressor,decompressor);

    if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YUY2)) return 0;

    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    long result = 1;
    mp_image_t* mpi;
    ComponentResult cres;

    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_STATIC, MP_IMGFLAG_PRESERVE, 
	sh->disp_w, sh->disp_h);
    if(!mpi) return NULL;

    decpar.data = (char*)data;
    decpar.bufferSize = len;
    (**framedescHandle).dataSize=len;

if(!codec_inited){
    result = QTNewGWorldFromPtr(
        &OutBufferGWorld,  
        kYUVSPixelFormat, //pixel format of new GWorld == YUY2
        &OutBufferRect,   //we should benchmark if yvu9 is faster for svq3, too
        0, 
        0, 
        0, 
        mpi->planes[0],
        mpi->stride[0]);
    printf("NewGWorldFromPtr returned:%i\n",result);

//    printf("IDesc=%d\n",sizeof(ImageDescription));

    decpar.imageDescription = framedescHandle;
    decpar.startLine=0;
    decpar.stopLine=(**framedescHandle).height;
    decpar.frameNumber = 1; //1
//    decpar.conditionFlags=0xFFD; // first
//    decpar.callerFlags=0x2001; // first
    decpar.matrixFlags = 0;
    decpar.matrixType = 0;
    decpar.matrix = 0;
    decpar.capabilities=&codeccap;
//    decpar.accuracy = 0x1680000; //codecNormalQuality;
    decpar.accuracy = codecNormalQuality;
    decpar.port = OutBufferGWorld;
//    decpar.preferredOffscreenPixelSize=17207;
    
//    decpar.sequenceID=malloc(1000);
//    memset(decpar.sequenceID,0,1000);

//    SrcRect.top=17207;
//    SrcRect.left=0;
//    SrcRect.right=0;//image_width;
//    SrcRect.bottom=0;//image_height;

//    decpar.srcRect = SrcRect;
    decpar.srcRect = OutBufferRect;
    
    decpar.transferMode = srcCopy;
    decpar.dstPixMap = **GetGWorldPixMap( OutBufferGWorld);//destPixmap; 
  
    cres=ImageCodecPreDecompress(ci,&decpar);
    printf("ImageCodecPreDecompress cres=0x%X\n",cres);

//    decpar.conditionFlags=0x10FFF; // first
//    decpar.preferredOffscreenPixelSize=17207;

//    decpar.conditionFlags=0x10FFD; // first

//	cres=ImageCodecPreDecompress(ci,&decpar);
//    printf("ImageCodecPreDecompress cres=0x%X\n",cres);


    codec_inited=1;
}

#if 0
    if(decpar.frameNumber==124){
	decpar.frameNumber=1;
	cres=ImageCodecPreDecompress(ci,&decpar);
	printf("ImageCodecPreDecompress cres=0x%X\n",cres);
    }
#endif

    cres=ImageCodecBandDecompress(ci,&decpar);
    printf("ImageCodecBandDecompress cres=0x%X (-0x%X) %d\n",cres,-cres,cres);

    ++decpar.frameNumber;
    
    return mpi;
}
#endif
