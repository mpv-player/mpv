typedef long ComponentResult;
typedef unsigned char UInt8;

struct ComponentParameters
{
    UInt8	flags;
    UInt8	paramSize;
    short	what;
    long	params[1];
};

struct ComponentInstace
{
    long	data[1];
};

typedef int OSType;

struct ComponentDescription
{
    OSType	componentType;
    OSType	componentSubType;
    OSType	componentManufacturer;
    unsigned long componentFlags;
    unsigned long componentFlagsMask;
};
