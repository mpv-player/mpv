// Basic types:

typedef char *                          Ptr;
typedef Ptr *                           Handle;
typedef long                            Size;
typedef unsigned char                   Boolean;
typedef unsigned char                   Str31[32];
typedef long                            Fixed;

typedef long OSErr;
typedef int OSType;

typedef long ComponentResult;
typedef unsigned char                   UInt8;
typedef signed char                     SInt8;
typedef unsigned short                  UInt16;
typedef signed short                    SInt16;
typedef unsigned long                   UInt32;
typedef signed long                     SInt32;

#define FOUR_CHAR_CODE(x)       ((unsigned long)(x)) /* otherwise compiler will complain about values with high bit set */

// codec private shit:
typedef void *GlobalsPtr;
typedef void **Globals;

//==================== COMPONENTS ===========================

struct ComponentParameters {
    UInt8                           flags;                      /* call modifiers: sync/async, deferred, immed, etc */
    UInt8                           paramSize;                  /* size in bytes of actual parameters passed to this call */
    short                           what;                       /* routine selector, negative for Component management calls */
    long                            params[1];                  /* actual parameters for the indicated routine */
};
typedef struct ComponentParameters      ComponentParameters;


struct ComponentDescription {
    OSType                          componentType;              /* A unique 4-byte code indentifying the command set */
    OSType                          componentSubType;           /* Particular flavor of this instance */
    OSType                          componentManufacturer;      /* Vendor indentification */
    unsigned long                   componentFlags;             /* 8 each for Component,Type,SubType,Manuf/revision */
    unsigned long                   componentFlagsMask;         /* Mask for specifying which flags to consider in search, zero during registration */
};
typedef struct ComponentDescription     ComponentDescription;


struct ResourceSpec {
    OSType                          resType;                    /* 4-byte code    */
    short                           resID;                      /*         */
};
typedef struct ResourceSpec             ResourceSpec;


struct ComponentResource {
    ComponentDescription            cd;                         /* Registration parameters */
    ResourceSpec                    component;                  /* resource where Component code is found */
    ResourceSpec                    componentName;              /* name string resource */
    ResourceSpec                    componentInfo;              /* info string resource */
    ResourceSpec                    componentIcon;              /* icon resource */
};
typedef struct ComponentResource        ComponentResource;
typedef ComponentResource *             ComponentResourcePtr;
typedef ComponentResourcePtr *          ComponentResourceHandle;


struct ComponentRecord {
    long                            data[1];
};
typedef struct ComponentRecord          ComponentRecord;
typedef ComponentRecord *               Component;


struct ComponentInstanceRecord {
    long                            data[1];
};
typedef struct ComponentInstanceRecord  ComponentInstanceRecord;

typedef ComponentInstanceRecord *       ComponentInstance;

// ========================= QUICKDRAW =========================

struct Rect {
    short                           top;
    short                           left;
    short                           bottom;
    short                           right;
};
typedef struct Rect                     Rect;
typedef Rect *                          RectPtr;

struct RGBColor {
    unsigned short                  red;                        /*magnitude of red component*/
    unsigned short                  green;                      /*magnitude of green component*/
    unsigned short                  blue;                       /*magnitude of blue component*/
};
typedef struct RGBColor                 RGBColor;
typedef RGBColor *                      RGBColorPtr;
typedef RGBColorPtr *                   RGBColorHdl;

struct ColorSpec {
    short                           value;                      /*index or other value*/
    RGBColor                        rgb;                        /*true color*/
};
typedef struct ColorSpec                ColorSpec;
typedef ColorSpec *                     ColorSpecPtr;
typedef ColorSpec                       CSpecArray[1];

struct ColorTable {
    long                            ctSeed;                     /*unique identifier for table*/
    short                           ctFlags;                    /*high bit: 0 = PixMap; 1 = device*/
    short                           ctSize;                     /*number of entries in CTTable*/
    CSpecArray                      ctTable;                    /*array [0..0] of ColorSpec*/
};
typedef struct ColorTable               ColorTable;
typedef ColorTable *                    CTabPtr;
typedef CTabPtr *                       CTabHandle;

struct MatrixRecord {
    Fixed                           matrix[3][3];
};
typedef struct MatrixRecord             MatrixRecord;
typedef MatrixRecord *                  MatrixRecordPtr;

typedef long                            ImageSequence;
typedef OSType                          CodecType;
typedef unsigned short                  CodecFlags;
typedef unsigned long                   CodecQ;

