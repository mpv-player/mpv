typedef long ComponentResult;
typedef unsigned char UInt8;

typedef struct {
    UInt8	flags;
    UInt8	paramSize;
    short	what;
    long	params[1];
} ComponentParameters;

typedef struct {
    long	data[1];
} ComponentInstace;

typedef int OSType;

typedef struct {
    OSType	componentType;
    OSType	componentSubType;
    OSType	componentManufacturer;
    unsigned long componentFlags;
    unsigned long componentFlagsMask;
} ComponentDescription;