struct ImageDescription {
    long                            idSize;                     /* total size of ImageDescription including extra data ( CLUTs and other per sequence data ) */
    CodecType                       cType;                      /* what kind of codec compressed this data */
    long                            resvd1;                     /* reserved for Apple use */
    short                           resvd2;                     /* reserved for Apple use */
    short                           dataRefIndex;               /* set to zero  */
    short                           version;                    /* which version is this data */
    short                           revisionLevel;              /* what version of that codec did this */
    long                            vendor;                     /* whose  codec compressed this data */
    CodecQ                          temporalQuality;            /* what was the temporal quality factor  */
    CodecQ                          spatialQuality;             /* what was the spatial quality factor */
    short                           width;                      /* how many pixels wide is this data */
    short                           height;                     /* how many pixels high is this data */
    Fixed                           hRes;                       /* horizontal resolution */
    Fixed                           vRes;                       /* vertical resolution */
    long                            dataSize;                   /* if known, the size of data for this image descriptor */
    short                           frameCount;                 /* number of frames this description applies to */
    Str31                           name;                       /* name of codec ( in case not installed )  */
    short                           depth;                      /* what depth is this data (1-32) or ( 33-40 grayscale ) */
    short                           clutID;                     /* clut id or if 0 clut follows  or -1 if no clut */
};
typedef struct ImageDescription         ImageDescription;
typedef ImageDescription *              ImageDescriptionPtr;
typedef ImageDescriptionPtr *           ImageDescriptionHandle;

/* values for PixMap.pixelFormat*/
enum {
    k16LE555PixelFormat         = FOUR_CHAR_CODE('L555'),       /* 16 bit LE rgb 555 (PC)*/
    k16LE5551PixelFormat        = FOUR_CHAR_CODE('5551'),       /* 16 bit LE rgb 5551*/
    k16BE565PixelFormat         = FOUR_CHAR_CODE('B565'),       /* 16 bit BE rgb 565*/
    k16LE565PixelFormat         = FOUR_CHAR_CODE('L565'),       /* 16 bit LE rgb 565*/
    k24BGRPixelFormat           = FOUR_CHAR_CODE('24BG'),       /* 24 bit bgr */
    k32BGRAPixelFormat          = FOUR_CHAR_CODE('BGRA'),       /* 32 bit bgra    (Matrox)*/
    k32ABGRPixelFormat          = FOUR_CHAR_CODE('ABGR'),       /* 32 bit abgr    */
    k32RGBAPixelFormat          = FOUR_CHAR_CODE('RGBA'),       /* 32 bit rgba    */
    kYUVSPixelFormat            = FOUR_CHAR_CODE('yuvs'),       /* YUV 4:2:2 byte ordering 16-unsigned = 'YUY2'*/
    kYUVUPixelFormat            = FOUR_CHAR_CODE('yuvu'),       /* YUV 4:2:2 byte ordering 16-signed*/
    kYVU9PixelFormat            = FOUR_CHAR_CODE('YVU9'),       /* YVU9 Planar    9*/
    kYUV411PixelFormat          = FOUR_CHAR_CODE('Y411'),       /* YUV 4:1:1 Interleaved  16*/
    kYVYU422PixelFormat         = FOUR_CHAR_CODE('YVYU'),       /* YVYU 4:2:2 byte ordering   16*/
    kUYVY422PixelFormat         = FOUR_CHAR_CODE('UYVY'),       /* UYVY 4:2:2 byte ordering   16*/
    kYUV211PixelFormat          = FOUR_CHAR_CODE('Y211'),       /* YUV 2:1:1 Packed   8*/
    k2vuyPixelFormat            = FOUR_CHAR_CODE('2vuy')        /* UYVY 4:2:2 byte ordering   16*/
};

struct PixMapExtension {
    long                            extSize;                    /*size of struct, duh!*/
    unsigned long                   pmBits;                     /*pixmap attributes bitfield*/
    void *                          pmGD;                       /*this is a GDHandle*/
    long                            pmSeed;
    Fixed                           gammaLevel;                 /*pixmap gammalevel*/
    Fixed                           requestedGammaLevel;
    unsigned long                   reserved2;
    long                            longRowBytes;               /*used when rowBytes > 16382*/
    unsigned long                   signature;
    Handle                          baseAddrHandle;
};
typedef struct PixMapExtension          PixMapExtension;

typedef PixMapExtension *               PixMapExtPtr;
typedef PixMapExtPtr *                  PixMapExtHandle;


struct PixMap {
    Ptr                             baseAddr;                   /*pointer to pixels*/
    short                           rowBytes;                   /*offset to next line*/
    Rect                            bounds;                     /*encloses bitmap*/
    short                           pmVersion;                  /*pixMap version number*/
    short                           packType;                   /*defines packing format*/
    long                            packSize;                   /*length of pixel data*/
    Fixed                           hRes;                       /*horiz. resolution (ppi)*/
    Fixed                           vRes;                       /*vert. resolution (ppi)*/
    short                           pixelType;                  /*defines pixel type*/
    short                           pixelSize;                  /*# bits in pixel*/
    short                           cmpCount;                   /*# components in pixel*/
    short                           cmpSize;                    /*# bits per component*/
    OSType                          pixelFormat;                /*fourCharCode representation*/
    CTabHandle                      pmTable;                    /*color map for this pixMap*/
    PixMapExtHandle                 pmExt;                      /*Handle to pixMap extension*/
};
typedef struct PixMap                   PixMap;
typedef PixMap *                        PixMapPtr;
typedef PixMapPtr *                     PixMapHandle;


struct BitMap {
    Ptr                             baseAddr;
    short                           rowBytes;
    Rect                            bounds;
};
typedef struct BitMap                   BitMap;
typedef BitMap *                        BitMapPtr;
typedef BitMapPtr *                     BitMapHandle;

// ============================== CODECS ===========================

typedef Component                       CompressorComponent;
typedef Component                       DecompressorComponent;
typedef Component                       CodecComponent;

// callbacks:
typedef void* ImageCodecDrawBandCompleteUPP;
typedef void* ICMProgressProcRecord;
typedef void* ICMCompletionProcRecord;
typedef void* ICMDataProcRecord;
typedef void* ICMFrameTimePtr;
typedef void* CDSequenceDataSourcePtr;
typedef void* ICMFrameTimeInfoPtr;

// graphics port
typedef struct OpaqueGrafPtr*           GrafPtr;
typedef GrafPtr                         CGrafPtr;

typedef struct OpaqueRgnHandle*         RgnHandle;

struct CodecCapabilities {
    long                            flags;
    short                           wantedPixelSize;
    short                           extendWidth;
    short                           extendHeight;
    short                           bandMin;
    short                           bandInc;
    short                           pad;
    unsigned long                   time;
    long                            flags2;                     /* field new in QuickTime 4.0 */
};
typedef struct CodecCapabilities        CodecCapabilities;

struct __attribute__((__packed__)) CodecDecompressParams {
    ImageSequence                   sequenceID;                 /* predecompress,banddecompress */
    ImageDescriptionHandle          imageDescription;           /* predecompress,banddecompress */
    Ptr                             data;
    long                            bufferSize;

    long                            frameNumber;
    long                            startLine;
    long                            stopLine;
    long                            conditionFlags;

    CodecFlags                      callerFlags;
 // short
    CodecCapabilities *             capabilities;               /* predecompress,banddecompress */
    ICMProgressProcRecord           progressProcRecord;
    ICMCompletionProcRecord         completionProcRecord;

    ICMDataProcRecord               dataProcRecord;
    CGrafPtr                        port;                       /* predecompress,banddecompress */
    PixMap                          dstPixMap;                  /* predecompress,banddecompress */
    BitMapPtr                       maskBits;
    PixMapPtr                       mattePixMap;
    Rect                            srcRect;                    /* predecompress,banddecompress */
    MatrixRecord *                  matrix;                     /* predecompress,banddecompress */
    CodecQ                          accuracy;                   /* predecompress,banddecompress */
    short                           transferMode;               /* predecompress,banddecompress */
    ICMFrameTimePtr                 frameTime;                  /* banddecompress */
    long                            reserved[1];

                                                                /* The following fields only exist for QuickTime 2.0 and greater */
    SInt8                           matrixFlags;                /* high bit set if 2x resize */
    SInt8                           matrixType;
    Rect                            dstRect;                    /* only valid for simple transforms */

                                                                /* The following fields only exist for QuickTime 2.1 and greater */
    UInt16                          majorSourceChangeSeed;
    UInt16                          minorSourceChangeSeed;
    CDSequenceDataSourcePtr         sourceData;

    RgnHandle                       maskRegion;

                                                                /* The following fields only exist for QuickTime 2.5 and greater */
    OSType **                       wantedDestinationPixelTypes; /* Handle to 0-terminated list of OSTypes */

    long                            screenFloodMethod;
    long                            screenFloodValue;
    short                           preferredOffscreenPixelSize;

                                                                /* The following fields only exist for QuickTime 3.0 and greater */
    ICMFrameTimeInfoPtr             syncFrameTime;              /* banddecompress */
    Boolean                         needUpdateOnTimeChange;     /* banddecompress */
    Boolean                         enableBlackLining;
    Boolean                         needUpdateOnSourceChange;   /* band decompress */
    Boolean                         pad;

    long                            unused;

    CGrafPtr                        finalDestinationPort;

    long                            requestedBufferWidth;       /* must set codecWantsSpecialScaling to indicate this field is valid*/
    long                            requestedBufferHeight;      /* must set codecWantsSpecialScaling to indicate this field is valid*/

                                                                /* The following fields only exist for QuickTime 4.0 and greater */
    Rect                            displayableAreaOfRequestedBuffer; /* set in predecompress*/
    Boolean                         requestedSingleField;
    Boolean                         needUpdateOnNextIdle;
    Boolean                         pad2[2];
    Fixed                           bufferGammaLevel;

                                                                /* The following fields only exist for QuickTime 5.0 and greater */
    UInt32                          taskWeight;                 /* preferred weight for MP tasks implementing this operation*/
    OSType                          taskName;                   /* preferred name (type) for MP tasks implementing this operation*/
};
typedef struct CodecDecompressParams    CodecDecompressParams;



struct ImageSubCodecDecompressCapabilities {
    long                            recordSize;                 /* sizeof(ImageSubCodecDecompressCapabilities)*/
    long                            decompressRecordSize;       /* size of your codec's decompress record*/
    Boolean                         canAsync;                   /* default true*/
    UInt8                           pad0;

                                                                /* The following fields only exist for QuickTime 4.0 and greater */
    UInt16                          suggestedQueueSize;
    Boolean                         canProvideTrigger;

                                                                /* The following fields only exist for QuickTime 5.0 and greater */
    Boolean                         subCodecFlushesScreen;      /* only used on Mac OS X*/
    Boolean                         subCodecCallsDrawBandComplete;
    UInt8                           pad2[1];

                                                                /* The following fields only exist for QuickTime 5.1 and greater */
    Boolean                         isChildCodec;               /* set by base codec before calling Initialize*/
    UInt8                           pad3[3];
};
typedef struct ImageSubCodecDecompressCapabilities ImageSubCodecDecompressCapabilities;


struct ImageSubCodecDecompressRecord {
    Ptr                             baseAddr;
    long                            rowBytes;
    Ptr                             codecData;
    ICMProgressProcRecord           progressProcRecord;
    ICMDataProcRecord               dataProcRecord;
    void *                          userDecompressRecord;       /* pointer to codec-specific per-band data*/
    UInt8                           frameType;
    Boolean                         inhibitMP;                  /* set this in BeginBand to tell the base decompressor not to call DrawBand from an MP task for this frame.  (Only has any effect for MP-capable subcodecs.  New in QuickTime 5.0.)*/
    UInt8                           pad[2];
    long                            priv[2];

                                                                /* The following fields only exist for QuickTime 5.0 and greater */
    ImageCodecDrawBandCompleteUPP   drawBandCompleteUPP;        /* only used if subcodec set subCodecCallsDrawBandComplete; if drawBandCompleteUPP is non-nil, codec must call it when a frame is finished, but may return from DrawBand before the frame is finished. */
    void *                          drawBandCompleteRefCon;     /* Note: do not call drawBandCompleteUPP directly from a hardware interrupt; instead, use DTInstall to run a function at deferred task time, and call drawBandCompleteUPP from that. */
};
typedef struct ImageSubCodecDecompressRecord ImageSubCodecDecompressRecord;


/* These are the bits that are set in the Component flags, and also in the codecInfo struct. */
enum {
    codecInfoDoes1              = (1L << 0),                    /* codec can work with 1-bit pixels */
    codecInfoDoes2              = (1L << 1),                    /* codec can work with 2-bit pixels */
    codecInfoDoes4              = (1L << 2),                    /* codec can work with 4-bit pixels */
    codecInfoDoes8              = (1L << 3),                    /* codec can work with 8-bit pixels */
    codecInfoDoes16             = (1L << 4),                    /* codec can work with 16-bit pixels */
    codecInfoDoes32             = (1L << 5),                    /* codec can work with 32-bit pixels */
    codecInfoDoesDither         = (1L << 6),                    /* codec can do ditherMode */
    codecInfoDoesStretch        = (1L << 7),                    /* codec can stretch to arbitrary sizes */
    codecInfoDoesShrink         = (1L << 8),                    /* codec can shrink to arbitrary sizes */
    codecInfoDoesMask           = (1L << 9),                    /* codec can mask to clipping regions */
    codecInfoDoesTemporal       = (1L << 10),                   /* codec can handle temporal redundancy */
    codecInfoDoesDouble         = (1L << 11),                   /* codec can stretch to double size exactly */
    codecInfoDoesQuad           = (1L << 12),                   /* codec can stretch to quadruple size exactly */
    codecInfoDoesHalf           = (1L << 13),                   /* codec can shrink to half size */
    codecInfoDoesQuarter        = (1L << 14),                   /* codec can shrink to quarter size */
    codecInfoDoesRotate         = (1L << 15),                   /* codec can rotate on decompress */
    codecInfoDoesHorizFlip      = (1L << 16),                   /* codec can flip horizontally on decompress */
    codecInfoDoesVertFlip       = (1L << 17),                   /* codec can flip vertically on decompress */
    codecInfoHasEffectParameterList = (1L << 18),               /* codec implements get effects parameter list call, once was codecInfoDoesSkew */
    codecInfoDoesBlend          = (1L << 19),                   /* codec can blend on decompress */
    codecInfoDoesWarp           = (1L << 20),                   /* codec can warp arbitrarily on decompress */
    codecInfoDoesRecompress     = (1L << 21),                   /* codec can recompress image without accumulating errors */
    codecInfoDoesSpool          = (1L << 22),                   /* codec can spool image data */
    codecInfoDoesRateConstrain  = (1L << 23)                    /* codec can data rate constrain */
};


enum {
    codecInfoDepth1             = (1L << 0),                    /* compressed data at 1 bpp depth available */
    codecInfoDepth2             = (1L << 1),                    /* compressed data at 2 bpp depth available */
    codecInfoDepth4             = (1L << 2),                    /* compressed data at 4 bpp depth available */
    codecInfoDepth8             = (1L << 3),                    /* compressed data at 8 bpp depth available */
    codecInfoDepth16            = (1L << 4),                    /* compressed data at 16 bpp depth available */
    codecInfoDepth32            = (1L << 5),                    /* compressed data at 32 bpp depth available */
    codecInfoDepth24            = (1L << 6),                    /* compressed data at 24 bpp depth available */
    codecInfoDepth33            = (1L << 7),                    /* compressed data at 1 bpp monochrome depth  available */
    codecInfoDepth34            = (1L << 8),                    /* compressed data at 2 bpp grayscale depth available */
    codecInfoDepth36            = (1L << 9),                    /* compressed data at 4 bpp grayscale depth available */
    codecInfoDepth40            = (1L << 10),                   /* compressed data at 8 bpp grayscale depth available */
    codecInfoStoresClut         = (1L << 11),                   /* compressed data can have custom cluts */
    codecInfoDoesLossless       = (1L << 12),                   /* compressed data can be stored in lossless format */
    codecInfoSequenceSensitive  = (1L << 13)                    /* compressed data is sensitive to out of sequence decoding */
};

struct CodecInfo {
    Str31                           typeName;                   /* name of the codec type i.e.: 'Apple Image Compression' */
    short                           version;                    /* version of the codec data that this codec knows about */
    short                           revisionLevel;              /* revision level of this codec i.e: 0x00010001 (1.0.1) */
    long                            vendor;                     /* Maker of this codec i.e: 'appl' */
    long                            decompressFlags;            /* codecInfo flags for decompression capabilities */
    long                            compressFlags;              /* codecInfo flags for compression capabilities */
    long                            formatFlags;                /* codecInfo flags for compression format details */
    UInt8                           compressionAccuracy;        /* measure (1-255) of accuracy of this codec for compress (0 if unknown) */
    UInt8                           decompressionAccuracy;      /* measure (1-255) of accuracy of this codec for decompress (0 if unknown) */
    unsigned short                  compressionSpeed;           /* ( millisecs for compressing 320x240 on base mac II) (0 if unknown)  */
    unsigned short                  decompressionSpeed;         /* ( millisecs for decompressing 320x240 on mac II)(0 if unknown)  */
    UInt8                           compressionLevel;           /* measure (1-255) of compression level of this codec (0 if unknown)  */
    UInt8                           resvd;                      /* pad */
    short                           minimumHeight;              /* minimum height of image (block size) */
    short                           minimumWidth;               /* minimum width of image (block size) */
    short                           decompressPipelineLatency;  /* in milliseconds ( for asynchronous codecs ) */
    short                           compressPipelineLatency;    /* in milliseconds ( for asynchronous codecs ) */
    long                            privateData;
};
typedef struct CodecInfo                CodecInfo;


